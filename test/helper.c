#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helper.h"

#include "../src/helper.h"

bool test_helper() {
	fprintf(stderr, "helper.o");

	bool is_ok = true;

	TEST_FUNC("smprintf")
	{
		char *str = smprintf("%s%s%s", "a", "b", "c");
		TEST_RES(!strcmp("abc", str));
		free(str);
	}

	TEST_FUNC("snprint_ftime");

	TEST_FUNC("strstrp")
	{
		char *_in  = strdup(" X ");
		char *_out = strstrp(_in);
		TEST_RES(!strcmp("X", _out));
		free(_in);
	}

	{
		char *_in  = strdup("    X    ");
		char *_out = strstrp(_in);
		TEST_RES(!strcmp("X", _out));
		free(_in);
	}
	{
		char *_in  = strdup(" X      X ");
		char *_out = strstrp(_in);
		TEST_RES(!strcmp("X      X", _out));
		free(_in);
	}

	{
		char *_in  = strdup("     ");
		char *_out = strstrp(_in);
		TEST_RES(!strcmp("", _out));
		free(_in);
	}

	{
		char *_in  = strdup("");
		char *_out = strstrp(_in);
		TEST_RES(!strcmp("", _out));
		free(_in);
	}

	TEST_FUNC("parse_time_to_sec")
	{
		TEST_RES(INVALID_TIME == parse_time_to_sec(""));
		TEST_RES(INVALID_TIME == parse_time_to_sec("x5"));
		TEST_RES(INVALID_TIME == parse_time_to_sec("ASDF"));
		TEST_RES(           5 == parse_time_to_sec("5x"));
		TEST_RES(         930 == parse_time_to_sec("15:30"));
		TEST_RES(        8130 == parse_time_to_sec("2:15:30"));
	}

	TEST_FUNC("yank")
	{
		TEST_RES(yank(""));
		TEST_RES(yank("asdf asdf asdf"));
	}

	return is_ok;
}
