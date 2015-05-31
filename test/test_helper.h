#define TEST_FUNC(N) { fprintf(stderr, "\n> "N"(): "); fflush(stderr); }
#define TEST_RES(OK) { fprintf(stderr, OK ? "[;32;02mOK[m " : "[;31;02mFAIL[m "); fflush(stderr); if(!(OK)) { is_ok = false; } }
