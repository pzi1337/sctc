#include "rc_string.h"

#include <assert.h>     // for assert
#include <stdarg.h>     // for va_list
#include <stdio.h>      // for NULL, vsnprintf
#include <stdlib.h>     // for free

#include "../helper.h"  // for lcalloc

struct rc_string {
	char         *value;
	unsigned int  rc;
};

struct rc_string* rcs_format(char *format, ...) {
	va_list ap;
	va_start(ap, format);
	int required_size = vsnprintf(NULL, 0, format, ap);
	va_end(ap);

	// check for output error
	if(0 > required_size) {
		return NULL;
	}

	size_t buffer_size = ((unsigned int) required_size) + 1;

	va_list aq;
	va_start(aq, format);
	struct rc_string *new_rcs = lcalloc(sizeof(char), sizeof(struct rc_string) + buffer_size);
	if(!new_rcs) {
		return NULL;
	}

	new_rcs->value = (char*) &new_rcs[1];
	new_rcs->rc    = 1;

	vsnprintf(new_rcs->value, buffer_size, format, aq);
	va_end(aq);

	return new_rcs;
}

void rcs_unref(struct rc_string *rcs) {
	if(rcs) {
		assert(rcs->rc && "rcs must have an rc >0");
		if(0 == __sync_sub_and_fetch(&rcs->rc, 1)) {
			free(rcs);
		}
	}
}

void rcs_ref(struct rc_string *rcs) {
	assert(rcs->rc && "rcs must have an rc >0");
	__sync_add_and_fetch(&rcs->rc, 1);
}

char* rcs_value(struct rc_string *rcs) {
	assert(rcs->rc && "rcs must have an rc >0");
	return rcs->value;
}
