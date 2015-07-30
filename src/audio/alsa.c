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

#define ALSA_DEFAULT_CARD "default"
#define ALSA_MASTER_MIXER "Master"

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
	if(( err = snd_pcm_open(&pcm, ALSA_DEFAULT_CARD, SND_PCM_STREAM_PLAYBACK, 0) )) {
		_log("libalsa: snd_pcm_open failed: %s", snd_strerror(err));
		return false;
	}

	if(atexit(finalize)) {
		_log("atexit: %s", strerror(errno));
		return false;
	}

	return true;
}

static void finalize() {
	snd_pcm_close(pcm);
}

#define ALSA_ERROR_CHECK(FUN) {int err = FUN; if(err) {_log("libalsa: "#FUN" failed: %s", snd_strerror(err)); return -1;} }
int audio_change_volume(off_t delta) {
	snd_mixer_t *mixer;

	ALSA_ERROR_CHECK( snd_mixer_open(&mixer, 0) );
	ALSA_ERROR_CHECK( snd_mixer_attach(mixer, ALSA_DEFAULT_CARD) );
	ALSA_ERROR_CHECK( snd_mixer_selem_register(mixer, NULL, NULL) );
	ALSA_ERROR_CHECK( snd_mixer_load(mixer) );

	snd_mixer_selem_id_t *sid;
	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, ALSA_MASTER_MIXER);

	snd_mixer_elem_t* elem = snd_mixer_find_selem(mixer, sid);

	long min;
	long max;
	ALSA_ERROR_CHECK( snd_mixer_selem_get_playback_volume_range(elem, &min, &max) );
	long range = max - min;

	long cur;
	ALSA_ERROR_CHECK( snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &cur) );
	long cur_perc = ( 100 * (cur - min) ) / range;

	long target_perc = cur_perc + delta;
	if(target_perc > 100) target_perc = 100;
	if(target_perc <   0) target_perc =   0;

	ALSA_ERROR_CHECK( snd_mixer_selem_set_playback_volume_all(elem, min + (target_perc * range) / 100) );

	// return the current volume (after modification)
	ALSA_ERROR_CHECK( snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &cur) );
	return ( 100 * (cur - min) ) / range;;
}

