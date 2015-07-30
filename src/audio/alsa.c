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

#include <alsa/asoundlib.h>
#include <mpg123.h>

//\cond
#include <assert.h>
#include <errno.h>                      // for errno
#include <stddef.h>                     // for NULL, size_t
#include <stdlib.h>                     // for atexit
#include <string.h>                     // for strerror
//\endcond

#include "../log.h"

static void finalize();

static snd_pcm_t *pcm;

void audio_play(void *buffer, size_t size) {

	snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames(pcm, size);

	int err;
	if(frames != (err = snd_pcm_writei(pcm, buffer, frames))) {
		_log("libalsa: %s", snd_strerror(err));
		if(snd_pcm_prepare(pcm) >= 0) {
			snd_pcm_writei(pcm, buffer, frames);
		}
	}
}


static snd_pcm_format_t mpg123_to_alsa_encoding(unsigned int mpg123_encoding) {
	switch(mpg123_encoding) {
		case MPG123_ENC_SIGNED_8:    return SND_PCM_FORMAT_S8;
		case MPG123_ENC_UNSIGNED_8:  return SND_PCM_FORMAT_U8;
		case MPG123_ENC_SIGNED_16:   return SND_PCM_FORMAT_S16_LE;
		case MPG123_ENC_UNSIGNED_16: return SND_PCM_FORMAT_U16_LE;
		default: _log("unknown mpg123_encoding %x", mpg123_encoding);
	}
	return 0;
}

bool audio_set_format(unsigned int encoding, unsigned int rate, unsigned int channels) {
	int err;
	snd_pcm_format_t format = mpg123_to_alsa_encoding(encoding);

	_log("snd_pcm_set_params(pcm, %x, SND_PCM_ACCESS_RW_INTERLEAVED, %i, %i, 1, 0)", format, channels, rate);
	if( (err = snd_pcm_set_params(pcm, format, SND_PCM_ACCESS_RW_INTERLEAVED, channels, rate, 1, 0)) ) {
		_log("libalsa: snd_pcm_set_params failed: %s", snd_strerror(err));
		return false;
	}

	return true;
}

bool audio_init() {
	_log("initializing libalsa...");
	int err;
	if(( err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) )) {
		_log("libalsa: snd_pcm_open failed: %s", snd_strerror(err));
		return false;
	}

	return true;
}

static void finalize() {
	// TODO
}

