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


#include "../_hard_config.h"            // for CERT_BRAIN_FOLDER
#include "tls.h"

//\cond
#include <assert.h>                     // for assert
#include <errno.h>
#include <stdarg.h>                     // for va_end, va_list, va_copy, etc
#include <stdbool.h>                    // for bool, true, false
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for sprintf, fclose, fopen, etc
#include <stdlib.h>                     // for NULL, free, exit, etc
#include <string.h>                     // for strlen, bzero, strcmp
//\endcond

#include <polarssl/x509.h>              // for x509_time, x509_dn_gets, etc
#include <polarssl/x509_crt.h>          // for x509_crt, x509_crt_free, etc
#include <polarssl/ctr_drbg.h>          // for ctr_drbg_free, etc
#include <polarssl/entropy.h>           // for entropy_free, entropy_init, etc
#include <polarssl/error.h>
#include <polarssl/net.h>               // for POLARSSL_ERR_NET_WANT_READ, etc
#include <polarssl/ssl.h>               // for ssl_read, ssl_close_notify, etc

#include "../config.h"
#include "../helper.h"                  // for lcalloc, lmalloc
#include "../log.h"                     // for _log
#include "network.h"                    // for network_conn

#define TLS_CONN_MAGIC 0x42434445 ///< Magic used to validate the type of network_conn

static void tls_finalize();

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
	char *cert_path = config_get_cert_path();

	_log("reading list of trusted CAs from %s:", cert_path);
	x509_crt_init(&cacerts);

	int ret = x509_crt_parse_file(&cacerts, cert_path);
	if(ret < 0) {
		_log("x509_crt_parse: %d", ret);
		return false;
	}

	for(x509_crt *cert = &cacerts; cert; cert = cert->next) {
		char buf[2048];

		ret = x509_dn_gets(buf, sizeof(buf), &cert->subject);
		if(0 < ret) {
			_log("| * %s", buf);
		} else {
			char buf[2048];
			polarssl_strerror(ret, buf, sizeof(buf));
			_log("| * x509_dn_gets: %s", buf);
		}
	}

	if(atexit(tls_finalize)) {
		_log("atexit: %s", strerror(errno));
	}

	return true;
}

struct network_conn* tls_connect(char *server, int port) {
	// allocate and initialize the wrapper-struct 'network_conn'
	struct network_conn *nwc = lcalloc(1, sizeof(struct network_conn) + sizeof(struct tls_conn));
	if(!nwc) return NULL;

	nwc->send       = tls_send;
	nwc->send_fmt   = tls_send_fmt;
	nwc->recv       = tls_recv;
	nwc->recv_byte  = tls_recv_byte;
	nwc->disconnect = tls_disconnect;

	// the data required for tls.o is directly `after` the network_conn
	struct tls_conn *tls = (struct tls_conn*) &nwc[1];
	nwc->mdata = tls;

	tls->magic = TLS_CONN_MAGIC;

	entropy_init(&tls->entropy);


	// intialize the RNG
	int ret = ctr_drbg_init(&tls->ctr_drbg, entropy_func, &tls->entropy, (const unsigned char*) SC_API_KEY, strlen(SC_API_KEY));
	if(ret) {
		_log("ctr_drbg_init: %d", ret);
		free(nwc);
		return NULL;
	}

	// do the actual connection
	ret = net_connect(&tls->fd, server, port);
	if(ret) {
		char buf[2048];
		polarssl_strerror(ret, buf, sizeof(buf));
		_log("connection to '%s:%d' cannot be established: %s", server, port, buf);
		free(nwc);
		return NULL;
	}

	// initialize the SSL context
	if( (ret = ssl_init(&tls->ssl)) ) {
		_log("ssl_init: %d\n", ret);
		free(nwc);
		return NULL;
	}

	// required verification, fail if the certificate cannot be verified
	// (is invalid/expired/...)
	ssl_set_authmode(&tls->ssl, SSL_VERIFY_REQUIRED);

	// use the certificates gathered in tls_init()
	// at this point CRL is not used
	ssl_set_ca_chain(&tls->ssl, &cacerts, NULL, server);

	// behave as client (not server)
	ssl_set_endpoint(&tls->ssl, SSL_IS_CLIENT);

	ssl_set_rng(&tls->ssl, ctr_drbg_random, &tls->ctr_drbg);
	ssl_set_bio(&tls->ssl, net_recv, &tls->fd, net_send, &tls->fd);

	while( ( ret = ssl_handshake( &tls->ssl ) ) != 0 ) {
		if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE ) {
			char buf[2048];
			polarssl_strerror(ret, buf, sizeof(buf));
			_log("failure in handshake: %s", buf);
			tls_disconnect(nwc);
			return NULL;
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

	/* Compare the certifiate supplied by the server to the one we know
	 * from one of our previous connection attempts.
	 */
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

	char sha512_fingerprint_string[SHA512_LEN * 3 + 1];
	sha512_string(sha512_fingerprint_string, rcert->raw.p, rcert->raw.len);

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
	for(size_t i = 0; i < rcert->serial.len; i++) {
		serial_str_off += sprintf(serial_str + serial_str_off, "%02x%s", rcert->serial.p[i], i == rcert->serial.len - 1 ? "" : ":");
	}
	_log("| * Serial: %s; Version %d", serial_str, rcert->version);
	x509_dn_gets( buf, 2048, &rcert->issuer );
	_log("| * Issuer: %s", buf);

	x509_dn_gets( buf, 2048, &rcert->subject );
	_log("| * Subject: %s", buf);

	_log("| * Valid from %04d-%02d-%02d %02d:%02d:%02d to %04d-%02d-%02d %02d:%02d:%02d",
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
	free(nwc);
}

/** \brief Free previous global initialization of TLS.
 */
static void tls_finalize() {
	x509_crt_free(&cacerts);
}
