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

#define CLIENTID_GET    "client_id="SC_API_KEY
#define GET_RQ_FULL     "https://api.soundcloud.com/users/%s/tracks.json?limit=200&linked_partitioning=1&"CLIENTID_GET
#define GET_RQ_SUBSCRIB "https://api.soundcloud.com/users/%s/followings.json?"CLIENTID_GET

struct track_list* soundcloud_get_stream(void) {
	state_set_status(cline_default, "Info: Connecting to soundcloud.com");
	struct network_conn *nwc = tls_connect(SERVER_NAME, SERVER_PORT);

	const size_t lists_size = config_get_subscribe_count() + 1;
	struct track_list *lists[lists_size];
	lists[lists_size - 1] = NULL;

	char status_msg[1024];
	for(size_t i = 0; i < config_get_subscribe_count(); i++) {
		snprintf(status_msg, sizeof(status_msg), "Info: Retrieving %zu/%zu lists from soundcloud.com: "F_BOLD"%s"F_RESET, i, config_get_subscribe_count(), config_get_subscribe(i));
		state_set_status(cline_default, status_msg);

		lists[i] = soundcloud_get_entries(nwc, config_get_subscribe(i));
	}
	state_set_status(cline_default, "Info: Merging lists");

	if(nwc) {
		nwc->disconnect(nwc);
	}

	BENCH_START(MP)
	struct track_list* list = track_list_merge(lists);
	track_list_sort(list);
	BENCH_STOP(MP, "Merging playlists")

	for(size_t i = 0; i < config_get_subscribe_count(); i++) {
		if(lists[i]) {
			track_list_destroy(lists[i], false);
		}
	}
	return list;
}

/** \brief Parse a single part of a response from soundcloud.com
 *
 *  \param [in]  resp  The response to be parsed (expected to be valid JSON)
 *  \param [out] href  A pointer to a string containing the URL of the subsequent http request
 *  \returns           A list containing all the tracks from `resp` or `NULL` if an allocation failes
 */
static struct track_list* parse_single_response(char *resp, char **href) {
	// in case any allocation failes we do not have any reference
	*href = NULL;

	yajl_val node = yajl_helper_parse(resp);
	if(!node) return NULL;

	struct track_list *list = lcalloc(1, sizeof(struct track_list));
	if(!list) {
		yajl_tree_free(node);
		return NULL;
	}

	yajl_val array = yajl_helper_get_array(node, "collection", NULL);
	if(array) {
		list->count = array->u.array.len;
		list->entries = lcalloc(list->count + 1, sizeof(struct track));
		if(!list->entries) {
			free(list);
			yajl_tree_free(node);
			return NULL;
		}

		for(size_t i = 0; i < array->u.array.len; i++) {

			list->entries[i].name          = yajl_helper_get_string(array->u.array.values[i], "title",         NULL);
			list->entries[i].stream_url    = yajl_helper_get_string(array->u.array.values[i], "stream_url",    NULL);
			list->entries[i].download_url  = yajl_helper_get_string(array->u.array.values[i], "download_url",  NULL);
			list->entries[i].permalink_url = yajl_helper_get_string(array->u.array.values[i], "permalink_url", NULL);
			list->entries[i].username      = yajl_helper_get_string(array->u.array.values[i], "user", "username");
			list->entries[i].description   = yajl_helper_get_string(array->u.array.values[i], "description",   NULL);

			list->entries[i].user_id       = yajl_helper_get_int   (array->u.array.values[i], "user", "id");
			list->entries[i].track_id      = yajl_helper_get_int   (array->u.array.values[i], "id", NULL);

			list->entries[i].duration      = yajl_helper_get_int   (array->u.array.values[i], "duration", NULL) / 1000;

			list->entries[i].url_count     = URL_COUNT_UNINITIALIZED;

			char *date_str = yajl_helper_get_string(array->u.array.values[i], "created_at", NULL);
			if(date_str) {
				struct tm ctime = { 0 };
				char *remaining = strptime(date_str, "%Y/%m/%d %H:%M:%S %z", &ctime);
				if(NULL == remaining || '\0' == *remaining) {
					_err("strptime: %s", (NULL == remaining ? "have remaining string" : strerror(errno)));
				}

				list->entries[i].created_at = mktime(&ctime);
				free(date_str);
			}

			list->entries[i].flags |= FLAG_NEW;
		}
	}

	*href = yajl_helper_get_string(node, "next_href", NULL);

	yajl_tree_free(node);
	return list;
}

struct subscription* soundcloud_get_subscriptions(char *user) {
	struct network_conn *nwc = tls_connect(SERVER_NAME, SERVER_PORT);

	struct subscription *list = NULL;
	if(nwc) {
		char request_url[strlen(GET_RQ_SUBSCRIB) + strlen(user) + 256 + 1];
		sprintf(request_url, GET_RQ_SUBSCRIB, user);

		struct url *u = url_parse_string(request_url);
		struct http_response *resp = http_request_get(nwc, u->request, u->host);
		url_destroy(u);

		if(!resp) {
			nwc->disconnect(nwc);
			return NULL;
		}

		if(200 != resp->http_status) {
			_err("server returned unexpected http status code %i", resp->http_status);
			_err("make sure the user you subscribed to is valid!");
		} else {
			yajl_val node = yajl_helper_parse(resp->body);
			if(YAJL_IS_ARRAY(node)) {
				list = lcalloc(sizeof(struct subscription), node->u.array.len + 1);

				for(size_t i = 0; i < node->u.array.len; i++) {
					list[i].name  = yajl_helper_get_string(node->u.array.values[i], "permalink", NULL);
					list[i].flags = 0;
				}
			}

			http_response_destroy(resp);
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
		time_t t = cache_tracks->entries[0].created_at + 1;

		strftime(created_at_from_string, sizeof(created_at_from_string), "%Y-%m-%d%%20%T", localtime(&t)); // TODO: localtime!?
		//_log("most recent track created at %s (user: `%s`)", created_at_from_string, user);
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

			// free any href, which is not the initial request url (strdup'd below, the initial request url is on stack)
			if(href != request_url) {
				free(href);
			}

			// set href to NULL to allow proper termination of loop
			href = NULL;

			if(!resp) { // check if communication succeeded (sc.com down / no network)
				_err("communication failed");

				track_list_destroy(list, true);
				list = NULL;
			} else if(200 != resp->http_status) { // check HTTP-status code
				_err("server returned unexpected http status code %i", resp->http_status);
				_err("make sure the user you subscribed to is valid!");

				track_list_destroy(list, true);
				list = NULL;
			} else {
				struct track_list *next_part = parse_single_response(resp->body, &href);

				if(list) {
					track_list_append(list, next_part);
				} else {
					list = next_part;
				}
			}

			http_response_destroy(resp);
		} while(href);
	}

	_log("%4zu/%4zu tracks from cache/soundcloud.com for %s", cache_tracks->count, list ? list->count : 0, user);

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

struct http_response* soundcloud_connect_track(struct track *track, char *range) {
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

	struct http_response *resp = http_request_get_only_header(u->nwc, u->request, u->host, range, MAX_REDIRECT_STEPS);

	url_destroy(u);

	return resp;
}
