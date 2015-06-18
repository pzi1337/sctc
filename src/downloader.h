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
		size_t bytes_recvd;
		bool   finished;
	};

	bool downloader_init();
	//bool downloader_queue_file(char *url, char *file);
	struct download_state* downloader_queue_buffer(struct track *track, void *buffer, size_t buffer_size, void (*callback)(struct download_state *));
	void downloader_finalize();
#endif
