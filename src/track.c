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

struct track_list* track_list_merge(struct track_list *list1, struct track_list *list2) {
	struct track_list *list = lmalloc(sizeof(struct track_list));
	if(!list) return NULL;

	list->count = list1->count + list2->count;
	list->entries = lcalloc(list->count, sizeof(struct track));
	if(!list->entries) {
		free(list);
		return NULL;
	}

	memcpy(&list->entries[0], list1->entries, list1->count * sizeof(struct track));
	memcpy(&list->entries[list1->count], list2->entries, list2->count * sizeof(struct track));

	return list;
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
