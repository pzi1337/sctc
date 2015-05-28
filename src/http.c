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

/** \file http.c
 *  \brief Basic HTTP implementation
 */

#include "_hard_config.h"

//\cond
#include <stdlib.h>                     // for free, atoi
#include <string.h>                     // for strncmp, strtok
//\endcond

#include "http.h"
#include "helper.h"                     // for lcalloc, lrealloc
#include "log.h"                        // for _log
#include "network/network.h"            // for network_conn
#include "url.h"                        // for url, url_connect, etc

#define DEFAULT_BUFFER_SIZE 16384

#define NWC_GET_BYTE(NWC, BUF, I) { \
		int byte = nwc->recv_byte(NWC); \
		if(-1 != byte) BUF[I] = byte;  \
		else { \
			_log("got EOF from ssL_recv_byte for byte %i", I); \
			return NULL; \
		} \
	}

struct http_response* http_request_get_only_header(struct network_conn *nwc, char *url, char *host, size_t follow_redirect_steps) {

	nwc->send_fmt(nwc, "GET %s HTTP/1.1\r\n", url);
	nwc->send_fmt(nwc, "Host: %s\r\n\r\n", host);

	struct http_response *resp = lcalloc(1, sizeof(struct http_response));
	char *buffer               = lcalloc(DEFAULT_BUFFER_SIZE, sizeof(char));
	if(!resp || !buffer) {
		free(resp);
		free(buffer);
		return NULL;
	}

	NWC_GET_BYTE(nwc, buffer, 0)
	NWC_GET_BYTE(nwc, buffer, 1)
	NWC_GET_BYTE(nwc, buffer, 2)
	NWC_GET_BYTE(nwc, buffer, 3)

	int i = 4;
	while(! ('\r' == buffer[i-4] && '\n' == buffer[i-3] && '\r' == buffer[i-2] && '\n' == buffer[i-1]) ) {
		NWC_GET_BYTE(nwc, buffer, i)
		i++;
	}

	resp->header_length = i;

	char *tok = strtok(buffer, "\r") - 1;
	while(tok) {
		// omit the '\n' after the '\r'
		tok++;

		if(!strncmp(tok, "HTTP/1.1 ", 9)) {
			resp->http_status = atoi(tok + 9);
		} else if(!strncmp(tok, "Content-Length: ", 16)) {
			resp->content_length = atoi(tok + 16);
		} else if(!strncmp(tok, "Location: ", 10)) {
			resp->location = tok + 10;
		}

		tok = strtok(NULL, "\r");
	}

	resp->buffer = buffer;

	if(resp->http_status >= 300 && resp->http_status < 400) {
		_log("http_status: %d, follow_redirect_steps: %d", resp->http_status, follow_redirect_steps);

		if(follow_redirect_steps) {
			_log("following redirect to '%s'", resp->location);
			struct url *u = url_parse_string(resp->location);

			http_response_destroy(resp);

			if(u) {
				url_connect(u);
				resp = http_request_get_only_header(u->nwc, u->request, u->host, follow_redirect_steps - 1);
				resp->nwc = u->nwc;
				url_destroy(u);
			} else {
				resp = NULL;
			}
		}
	}

	return resp;
}

struct http_response* http_request_get(struct network_conn *nwc, char *url, char *host) {

	struct http_response* resp = http_request_get_only_header(nwc, url, host, MAX_REDIRECT_STEPS);
	//_log("content_length: %d", resp->content_length);
	//_log("http_status: %d", resp->http_status);

	resp->body = &resp->buffer[resp->header_length];
	if(resp->nwc) {
		nwc = resp->nwc;
	}

	/* realloc enough memory if the initial buffer is too small */
	if(resp->header_length + resp->content_length >= DEFAULT_BUFFER_SIZE) {
		size_t new_size = (resp->header_length + resp->content_length + 1) * sizeof(char);

		char *buffer = lrealloc(resp->buffer, new_size);
		if(!buffer) {
			free(resp->buffer); // free the 'old' buffer
			free(resp);
			return NULL;
		}

		buffer[resp->header_length + resp->content_length] = '\0';

		resp->buffer = buffer;
		resp->body = &buffer[resp->header_length];
	}

	size_t body_read = 0;
	while(body_read < resp->content_length) {
		int res = nwc->recv(nwc, &resp->body[body_read], resp->content_length - body_read);
		if(res > 0) {
			//_log("+ %dB: %dB", res, body_read);
			body_read += res;
		}
	}

	return resp;
}

void http_response_destroy(struct http_response *resp) {
	free(resp->buffer);
	free(resp);
}
