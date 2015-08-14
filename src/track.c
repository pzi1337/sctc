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

struct track_list* track_list_create(char *name) {
	struct track_list *list = lcalloc(1, sizeof(struct track_list));
	if(list) {
		list->name  = lstrdup( streq("", name) ? "unnamed list" : name );
		list->count = 0;
	}
	return list;
}

// TODO: improve memory management, might be slow that way
bool track_list_add(struct track_list *list, struct track *track) {
	struct track *tracks = lrealloc(list->entries, (list->count + 1) * sizeof(struct track));
	if(!tracks) return false;

	memcpy(&tracks[list->count], track, sizeof(struct track));

	list->entries = tracks;
	list->count++;

	return true;
}

bool track_list_del(struct track_list *list, size_t track_id) {
	memmove(&list->entries[track_id], &list->entries[track_id + 1], (list->count - track_id) * sizeof(struct track));
	list->count--;

	return true;
}

static int entry_compare(const void *v1, const void *v2) {
	const struct track *e1 = (const struct track*)v1;
	if(!e1->name) e1 = e1->href;

	const struct track *e2 = (const struct track*)v2;
	if(!e2->name) e2 = e2->href;

	return difftime(e2->created_at, e1->created_at);
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
struct track* track_list_get(struct track_list *list, char *permalink) {
	for(size_t i = 0; i < list->count; i++) {
		if(!strcmp(TRACK(list, i)->permalink_url, permalink)) {
			return &list->entries[i];
		}
	}
	return NULL;
}

void track_list_href_to(struct track_list *list, struct track_list *target) {
	for(size_t i = 0; i < target->count; i++) {
		struct track *strack = track_list_get(list, TRACK(target, i)->permalink_url);

		if(NULL != strack) {
			track_destroy(strack);

			strack->name = NULL;
			strack->href = TRACK(target, i);
		}
	}
}

void track_destroy(struct track *track) {
	if(track->name) {
		free(track->name);
		free(track->download_url);
		free(track->stream_url);
		free(track->permalink_url);
		free(track->username);
		free(track->description);
	}
}

void track_list_destroy(struct track_list *list, bool free_trackdata) {
	if(free_trackdata) {
		for(size_t i = 0; i < list->count; i++) {
			track_destroy(&list->entries[i]);
		}
	}

	free(list->entries);
	free(list->name);
	free(list);
}

#ifndef NDEBUG
void track_list_dump_mem_usage(struct track_list *list) {
	size_t mem = sizeof(struct track_list) + list->count * sizeof(struct track);

	mem += strlen(list->name) + 1;

	for(size_t i = 0; i < list->count; i++) {
		struct track *track = &list->entries[i];
		if(track->name) {
			mem += strlen(track->name) + 1;

			if(track->stream_url)    mem += strlen(track->stream_url)    + 1;
			if(track->permalink_url) mem += strlen(track->permalink_url) + 1;
			if(track->download_url)  mem += strlen(track->download_url)  + 1;
			if(track->username)      mem += strlen(track->username)      + 1;
			if(track->description)   mem += strlen(track->description)   + 1;
		}
	}
	_log("List `%s` occupies %zukB", list->name, mem / 1024);
}
#endif
