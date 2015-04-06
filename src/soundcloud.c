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

#define _XOPEN_SOURCE 500

//\cond
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
//\endcond

#include "log.h"
#include "soundcloud.h"
#include "network/network.h"
#include "xspf.h"
#include "http.h"
#include "helper.h"
#include "url.h"
#include "json.h"

#define CLIENTID_GET "client_id=848ee866ea93c21373f6a8b61772b412"
#define GET_RQ_FULL "https://api.soundcloud.com/users/%s/tracks.json?limit=200&linked_partitioning=1&"CLIENTID_GET

#define CACHE_LIST_FOLDER "./cache/lists/"
#define CACHE_LIST_EXT ".xspf"

struct track_list* soundcloud_get_entries(struct network_conn *nwc, char *user) {
	char cache_file[strlen(CACHE_LIST_FOLDER) + strlen(user) + strlen(CACHE_LIST_EXT) + 1];
	sprintf(cache_file, CACHE_LIST_FOLDER"%s"CACHE_LIST_EXT, user);
	struct track_list* cache_tracks = xspf_read(cache_file);

	char created_at_from_string[256] = { 0 };
	if(cache_tracks->count) {
		struct tm mtime = cache_tracks->entries[0].created_at;
		mtime.tm_sec += 1;
		mktime(&mtime); // TODO

		strftime(created_at_from_string, sizeof(created_at_from_string), "%Y-%m-%d%%20%T", &mtime);
		//_log("most recent track created at %s", created_at_from_string);
	}

	struct track_list *list = NULL;
	if(nwc) {
		char request_url[strlen(GET_RQ_FULL) + strlen(user) + 256 + strlen(created_at_from_string) + 1];
		if(cache_tracks->count) {
			sprintf(request_url, GET_RQ_FULL"&created_at[from]=%s", user, created_at_from_string);
		} else {
			sprintf(request_url, GET_RQ_FULL, user);
		}

		char *href = request_url;
		do {
			struct url *u = url_parse_string(href);
			struct http_response *resp = http_request_get(nwc, u->request, u->host);

			/* free any href, which is not the initial request url (strdup'd below, the initial request url is on stack) */
			if(href != request_url) {
				free(href);
			}

			if(200 != resp->http_status) {
				_log("server returned unexpected http status code %i", resp->http_status);
				_log("make sure the user you subscribed to is valid!");
				list = NULL; // TODO: free the existing part!
				break;
			}

			struct json_node *node = json_parse(resp->body);

			struct track_list *next_part = lcalloc(1, sizeof(struct track_list));
			if(!next_part) {
				return NULL;
			}

			struct json_array *array = json_get_array(node, "collection", NULL);
			if(array) {
				next_part->entries = lcalloc(array->len + 1, sizeof(struct track));
				next_part->count   = array->len;
				for(int i = 0; i < array->len; i++) {

					next_part->entries[i].name          = json_get_string(array->nodes[i], "title",         NULL);
					next_part->entries[i].stream_url    = json_get_string(array->nodes[i], "stream_url",    NULL);
					next_part->entries[i].download_url  = json_get_string(array->nodes[i], "download_url",  NULL);
					next_part->entries[i].permalink_url = json_get_string(array->nodes[i], "permalink_url", NULL);
					next_part->entries[i].username      = json_get_string(array->nodes[i], "user", "username");
					next_part->entries[i].description   = json_get_string(array->nodes[i], "description",   NULL);

					next_part->entries[i].user_id       = json_get_int   (array->nodes[i], "user", "id");
					next_part->entries[i].track_id      = json_get_int   (array->nodes[i], "id", NULL);

					next_part->entries[i].duration      = json_get_int   (array->nodes[i], "duration", NULL) / 1000;
					next_part->entries[i].bpm           = json_get_int   (array->nodes[i], "bpm", NULL);

					char *date_str = json_get_string(array->nodes[i], "created_at", NULL);
					if(date_str) {
						char *ret = strptime(date_str, "%Y/%m/%d %H:%M:%S %z", &next_part->entries[i].created_at);
						if(!ret || *ret) {
							_log("strptime(\"%s\"): '%s'", date_str, strerror(errno));
						}
						free(date_str);
					}

					next_part->entries[i].flags |= FLAG_NEW;
				}
		
			}

			href = json_get_string(node, "next_href", NULL);

			http_response_destroy(resp);

			if(list) {
				track_list_append(list, next_part);
			} else {
				list = next_part;
			}
		} while(href);
	}
	
	_log("%4d/%4d tracks from cache/soundcloud.com for %s", cache_tracks->count, list ? list->count : -1, user);

	/* only return the tracks received from the cache in case of an error in the request to soundcloud.com */
	if(!list) {
		return cache_tracks;
	}

	struct track_list *lists[] = {list, cache_tracks, NULL};
	struct track_list *result = track_list_merge(lists);

	track_list_destroy(list, false);
	track_list_destroy(cache_tracks, false);

	xspf_write(cache_file, result);

	return result;
}

struct http_response* soundcloud_connect_track(struct track *track) {
	char request[strlen(track->stream_url) + 1 + strlen(CLIENTID_GET) + 1];
	sprintf(request, "%s?"CLIENTID_GET, track->stream_url);

	struct url *u = url_parse_string(request);
	if(!u) {
		_log("invalid request '%s'", request);
		return NULL;
	}

	url_connect(u);
	if(!u->nwc) {
		return NULL;
	}

	struct http_response *resp = http_request_get_only_header(u->nwc, u->request, u->host, MAX_REDIRECT_STEPS);

	return resp;
}
