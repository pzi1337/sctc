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

	#include "_hard_config.h"               // for ATTR

	enum scope {
		scope_global,   ///< Key can be used globally, that is at **every location**
		scope_playlist, ///< Key can be used on **playlist view**
		scope_textbox,  ///< Key can be used on **textbox view** (e.g. the `Details`-Textbox)
		scope_size      ///< The size of `enum scope`
	};

	//\cond
	#include <stdbool.h>                    // for bool
	#include <stddef.h>                     // for size_t
	//\endcond

	#include "command.h"                    // for command_func_ptr

	/** \brief Initialize the configuration.
	 *
	 *  Initialize the internal data structures and read the configuration from SCTC_CONFIG_FILE.
	 *  Calling any other config_* function prior to calling `config_init()` yields undefined behaviour
	 *  and is an error.
	 *
	 *  *Typically this function is called only once directly after starting up the execution*
	 */
	bool config_init(void);

	/** \brief Returns the number of subscriptions.
	 *
	 *  \return  The number of subscriptions
	 */
	size_t config_get_subscribe_count(void);

	/** \brief Returns the number of subscriptions.
	 *
	 *  \return  The number of subscriptions
	 */
	char* config_get_subscribe(size_t id);

	/** \brief Returns the command-function to be executed on keypress
	 *
	 *  At first the specified `scope` is searched.
	 *  If the specified `scope` does not contain a mapped key the `global` scope is searched.
	 *
	 *  Consequently, if `NULL` was returned neither `scope` nor `scope_global` contain a mapping.
	 *
	 *  \param key  The key to find the function for
	 *  \return     The function mapped to a specific key (of NULL if unmapped)
	 */
	command_func_ptr config_get_function(enum scope scope, int key);

	/** \brief Returns the parameter to the command-function to be executed on keypress
	 *
	 *  For details on how the scopes are searched see `config_get_subscribe()`
	 *
	 *  \param key  The key to find the parameter for
	 *  \return     The parameter for the function mapped to a specific key (of NULL if unmapped)
	 *  \see config_get_subscribe()
	 */
	const char* config_get_param(enum scope scope, int key);

	/** \brief Returns the path to the cache
	 *
	 *  \return Path to the cache (guaranteed to be non-`NULL`)
	 */
	char* config_get_cache_path(void) ATTR(returns_nonnull);

	/** \brief Returns the path to the certificates
	 *
	 *  \return Path to the certificates (guaranteed to be non-`NULL`)
	 */
	char* config_get_cert_path (void) ATTR(returns_nonnull);

	#define EQUALIZER_SIZE 32

	/** \brief Returns the value for band `band`
	 *
	 *  \param band  The band to return the value for
	 *  \return      The corresponding value, where `1.0` denotes `no change in volume`
	 */
	double config_get_equalizer(int band);

#endif /* _CONFIG_H */

