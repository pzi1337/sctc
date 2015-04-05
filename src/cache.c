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

#include <string.h>
#include <stdio.h>

#include "track.h"
#include "cache.h"

#define CACHE_STREAM_FOLDER "./cache/streams/"
#define CACHE_STREAM_EXT ".mp3"

bool cache_track_exists(struct track *track) {
	char cache_file[strlen(CACHE_STREAM_FOLDER) + 1 + strlen(CACHE_STREAM_EXT)];
	sprintf(cache_file, CACHE_STREAM_FOLDER"%d_%d"CACHE_STREAM_EXT, track->user_id, track->track_id);
	FILE *fh = fopen(cache_file, "r");

	if(fh) {
		fclose(fh);
		return true;
	}

	return false;
}

