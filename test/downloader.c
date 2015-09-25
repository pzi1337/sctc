#include "tls.h"
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test_helper.h"

#include "../src/network/network.h"
#include "../src/network/plain.h"
#include "../src/network/tls.h"

#include "../src/http.h"
#include "../src/downloader.h"
#include "../src/helper.h"
#include "../src/log.h"

#define BUFFER_16MB (16 * 1024 * 1024)

#define SHA512_TESTIMG_JPG "28:10:6B:42:67:A3:53:FC:A1:21:0D:0E:49:29:DD:D4:8C:06:B0:29:58:19:6D:24:E8:AC:E9:0C:35:66:64:C0:98:18:24:50:0C:53:EF:4A:5C:C2:39:44:32:EC:6E:F2:8A:81:95:AA:EB:AB:39:4C:E1:6D:39:AB:C6:59:29:D7"

bool test_downloader() {
	fprintf(stderr, "\n\ndownloader.o");

	log_init("downloader.log");

	bool is_ok = true;

	tls_init();

	TEST_FUNC("downloader_init");
	TEST_RES(downloader_init());
	TEST_FUNC_RESULT(is_ok);

	TEST_FUNC("downloader_queue_buffer");
	{
		void *buffer_http  = lmalloc(BUFFER_16MB);
		void *buffer_https = lmalloc(BUFFER_16MB);
		assert(buffer_http && buffer_https);

		struct download_state *stat_http  = downloader_queue_buffer("http://nossl.narbo.de/testimg.jpg", buffer_http,  BUFFER_16MB);
		struct download_state *stat_https = downloader_queue_buffer("https://narbo.de/testimg.jpg",      buffer_https, BUFFER_16MB);
		TEST_RES(stat_http && stat_https);

		_log("waiting for stat->finished");
		struct timespec t = {.tv_sec = 0, .tv_nsec = 50000000};
		while(!stat_http->finished || !stat_https->finished) {
			nanosleep(&t, NULL);
		}

		char sha512_http [SHA512_LEN * 3 + 1];
		char sha512_https[SHA512_LEN * 3 + 1];

		sha512_string(sha512_http,  buffer_http,  stat_http->bytes_recvd);
		sha512_string(sha512_https, buffer_https, stat_https->bytes_recvd);

		free(stat_http);
		free(stat_https);

		TEST_RES( !strcmp(sha512_http,  SHA512_TESTIMG_JPG)
		       && !strcmp(sha512_https, SHA512_TESTIMG_JPG) );

		free(buffer_http);
		free(buffer_https);
	}
	TEST_FUNC_RESULT(is_ok);

	downloader_finalize();
	tls_finalize();
	log_close();

	return is_ok;
}
