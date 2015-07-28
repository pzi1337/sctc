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

	#include "command.h"                    // for command_func_ptr

	/** \brief Initialize the configuration.
	 *
	 *  Initialize the internal data structures and read the configuration from SCTC_CONFIG_FILE.
	 *  Calling any other config_* function prior to calling `config_init()` yields undefined behaviour
	 *  and is an error.
	 *
	 *  *Typically this function is called only once directly after starting up the execution*
	 */
	void config_init();

	/** \brief Returns the number of subscriptions.
	 *
	 *  \return  The number of subscriptions
	 */
	size_t config_get_subscribe_count();


	char* config_get_subscribe(int id);
	command_func_ptr config_get_function(int key);
	const char* config_get_param(int key);
	char* config_get_cache_path();
	char* config_get_cert_path();
	float config_get_equalizer(int band);

#endif /* _CONFIG_H */

