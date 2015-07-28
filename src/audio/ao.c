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

#include "ao.h"
//#include "../_hard_config.h"

#include <ao/ao.h>                      // for ao_close, etc

//\cond
#include <errno.h>                      // for errno
#include <stddef.h>                     // for NULL, size_t
#include <stdlib.h>                     // for atexit
#include <string.h>                     // for strerror
//\endcond

#include "../log.h"                     // for _log

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

void sound_ao_play(void *buffer, size_t size) {
	ao_play(dev, (char*)buffer, size);
}

bool sound_ao_set_format(unsigned int bits, unsigned int rate, unsigned int channels) {

	ao_sample_format format = {.bits = bits, .rate = rate, .channels = channels, .byte_format = AO_FMT_NATIVE, .matrix = 0};

	if(dev) {
		ao_close(dev);
	}

	ao_info *info = ao_driver_info(ao_default_driver_id());
	_log("libao: opening default output '%s' with: rate: %i, %i channels, %i bits per sample", info->name, format.rate, format.channels, format.bits);

	dev = ao_open_live(ao_default_driver_id(), &format, options);
	if(!dev) {
		_log("ao_open_live: %s", ao_strerror(errno));
		return false;
	}
	return true;
}

bool sound_ao_init() {
	_log("initializing libao...");
	ao_initialize();

	if(atexit(finalize)) {
		_log("atexit: %s", strerror(errno));
		return false;
	}

	ao_append_option(&options, "quiet", NULL);
	ao_append_global_option("quiet", "true");

	return true;
}

static void finalize() {
	ao_free_options(options);

	if(dev) {
		ao_close(dev);
	}
	// cleanup libao
	ao_shutdown();
}

