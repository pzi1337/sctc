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

#include "json.h"

#include "helper.h"
#include "log.h"

struct json_node {
	yajl_val val;
};

char* json_get_string(struct json_node *parent, char *path1, char *path2) {
	const char* path[] = {path1, path2, NULL};

	yajl_val val = yajl_tree_get(parent->val, path, yajl_t_string);
	if(val) {
		char *str = YAJL_GET_STRING(val);
		if(str) return lstrdup(str);
	}
	return NULL; 
}

int json_get_int(struct json_node *parent, char *path1, char *path2) {
	const char* path[] = {path1, path2, NULL};

	yajl_val val = yajl_tree_get(parent->val, path, yajl_t_number);
	if(val) return YAJL_GET_INTEGER(val);
	return 0; 
}

struct json_array* json_get_array(struct json_node *parent, char *path1, char *path2) {
	const char* path[] = {path1, path2, NULL};

	struct json_array *array = NULL;

	yajl_val val = yajl_tree_get(parent->val, path, yajl_t_array);
	if(YAJL_IS_ARRAY(val)) {
		array = lcalloc(1, sizeof(struct json_array));
		if(array) {
			array->len   = val->u.array.len;

			struct json_node *nodes = lcalloc(array->len + 1, sizeof(struct json_node));
			array->nodes = lcalloc(array->len + 1, sizeof(struct json_node));

			for(size_t i = 0; i < array->len; i++) {
				array->nodes[i]      = &nodes[i];
				array->nodes[i]->val = val->u.array.values[i];
			}
		}
	}

	return array;
}

struct json_node* json_parse(char *data) {

	char errbuf[1024];
	yajl_val val = yajl_tree_parse((const char *) data, errbuf, sizeof(errbuf));

	if(!val) {
		_log("%s", errbuf);
		_log("length of affected string: %d", strlen(data));
		_log("affected string: %s", data);
		return NULL;
	}

	struct json_node *node = lmalloc(sizeof(struct json_node));
	if(node) {
		node->val = val;
	}

	return node;
}
