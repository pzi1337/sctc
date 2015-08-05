/*
	SCTC - the soundcloud.com client
	Copyright (C) 2015   Christian Eichler

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>
*/


#include "_hard_config.h"
#include "sound.h"

//\cond
#include <errno.h>                      // for errno
#include <pthread.h>                    // for pthread_create, etc
#include <semaphore.h>                  // for sem_post, sem_wait, etc
#include <stddef.h>                     // for NULL, size_t
#include <stdlib.h>                     // for free, atexit
#include <string.h>                     // for memcpy, strerror
#include <sys/types.h>                  // for off_t, ssize_t
#include <unistd.h>                     // for SEEK_SET, SEEK_CUR, etc
//\endcond

#include <mpg123.h>                     // for mpg123_strerror, etc

#include "audio/ao_module.h"            // for ao_module_load, etc
#include "cache.h"                      // for cache_track_get, etc
#include "config.h"                     // for config_get_equalizer
#include "downloader.h"                 // for download_state, etc
#include "helper.h"                     // for lmalloc
#include "log.h"                        // for _log
#include "state.h"                      // for state_set_volume
#include "track.h"                      // for track, etc

static char *aos[] = {"audio/alsa.so", "audio/ao.so", NULL};

#define SEEKPOS_NONE ((unsigned int) ~0)

static audio_init_t          audio_init          = NULL;
static audio_set_format_t    audio_set_format    = NULL;
static audio_get_volume_t    audio_get_volume    = NULL;
static audio_change_volume_t audio_change_volume = NULL;
static audio_play_t          audio_play          = NULL;

struct io_handle {
	size_t                 position;       //< the current position
	struct download_state *download_state; //< contains the maximum (currently possible) position
};

static sem_t sem_stopped;
static pthread_t thread_play; // thread decoding and playing downloaded data

static sem_t sem_data_available;

static volatile unsigned int seek_to_pos = SEEKPOS_NONE;
static volatile bool         stopped     = false;
static volatile bool         terminate   = false;

static struct download_state *state = NULL;

static void (*time_callback)(int);

static void sound_finalize();

static ssize_t _io_read(void *_iohandle, void *mpg123buffer, size_t count) {
	struct io_handle *iohandle    = (struct io_handle*) _iohandle;
	struct download_state *dlstat = iohandle->download_state;

	size_t bytes_copied = 0;
	if(iohandle->position >= dlstat->bytes_total - 4096) {
		size_t bytes_available = dlstat->bytes_total - iohandle->position;
		bytes_copied = count < bytes_available ? count : bytes_available;

		memcpy(mpg123buffer, &dlstat->buffer[iohandle->position], bytes_copied);
		iohandle->position += bytes_copied;
	} else {
		size_t bytes_available = 0;
		if(dlstat->bytes_recvd > iohandle->position) {
			bytes_available = dlstat->bytes_recvd - iohandle->position;
		}
		bytes_copied = count < bytes_available ? count : bytes_available;

		memcpy(mpg123buffer, &dlstat->buffer[iohandle->position], bytes_copied);
		iohandle->position += bytes_copied;
	}

	if(bytes_copied < count) {
		_log("WARNING: %zu bytes at position %zu requested, but can only deliver %zu bytes", count, iohandle->position, bytes_copied);
	}

	return bytes_copied;
}

static off_t _io_seek(void *_iohandle, off_t offset, int whence) {
	struct io_handle *iohandle    = (struct io_handle*) _iohandle;
	struct download_state *dlstat = iohandle->download_state;

	size_t abs_offset = 0;
	switch(whence) {
		case SEEK_SET: abs_offset = offset;                       break;
		case SEEK_CUR: abs_offset = iohandle->position  + offset; break;
		case SEEK_END: abs_offset = dlstat->bytes_total + offset; break;
		default: {
			_log("invalid value for whence: %i", whence);
			return (off_t) -1;
		}
	}

	if(abs_offset > dlstat->bytes_total) {
		_log("cannot seek to %zu, only have at max. %zu bytes", abs_offset, dlstat->bytes_total);
		return (off_t) -1;
	}

	iohandle->position = abs_offset;
	return abs_offset;
}

static void _io_cleanup(void *iohandle) {
	free(iohandle);
	_log("cleanup called");
}

/** \brief Reinitialize libmpg123
 *
 *  \param mh  The old handle to close (may be NULL)
 *  \return    A pointer to the new handle (or NULL in case of failure)
 */
static mpg123_handle* mpg123_init_playback(mpg123_handle *mh, struct download_state *download_state) {
	// close the old mh if available
	if(mh) {
		mpg123_close(mh);
		mpg123_delete(mh);
	}

	mpg123_handle *new_mh = mpg123_new(NULL, NULL);
	if(!new_mh) {
		_log("mpg123_new failed");
		return NULL;
	}

	if(MPG123_OK != mpg123_replace_reader_handle(new_mh, _io_read, _io_seek, _io_cleanup)) {
		_log("mpg123_replace_reader_handle: %s", mpg123_strerror(new_mh));
		return NULL;
	}

	struct io_handle *iohandle = lmalloc( sizeof(struct io_handle) );
	iohandle->position       = 0;
	iohandle->download_state = download_state;

	if(MPG123_OK != mpg123_open_handle(new_mh, iohandle)) {
		_log("mpg123_open_handle: %s", mpg123_strerror(new_mh));
		free(iohandle);
		return NULL;
	}

	if(MPG123_OK != mpg123_param(new_mh, MPG123_FLAGS, MPG123_QUIET, 0.0)) {
		_log("mpg123_param: %s", mpg123_strerror(new_mh));
		free(iohandle);
		return NULL;
	}

	// set the new values for the equalizer obtained from configuration
	for(int i = 0; i < 32; i++) {
		mpg123_eq(new_mh, MPG123_LR, i, config_get_equalizer(i));
	}

	return new_mh;
}

static void io_callback(struct download_state *state) {
	sem_post(&sem_data_available);

	struct track *track = state->track;

	if(state->finished) {
		_log("download of `%s` is finished, saving track to cache", track->name);
		if(cache_track_save(track, (void*) state->buffer, state->bytes_recvd)) {
			track->flags |= FLAG_CACHED;
		}
	}
}

/** \brief main function for playback thread.
*
*  \param unused  Unused parameter (never read), required due to pthread interface
*  \return NULL   Unused return value, required due to pthread interface
*/
static void* _thread_play_function(void *unused UNUSED) {
	mpg123_handle *mh = NULL;
	do {
		_log("waiting for data to play");
		sem_wait(&sem_data_available);

		if(terminate) {
			return NULL;
		}

		mh = mpg123_init_playback(mh, state);

		unsigned int last_reported_pos = ~0;

		bool playback_done = false;

		size_t done;
		off_t frame_offset;
		unsigned char *audio = NULL;

		while(!terminate && !stopped && !playback_done) {
			int err = mpg123_decode_frame(mh, &frame_offset, &audio, &done);
			switch(err) {
				case MPG123_NEW_FORMAT: {
					int channels, encoding;
					long rate;

					mpg123_getformat(mh, &rate, &channels, &encoding);
					audio_set_format(encoding, rate, channels);
					break;
				}

				case MPG123_OK:
					audio_play(audio, done);

					unsigned int current_pos = (unsigned int) (mpg123_tpf(mh) * mpg123_tellframe(mh));
					// only report position of playback if it has changed
					// meant to reduce the number of redraws possibly issued by time_callback
					if(current_pos != last_reported_pos) {
						last_reported_pos = current_pos;

						time_callback(current_pos);
						state_set_current_time(current_pos);
					}
					break;

				default:
					break;
			}

			// do seeking to specified position if required
			if(SEEKPOS_NONE != seek_to_pos) {
				off_t target_frame_off = mpg123_timeframe(mh, seek_to_pos);
				if(0 > target_frame_off) {
					_log("cannot get offset for time %us: %s", seek_to_pos, mpg123_strerror(mh));
				} else {
					_log("requested seek to %us, frame at %zi", seek_to_pos, target_frame_off);
					if(0 > mpg123_seek_frame(mh, target_frame_off, SEEK_SET)) {
						_log("mpg123_seek_frame: %s", mpg123_strerror(mh));
					}
				}

				// reset seek_to_pos to avoid seeking multiple times
				seek_to_pos = SEEKPOS_NONE;
			}
		}

		if(stopped) {
			sem_post(&sem_stopped);
		}
	} while(!terminate);

	mpg123_close(mh);
	mpg123_delete(mh);

	return NULL;
}

bool sound_init(void (*_time_callback)(int)) {
	// find the correct soundsystem to use
	for(unsigned int i = 0; aos[i]; i++) {
		if(ao_module_load(aos[i], &audio_init, &audio_play, &audio_set_format, &audio_get_volume, &audio_change_volume))
			break;
	}

	if(!audio_init) {
		_log("failed to load any soundsystem");
		return false;
	}

	time_callback = _time_callback;

	audio_init();
	if(audio_get_volume)
		state_set_volume(audio_get_volume());

	mpg123_init();

	if(sem_init(&sem_data_available, 0, 0)) {
		_log("sem_init failed: %s", strerror(errno));
		return false;
	}

	sem_init(&sem_stopped, 0, 0);

	pthread_create(&thread_play, NULL, _thread_play_function, NULL);

	if(atexit(sound_finalize)) {
		_log("atexit: %s", strerror(errno));
	}

	return true;
}

int sound_change_volume(off_t delta) {
	if(!audio_change_volume) {
		return -1;
	}

	return audio_change_volume(delta);
}

static void sound_finalize() {
	terminate = true;

	_log("waiting for threads to terminate...");

	_log("thread_play...");
	sem_post(&sem_data_available);
	pthread_join(thread_play, NULL);

	// cleanup libmpg123
	mpg123_exit();

	ao_module_unload();
}

#define SEM_SET_TO_ZERO(S) {int sval; while(!sem_getvalue(&S, &sval) && sval) {sem_wait(&S);}}

bool sound_stop() {

	SEM_SET_TO_ZERO(sem_data_available)

	stopped = true;

	_log("waiting for threads to stop playback");

	// if stop() is called by the thread doing the playback,
	// for instance caused by the `time_callback`, then we may not block
	if(pthread_self() != thread_play) {
		sem_post(&sem_data_available);

		sem_wait(&sem_stopped);
	}

	// TODO: free state here?
	state = NULL;

	stopped = false;

	_log("stopping done");

	return true;
}

void sound_seek(unsigned int pos) {
	seek_to_pos = pos;
}

bool sound_play(struct track *track) {

	if(state) sound_stop();

	seek_to_pos = track->current_position ? track->current_position : SEEKPOS_NONE;

	size_t cache_track_size;
	void  *cache_track_buffer;
	if( ( cache_track_buffer = cache_track_get(track, &cache_track_size) ) ) {
		_log("using file from cache for '%s' by '%s'", track->name, track->username);

		struct download_state *cache_state = lmalloc(sizeof(struct download_state));
		if(!cache_state) {
			return false;
		}

		cache_state->track       = track;
		cache_state->started     = true;
		cache_state->finished    = true;
		cache_state->bytes_recvd = cache_track_size;
		cache_state->bytes_total = cache_track_size;
		cache_state->buffer      = cache_track_buffer;

		state = cache_state;

		sem_post(&sem_data_available);
	} else {
		state = downloader_queue_buffer(track, io_callback);
		if(!state) {
			return false;
		}
	}

	return true;
}

