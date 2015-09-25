#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helper.h"

#include "../src/network/network.h"
#include "../src/network/plain.h"

#include "../src/http.h"
#include "../src/helper.h"
#include "../src/log.h"

#define SHA512_TESTIMG_JPG "28:10:6B:42:67:A3:53:FC:A1:21:0D:0E:49:29:DD:D4:8C:06:B0:29:58:19:6D:24:E8:AC:E9:0C:35:66:64:C0:98:18:24:50:0C:53:EF:4A:5C:C2:39:44:32:EC:6E:F2:8A:81:95:AA:EB:AB:39:4C:E1:6D:39:AB:C6:59:29:D7"

bool test_plain() {
	TEST_INIT();
	fprintf(stderr, "\n\nplain.o");

	TEST_FUNC_START(plain_connect);
	{
		struct network_conn *nwc = plain_connect("nossl.narbo.de", 80);
		TEST_RES(nwc);

		struct http_response *resp = http_request_get(nwc, "/testimg.jpg", "nossl.narbo.de");

		char sha512_fingerprint_string[SHA512_LEN * 3 + 1];
		sha512_string(sha512_fingerprint_string, resp->body, resp->content_length);

		TEST_RES(!strcmp(sha512_fingerprint_string, SHA512_TESTIMG_JPG));

		http_response_destroy(resp);
		nwc->disconnect(nwc);
	}
	TEST_FUNC_END();

	TEST_END();
}
