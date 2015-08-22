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

#ifndef _DOWNLOADER_H
	#define _DOWNLOADER_H
	//\cond
	#include <stdbool.h>
	#include <stdlib.h>
	//\endcond
	#include "track.h"

	struct download_state {
		struct track *track;      ///< Pointer to the track whose data is being downloaded
		char  *buffer;            ///< The buffer containing the actual data
		size_t bytes_recvd;       ///< The number of Bytes already recvd (already in buffer)
		size_t bytes_total;       ///< The total number of Bytes (total size, as announced by server)
		pthread_mutex_t io_mutex; ///< The mutex to lock on in case of `buffer-underruns`
		pthread_cond_t  io_cond;  ///< The corresponding condition
	};

	/** \brief Initialize the downloader
	 *
	 *  \return `true` on success, `false` otherwise
	 */
	bool downloader_init(void);

	//bool downloader_queue_file(char *url, char *file);

	/** \brief Enqueue a track for downloading
	 *
	 *  Keep in mind that an enqueued track does not indicate a started download.
	 *
	 *  \param track     The track to download
	 *  \param callback  The callback to be called if new data has been downloaded
	 *  \return          A download_state (or `NULL` in case of a failing `malloc`)
	 */
	struct download_state* downloader_queue_buffer(struct track *track, void (*callback)(struct download_state *));

	/** \brief Create (and initialize) a download_state
	 *
	 *  \param track  The track used for initialization of the download_state
	 *  \return       The newly allocated download_state, `NULL` if malloc failed
	 */
	struct download_state* downloader_create_state(struct track *track);
#endif
