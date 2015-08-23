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

#ifndef _COMMAND_H
	#define _COMMAND_H

	//\cond
	#include <stddef.h>                     // for size_t
	//\endcond

	typedef void (*command_func_ptr)(const char*);

	#include "config.h"

	struct command {
		const char  *name;
		command_func_ptr func;
		enum scope   valid_scope;
		const char  *desc_param;
		const char  *desc;
	};

	extern const struct command commands[];
	extern const size_t command_count;
#endif
