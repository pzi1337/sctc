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
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
//\endcond

static FILE* log_fh = NULL;
static sem_t log_sem;

/* init the logging (subsequent calls to _log write to 'file') */
bool log_init(char *file) {
	/* prevent multiple subsequent initializations of logging */
	assert(!log_fh);
	log_fh = fopen(file, "w");

	if(!log_fh) {
		fprintf(stderr, "internal error: failed to open logfile '%s'\n", file);
		return false;
	}

	/* initialize the semaphore */
	if(-1 == sem_init(&log_sem, 0, 1)) {
		_log("sem_init failed: %s", strerror(errno));

		fclose(log_fh);
		log_fh = NULL;
		return false;
	}

	return true;
}

/* log to file using fmt, just as known from printf */
void __log(char *srcfile, int srcline, const char *srcfunc, char *fmt, ...) {
	if(!log_fh) return;

	sem_wait(&log_sem);

	// append the current timestamp
	time_t t = time(NULL);
	char *tstring = ctime(&t);
	tstring[strlen(tstring) - 1] = '\0';
	fprintf(log_fh, "[;33;02m[%s][m ", tstring);

	// append the remaining (user-supplied) data
	va_list ap;
	va_start(ap, fmt);
	vfprintf(log_fh, fmt, ap);
	va_end(ap);

	// append sourcefile and -line (of call to _log)
	fprintf(log_fh, " [;32;02m{ in %s (%s:%i) }[m", srcfunc, srcfile, srcline);

	fprintf(log_fh, "\n");
	fflush(log_fh);

	sem_post(&log_sem);
}

/* close files after logging */
void log_close() {
	assert(log_fh);
	fclose(log_fh);
	log_fh = NULL;

	sem_destroy(&log_sem);
}

