#include <stdio.h>
#include <stdlib.h>

#include "../src/config.h"

#include "helper.h"
#include "url.h"
#include "plain.h"
#include "tls.h"
#include "http.h"

#define BUFFER_SIZE 1024 * 512

int main(int argc, char **argv) {

	size_t failed_tcs = 0;

	log_init("run_tests.log");
	config_init();

	if(!test_helper()) failed_tcs++;
	if(!test_url())    failed_tcs++;
	if(!test_plain())  failed_tcs++;
	if(!test_tls())    failed_tcs++;
	if(!test_http())   failed_tcs++;

	if(failed_tcs) {
		fprintf(stderr, "\n\nRESULT: FOUND ERRORS IN %lu MODULES\n", failed_tcs);
	} else {
		fprintf(stderr, "\n\nRESULT: ALL OK\n");
	}

	return EXIT_SUCCESS;
}
