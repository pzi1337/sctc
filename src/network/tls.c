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

#include "../_hard_config.h"

//\cond
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
//\endcond

#include "polarssl/oid.h"
#include "polarssl/net.h"
#include "polarssl/debug.h"
#include "polarssl/ssl.h"
#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"
#include "polarssl/error.h"
#include "polarssl/certs.h"

#include "../log.h"
#include "../helper.h"
#include "tls.h"

#include "network.h"

#define TLS_CONN_MAGIC 0x42434445 ///< Magic used to validate the type of network_conn
#define SHA512_LEN 64

static x509_crt cacerts;

struct tls_conn {
	ssl_context ssl;
	entropy_context entropy;
	ctr_drbg_context ctr_drbg;

	int fd;
	uint32_t magic; /// the magic is used to assure that the data passed via mdata is actually a tls_conn struct
};

bool tls_send      (struct network_conn *nwc, char *buffer, size_t buffer_len);
bool tls_send_fmt  (struct network_conn *nwc, char *fmt, ...);
int  tls_recv      (struct network_conn *nwc, char *buffer, size_t buffer_len);
int  tls_recv_byte (struct network_conn *nwc);
void tls_disconnect(struct network_conn *nwc);

bool tls_init() {
	_log("reading list of trusted CAs:");
	x509_crt_init(&cacerts);

	int ret = x509_crt_parse_file(&cacerts, "/etc/ssl/certs/ca-certificates.crt");
	if(ret < 0) {
		_log("x509_crt_parse: %d", ret);
		return false;
	}

	size_t pos = 0;
	for(x509_crt *cert = &cacerts; cert; cert = cert->next) {
		char buf[2048];

		ret = x509_dn_gets(buf, sizeof(buf), &cert->subject);
		_log("| %3i. %s", pos, buf);

		pos++;
	}

	return true;
}

struct network_conn* tls_connect(char *server, int port) {
	/* allocate and initialize the wrapper-struct 'network_conn' */
	struct network_conn *nwc = lmalloc(sizeof(struct network_conn));
	if(!nwc) return NULL;

	nwc->send       = tls_send;
	nwc->send_fmt   = tls_send_fmt;
	nwc->recv       = tls_recv;
	nwc->recv_byte  = tls_recv_byte;
	nwc->disconnect = tls_disconnect;

	/* allocate the memory actually used by ssl.o */
	struct tls_conn *tls = lcalloc(1, sizeof(struct tls_conn));
	if(!tls) {
		free(nwc);
		return NULL;
	}
	nwc->mdata = tls;

	tls->magic = TLS_CONN_MAGIC;

	entropy_init(&tls->entropy);

	const char *pers = "ssl_client1";

	int ret;
	if( (ret = ctr_drbg_init(&tls->ctr_drbg, entropy_func, &tls->entropy, (const unsigned char *)pers, strlen(pers))) ) {
		_log("ctr_drbg_init: %d", ret);
		exit(EXIT_FAILURE);
	}

	if( (ret = net_connect(&tls->fd, server, port)) ) {
		_log("connection to '%s:%d' cannot be established: %d", server, port, ret);
		return NULL;
	}

	if( (ret = ssl_init(&tls->ssl)) ) {
		_log("ssl_init: %d\n", ret);
		return NULL;
	}

	ssl_set_authmode(&tls->ssl, SSL_VERIFY_REQUIRED);
	ssl_set_ca_chain(&tls->ssl, &cacerts, NULL, server); // TODO

	ssl_set_endpoint(&tls->ssl, SSL_IS_CLIENT);

	ssl_set_rng(&tls->ssl, ctr_drbg_random, &tls->ctr_drbg);
	ssl_set_bio(&tls->ssl, net_recv, &tls->fd, net_send, &tls->fd);

	while( ( ret = ssl_handshake( &tls->ssl ) ) != 0 ) {
		if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE ) {
			_log("failure in handshake: %d", ret);
		}
	}

	if( (ret = ssl_get_verify_result(&tls->ssl)) ) {

		char *reason = "unknown reason";
		if(ret & BADCERT_EXPIRED)     reason = "server certificate has expired";
		if(ret & BADCERT_REVOKED)     reason = "server certificate has been revoked";
		if(ret & BADCERT_CN_MISMATCH) reason = "CN mismatch";
		if(ret & BADCERT_NOT_TRUSTED) reason = "self-signed or not signed by a trusted CA";
		_log("verification of certificate failed: %s; aborting connection to %s:%d", reason, server, port);

		return NULL;
	}

	const x509_crt *rcert = ssl_get_peer_cert(&tls->ssl);

	char expected_sha512_fingerprint_string[SHA512_LEN * 3 + 1] = { 0 };
	{
		char fingerprint_file[strlen(CERT_BRAIN_FOLDER) + strlen(server)];
		sprintf(fingerprint_file, CERT_BRAIN_FOLDER"%s", server);

		FILE *fh = fopen(fingerprint_file, "r");

		if(fh) {
			fgets(expected_sha512_fingerprint_string, SHA512_LEN * 3, fh);
			fclose(fh);
		}
	}

	unsigned char sha512_fingerprint[SHA512_LEN];
	sha512(rcert->raw.p, rcert->raw.len, sha512_fingerprint, false);

	char sha512_fingerprint_string[SHA512_LEN * 3 + 1];
	for(size_t i = 0; i < sizeof(sha512_fingerprint); i++) {
		sprintf(sha512_fingerprint_string + 3*i, "%02X%s", sha512_fingerprint[i], i+1 < sizeof(sha512_fingerprint) ? ":" : "");
	}
	sha512_fingerprint_string[SHA512_LEN * 3] = '\0';

	if(strcmp(expected_sha512_fingerprint_string, sha512_fingerprint_string)) {
		_log("comparison of SHA512 FAILED:");
		_log("| expected: %s", expected_sha512_fingerprint_string[0] ? expected_sha512_fingerprint_string : "<no expectation>");
		_log("| is:       %s", sha512_fingerprint_string);

		if(expected_sha512_fingerprint_string[0]) {
			_log("aborting connection to %s:%d", server, port);
			return NULL;
		}
	}

	/* write key to brain ;) */
	{
		char fingerprint_file[strlen(CERT_BRAIN_FOLDER) + strlen(server)];
		sprintf(fingerprint_file, CERT_BRAIN_FOLDER"%s", server);

		FILE *fh = fopen(fingerprint_file, "w");
		fprintf(fh, "%s", sha512_fingerprint_string);
		fclose(fh);
	}

	char buf[2048];

	char serial_str[64];
	size_t serial_str_off = 0;
	for(int i = 0; i < rcert->serial.len; i++) {
		serial_str_off += sprintf(serial_str + serial_str_off, "%02x%s", rcert->serial.p[i], i == rcert->serial.len - 1 ? "" : ":");
	}
	_log("cert serial: %s", serial_str);

	_log("cert version: %d", rcert->version);
	x509_dn_gets( buf, 2048, &rcert->issuer );
	_log("issuer: %s", buf);

	x509_dn_gets( buf, 2048, &rcert->subject );
	_log("subject: %s", buf);

	_log("valid from %04d-%02d-%02d %02d:%02d:%02d to %04d-%02d-%02d %02d:%02d:%02d", 
		rcert->valid_from.year, rcert->valid_from.mon, rcert->valid_from.day, rcert->valid_from.hour, rcert->valid_from.min, rcert->valid_from.sec, 
		rcert->valid_to.year,   rcert->valid_to.mon,   rcert->valid_to.day,   rcert->valid_to.hour,   rcert->valid_to.min,   rcert->valid_to.sec);


	_log("connected to %s:%d using %s (%s)", server, port, ssl_get_ciphersuite(&tls->ssl), ssl_get_version(&tls->ssl));

	return nwc;
}

bool tls_send(struct network_conn *nwc, char *buffer, size_t buffer_len) {
	struct tls_conn *tls = (struct tls_conn*) nwc->mdata;
	assert(TLS_CONN_MAGIC == tls->magic);

	int ret;
	while( (ret = ssl_write(&tls->ssl, (const unsigned char*)buffer, buffer_len)) <= 0 ) {
		if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE ) {
			return false;
		}
	}

	return true;
}

bool tls_send_fmt(struct network_conn *nwc, char *fmt, ...) {
	va_list va;
	va_start(va, fmt);

	va_list va2;
	va_copy(va2, va);

	// vsnprintf returns the number of bytes without the terminating '\0'
	size_t buffer_size = vsnprintf(NULL, 0, fmt, va) + 1;

	char buffer[buffer_size];
	vsnprintf(buffer, buffer_size, fmt, va2);

	va_end(va2);
	va_end(va);

	return tls_send(nwc, buffer, buffer_size - 1);
}

int tls_recv(struct network_conn *nwc, char *buffer, size_t buffer_len) {
	struct tls_conn *tls = (struct tls_conn*) nwc->mdata;
	assert(TLS_CONN_MAGIC == tls->magic);

	int buffer_pos = 0;
	do {
		int ret = ssl_read(&tls->ssl, (unsigned char*)buffer + buffer_pos, buffer_len - buffer_pos);

		// check for EOF
		if(!ret) break;

		if(ret == POLARSSL_ERR_NET_WANT_READ || ret == POLARSSL_ERR_NET_WANT_WRITE)
			continue;

		if(ret < 0) return buffer_pos;

		buffer_pos += ret;

		if(!ssl_get_bytes_avail(&tls->ssl)) break;
	} while(true);

	return buffer_pos;
}

int tls_recv_byte(struct network_conn *nwc) {
	struct tls_conn *tls = (struct tls_conn*) nwc->mdata;
	assert(TLS_CONN_MAGIC == tls->magic);

	char byte;

	do {
		int ret = ssl_read(&tls->ssl, (unsigned char*) &byte, 1);

		if(!ret)     return -1; // on EOF
		if(1 == ret) return (int) byte;
	} while(true);
}

void tls_disconnect(struct network_conn *nwc) {
	struct tls_conn *tls = (struct tls_conn*) nwc->mdata;
	assert(TLS_CONN_MAGIC == tls->magic);

	ssl_close_notify(&tls->ssl);

	if(-1 != tls->fd) {
		net_close(tls->fd);
	}

	ssl_free     (&tls->ssl);
	ctr_drbg_free(&tls->ctr_drbg);
	entropy_free (&tls->entropy);

	bzero(tls, sizeof(struct tls_conn));
	free(tls);

	free(nwc);
}

bool tls_finalize() {
	x509_crt_free(&cacerts);

	return true;
}
