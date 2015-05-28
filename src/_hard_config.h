#ifndef __HARD_CONFIG_H
	#define __HARD_CONFIG_H

	#define BOOKMARK_FILE ".bookmarks.jspf"

	#define SERVER_PORT 443
	#define SERVER_NAME "api.soundcloud.com"

	#define MAX_LISTS 16

	#define MAX_REDIRECT_STEPS 20

	#define CACHE_STREAM_FOLDER "./cache/streams/"
	#define CACHE_STREAM_EXT ".mp3"

	#define CERT_BRAIN_FOLDER "./remembered_certs/" ///< Folder containing the fingerprints of the certificates used by the servers in one of the previous connections
#endif
