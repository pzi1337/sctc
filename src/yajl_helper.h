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

#ifndef _JSON_H
	#define _JSON_H

	#include <stddef.h>                     // for size_t

	yajl_val yajl_helper_parse(const char *data);
	char*    yajl_helper_get_string(yajl_val parent, const char *path1, const char *path2);
	int      yajl_helper_get_int   (yajl_val parent, const char *path1, const char *path2);
	yajl_val yajl_helper_get_array (yajl_val parent, const char *path1, const char *path2);
#endif
