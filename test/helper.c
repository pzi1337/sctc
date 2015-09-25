#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helper.h"

#include "../src/helper.h"

bool test_helper() {
	TEST_INIT();
	fprintf(stderr, "helper.o");

	TEST_FUNC_START(smprintf)
	char *str = smprintf("%s%s%s", "a", "b", "c");
	TEST_RES(!strcmp("abc", str));
	free(str);
	TEST_FUNC_END();

	TEST_FUNC_START(snprint_ftime);
	TEST_FUNC_END();

	TEST_FUNC_START(strstrp)
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
	TEST_FUNC_END();

	TEST_FUNC_START(parse_time_to_sec)
	{
		TEST_RES(INVALID_TIME == parse_time_to_sec(""));
		TEST_RES(INVALID_TIME == parse_time_to_sec("x5"));
		TEST_RES(INVALID_TIME == parse_time_to_sec("ASDF"));
		TEST_RES(           5 == parse_time_to_sec("5x"));
		TEST_RES(         930 == parse_time_to_sec("15:30"));
		TEST_RES(        8130 == parse_time_to_sec("2:15:30"));
	}
	TEST_FUNC_END();

	TEST_FUNC_START(yank)
	{
		TEST_RES(yank(""));
		TEST_RES(yank("asdf asdf asdf"));
	}
	TEST_FUNC_END();

	TEST_FUNC_START(add_delta_within_limits)
	{
		TEST_RES(41 == add_delta_within_limits(42, -1, 43));
		TEST_RES(40 == add_delta_within_limits(42, -2, 43));
		TEST_RES(39 == add_delta_within_limits(42, -3, 43));
		TEST_RES(43 == add_delta_within_limits(42,  1, 43));
		TEST_RES(43 == add_delta_within_limits(42,  2, 43));
		TEST_RES(43 == add_delta_within_limits(42,  4, 43));
		TEST_RES(43 == add_delta_within_limits(42,  8, 43));

		TEST_RES(42 == add_delta_within_limits(42,  8, 42));
	}
	TEST_FUNC_END();

	TEST_END();
}
