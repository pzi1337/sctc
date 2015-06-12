#define TEST_FUNC(N) { fprintf(stderr, "\n> "N"(): "); fflush(stderr); }
#define TEST_RES(OK) { bool _ok = (OK); fprintf(stderr, _ok ? "[;32;02mOK[m " : "[;31;02mFAIL[m "); fflush(stderr); if(!_ok) { is_ok = false; } }
