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
#include <stdlib.h>
#include <stdbool.h>

#include "network/network.h"
#include "network/plain.h"
#include "network/tls.h"
#include "helper.h"
#include "url.h"
#include "log.h"

struct url* url_parse_string(char *str) {
	char *str_clone = lstrdup(str);
	str = str_clone;

	struct url *u = lcalloc(1, sizeof(struct url));

	char *hit = strchr(str, ':');
	if(!hit) {
		_log("URL seems invalid, cannot find ':'");
		free(u);

		return NULL;
	}
	*hit = '\0';

	u->scheme = lstrdup(str);

	str = hit + 1 + 2;

	const size_t str_len = strlen(str);

	char *start = str;

	char *host_or_user = NULL;
	char *port = NULL;

	size_t number_of_col = 0;
	size_t number_of_at  = 0;

	for(size_t i = 0; i < str_len; i++) {
		switch(str[i]) {
			case ':': // found ':', part prior to ':' is the username
				if(number_of_at) {
					u->host = start;
				} else {
					host_or_user = start;
				}
				str[i] = '\0';
				start = &str[i + 1];

				number_of_col++;
				break;

			case '@': // found '@', if ':' was found prior to '@': password
				if(host_or_user) {
					u->user = host_or_user;
					host_or_user = NULL;

					u->pass = start;
				} else {
					u->user = start;
				}
				

				str[i] = '\0';
				start = &str[i + 1];
				
				number_of_at++;
				break;

			case '/':
				if(host_or_user) {
					u->host = host_or_user;
					host_or_user = NULL;
				}

				if(!u->host) {
					u->host = start;
				} else if(1 == number_of_col || (2 == number_of_col && number_of_at)) {
					port = start;
				}

				str[i] = '\0';
				u->request = &str[i];
				i = str_len; // 'abort'
				break;

			default: break;
		}
	}

	if(u->user) u->user = lstrdup(u->user);
	if(u->pass) u->pass = lstrdup(u->pass);
	if(u->host) u->host = lstrdup(u->host);
	if(port) {
		u->port = atoi(port);
	} else {
		if(!strcmp(u->scheme, "http")) {
			u->port = 80;
		} else if(!strcmp(u->scheme, "https")) {
			u->port = 443;
		} else {
			fprintf(stderr, "Invalid URL '%s', unknown scheme '%s'\n", str, u->scheme);
		}
	}

	if(u->request) {
		u->request[0] = '/';
		u->request = lstrdup(u->request);
	}

	free(str_clone);

	return u;
}

bool url_connect(struct url *u) {
	if(!strcmp(u->scheme, "http")) {
		u->nwc = plain_connect(u->host, u->port);
		if(u->nwc) return true;
	} else if(!strcmp(u->scheme, "https")) {
		u->nwc = tls_connect(u->host, u->port);
		if(u->nwc) return true;
	}
	return false;
}

void url_destroy(struct url *u) {
	free(u->scheme);
	free(u->user);
	free(u->pass);
	free(u->host);
	free(u->request);
	free(u);
}

