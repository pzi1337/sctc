#include <assert.h>
#include <stdlib.h>

#define TEST_INIT() \
	bool _all_ok = true; \
	bool *all_ok = &_all_ok;

#define TEST_END() return *all_ok

#define TEST_FUNC_START(N) { \
	bool _is_ok = true; \
	bool *is_ok = &_is_ok; \
	fprintf(stderr, "\n[[;33;02m..[m] "#N"() "); \
	fflush(stderr); \

#define TEST_FUNC_END() \
	fprintf(stderr, *is_ok ? "\r[[;32;02mOK[m]" : "\r[[;31;02mER[m]"); \
}

#define TEST_RES(OK) { \
	bool _ok = (OK); \
	fprintf(stderr, _ok ? "[;32;02m.[m" : "[;31;02m.[m"); \
	fflush(stderr); \
	if(!_ok) { \
		*is_ok  = false; \
		*all_ok = false; \
	} \
}

#define TEST_PARAM bool *all_ok, bool *is_ok
#define TEST_PARAM_ACTUAL all_ok, is_ok

static inline char* string_mutate(char *input, int severity) {
	assert(input && "input must not be NULL");
	assert(0 <= severity && severity <= 100 && "severity is required to be within [0; 100]");

	char *output  = strdup(input);
	size_t length = strlen(input);

	size_t mutation_count = (severity * length) / 100;

	for(size_t i = 0; i < mutation_count; i++) {
		size_t idx = rand() % length;
		output[idx] += rand() % 0xFF;
	}

	return output;
}
