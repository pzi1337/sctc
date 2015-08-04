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

#ifndef _LOG_H
	#define _LOG_H

	#include "_hard_config.h"

	//\cond
	#include <stdbool.h>
	//\endcond

	/** Initialize logging.
	 *
	 *  Initialize logging and thus allow subsequent calls to _log().
	 *
	 *  *Typically this function is called only once directly after starting up the execution*
	 *
	 *  \param file  The file to log to, either relative or absolute path.
	 *  \returns     true on success, false on error
	 */
	bool log_init(char *file);

	/** Write a line to the logfile.
	 *
	 *  A timestamp it prepended to the line and the location of the call to _log is appended.\n
	 *  Format of timestamp: "Wed Jun 30 21:49:08 1993".\n
	 *  Format of the location: "{ in function_name (file_name.c:line_number) }"\n
	 *  Calling _log() prior to log_init() or after log_close() does not have any effect.
	 *
	 *  Due to the synchronisation of __log(), _log() may be called by several threads 'at once' as well.
	 *
	 *  Always use _log() instead of __log().
	 */
	#define _log(...) __log(__FILE__, __LINE__, __func__, __VA_ARGS__)

	/** The internal implementation for logging.
	 *
	 *  \warning This function is not intended to be called directly by the user.
	 *  Use _log() instead, as this macro inserts the correct position of the call to __log().
	 *
	 *  This function is synchronized internally and therefore may be called by several threads 'at once'.
	 *
	 *  \param srcfile  The file executing the call to __log(); filled by macro _log(), do not use "by hand"
	 *  \param srcline  The line in the file executing the call to __log(); filled by macro _log(), do not use "by hand"
	 *  \param srcfunc  The function callint __log(); filled by macro _log(), do not use "by hand"
	 *  \param fmt      The format used format the line, see man 3 printf for usage.
	 */
	void __log(char *srcfile, int srcline, const char *srcfunc, char *fmt, ...) ATTR(format (printf, 4, 5));
#endif /* _LOG_H */
