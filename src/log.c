/*
	SCTC - the soundcloud.com client
	Copyright (C) 2015   Christian Eichler

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include "log.h"

//\cond
#include <assert.h>                     // for assert
#include <errno.h>                      // for errno
#include <semaphore.h>                  // for sem_destroy, sem_init, etc
#include <stdarg.h>                     // for va_end, va_list, va_start
#include <stdio.h>                      // for fprintf, NULL, fclose, etc
#include <stdlib.h>                     // for atexit
#include <string.h>                     // for strerror, strlen
#include <time.h>                       // for ctime, time, time_t
//\endcond

#define SHELL_ESC "\x1b"

static void log_close(void);

static FILE* log_fh = NULL;
static sem_t log_sem;

/* init the logging (subsequent calls to _log write to 'file') */
bool log_init(char *file) {
	/* prevent multiple subsequent initializations of logging */
	assert(!log_fh && "logging already initialized");

	log_fh = fopen(file, "w");
	if(NULL == log_fh) {
		fprintf(stderr, "internal error: failed to open logfile '%s'\n", file);
		return false;
	}

	if(-1 != sem_init(&log_sem, 0, 1)) {
		if(!atexit(log_close)) {
			return true;
		}
		_err("atexit: %s", strerror(errno));
		sem_destroy(&log_sem);
	} else {
		fprintf(stderr, "internal error: failed to initialize sem: %s\n", strerror(errno));
	}
	fclose(log_fh);
	log_fh = NULL;

	return false;
}

/* log to file using fmt, just as known from printf */
void __log(const char *srcfile, int srcline, const char *srcfunc, bool is_error, const char *fmt, ...) {
	assert(log_fh && "logging not yet initialized");

	sem_wait(&log_sem);

	// append the current timestamp
	time_t t = time(NULL);
	char *tstring = ctime(&t);
	tstring[strlen(tstring) - 1] = '\0';
	fprintf(log_fh, SHELL_ESC"[;33;02m[%s]"SHELL_ESC"[m ", tstring);
	if(is_error) {
		fprintf(log_fh, SHELL_ESC"[;31;02mERROR: ");
	}

	// append the remaining (user-supplied) data
	va_list ap;
	va_start(ap, fmt);
	vfprintf(log_fh, fmt, ap);
	va_end(ap);

	// append sourcefile and -line (of call to _log)
	fprintf(log_fh, SHELL_ESC"[m "SHELL_ESC"[;32;02m{ in %s (%s:%i) }"SHELL_ESC"[m", srcfunc, srcfile, srcline);

	fprintf(log_fh, "\n");
	fflush(log_fh);

	sem_post(&log_sem);
}

/** Finalize logging.
 *
 *  Closes any opened descriptors and frees any remaining memory allocated during usage.
 *  Any calls to _log() after call to log_close() do not have any effect.
 *
 *  *Typically this function is only called prior to termination.*
 */
static void log_close(void) {
	assert(log_fh && "logging not yet initialized");
	fclose(log_fh);
	log_fh = NULL;

	sem_destroy(&log_sem);
}

