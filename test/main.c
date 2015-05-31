#include <stdio.h>
#include <stdlib.h>

#include "helper.h"

#define BUFFER_SIZE 1024 * 512

int main(int argc, char **argv) {

	size_t failed_tcs = 0;

	if(!test_helper()) failed_tcs++;


	if(failed_tcs) {
		fprintf(stderr, "\n\nRESULT: FOUND ERRORS IN %lu MODULES\n", failed_tcs);
	} else {
		fprintf(stderr, "\n\nRESULT: ALL OK\n");
	}

	return EXIT_SUCCESS;
}
