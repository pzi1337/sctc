#include "url.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helper.h"

#include "../src/url.h"

#define CSTR(X, Y) (X == NULL ? !Y : !strcmp(X, Y))

static void test_url_parse(TEST_PARAM, char *str, char *scheme, char *user, char *pass, char *host, int port, char *request) {
	struct url *u = url_parse_string(str);

	TEST_RES( !strcmp(scheme,  u->scheme)
	       && CSTR(user, u->user)
	       && CSTR(pass, u->pass)
	       && !strcmp(host,    u->host)
	       && !strcmp(request, u->request)
	       && u->port == port );

	url_destroy(u);
}

bool test_url() {
	TEST_INIT();

	fprintf(stderr, "\n\nurl.o");

	TEST_FUNC_START(url_parse_string)
	test_url_parse(TEST_PARAM_ACTUAL, "http://narbo.de/",                      "http",  NULL,    NULL,   "narbo.de",   80, "/");
	test_url_parse(TEST_PARAM_ACTUAL, "https://narbo.de/",                     "https", NULL,    NULL,   "narbo.de",  443, "/");

	test_url_parse(TEST_PARAM_ACTUAL, "http://narbo.de:1337/",                 "http",  NULL,    NULL,   "narbo.de", 1337, "/");
	test_url_parse(TEST_PARAM_ACTUAL, "http://chris:test@narbo.de:1337/kekse", "http",  "chris", "test", "narbo.de", 1337, "/kekse");
	test_url_parse(TEST_PARAM_ACTUAL, "http://chris@narbo.de:1337/kekse",      "http",  "chris", NULL,   "narbo.de", 1337, "/kekse");
	test_url_parse(TEST_PARAM_ACTUAL, "http://chris:test@narbo.de/kekse",      "http",  "chris", "test", "narbo.de",   80, "/kekse");
	TEST_FUNC_END();

	TEST_END();
}
