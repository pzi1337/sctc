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
#include "url.h"

#include <stdbool.h>                    // for true, bool, false
#include <stddef.h>                     // for size_t
#include <stdio.h>                      // for NULL, fprintf, stderr
#include <stdlib.h>                     // for free, atoi
#include <string.h>                     // for strcmp, strchr, strlen

#include "helper.h"                     // for lstrdup, lcalloc
#include "log.h"                        // for _log
#include "network/plain.h"              // for plain_connect
#include "network/tls.h"                // for tls_connect

struct url* url_parse_string(char *str) {
	char *str_clone = lstrdup(str);
	str = str_clone;

	char *hit = strchr(str, ':');
	if(!hit) {
		_err("URL seems invalid, cannot find ':'");
		free(str);
		return NULL;
	}

	if(strncmp("://", hit, 3)) {
		_err("Protocol in URL is not followed by `://`: %s", hit);
		free(str);
		return NULL;
	}

	*hit = '\0';

	struct url *u = lcalloc(1, sizeof(struct url));
	u->scheme = lstrdup(str);

	// str now contains the part after `http://`
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
		if(streq(u->scheme, "http")) {
			u->port = 80;
		} else if(streq(u->scheme, "https")) {
			u->port = 443;
		} else {
			_err("Invalid URL '%s', unknown scheme '%s'", str, u->scheme);
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
	if(streq(u->scheme, "http")) {
		u->nwc = plain_connect(u->host, u->port);
		if(u->nwc) return true;
	} else if(streq(u->scheme, "https")) {
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

