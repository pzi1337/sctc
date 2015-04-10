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

/** \file helper.h
 *  \brief Several basic helper functions.
*/

#ifndef _HELPER_H
	#define _HELPER_H
	//\cond
	#include <stdbool.h>                    // for bool
	#include <stddef.h>                     // for size_t
	//\endcond

	/** \brief Write a formated version of time_secs to buffer.
	 *
	 *  If time_secs is above one hour the format "%d:%02d:%02d" is used, otherwise "%02d:%02d" is used.\n
	 *  At most buffer_size bytes are written to buffer. If `buffer_size - 1` is returned your output is most likely truncated
	 *  and thus the used buffer should be resized.
	 *
	 *  \param  buffer       The buffer receiving the formated time.
	 *  \param  buffer_size  The size of the buffer.
	 *  \param  time_secs    The seconds to be formated.
	 *  \return              The number of bytes written to buffer (excluding the terminating '\0')
	 */
	int snprint_ftime(char *buffer, size_t buffer_size, int time_secs);

	/** \brief Yank the provided text
	 *
	 *  \param text  The text to yank
	 *  \return      true on success, false otherwise
	 */
	bool yank(char *text);

	/** \brief Logging wrapper for malloc().
	 *
	 *  Behaves like malloc(), but writes a message to the logfile in case of an error.
	 *  lmalloc() does **not terminate** the execution.
	 *
	 *  Always use this macro instead of directly calling _lmalloc().
	 */
	#define lmalloc(S)    _lmalloc (__FILE__, __LINE__, __func__, S)

	/** \brief Logging wrapper for calloc().
	 *
	 *  Behaves like calloc(), but writes a message to the logfile in case of an error.
	 *  lcalloc() does **not terminate** the execution.
	 *
	 *  Always use this macro instead of directly calling _lrealloc().
	 */
	#define lcalloc(N,S)  _lcalloc (__FILE__, __LINE__, __func__, N, S)

	/** \brief Logging wrapper for realloc().
	 *
	 *  Behaves like realloc(), but writes a message to the logfile in case of an error.
	 *  lrealloc() does **not terminate** the execution.
	 *
	 *  Always use this macro instead of directly calling _lrealloc().
	 */
	#define lrealloc(P,S) _lrealloc(__FILE__, __LINE__, __func__, P, S)

	/** The internal implementation for lmalloc.
	 *
	 *  **Warning**: This function is not intended to be called directly by the user.
	 *  Use lmalloc() instead, as this macro inserts the correct position of the call to _lmalloc().
	 *
	 *  \param srcfile  The file executing the call to _lmalloc(); filled by macro lmalloc(), do not use "by hand"
	 *  \param srcline  The line in the file executing the call to _lmalloc(); filled by macro lmalloc(), do not use "by hand"
	 *  \param srcfunc  The function calling _lmalloc(); filled by macro lmalloc(), do not use "by hand"
	 *  \param size     The number of bytes to be allocated
	 */
	void* _lmalloc (char *srcfile, int srcline, const char *srcfunc, size_t size);

	/** The internal implementation for lcalloc.
	 *
	 *  **Warning**: This function is not intended to be called directly by the user.
	 *  Use lcalloc() instead, as this macro inserts the correct position of the call to _lcalloc().
	 *
	 *  \param srcfile  The file executing the call to _lcalloc(); filled by macro lcalloc(), do not use "by hand"
	 *  \param srcline  The line in the file executing the call to _lcalloc(); filled by macro lcalloc(), do not use "by hand"
	 *  \param srcfunc  The function calling _lcallocg(); filled by macro lcalloc(), do not use "by hand"
	 *  \param nmemb    The number of elements to be allocated
	 *  \param size     The size of each single element to be allocated
	 */
	void* _lcalloc (char *srcfile, int srcline, const char *srcfunc, size_t nmemb, size_t size);

	/** The internal implementation for lrealloc.
	 *
	 *  **Warning**: This function is not intended to be called directly by the user.
	 *  Use lrealloc() instead, as this macro inserts the correct position of the call to _lrealloc().
	 *
	 *  \param srcfile  The file executing the call to _lrealloc(); filled by macro lrealloc(), do not use "by hand"
	 *  \param srcline  The line in the file executing the call to _lrealloc(); filled by macro lrealloc(), do not use "by hand"
	 *  \param srcfunc  The function calling _lrealloc(); filled by macro lrealloc(), do not use "by hand"
	 *  \param ptr      Pointer to the memory to resize
	 *  \param size     The number of bytes to be reallocated
	 */
	void* _lrealloc(char *srcfile, int srcline, const char *srcfunc, void *ptr, size_t size);

	/** \brief Logging wrapper for strdup().
	 *
	 *  Behaves like strdup(), but writes a message to the logfile in case of an error.
	 *  lstrdup() does **not terminate** the execution.
	 *
	 *  Always use this macro instead of directly calling _lstrdup().
	 */
	#define lstrdup(S) _lstrdup(__FILE__, __LINE__, __func__, S)

	/** The internal implementation for lstrdup.
	 *
	 *  **Warning**: This function is not intended to be called directly by the user.
	 *  Use lstrdup() instead, as this macro inserts the correct position of the call to _lstrdup().
	 *
	 *  \param srcfile  The file executing the call to _lstrdup(); filled by macro lstrdup), do not use "by hand"
	 *  \param srcline  The line in the file executing the call to _lstrdup(); filled by macro lstrdup(), do not use "by hand"
	 *  \param srcfunc  The function calling _lstrdup(); filled by macro lstrdup(), do not use "by hand"
	 *  \param s        The string to be duplicated
	 */
	char *_lstrdup(char *srcfile, int srcline, const char *srcfunc, const char *s);
#endif /* _HELPER_H */
