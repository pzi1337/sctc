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

//\cond
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
//\endcond

#include "../log.h"
#include "../helper.h"
#include "plain.h"
#include "network.h"

#define PLAIN_CONN_MAGIC 0x35363738

struct plain_conn {
	FILE *fh; ///< File handle, used for communication
	uint32_t magic; ///< Magic number, used to assert that the data passed via mdata is actually a struct ssl_conn
};

bool plain_send      (struct network_conn *nwc, char *buffer, size_t buffer_len);
bool plain_send_fmt  (struct network_conn *nwc, char *fmt, ...);
int  plain_recv      (struct network_conn *nwc, char *buffer, size_t buffer_len);
int  plain_recv_byte (struct network_conn *nwc);
void plain_disconnect(struct network_conn *nwc);

struct network_conn* plain_connect(char *server, int port) {
	struct network_conn *nwc = lcalloc(1, sizeof(struct network_conn) + sizeof(struct plain_conn));
	if(!nwc) return NULL;

	nwc->send       = plain_send;
	nwc->send_fmt   = plain_send_fmt;
	nwc->recv       = plain_recv;
	nwc->recv_byte  = plain_recv_byte;
	nwc->disconnect = plain_disconnect;

	struct plain_conn *plain = (struct plain_conn*) &nwc[1];
	nwc->mdata = plain;

	plain->magic = PLAIN_CONN_MAGIC;

	struct addrinfo  addr_config = {
		.ai_socktype = SOCK_STREAM,
		.ai_family = PF_UNSPEC,
		.ai_flags = AI_ADDRCONFIG
	};

	struct addrinfo *addr;

	char port_str[6];
	snprintf(port_str, sizeof(port_str), "%d", port);

	int ret = getaddrinfo(server, port_str, &addr_config, &addr);
	if(ret) {
		_log("getaddrinfo: %s", gai_strerror(ret));
		return NULL;
	}

	struct addrinfo *ai;
	char ip_buffer[INET6_ADDRSTRLEN] = { 0 };
	for(ai = addr; ai; ai = ai->ai_next) {
		int sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if(-1 == sock) {
			_log("socket: %s", strerror(errno));
			continue;
		}

		if(connect(sock, ai->ai_addr, ai->ai_addrlen)) {
			_log("connect: %s", strerror(errno));
			close(sock);
			continue; // connect failed, try next ip if available
		}

		plain->fh = fdopen(sock, "a+");

		ret = getnameinfo(ai->ai_addr, ai->ai_addrlen, ip_buffer, sizeof(ip_buffer), 0, 0, NI_NUMERICHOST);
		if(ret) {
			_log("getnameinfo: %s", gai_strerror(ret));
			strncpy(ip_buffer, "<error>", INET6_ADDRSTRLEN - 1);
		}

		break;
	}

	freeaddrinfo(addr);

	if(!ai) {
		_log("connection to '%s:%i' failed", server, port);
		free(nwc);
		return NULL;
	}

	_log("connected to %s:%d (%s) using PLAIN (NO ENCRYPTION)", server, port, ip_buffer);
	return nwc;
}

bool plain_send(struct network_conn *nwc, char *buffer, size_t buffer_len) {
	struct plain_conn *plain = (struct plain_conn*) nwc->mdata;
	assert(PLAIN_CONN_MAGIC == plain->magic);

	return buffer_len == fwrite(buffer, buffer_len, 1, plain->fh);
}

bool plain_send_fmt(struct network_conn *nwc, char *fmt, ...) {
	struct plain_conn *plain = (struct plain_conn*) nwc->mdata;
	assert(PLAIN_CONN_MAGIC == plain->magic);

	va_list va;
	va_start(va, fmt);

	int count = vfprintf(plain->fh, fmt, va);

	va_end(va);

	return count >= 0;
}

/** \brief Receive a data from a connected plain TCP/IP socket.
 *
 *  TODO
 *
 *  \param nwc         The connection to read from
 *  \param buffer      The buffer receiving the read data
 *  \param buffer_len  The size of the buffer
 *  \return            The byte or `-1` on EOF
 */
int plain_recv(struct network_conn *nwc, char *buffer, size_t buffer_len) {
	struct plain_conn *plain = (struct plain_conn*) nwc->mdata;
	assert(PLAIN_CONN_MAGIC == plain->magic);

	if(buffer_len > 4095) buffer_len = 4095; // TODO: hack!
	return fread(buffer, 1, buffer_len, plain->fh);
}

/** \brief Receive a single byte from a connected plain TCP/IP socket.
 *
 *  As soon as -1 is returned the underlying TCP/IP connection has been disconnected (for instance by the remote server).
 *  Any further call to plain_recv* will fail, so just call plain_disconnect() to free the data.
 *  "The horse is dead"
 *
 *  \param nwc  The connection to read from
 *  \return     The byte or -1 on EOF
 */
int plain_recv_byte(struct network_conn *nwc) {
	struct plain_conn *plain = (struct plain_conn*) nwc->mdata;
	assert(PLAIN_CONN_MAGIC == plain->magic);

	int byte = getc(plain->fh);
	if(EOF == byte) return -1;
	return byte;
}

/** \brief Disconnect a connected plain TCP/IP socket.
 *
 *  After a call to plain_disconnect() all the memory assocatied to `nwc` is free'd, `nwc` may not be used anymore.
 *
 *  \param nwc  The connection to disconnect
 */
void plain_disconnect(struct network_conn *nwc) {
	struct plain_conn *plain = (struct plain_conn*) nwc->mdata;
	assert(PLAIN_CONN_MAGIC == plain->magic);

	fclose(plain->fh);
	free(nwc);
}

