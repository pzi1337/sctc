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

	#define USERLIST_FOLDER "custom_lists"
	#define USERLIST_EXT ".jspf"

	#define SUGGESTION_LIST_HEIGHT 15

	#define MAX_LISTS 16

	#define ATTR(...) __attribute__((__VA_ARGS__))
	#define UNUSED __attribute__((__unused__))

	/** \brief The default path to the ca-certificates file
	 *
	 *  Should point to the system wide list of trusted certificates.
	 */
	#define CERT_DEFAULT_PATH "/etc/ssl/certs/ca-certificates.crt"

	/** \brief The maximum number of allowed HTTP redirects */
	#define MAX_REDIRECT_STEPS 20

	#define SCTC_LOG_FILE "sctc.log"

	/** \brief The name of the (default) configfile
	 *
	 *  Keep in mind: this is a default value, which can be modified by the user.
	 *  Do not use its constant value directly.
	 */
	#define SCTC_CONFIG_FILE "sctc.conf"

	/** \brief The default cache path
	 *
	 *  Keep in mind: this is a default value, which can be modified by the user.
	 *  Do not use this constant directly.
	 */
	#define CACHE_DEFAULT_PATH "./cache/"

	/** \brief Folder holding the cached streams
	 *
	 */
	#define CACHE_STREAM_FOLDER "streams"

	/** \brief The file extension used for caching lists.
	 *
	 *  As MP3 is the format provided by soundcloud.com and we directly save
	 *  cache the data from the server, we use .mp3 too.
	 */
	#define CACHE_STREAM_EXT ".mp3"

	/** \brief Folder holding the cached lists
	 *
	 *  The folder specified here is *relative* to the config option `cache_path`
	 *
	 *  \see config.c
	 */
	#define CACHE_LIST_FOLDER "lists"

	/** \brief The file extension used for caching lists.
	 *
	 *  As we use JSPF as format, we'll just use .jspf as extension.
	 */
	#define CACHE_LIST_EXT ".jspf"

	#define DOWNLOAD_MAX_SIZE ( 256 * 1024 * 1024 )

	/** \brief The soundcloud.com API key
	 *
	 *  This key is required to access api.soundcloud.com
	 *  and is appended to every request sent.
	 *
	 *  \see https://developers.soundcloud.com/
	 *  \see http://soundcloud.com/you/apps/new
	 */
	#define SC_API_KEY "848ee866ea93c21373f6a8b61772b412"

	#define CERT_BRAIN_FOLDER "./remembered_certs/" ///< Folder containing the fingerprints of the certificates used by the servers in one of the previous connections

	#define BENCH_START(ID) \
		struct timespec bench_start##ID; \
		clock_gettime(CLOCK_MONOTONIC, &bench_start##ID);

	#define BENCH_STOP(ID, DESC) { \
			struct timespec bench_end; \
			clock_gettime(CLOCK_MONOTONIC, &bench_end); \
			_log("%s took %ldms", DESC, (bench_end.tv_sec - bench_start##ID.tv_sec) * 1000 + (bench_end.tv_nsec - bench_start##ID.tv_nsec) / (1000 * 1000)); \
		}

	#define LOGO_PART "   ____________________\n"\
	"  / __/ ___/_  __/ ___/\n"\
	" _\\ \\/ /__  / / / /__  \n"\
	"/___/\\___/ /_/  \\___/  \n"\
	"  - the soundcloud.com terminal client"
	#define DESCRIPTION_PART "SCTC is a curses based client for soundcloud.com.\nIts primary target is streaming - the most social media - like features are and will not (be) supported."
	#define LICENSE_PART "SCTC is Free Software released under the GPL v3 or later license.\nFor details see LICENSE or visit https://www.gnu.org/copyleft/gpl.html"
	#define ALPHA_PART "This version is NOT a stable release - expect bugs!"
	#define FEATURE_PART "Implemented / functional features:\n - TUI (resizing, updating)\n - playback\n - caching\n - bookmarks"
	#define NONFEATURE_PART "Planned features not yet implemented:\n - playlist management\n - logging in to soundcloud.com (OAuth)\n - reading playlists from soundcloud.com"
	#define KNOWN_BUGS_PART "Known bugs:\n {none}"
	#define PARAGRAPH_PART "\n \n"
#endif
