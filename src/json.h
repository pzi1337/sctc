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

	struct json_node;

	struct json_array {
		size_t             len;
		struct json_node **nodes;
	};

	struct json_node*  json_parse     (char *data);
	char*              json_get_string(struct json_node *parent, char *path1, char *path2);
	int                json_get_int   (struct json_node *parent, char *path1, char *path2);
	struct json_array* json_get_array (struct json_node *parent, char *path1, char *path2);
#endif
