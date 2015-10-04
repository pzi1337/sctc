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

/** \file rc_string.h
 */

#ifndef _RC_STRING_H
	#define _RC_STRING_H

	#include "../_hard_config.h"

	struct rc_string;

	/** \brief printf style function with automated allocation of a reference counting string
	 *
	 *  \param format  The formatstring, as known from `printf`, see `man 3 printf`
	 *
	 *  \return Pointer to an reference counting string (calls to `rcs_ref()` and `rcs_unref()` required) 
	 *          or `NULL` if an error occures
	 */
	struct rc_string* rcs_format(char *format, ...) ATTR(nonnull, format (printf, 1, 2));

	/** \brief Decrement the reference counter for a specific string.
	 *
	 *  As soon as the reference counter reaches zero, the whole structure is freed and therefore
	 *  must no longer be accessed.
	 *
	 *  \param The specific rc_string
	 */
	void rcs_unref(struct rc_string *rcs) ATTR(nonnull);

	/** \brief Increment the reference counter for a specific string
	 *
	 *  \param The specific rc_string
	 */
	void rcs_ref(struct rc_string *rcs) ATTR(nonnull);

	/** \brief Returns the value associated with the rc_string
	 *
	 *  \remark The reference counter is required to be > 0, otherwise we are accessing freed memory.
	 *
	 *  \param rcs  The rc_string to receive the value for
	 *
	 *  \return The value associated with `rcs`
	 */
	char* rcs_value(struct rc_string *rcs) ATTR(nonnull, returns_nonnull);
#endif /* _RC_STRING_H */
