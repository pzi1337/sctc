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
#include <string.h>
#include <assert.h>
//\endcond

#include <confuse.h> /* cfg_*  */
#include <ncurses.h>

#include "config.h"
#include "helper.h"
#include "log.h"
#include "state.h" // FIXME

#define SCTC_CONFIG_FILE "sctc.conf"

#define OPTION_SUBSCRIBE   "subscribe"
#define OPTION_CACHE_PATH  "cache_path"
#define OPTION_CACHE_LIMIT "cache_limit"

static char** config_subscribe = NULL;
static int config_subscribe_count = 0;

static char* cache_path;
static int   cache_limit;

// FIXME `cleanup`
extern struct command commands[];

static struct {
	void (*func)(char*);
	const char *param;
} key_command_mapping[KEY_MAX] = { {NULL} };

static int get_curses_ch(const char *str) {
	if(!str[0]) {
		return ERR;
	}

	if(!str[1]) {
		return str[0];
	}

	if(!strcmp("KEY_ENTER",     str)) return KEY_ENTER;
	if(!strcmp("KEY_UP",        str)) return KEY_UP;
	if(!strcmp("KEY_DOWN",      str)) return KEY_DOWN;
	if(!strcmp("KEY_LEFT",      str)) return KEY_LEFT;
	if(!strcmp("KEY_RIGHT",     str)) return KEY_RIGHT;
	if(!strcmp("KEY_BACKSPACE", str)) return KEY_BACKSPACE;
	if(!strcmp("KEY_PPAGE",     str)) return KEY_PPAGE;
	if(!strcmp("KEY_NPAGE",     str)) return KEY_NPAGE;

	return ERR;
}

// KEY_MAX

int config_map_command(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv) {
	if(2 != argc) {
		_log("map() requires exactly 2 parameters!");
		return -1;
	}

	// search for corresponding key
	int key = get_curses_ch(argv[0]);
	if(ERR == key) {
		_log("Unknown key '%s', ommiting `map(\"%s\", \"%s\")`", argv[0], argv[0], argv[1]);
		return -1;
	}

	// search for function to call
	const size_t in_len = strlen(argv[1]);
	for(size_t i = 0; commands[i].name; i++) {
		const size_t cmd_len = strlen(commands[i].name);

		if(in_len >= cmd_len) {
			if(!strncmp(commands[i].name, argv[1], strlen(commands[i].name))) {
				_log("Mapping key \"%s\"(%i) to command \"%s\"", argv[0], key, argv[1]);
				key_command_mapping[key].func  = commands[i].func;
				key_command_mapping[key].param = argv[1] + strlen(commands[i].name);
				break;
			}
		}
	}

	return 0;
}

void config_init() {
	cfg_opt_t opts[] = {
		CFG_STR_LIST(OPTION_SUBSCRIBE, "{}", CFGF_NONE), // the list of subscribed users
		CFG_SIMPLE_STR(OPTION_CACHE_PATH,  &cache_path),
		CFG_SIMPLE_INT(OPTION_CACHE_LIMIT, &cache_limit),
		CFG_FUNC("map", config_map_command),
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

command_func_ptr config_get_function(int key) {
	assert(key < KEY_MAX);
	return key_command_mapping[key].func;
}

const char* config_get_param(int key) {
	assert(key < KEY_MAX);
	return key_command_mapping[key].param;
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
	free(cache_path);

	return true;
}
