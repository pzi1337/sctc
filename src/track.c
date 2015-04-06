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

/** \file track.c
 *  \brief Functions for handling tracks and track_lists
 */

//\cond
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
//\endcond

#include "log.h"
#include "track.h"
#include "helper.h"

// TODO: improve memory management, might be slow that way
bool track_list_add(struct track_list *list, struct track *track) {
	struct track *tracks = lrealloc(list->entries, (list->count + 1) * sizeof(struct track));
	if(!tracks) return false;

	memcpy(&tracks[list->count], track, sizeof(struct track));

	list->entries = tracks;
	list->count++;

	return true;
}

static int entry_compare(const void *v1, const void *v2) {
	struct track *e1 = (struct track*)v1;
	struct track *e2 = (struct track*)v2;
	return difftime(mktime(&e2->created_at), mktime(&e1->created_at));
}

struct track_list* track_list_merge(struct track_list **lists) {
	struct track_list *list = lcalloc(1, sizeof(struct track_list));
	if(!list) return NULL;

	list->count = 0;
	for(int i = 0; lists[i]; i++) {
		list->count += lists[i]->count;
	}

	list->entries = lmalloc(list->count * sizeof(struct track));

	size_t pos = 0;
	for(size_t i = 0; lists[i]; i++) {
		memcpy(&(list->entries[pos]), lists[i]->entries, lists[i]->count * sizeof(struct track));
		pos += lists[i]->count;
	}

	return list;
}

void track_list_sort(struct track_list *list) {
	qsort(list->entries, list->count, sizeof(struct track), entry_compare);
}

bool track_list_append(struct track_list *target, struct track_list *source) {
	struct track *new_entries = lrealloc(target->entries, (target->count + source->count) * sizeof(struct track));
	if(!new_entries) return false;

	target->entries = new_entries;

	memcpy(&target->entries[target->count], &source->entries[0], source->count * sizeof(struct track));

	target->count += source->count;

	return true;
}

// TODO: speed!
bool track_list_contains(struct track_list *list, char *permalink) {
	for(int i = 0; i < list->count; i++) {
		if(!strcmp(list->entries[i].permalink_url, permalink)) {
			return true;
		}
	}
	return false;
}

void track_list_destroy(struct track_list *list, bool free_trackdata) {
	if(free_trackdata) {
		for(int i = 0; i < list->count; i++) {
			free(list->entries[i].name);
			free(list->entries[i].download_url);
			free(list->entries[i].stream_url);
			free(list->entries[i].permalink_url);
			free(list->entries[i].username);
			free(list->entries[i].description);
		}
	}

	free(list->entries);
	free(list);
}
