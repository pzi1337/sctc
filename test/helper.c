#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helper.h"

#include "../src/helper.h"

#define TEST_STRSTRP(IN, EXP) { \
	char *_in  = strdup(IN); \
	TEST_RES(!strcmp(EXP, strstrp(_in))); \
	free(_in); \
}

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

	// void sha512_string(char *sha512_buf, void *inbuf, size_t inbuf_size)
	TEST_FUNC_START(sha512_string);
		char sha512_buf[SHA512_LEN * 3 + 1];
		char *inbuf = "TESTTESTTEST";
		sha512_string(sha512_buf, inbuf, strlen(inbuf));
		TEST_RES(!strcmp("0C:9D:DA:E1:DB:1C:E1:C8:E1:EF:7C:CD:8E:B7:F8:34:6C:73:88:9F:24:8F:E4:52:9D:48:94:3A:11:B3:36:CF:0E:3F:89:C9:58:71:9B:EA:96:91:7A:28:B7:7C:93:7E:9E:AE:12:7B:4F:2E:F6:AF:C1:24:30:52:72:09:BA:A7", sha512_buf));

		sha512_string(sha512_buf, "", 0);
		TEST_RES(!strcmp("CF:83:E1:35:7E:EF:B8:BD:F1:54:28:50:D6:6D:80:07:D6:20:E4:05:0B:57:15:DC:83:F4:A9:21:D3:6C:E9:CE:47:D0:D1:3C:5D:85:F2:B0:FF:83:18:D2:87:7E:EC:2F:63:B9:31:BD:47:41:7A:81:A5:38:32:7A:F9:27:DA:3E", sha512_buf));
	TEST_FUNC_END();


	TEST_FUNC_START(strstrp)
	TEST_STRSTRP(" X ",        "X"       )
	TEST_STRSTRP("    X    ",  "X"       )
	TEST_STRSTRP(" X      X ", "X      X")
	TEST_STRSTRP("     ",      ""        )
	TEST_STRSTRP("",           ""        )

	bool complex_ok = true;
	for(size_t i = 0; i < 25000; i++) {
		char *input   = "                                                                                                                                                                                   ";
		char *mutated = string_mutate(input, i % 101);
		char *output  = strstrp(mutated);

		size_t output_len = strlen(output);
		if(output_len > 0) {
			if(' ' == output[0] || ' ' == output[output_len - 1]) {
				complex_ok = false;
			}
		}

		free(mutated);
	}
	TEST_RES(complex_ok);

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
