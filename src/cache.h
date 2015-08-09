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

#ifndef _CACHE_H
	#define _CACHE_H
	//\cond
	#include <stdbool.h>                    // for bool
	#include <stddef.h>                     // for size_t
	//\endcond

	#include "track.h"

	/** \brief Check weather a track is cached or not
	 *
	 *  \param track  The track to search for
	 *  \return true if track is cached, otherwise false
	 */
	bool cache_track_exists(struct track *track);

	/** \brief Load a track from the cache into buffer
	 *
	 *  \param track   The track to load
	 *  \return        Pointer to a buffer containing the track, or `NULL` if the track is not within the cache.
	 */
	struct mmapped_file cache_track_get(struct track *track);

	/** \brief Save data for a specific track to cache
	 *
	 *  \param track   The track to save data for
	 *  \param buffer  The data to save
	 *  \param size    The number of Bytes to save
	 *  \return true if saving to cache was successfull, otherwise false 
	 */
	bool cache_track_save(struct track *track, void *buffer, size_t size);
#endif
