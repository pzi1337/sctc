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

#include <ao/ao.h>                      // for ao_close, etc

//\cond
#include <assert.h>
#include <errno.h>                      // for errno
#include <stddef.h>                     // for NULL, size_t
#include <stdlib.h>                     // for atexit
#include <string.h>                     // for strerror
//\endcond

#include "../log.h"

static void finalize();

static ao_device *dev = NULL;
static ao_option *options = NULL;

static char* ao_strerror(int err) {
	switch(err) {
		case AO_ENODRIVER:   return "no driver with given id exists";
		case AO_ENOTLIVE:    return "not live";
		case AO_EBADOPTION:  return "bad option";
		case AO_EOPENDEVICE: return "failed to open device";
		case AO_EFAIL:       return "error unknown";
		default:             return "<unknown ao error>";
	}
}

void audio_play(void *buffer, size_t size) {
	ao_play(dev, (char*)buffer, size);
}

bool audio_set_format(unsigned int encoding, unsigned int rate, unsigned int channels) {

	if(dev) {
		ao_close(dev);
	}

	assert(-1 != ao_default_driver_id());

	ao_sample_format format = {.bits = mpg123_encsize(encoding) * 8, .rate = rate, .channels = channels, .byte_format = AO_FMT_NATIVE, .matrix = 0};
	ao_info *info = ao_driver_info(ao_default_driver_id());

	_log("libao: opening default output '%s' with: rate: %i, %i channels, %i bits per sample", info->name, format.rate, format.channels, format.bits);

	dev = ao_open_live(ao_default_driver_id(), &format, options);
	if(!dev) {
		_log("ao_open_live: %s", ao_strerror(errno));
		return false;
	}
	return true;
}

bool audio_init() {
	_log("initializing libao...");
	ao_initialize();

	_log("libao: available drivers:");
	int driver_count;
	ao_info **drivers = ao_driver_info_list(&driver_count);
	for(int i = 0; i < driver_count; i++) {
		_log("libao: - %s", drivers[i]->name);
	}

	if(atexit(finalize)) {
		_log("atexit: %s", strerror(errno));
		return false;
	}

	ao_append_option(&options, "quiet", NULL);
	ao_append_global_option("quiet", "true");

	return true;
}

static void finalize() {
	if(dev) {
		ao_close(dev);
	}

	ao_free_options(options);

	// cleanup libao
	ao_shutdown();
}

