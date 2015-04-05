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
#include <stdbool.h>                    // for true, false, bool
#include <stddef.h>                     // for size_t
#include <stdio.h>                      // for fclose, fprintf, NULL, etc
#include <stdlib.h>                     // for calloc, atoi, free
#include <string.h>                     // for strerror, strcmp, strlen, etc
//\endcond

#include "m3u.h"
#include "helper.h"                     // for lstrdup
#include "log.h"                        // for _log
#include "track.h"                      // for track_list, track, etc

#define EXTM3U_HEADER "#EXTM3U"
#define EXTM3U_PREINF "#EXTINF:"

#define LINE_BUFFER_SIZE 2048
struct track_list* m3u_read(char *file) {
	_log("reading file '%s'", file);

	FILE *fh = fopen(file, "r");
	if(!fh) {
		_log("cannot open file '%s': %s", strerror(errno));
		return NULL;
	}

	size_t list_size = 32;

	struct track_list *list = lcalloc(1, sizeof(struct track_list));
	bool have_header = false;

	list->entries = lcalloc(list_size, sizeof(struct track));
	if(!list->entries) {
		free(list);
		fclose(fh);
		return NULL;
	}

	int line = 0;
	char line_buffer[LINE_BUFFER_SIZE];

	while(fgets(line_buffer, LINE_BUFFER_SIZE, fh)) {
		line_buffer[strlen(line_buffer) - 1] = '\0';

		if(!line && !strcmp(EXTM3U_HEADER, line_buffer)) {
			_log("'%s' contains extended m3u", file);
		} else {
			if(!strncmp(EXTM3U_PREINF, line_buffer, sizeof(EXTM3U_PREINF) - 1)) {
				char *data = line_buffer + (sizeof(EXTM3U_PREINF) - 1);

				int i;
				for(i = 0; ',' != data[i] && '\0' != data[i]; i++);

				if(data[i]) {
					data[i] = '\0';
					char *length = data;
					char *name   = &data[i + 1];

					list->entries[list->count].name     = lstrdup(name);
					list->entries[list->count].duration = atoi(length);

					have_header = true;
				} else {
					_log("line in '%s' has invalid format: %s", file, line_buffer);
					track_list_destroy(list, true);
					fclose(fh);
					return NULL;
				}
			} else {
				if(have_header) {
					list->entries[list->count].stream_url = lstrdup(line_buffer);

					list->count++;
					have_header = false;
				} else {
					_log("'%s' is invalid: expected "EXTM3U_PREINF" prior to path to file '%s'", file, line_buffer);
					track_list_destroy(list, true);
					fclose(fh);
					return NULL;
				}
			}
		}

		line++;
	}

	fclose(fh);

	return list;
}

bool m3u_write(char *file, struct track_list *list) {
	_log("writing to file '%s'", file);

	FILE *fh = fopen(file, "w");
	if(!fh) {
		_log("cannot open file '%s': %s", strerror(errno));
		return false;
	}

	// write header
	fprintf(fh, EXTM3U_HEADER"\n");

	// write entries (2 lines per entry)
	for(int i = 0; i < list->count; i++) {
		fprintf(fh, EXTM3U_PREINF"%d,%s\n", list->entries[i].duration, list->entries[i].name);
		fprintf(fh, "%s?client_id=848ee866ea93c21373f6a8b61772b412\n", list->entries[i].stream_url);
	}

	fclose(fh);

	return true;
}

