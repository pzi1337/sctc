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

/** \file json.c
 *  \brief Functions wrapping the JSON-implementation (abstraction)
 */

#include <yajl/yajl_gen.h>
#include <yajl/yajl_version.h>
#include <yajl/yajl_tree.h>
#include <string.h>

#include "yajl_helper.h"
#include "helper.h"
#include "log.h"

char* yajl_helper_get_string(yajl_val parent, char *path1, char *path2) {
	const char* path[] = {path1, path2, NULL};

	yajl_val val = yajl_tree_get(parent, path, yajl_t_string);
	if(val) {
		char *str = YAJL_GET_STRING(val);
		if(str) return lstrdup(str);
	}
	return NULL; 
}

int yajl_helper_get_int(yajl_val parent, char *path1, char *path2) {
	const char* path[] = {path1, path2, NULL};

	yajl_val val = yajl_tree_get(parent, path, yajl_t_number);
	return val ? YAJL_GET_INTEGER(val) : 0;
}

yajl_val yajl_helper_get_array(yajl_val parent, char *path1, char *path2) {
	const char* path[] = {path1, path2, NULL};

	yajl_val val = yajl_tree_get(parent, path, yajl_t_array);
	return YAJL_IS_ARRAY(val) ? val : NULL;
}

yajl_val yajl_helper_parse(char *data) {
	char errbuf[1024];
	yajl_val val = yajl_tree_parse((const char *) data, errbuf, sizeof(errbuf));

	if(!val) {
		_log("%s", errbuf);
		_log("length of affected string: %d", strlen(data));
		_log("affected string: %s", data);
	}
	return val;
}
