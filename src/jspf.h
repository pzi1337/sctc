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

#ifndef _XSPF_H
	#include "track.h"

	/** \brief Write a whole tracklist to file.
	 *
	 *  \param file  The name of the file to write to
	 *  \param list  The tracklist to be written to `file`
	 *  \return      `true` in case of success, `false` otherwise
	 */
	bool jspf_write(char *file, struct track_list *list);

	/** \brief Read a JSPF playlist to a track_list
	 *
	 *  \param file  The file to read from
	 *  \return      The track_list containing data from `file` (or an empty list in case of error)
	 */
	struct track_list* jspf_read(char *file);

	/** \brief Return a descriptive message for the error occured in the last call to jspf_*
	 *
	 *  The memory returned is allocated statically, it must not be `free`'d.
	 *  This function may only be used directly after an error occured(!), calling another
	 *  jspf_* function after the failing call and jspf_error() is undefined behaviour.
	 *
	 *  \return  Pointer the message
	 */
	char* jspf_error();

#endif /* _XSPF_H */
