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

/** \file cache.c
 *  \brief Implementation of the cache
 *
 *  This file contains the actual implementation of the cache for both
 *  the actual stream (an .mp3 file) and the track_lists.
 *
 *  \todo move code for track_lists to cache.c
 */

#include "_hard_config.h"
#include "cache.h"

//\cond
#include <errno.h>                      // for errno, ENOENT
#include <stdio.h>                      // for fclose, fopen, snprintf, etc
#include <string.h>                     // for strlen, strerror
#include <sys/stat.h>                   // for stat, fstat, mkdir
//\endcond

#include "config.h"
#include "helper.h"
#include "log.h"                        // for _log
#include "track.h"                      // for track

bool cache_track_exists(struct track *track) {
	char *cache_path = config_get_cache_path();
	const size_t buffer_size = strlen(cache_path) + 1 + strlen(CACHE_STREAM_FOLDER) + 1 + 64 + strlen(CACHE_STREAM_EXT);
	char cache_file[buffer_size];
	snprintf(cache_file, buffer_size, "%s/"CACHE_STREAM_FOLDER"/%d_%d"CACHE_STREAM_EXT, cache_path, track->user_id, track->track_id);

	FILE *fh = fopen(cache_file, "r");

	if(fh) {
		fclose(fh);
		return true;
	}
	return false;
}

struct mmapped_file cache_track_get(struct track *track) {
	char *cache_path = config_get_cache_path();
	const size_t buffer_size = strlen(cache_path) + 1 + strlen(CACHE_STREAM_FOLDER) + 1 + 64 + strlen(CACHE_STREAM_EXT);
	char cache_file[buffer_size];
	snprintf(cache_file, buffer_size, "%s/"CACHE_STREAM_FOLDER"/%d_%d"CACHE_STREAM_EXT, cache_path, track->user_id, track->track_id);

	return file_read_contents(cache_file);
}

bool cache_track_save(struct track *track, void *buffer, size_t size) {
	char *cache_path = config_get_cache_path();
	const size_t buffer_size = strlen(cache_path) + 1 + strlen(CACHE_STREAM_FOLDER) + 1 + 64 + strlen(CACHE_STREAM_EXT);
	char cache_file[buffer_size];
	snprintf(cache_file, buffer_size, "%s/"CACHE_STREAM_FOLDER"/%d_%d"CACHE_STREAM_EXT, cache_path, track->user_id, track->track_id);

	FILE *fh = fopen(cache_file, "w");

	if(fh) {
		fwrite((void *) buffer, 1, size, fh);
		fclose(fh);

		return true;
	} else {
		if(ENOENT == errno) {
			// create the cache folder if it does not exist
			// and retry saving the track to cache
			if(!mkdir(CACHE_STREAM_FOLDER, 0770)) {
				_log("created "CACHE_STREAM_FOLDER", now saving track to cache");
				return cache_track_save(track, buffer, size);
			} else {
				_log("failed to create "CACHE_STREAM_FOLDER": %s", strerror(errno));
			}
		} else {
			_log("failed to open file '%s': %s", cache_file, strerror(errno));
		}
	}

	return false;
}

