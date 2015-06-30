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

#include "_hard_config.h"
#include "downloader.h"

#define MAX_PARALLEL_DOWNLOADS 4
#define CHUNK_SIZE 4096

//\cond
#include <assert.h>                     // for assert
#include <errno.h>                      // for errno
#include <pthread.h>                    // for pthread_t, pthread_create, etc
#include <semaphore.h>                  // for sem_post, sem_wait, etc
#include <stdio.h>                      // for fclose, fopen, fwrite, FILE
#include <stdlib.h>                     // for NULL, free
#include <string.h>                     // for strerror
//\endcond

#include "helper.h"                     // for lcalloc, lmalloc
#include "http.h"                       // for http_response, etc
#include "log.h"                        // for _log
#include "network/network.h"            // for network_conn
#include "soundcloud.h"                 // for soundcloud_connect_track

static void downloader_finalize();

static pthread_t threads[MAX_PARALLEL_DOWNLOADS];
static sem_t have_url;
static sem_t sem_url_queue;

static volatile bool terminate = false;

struct download {
	struct track *track;
	struct download_state *state;
	void (*callback)(struct download_state *);
	bool target_file;
	union {
		char *file;
		struct {
			void  *buffer;
			size_t buffer_size;
		};
	};

	struct download *next;
};

static struct download *head;
static struct download *tail;

static void* _download_thread(void *unused) {
	while(!terminate) {
		sem_wait(&have_url);
		if(terminate) return NULL;

		sem_wait(&sem_url_queue);
		struct download *my = head;
		head = my->next;

		if(my == tail) {
			tail = NULL;
		}
		sem_post(&sem_url_queue);

		__sync_bool_compare_and_swap(&my->state->started, false, true);

		FILE *fh = NULL;
		if(my->target_file) fh = fopen(my->file, "w");

		if(!my->target_file || fh) {
			char buffer[CHUNK_SIZE];
			struct http_response *resp = soundcloud_connect_track(my->track);
			struct network_conn *nwc = resp->nwc;

			__sync_bool_compare_and_swap(&my->state->bytes_total, 0, resp->content_length);

			size_t remaining = resp->content_length;
			_log("have content length %u", remaining);

			while( remaining ) {
				size_t request_size = remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining;
				if(my->target_file) {
					int ret = nwc->recv(nwc, buffer, request_size);
					if(ret > 0) {
						remaining -= ret;
						fwrite(buffer, 1, request_size, fh);
						__sync_add_and_fetch(&my->state->bytes_recvd, ret);
					}
				} else {
					int ret = nwc->recv(nwc, &((char*)my->buffer)[my->state->bytes_recvd], request_size);
					if(ret > 0) {
						remaining -= ret;
						__sync_add_and_fetch(&my->state->bytes_recvd, ret);
						if(my->callback) my->callback(my->state);
					}
				}
			}

			nwc->disconnect(nwc);
			http_response_destroy(resp);

			if(my->target_file) fclose(fh);
			__sync_bool_compare_and_swap(&my->state->finished, false, true);
			if(my->callback) my->callback(my->state);
		} else {
			_log("failed to open `%s`: `%s`", my->file, strerror(errno));
		}
		free(my);
	}

	return NULL;
}

static bool queue_add(struct download *new_dl) {
	sem_wait(&sem_url_queue);
	if(tail) {
		tail->next = new_dl;
	} else {
		head = new_dl;
	}
	tail = new_dl;
	sem_post(&sem_url_queue);

	sem_post(&have_url);

	return true;
}

/*
bool downloader_queue_file(char *url, char *file) {
	struct download *new_dl = lmalloc(sizeof(struct download));
	if(!new_dl) return false;

	new_dl->url         = url;
	new_dl->target_file = true;
	new_dl->file        = file;
	new_dl->next        = NULL;

	return queue_add(new_dl);
}
*/

struct download_state* downloader_queue_buffer(struct track *track, void *buffer, size_t buffer_size, void (*callback)(struct download_state *)) {
	struct download *new_dl = lmalloc(sizeof(struct download));
	if(!new_dl) return NULL;

	struct download_state *dl_stat = lcalloc(1, sizeof(struct download_state));
	if(!dl_stat) {
		free(new_dl);
		return NULL;
	}

	new_dl->state       = dl_stat;
	new_dl->track       = track;
	new_dl->target_file = false;
	new_dl->buffer      = buffer;
	new_dl->buffer_size = buffer_size;
	new_dl->next        = NULL;
	new_dl->callback    = callback;

	queue_add(new_dl);

	return dl_stat;
}

bool downloader_init() {
	assert(!sem_init(&have_url, 0, 0));
	assert(!sem_init(&sem_url_queue, 0, 1));

	for(size_t i = 0; i < MAX_PARALLEL_DOWNLOADS; i++) {
		pthread_create(&threads[i], NULL, _download_thread, NULL);
	}

	if(atexit(downloader_finalize)) {
		_log("atexit: %s", strerror(errno));
	}

	return true;
}

static void downloader_finalize() {
	terminate = true;

	for(size_t i = 0; i < MAX_PARALLEL_DOWNLOADS; i++)
		sem_post(&have_url);

	for(size_t i = 0; i < MAX_PARALLEL_DOWNLOADS; i++)
		pthread_join(threads[i], NULL);

	sem_destroy(&have_url);
	sem_destroy(&sem_url_queue);
}
