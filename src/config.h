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

#ifndef _CONFIG_H
	#define _CONFIG_H

	//\cond
	#include <stdbool.h>
	//\endcond

	#include "command.h"

	void config_init();
	int config_get_subscribe_count();
	char* config_get_subscribe(int id);
	command_func_ptr config_get_function(int key);
	const char* config_get_param(int key);
	char* config_get_cache_path();
	char* config_get_cert_path();
	bool config_finalize();

#endif /* _CONFIG_H */

