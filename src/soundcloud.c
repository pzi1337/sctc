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
#include "_hard_config.h"               // for MAX_REDIRECT_STEPS
#include "soundcloud.h"

//\cond
#include <errno.h>                      // for errno
#include <stdbool.h>                    // for false
#include <stdio.h>                      // for NULL, sprintf
#include <stdlib.h>                     // for free
#include <string.h>                     // for strlen, strerror
#include <time.h>                       // for mktime, strftime, tm
//\endcond

#include <yajl/yajl_tree.h>             // for yajl_val_s, etc

#include "helper.h"                     // for lcalloc
#include "jspf.h"                       // for jspf_read, jspf_write
#include "log.h"                        // for _log
#include "url.h"                        // for url, url_destroy, etc
#include "network/tls.h"
#include "config.h"
#include "state.h"
#include "yajl_helper.h"                // for yajl_helper_get_string, etc

#define CLIENTID_GET "client_id="CLIENTID
#define GET_RQ_FULL "https://api.soundcloud.com/users/%s/tracks.json?limit=200&linked_partitioning=1&"CLIENTID_GET

struct track_list* soundcloud_get_stream() {
	state_set_status(cline_default, "Info: Connecting to soundcloud.com");
	struct network_conn *nwc = tls_connect(SERVER_NAME, SERVER_PORT);

	const size_t lists_size = config_get_subscribe_count() + 1;
	struct track_list *lists[lists_size];
	lists[lists_size - 1] = NULL;

	for(int i = 0; i < config_get_subscribe_count(); i++) {
		state_set_status(cline_default, smprintf("Info: Retrieving %i/%i lists from soundcloud.com: "F_BOLD"%s"F_RESET, i, config_get_subscribe_count(), config_get_subscribe(i)));
		lists[i] = soundcloud_get_entries(nwc, config_get_subscribe(i));
	}

	if(nwc) {
		nwc->disconnect(nwc);
	}

	BENCH_START(MP)
	struct track_list* list = track_list_merge(lists);
	track_list_sort(list);
	BENCH_STOP(MP, "Merging playlists")

	for(int i = 0; i < config_get_subscribe_count(); i++) {
		if(lists[i]) {
			track_list_destroy(lists[i], false);
		}
	}
	return list;
}

struct track_list* soundcloud_get_entries(struct network_conn *nwc, char *user) {
	char *cache_path = config_get_cache_path();
	char cache_file[strlen(cache_path) + 1 + strlen(CACHE_LIST_FOLDER) + 1 + strlen(user) + strlen(CACHE_LIST_EXT) + 1];
	sprintf(cache_file, "%s/"CACHE_LIST_FOLDER"/%s"CACHE_LIST_EXT, cache_path, user);
	struct track_list* cache_tracks = jspf_read(cache_file);

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
			url_destroy(u);

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

			yajl_val node = yajl_helper_parse(resp->body);

			struct track_list *next_part = lcalloc(1, sizeof(struct track_list));
			if(!next_part) {
				return NULL;
			}

			yajl_val array = yajl_helper_get_array(node, "collection", NULL);
			if(array) {
				next_part->entries = lcalloc(array->u.array.len + 1, sizeof(struct track));
				next_part->count   = array->u.array.len;
				for(int i = 0; i < array->u.array.len; i++) {

					next_part->entries[i].name          = yajl_helper_get_string(array->u.array.values[i], "title",         NULL);
					next_part->entries[i].stream_url    = yajl_helper_get_string(array->u.array.values[i], "stream_url",    NULL);
					next_part->entries[i].download_url  = yajl_helper_get_string(array->u.array.values[i], "download_url",  NULL);
					next_part->entries[i].permalink_url = yajl_helper_get_string(array->u.array.values[i], "permalink_url", NULL);
					next_part->entries[i].username      = yajl_helper_get_string(array->u.array.values[i], "user", "username");
					next_part->entries[i].description   = yajl_helper_get_string(array->u.array.values[i], "description",   NULL);

					next_part->entries[i].user_id       = yajl_helper_get_int   (array->u.array.values[i], "user", "id");
					next_part->entries[i].track_id      = yajl_helper_get_int   (array->u.array.values[i], "id", NULL);

					next_part->entries[i].duration      = yajl_helper_get_int   (array->u.array.values[i], "duration", NULL) / 1000;

					char *date_str = yajl_helper_get_string(array->u.array.values[i], "created_at", NULL);
					if(date_str) {
						char *ret = strptime(date_str, "%Y/%m/%d %H:%M:%S %z", &next_part->entries[i].created_at);
						if(!ret || *ret) {
							_log("strptime(\"%s\"): '%s'", date_str, strerror(errno));
						}
						free(date_str);
					}

					next_part->entries[i].flags |= FLAG_NEW;

					_log("got new entry '%s' by '%s'", next_part->entries[i].name, next_part->entries[i].username);
				}
			}

			href = yajl_helper_get_string(node, "next_href", NULL);

			yajl_tree_free(node);

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

	jspf_write(cache_file, result);

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
		url_destroy(u);
		return NULL;
	}

	struct http_response *resp = http_request_get_only_header(u->nwc, u->request, u->host, MAX_REDIRECT_STEPS);

	url_destroy(u);

	return resp;
}
