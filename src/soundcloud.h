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

/** \file soundcloud.h
 *  \brief Definitions of the functions used for connecting to and receiving data from soundcloud.com
 */

#ifndef _SOUNDCLOUD_H
	#define _SOUNDCLOUD_H

	#include "network/network.h"
	#include "track.h"
	#include "http.h"

	/** \brief Retrieve the whole stream from soundcloud.com/cache (all users specified in sctc.conf)
	 *
	 *  The `track_list` returned is allocated via `malloc` and therefore needs to be freed / passed to `track_list_destroy()`.
	 *
	 *  \return  A `track_list` containing all tracks, or `NULL` in case of failure (failed allocation / connection etc.)
	 *
	 *  \see `track_list_destroy()`
	 */
	struct track_list* soundcloud_get_stream(void);

	/** \brief Retrieve all tracks for a specific user using the provided network connection
	 *
	 *  Retrieve a `track_list` containing all tracks from the specified users stream.
	 *  The data might be cached (and consequently stored to disk/ loaded from disk) to reduce the 
	 *  amount of transfered data and speedup the execution.
	 *
	 *  The network connection `nwc` may be reused afterwards, it is neither closed nor freed.
	 *
	 *  The `track_list` returned is allocated via `malloc` and therefore needs to be freed / passed to `track_list_destroy()`.
	 *
	 *  \param nwc   The network connection to be used, *must not be `NULL`*
	 *  \param user  The user to fetch the `track_list` for, *must not be `NULL`*
	 *  \return      A `track_list` containing the requested data or `NULL` in case of failure
	 *
	 *  \see `track_list_destroy()`
	 */
	struct track_list* soundcloud_get_entries(struct network_conn *nwc, char *user) ATTR(nonnull);

	/** \brief Connect to the stream associated to a specific track
	 *
	 *  Establishes a network connection to a tracks `stream_url`.
	 *  The returned `http_response` contains valid header data and a network connection,
	 *  which is to be used to retrieve the actual data from the server.
	 *
	 *  \param track  The track whose stream to connect to, *must not be `NULL`*
	 *  \param range  
	 *  \return       An `http_response` containing the header and a network connection to receive the actual data
	 */
	struct http_response* soundcloud_connect_track(struct track *track, char *range);

#endif /* _SOUNDCLOUD_H */
