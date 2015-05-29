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

/** \file _hard_config.h
 *  \brief Basic configuration of SCTC (**compile time**).
 */

#ifndef __HARD_CONFIG_H
	#define __HARD_CONFIG_H

	#define BOOKMARK_FILE ".bookmarks.jspf"

	#define SERVER_PORT 443
	#define SERVER_NAME "api.soundcloud.com"

	#define MAX_LISTS 16

	#define MAX_REDIRECT_STEPS 20

	#define CACHE_STREAM_FOLDER "./cache/streams/"
	#define CACHE_STREAM_EXT ".mp3"

	#define CACHE_LIST_FOLDER "./cache/lists/"
	#define CACHE_LIST_EXT ".jspf"

	#define CLIENTID "848ee866ea93c21373f6a8b61772b412"

	#define CERT_BRAIN_FOLDER "./remembered_certs/" ///< Folder containing the fingerprints of the certificates used by the servers in one of the previous connections

	#define BENCH_START(ID) \
		struct timespec bench_start##ID; \
		clock_gettime(CLOCK_MONOTONIC, &bench_start##ID);

	#define BENCH_STOP(ID, DESC) { \
			struct timespec bench_end; \
			clock_gettime(CLOCK_MONOTONIC, &bench_end); \
			_log("%s took %dms", DESC, (bench_end.tv_sec - bench_start##ID.tv_sec) * 1000 + (bench_end.tv_nsec - bench_start##ID.tv_nsec) / (1000 * 1000)); \
		}

#endif
