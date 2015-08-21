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

/** \file jspf.c
 *  \brief Implements JSPF, a variation of the XSPF playlist format.
 *
 *  XSPF and JSPF are two related, free and extensible formats for saving playlists.
 *  JSPF uses JSON as backing format.
 *  
 *  \see http://www.xspf.org/jspf/
 *  \see http://json.org/
 */


#define _XOPEN_SOURCE 500

#include "_hard_config.h"
#include "jspf.h"

//\cond
#include <errno.h>                      // for errno
#include <stdbool.h>                    // for bool, false, true
#include <stddef.h>                     // for size_t
#include <stdio.h>                      // for NULL, fclose, FILE, fopen, etc
#include <stdlib.h>                     // for free
#include <string.h>                     // for strlen, strerror
#include <sys/stat.h>                   // for stat, fstat
#include <time.h>                       // for strftime, strptime
//\endcond

#include <yajl/yajl_gen.h>              // for yajl_gen_string, etc
#include <yajl/yajl_tree.h>             // for yajl_val_s, etc

#include "helper.h"                     // for lcalloc, lmalloc
#include "log.h"                        // for _log
#include "track.h"                      // for track, etc
#include "yajl_helper.h"                // for yajl_helper_get_string, etc

#define YAJL_GEN_STRING(hand, str) yajl_gen_string(hand, (unsigned char*)str, strlen(str))

#define YAJL_GEN_ENTRY(hand, title, content) { \
	if(content) { \
		YAJL_GEN_STRING(hand, title); \
		YAJL_GEN_STRING(hand, content); \
	} \
}

#define YAJL_GEN_ENTRY_INT(hand, title, value) { \
	YAJL_GEN_STRING(hand, title); \
	yajl_gen_integer(hand, value); \
}

#define YAJL_GEN_SCOPED_ENTRY(hand, title, content) { \
	yajl_gen_map_open(hand); \
	YAJL_GEN_ENTRY(hand, title, content) \
	yajl_gen_map_close(hand); \
}

#define YAJL_GEN_SCOPED_ENTRY_INT(hand, title, content) { \
	yajl_gen_map_open(hand); \
	YAJL_GEN_ENTRY_INT(hand, title, content) \
	yajl_gen_map_close(hand); \
}

static char* last_error = NULL;

/** \brief YAJL print callback function for writing JSPF.
 *
 *  \param ctx  A context, here: a `FILE*` receiving the JSPF
 *  \param str  The string to write to file
 *  \param len  The length of `str`
 */
static void jspf_filewriter(void *ctx, const char *str, size_t len) {
	FILE *fh = (FILE*) ctx;
	fwrite(str, sizeof(char), len, fh);
}

/** \brief Write a single track to file.
 *
 *  \param hand   A YAJL handle (represents the document)
 *  \param track  The track to be written to file
 */
static void write_jspf_track(yajl_gen hand, struct track *track) {
	yajl_gen_map_open(hand);

	YAJL_GEN_ENTRY    (hand, "title",      track->name);
	YAJL_GEN_ENTRY    (hand, "location",   track->stream_url);
	YAJL_GEN_ENTRY    (hand, "creator",    track->username);
	YAJL_GEN_ENTRY    (hand, "identifier", track->permalink_url);
	YAJL_GEN_ENTRY    (hand, "annotation", track->description);
	YAJL_GEN_ENTRY_INT(hand, "duration",   track->duration);

	// use meta-tags to save date of creation, user_id and track_id
	YAJL_GEN_STRING(hand, "meta");
	yajl_gen_array_open(hand);

	YAJL_GEN_SCOPED_ENTRY_INT(hand, "https://sctc.narbo.de/user_id",    track->user_id);
	YAJL_GEN_SCOPED_ENTRY_INT(hand, "https://sctc.narbo.de/track_id",   track->track_id);
	YAJL_GEN_SCOPED_ENTRY_INT(hand, "https://sctc.narbo.de/created_at", track->created_at);

	yajl_gen_array_close(hand);

	yajl_gen_map_close(hand);
}

bool jspf_write(char *file, struct track_list *list) {
	FILE *fh = fopen(file, "w");
	if(!fh) {
		last_error = strerror(errno);
		_log("failed to open '%s': %s", file, last_error);
		return false;
	}

	yajl_gen hand = yajl_gen_alloc(NULL);

	yajl_gen_config(hand, yajl_gen_print_callback, jspf_filewriter, fh);

	yajl_gen_map_open(hand);
	YAJL_GEN_STRING(hand, "playlist");
	yajl_gen_map_open(hand);
	YAJL_GEN_STRING(hand, "track");

	yajl_gen_array_open(hand);
	for(size_t i = 0; i < list->count; i++) {
		write_jspf_track(hand, TRACK(list, i));
	}
	yajl_gen_array_close(hand);

	yajl_gen_map_close(hand);
	yajl_gen_map_close(hand);

	yajl_gen_free(hand);
	fclose(fh);

	return true;
}

static void yajl_to_track(yajl_val parent, struct track *track) {
	track->name          = yajl_helper_get_string(parent, "title",      NULL);
	track->stream_url    = yajl_helper_get_string(parent, "location",   NULL);
	track->username      = yajl_helper_get_string(parent, "creator",    NULL);
	track->permalink_url = yajl_helper_get_string(parent, "identifier", NULL);
	track->description   = yajl_helper_get_string(parent, "annotation", NULL);
	track->duration      = yajl_helper_get_int   (parent, "duration",   NULL);
	track->url_count     = URL_COUNT_UNINITIALIZED;

	// TODO \todo download_url not part of cached data
	// track->download_url  = yajl_helper_get_string(parent, "download_url",  NULL);

	yajl_val node_meta = yajl_helper_get_array(parent, "meta", NULL);
	for(size_t j = 0; j < node_meta->u.array.len; j++) {
		int val_user_id    = yajl_helper_get_int(node_meta->u.array.values[j], "https://sctc.narbo.de/user_id", NULL);
		int val_track_id   = yajl_helper_get_int(node_meta->u.array.values[j], "https://sctc.narbo.de/track_id", NULL);
		int val_created_at = yajl_helper_get_int(node_meta->u.array.values[j], "https://sctc.narbo.de/created_at", NULL);

		if(val_user_id)    track->user_id    = val_user_id;
		if(val_track_id)   track->track_id   = val_track_id;
		if(val_created_at) track->created_at = val_created_at;
	}
}

struct track_list* jspf_read(char *path) {
	struct track_list *list = lcalloc(1, sizeof(struct track_list));
	if(!list) {
		last_error = "*alloc failed";
		return NULL;
	}

	// allocate buffer and read whole .jspf-file into the buffer
	struct mmapped_file file = file_read_contents(path);
	if(file.data) {
		yajl_val node_root = yajl_helper_parse(file.data);
		yajl_val array = yajl_helper_get_array(node_root, "playlist", "track");

		if(array) {
			list->entries = lcalloc(array->u.array.len + 1, sizeof(struct track));
			list->count   = array->u.array.len;

			for(size_t i = 0; i < array->u.array.len; i++) {
				yajl_to_track(array->u.array.values[i], &list->entries[i]);
			}
		}

		yajl_tree_free(node_root);
		file_release_contents(file);
	}

	return list;
}

char* jspf_error(void) {
	return last_error;
}
