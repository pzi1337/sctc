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
#include <stdio.h>                      // for fclose, fileno, fopen, etc
#include <string.h>                     // for strlen
#include <sys/stat.h>                   // for stat, fstat
#include <errno.h>
//\endcond

#include "cache.h"

#include "log.h"                        // for _log
#include "track.h"                      // for track

#define CACHE_STREAM_FOLDER "./cache/streams/"
#define CACHE_STREAM_EXT ".mp3"

bool cache_track_exists(struct track *track) {
	const size_t buffer_size = strlen(CACHE_STREAM_FOLDER) + 1 + 64 + strlen(CACHE_STREAM_EXT);
	char cache_file[buffer_size];
	snprintf(cache_file, buffer_size, CACHE_STREAM_FOLDER"%d_%d"CACHE_STREAM_EXT, track->user_id, track->track_id);

	FILE *fh = fopen(cache_file, "r");

	if(fh) {
		fclose(fh);
		return true;
	}
	return false;
}

size_t cache_track_get(struct track *track, void *buffer) {
	const size_t buffer_size = strlen(CACHE_STREAM_FOLDER) + 1 + 64 + strlen(CACHE_STREAM_EXT);
	char cache_file[buffer_size];
	snprintf(cache_file, buffer_size, CACHE_STREAM_FOLDER"%d_%d"CACHE_STREAM_EXT, track->user_id, track->track_id);

	FILE *fh = fopen(cache_file, "r");

	if(fh) {
		_log("using file '%s'", cache_file);

		struct stat cache_stat;
		fstat(fileno(fh), &cache_stat);

		size_t bytes_read = fread(buffer, 1, cache_stat.st_size, fh);
		if(bytes_read != cache_stat.st_size) {
			_log("expected %iBytes, but got %iBytes, reason: %s", cache_stat.st_size, bytes_read, ferror(fh) ? strerror(errno) : (feof(fh) ? "EOF" : "unknown error"));
		}

		fclose(fh);
		return bytes_read;
	}

	return 0;
}

bool cache_track_save(struct track *track, void *buffer, size_t size) {
	const size_t buffer_size = strlen(CACHE_STREAM_FOLDER) + 1 + 64 + strlen(CACHE_STREAM_EXT);
	char cache_file[buffer_size];
	snprintf(cache_file, buffer_size, CACHE_STREAM_FOLDER"%d_%d"CACHE_STREAM_EXT, track->user_id, track->track_id);

	FILE *fh = fopen(cache_file, "w");

	if(fh) {
		fwrite((void *) buffer, 1, size, fh);
		fclose(fh);

		return true;
	}

	return false;
}

