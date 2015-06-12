#include "url.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helper.h"

#include "../src/url.h"

#define CSTR(X, Y) (X == NULL ? !Y : !strcmp(X, Y))

static bool is_ok;

static void test_url_parse(char *str, char *scheme, char *user, char *pass, char *host, int port, char *request) {
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
	fprintf(stderr, "\n\nurl.o");

	is_ok = true;

	TEST_FUNC("url_parse_string")
	test_url_parse("http://narbo.de/",                      "http",  NULL,    NULL,   "narbo.de",   80, "/");
	test_url_parse("https://narbo.de/",                     "https", NULL,    NULL,   "narbo.de",  443, "/");

	test_url_parse("http://narbo.de:1337/",                 "http",  NULL,    NULL,   "narbo.de", 1337, "/");
	test_url_parse("http://chris:test@narbo.de:1337/kekse", "http",  "chris", "test", "narbo.de", 1337, "/kekse");
	test_url_parse("http://chris@narbo.de:1337/kekse",      "http",  "chris", NULL,   "narbo.de", 1337, "/kekse");
	test_url_parse("http://chris:test@narbo.de/kekse",      "http",  "chris", "test", "narbo.de",   80, "/kekse");
	
	return is_ok;
}
