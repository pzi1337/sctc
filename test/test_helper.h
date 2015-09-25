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
