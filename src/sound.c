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

//\cond
#include <errno.h>                      // for errno
#include <pthread.h>                    // for pthread_create, etc
#include <semaphore.h>                  // for sem_post, sem_wait, etc
#include <stddef.h>                     // for size_t
#include <stdio.h>                      // for NULL, fclose, fopen, etc
#include <stdlib.h>                     // for free
#include <string.h>                     // for strerror, strlen
#include <sys/stat.h>                   // for stat, fstat, off_t
//\endcond

#include <ao/ao.h>                      // for ao_sample_format, ao_close, etc
#include <mpg123.h>                     // for mpg123_close, mpg123_delete, etc

#include "sound.h"
#include "helper.h"                     // for lmalloc
#include "http.h"                       // for http_response, etc
#include "log.h"                        // for _log
#include "network/network.h"            // for network_conn
#include "track.h"                      // for track, FLAG_CACHED
#include "tui.h"
#include "url.h"                        // for url, url_connect, etc

#define BUFFER_SIZE 256 * 1024 * 1024

#define CACHE_STREAM_FOLDER "./cache/streams/"
#define CACHE_STREAM_EXT ".mp3"

static sem_t sem_io;
static sem_t sem_play;
static sem_t sem_stopped;
static pthread_t thread_io;   // thread handling the download from sc.com
static pthread_t thread_play; // thread decoding and playing downloaded data

static sem_t sem_data_available;

static mpg123_handle *mh = NULL;

static volatile int           buffer_pos  = 0;
static volatile char         *buffer      = NULL;
static volatile char         *request     = NULL;
static volatile struct track *track       = NULL;
static volatile int           current_pos = 0;
static volatile bool          stopped     = false;
static volatile bool          terminate   = false;

static void (*time_callback)(int);

void* _thread_io_function(void *unused) {
	do {
		sem_wait(&sem_io); // wait until IO is required (downloading a new track)
		if(terminate) return NULL;
		if(stopped) {
			sem_post(&sem_stopped);
			continue;
		}

		char cache_file[256]; // TODO
		sprintf(cache_file, CACHE_STREAM_FOLDER"%d_%d"CACHE_STREAM_EXT, track->user_id, track->track_id);
		FILE *fh = fopen(cache_file, "r");
		if(fh) {
			_log("using file '%s'", cache_file);

			struct stat cache_stat;
			fstat(fileno(fh), &cache_stat);

			fread((void *) buffer, 1, cache_stat.st_size, fh);
			fclose(fh);

			buffer_pos = cache_stat.st_size;

			_log("whole file read!");

			sem_post(&sem_data_available);
			sem_post(&sem_play);
		} else {
			struct url *u = url_parse_string((char*) request);
			if(!u) {
				_log("invalid request '%s'", request);
				continue;
			}

			url_connect(u);
			struct network_conn *nwc = u->nwc;
			if(!nwc) {
				tui_submit_status_line_print(cline_warning, F_BOLD"Error:"F_RESET" connection failed");
				break;
			}
			struct http_response *resp = http_request_get_only_header(nwc, u->request, u->host, MAX_REDIRECT_STEPS);
			nwc = resp->nwc;

			if(nwc) {
				_log("request sent: status %i, %i + %i Bytes in response", resp->http_status, resp->header_length, resp->content_length);

				sem_post(&sem_play);

				size_t remaining_buffer = BUFFER_SIZE;
				_log("required length: %i Bytes, actual length: %i Bytes", resp->content_length, remaining_buffer);
				if(resp->content_length < remaining_buffer) {
					remaining_buffer = resp->content_length;
				}

				while( remaining_buffer && !terminate && !stopped && request ) {
					int ret = nwc->recv(nwc, (char*) buffer + buffer_pos, remaining_buffer);
					if(ret > 0) {
						sem_post(&sem_data_available);
						__sync_fetch_and_sub(&remaining_buffer, ret);
						__sync_fetch_and_add(&buffer_pos,       ret);
					}
				}

				_log("streaming done, %d Bytes streamed", buffer_pos);
				nwc->disconnect(nwc);

				if(resp->content_length == buffer_pos) {
					_log("saving buffer to file '%s'", cache_file);
					FILE *fh = fopen(cache_file, "w");
					fwrite((void *) buffer, 1, buffer_pos, fh);
					fclose(fh);

					track->flags |= FLAG_CACHED;
				} else {
					_log("server announced %i Bytes, only got %i Bytes", resp->content_length, buffer_pos);
					_log("NOT saving buffer to file");
				}
			}
			http_response_destroy(resp);
		}

		if(stopped) {
			sem_post(&sem_stopped);
		}
	} while(!terminate);

	return NULL;
}

void* _thread_play_function(void *unused) {
	ao_device *dev = NULL;
	ao_option *ao_opt = NULL;
	ao_append_option(&ao_opt, "quiet", NULL);

	do {
		_log("waiting for playback");
		sem_wait(&sem_play); // wait until data can be decoded and played
		if(terminate) {
			ao_free_options(ao_opt);
			if(dev) {
				ao_close(dev);
			}
			return NULL;
		}

		_log("waiting for data");
		sem_wait(&sem_data_available);

		_log("starting playback");
		int channels, encoding;
		long rate;
		ao_sample_format format;

		off_t frame_offset;
		unsigned char *audio = NULL;
		size_t done;

		double time_per_frame = 0;

		int last_buffer_pos = 0;
		bool started = false;

		int last_reported_pos = -1;

		long decoded_frames = 0;
		while(!terminate && !stopped && request) {
			int current_buffer_pos = buffer_pos;
			if(last_buffer_pos != current_buffer_pos) {
				mpg123_feed(mh, (const unsigned char*)buffer + last_buffer_pos, current_buffer_pos - last_buffer_pos);
				last_buffer_pos = current_buffer_pos;
			}

			int err = mpg123_decode_frame(mh, &frame_offset, &audio, &done);
			switch(err) {
				case MPG123_NEW_FORMAT:
					mpg123_getformat(mh, &rate, &channels, &encoding);
					format.bits = mpg123_encsize(encoding) * 8; // 8 bit per Byte
					format.rate = rate;
					format.channels = channels;
					format.byte_format = AO_FMT_NATIVE;
					format.matrix = 0;

					time_per_frame = mpg123_tpf(mh);

					ao_info *info = ao_driver_info(ao_default_driver_id());
					_log("libao: opening default output '%s' with: rate: %i, %i channels, %i bits per sample | %fs per frame", info->name, format.rate, format.channels, format.bits, time_per_frame);
					if(dev) {
						ao_close(dev);
					}
					dev = ao_open_live(ao_default_driver_id(), &format, ao_opt);
					break;

				case MPG123_OK:
					//_log("%i Bytes (= %i Samples) ready in buffer", done, done / (format.channels * format.bits/8));

					decoded_frames++;
					ao_play(dev, (char*)audio, done);

					current_pos = (int) (time_per_frame * decoded_frames);

					// only report position of playback if it has changed
					// meant to reduce the number of redraws possibly issued by time_callback
					if(current_pos != last_reported_pos) {
						last_reported_pos = current_pos;
						time_callback(current_pos);
					}

					started = true;
					break;

				case MPG123_NEED_MORE:
					if(started) {
						_log("your network is too slow: buffer empty!");
					}
					sem_wait(&sem_data_available); // wait for data
					break;

				default:
					break;
			}
		}

		if(stopped) {
			sem_post(&sem_stopped);
		}
	} while(!terminate);
	ao_free_options(ao_opt);
	ao_close(dev);

	return NULL;
}

bool sound_init(void (*_time_callback)(int)) {
	time_callback = _time_callback;

	buffer = lmalloc(BUFFER_SIZE);
	if(buffer) {
		_log("initializing libao...");
		ao_initialize();

		mpg123_init();
		mh = mpg123_new(NULL, NULL);
		mpg123_open_feed(mh);

		long mpg123_verbose = -1;
		mpg123_getparam(mh, MPG123_VERBOSE, &mpg123_verbose, NULL);
		_log("libmpg123 verbosity: %i", mpg123_verbose);

		mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_QUIET, 0.0);

		if(!sem_init(&sem_io, 0, 0)) {
			if(sem_init(&sem_play, 0, 0)) {
				_log("sem_init failed: %s", strerror(errno));
				free((void*) buffer);
				sem_destroy(&sem_io);
				return false;
			}

			if(sem_init(&sem_data_available, 0, 0)) {
				_log("sem_init failed: %s", strerror(errno));
				free((void*) buffer);
				sem_destroy(&sem_io);
				sem_destroy(&sem_play);
				return false;
			}

			sem_init(&sem_stopped, 0, 0);

			pthread_create(&thread_io,   NULL, _thread_io_function,   NULL);
			pthread_create(&thread_play, NULL, _thread_play_function, NULL);

			return true;
		} else {
			_log("sem_init failed: %s", strerror(errno));
		}

		free((void*) buffer);
	}

	return false;
}

bool sound_finalize() {
	// TODO
	terminate = true;

	_log("waiting for threads to terminate...");

	_log("thread_io...");
	sem_post(&sem_io);
	pthread_join(thread_io, NULL);

	_log("thread_play...");
	sem_post(&sem_data_available);
	sem_post(&sem_play);
	pthread_join(thread_play, NULL);

	// cleanup libmpg123
	mpg123_close(mh);
	mpg123_delete(mh);
	mpg123_exit();

	// cleanup libao
	ao_shutdown();

	free((void*) buffer);

	return true;
}

#define SEM_SET_TO_ZERO(S) {int sval; while(!sem_getvalue(&S, &sval) && sval) {sem_wait(&S);}}

bool sound_stop() {
	stopped = true;

	_log("waiting for threads to stop playback");

	sem_post(&sem_io);
	sem_post(&sem_play);
	sem_post(&sem_data_available);

	sem_wait(&sem_stopped);
	sem_wait(&sem_stopped);

	request = NULL;
	track   = NULL;
	stopped = false;

	SEM_SET_TO_ZERO(sem_io)
	SEM_SET_TO_ZERO(sem_play)
	SEM_SET_TO_ZERO(sem_data_available)

	/* reinitialize the mh to drop 'old' data */
	mpg123_close(mh);
	mpg123_delete(mh);

	mh = mpg123_new(NULL, NULL);
	mpg123_open_feed(mh);

	_log("stopping done");

	return true;
}

int sound_get_current_pos() {
	return current_pos;
}

bool sound_play(struct track *_track, char *url_suffix) {
	request = lmalloc(strlen(_track->stream_url) + strlen(url_suffix) + 1);
	sprintf((char*) request, "%s%s", _track->stream_url, url_suffix);

	track       = _track;
	current_pos = 0;
	buffer_pos  = 0;

	sem_post(&sem_io);
/*
	struct timespec prebuffer_start;
	clock_gettime(CLOCK_MONOTONIC, &prebuffer_start);

	printf("pre-buffering..."); fflush(stdout);
	size_t remaining_buffer = buffer_size;
	int ret;
	while( remaining_buffer ) {
		ret = nwc2->recv(nwc2, buffer + (buffer_size - remaining_buffer), remaining_buffer);
		if(ret > 0) {
			remaining_buffer -= ret;
		}
	}
	struct timespec prebuffer_end;
	clock_gettime(CLOCK_MONOTONIC, &prebuffer_end);

	long prebuffer_ms = (prebuffer_end.tv_sec - prebuffer_start.tv_sec) * 1000 + (prebuffer_end.tv_nsec - prebuffer_start.tv_nsec) / (1000 * 1000);

	printf("done, %d Bytes in prebuffer, %ums, %fkB/s\n", buffer_size, prebuffer_ms, ((float)buffer_size / prebuffer_ms));

	mpg123_feed(mh, (const unsigned char*) buffer, buffer_size);
*/
	return true;
}

