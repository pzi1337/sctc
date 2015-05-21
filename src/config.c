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

//\cond
#include <stdlib.h>  /* free   */
//\endcond

#include <confuse.h> /* cfg_*  */

#include "config.h"
#include "helper.h"
#include "log.h"

#define SCTC_CONFIG_FILE "sctc.conf"

#define OPTION_SUBSCRIBE   "subscribe"
#define OPTION_CACHE_PATH  "cache_path"
#define OPTION_CACHE_LIMIT "cache_limit"

static char** config_subscribe = NULL;
static int config_subscribe_count = 0;

static char* cache_path;
static int   cache_limit;

void config_init() {
	cfg_opt_t opts[] = {
		CFG_STR_LIST(OPTION_SUBSCRIBE, "{}", CFGF_NONE), // the list of subscribed users
		CFG_SIMPLE_STR(OPTION_CACHE_PATH,  &cache_path),
		CFG_SIMPLE_INT(OPTION_CACHE_LIMIT, &cache_limit),
		CFG_END()
	};

	/* set default values for options */
	cache_path  = lstrdup("./cache/"); // default subdir 'cache' in bin
	cache_limit = -1;                  // default: no limit

	cfg_t *cfg = cfg_init(opts, CFGF_NOCASE);
	cfg_parse(cfg, SCTC_CONFIG_FILE);

	config_subscribe_count = cfg_size(cfg, OPTION_SUBSCRIBE);
	config_subscribe       = lcalloc(config_subscribe_count, sizeof(char*));
	for(int i = 0; i < config_subscribe_count; i++) {
		config_subscribe[i] = lstrdup(cfg_getnstr(cfg, OPTION_SUBSCRIBE, i));
	}

	cfg_free(cfg);

	_log("config initialized, have values:");
	_log("| subscriptions:");
	for(int i = 0; i < config_subscribe_count; i++) {
		_log("| * %s", config_subscribe[i]);
	}
	_log("| cache: `%s`, limit: %i", cache_path, cache_limit);
}

int config_get_subscribe_count() {
	return config_subscribe_count;
}

char* config_get_subscribe(int id) {
	return config_subscribe[id];
}

bool config_finalize() {
	for(int i = 0; i < config_subscribe_count; i++) {
		free(config_subscribe[i]);
	}
	free(config_subscribe);

	return true;
}
