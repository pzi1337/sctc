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

/** \file config.c
 *  \brief Implements the parsing of the configfile and provides access to those values.
 */
#include "_hard_config.h"
#include "config.h"

//\cond
#include <assert.h>                     // for assert
#include <errno.h>
#include <stddef.h>                     // for size_t
#include <stdlib.h>                     // for free, NULL
#include <string.h>                     // for strcmp, strlen, strncmp
//\endcond

#include <confuse.h>                    // for cfg_free, cfg_getnstr, etc
#include <ncurses.h>                    // for ERR, KEY_MAX, KEY_BACKSPACE, etc

#include "command.h"
#include "helper.h"                     // for lstrdup, lcalloc
#include "log.h"                        // for _log

#define OPTION_CERT_PATH   "cert_path"
#define OPTION_EQUALIZER   "equalizer"
#define OPTION_SUBSCRIBE   "subscribe"
#define OPTION_CACHE_PATH  "cache_path"
#define OPTION_CACHE_LIMIT "cache_limit"

static char** config_subscribe = NULL;
static size_t config_subscribe_count = 0;

static double config_equalizer[EQUALIZER_SIZE] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

static char* cert_path;
static char* cache_path;
static int   cache_limit;

static void config_finalize(void);

static const char* scopes[] = {
	[scope_global] = "global",
	[scope_playlist] = "playlist",
	[scope_textbox] = "textbox"
};

static struct {
	void (*func)(const char*);
	const char *param;
} key_command_mapping[sizeof(scopes) / sizeof(scopes[0])][KEY_MAX] = { {{NULL, NULL}} };

/** \brief Returns the ncurses keycode for a stringified key
 *
 *  \param str  The stringified key to `decode`
 *  \return     The ncurses keycode or ERR on unknown key / invalid string
 */
static int get_curses_ch(const char *str) {
	// an empty string is always invalid
	if(!str[0]) {
		return ERR;
	}

	// a string of length 1 contains always a single character,
	// which already is a ncurses keycode.
	if(!str[1]) {
		return str[0];
	}

	if(streq("KEY_ENTER",     str)) return KEY_ENTER;
	if(streq("KEY_UP",        str)) return KEY_UP;
	if(streq("KEY_DOWN",      str)) return KEY_DOWN;
	if(streq("KEY_LEFT",      str)) return KEY_LEFT;
	if(streq("KEY_RIGHT",     str)) return KEY_RIGHT;
	if(streq("KEY_BACKSPACE", str)) return KEY_BACKSPACE;
	if(streq("KEY_PPAGE",     str)) return KEY_PPAGE;
	if(streq("KEY_NPAGE",     str)) return KEY_NPAGE;
	if(streq("KEY_SLEFT",     str)) return KEY_SLEFT;
	if(streq("KEY_SRIGHT",    str)) return KEY_SRIGHT;
	if(streq("KEY_HOME",      str)) return KEY_HOME;
	if(streq("KEY_END",       str)) return KEY_END;

	return ERR;
}


static unsigned int kcm_count = 0;

/** \brief Callback for `map()` used by libConfuse
 *
 *  \param cfg   Configuration file context (unused)
 *  \param opt   The option (unused)
 *  \param argc  The (actual) number of parameters to `map`
 *  \param argv  The parameters themself
 *  \return      `0` on success, non-`0` otherwise
 */
static int config_map_command(cfg_t *cfg UNUSED, cfg_opt_t *opt UNUSED, int argc, const char **argv) {
	if(3 != argc) {
		_log("map() requires exactly 3 parameters!");
		return -1;
	}

	// search for scope
	enum scope scope;
	for(scope = 0; scope < scope_size; scope++) {
		if(streq(argv[0], scopes[scope])) {
			break;
		}
	}
	if(scope_size == scope) {
		_log("Unknown scope '%s', ommitting `map(\"%s\", \"%s\", \"%s\")`", argv[0], argv[0], argv[1], argv[2]);
		return 0;
	}

	// search for corresponding key
	int key = get_curses_ch(argv[1]);
	if(ERR == key) {
		_log("Unknown key '%s', ommiting `map(\"%s\", \"%s\", \"%s\")`", argv[1], argv[0], argv[1], argv[2]);
		return 0;
	}

	// search for function to call
	const struct command *cmd = command_search(argv[2], scope);
	if(!cmd) {
		_log("Command '%s' is not known in scope '%s', ommiting `map(\"%s\", \"%s\", \"%s\")`", argv[0], argv[1], argv[0], argv[1], argv[2]);
		return 0;
	}

	kcm_count++;
	_log("| * mapping key \"%s\"(%i) to command \"%s\" (param: \"%s\")", argv[1], key, argv[2], argv[2] + strlen(cmd->name));
	key_command_mapping[scope][key].func  = cmd->func;
	key_command_mapping[scope][key].param = lstrdup( argv[2] + strlen(cmd->name) );

	return 0;
}

void config_error_function(cfg_t *cfg UNUSED, const char *fmt, va_list ap) {

	const size_t buffer_size = 2048;
	char buffer[buffer_size + 1];

	vsnprintf(buffer, buffer_size, fmt, ap);
	_err("%s", buffer);
}

bool config_init(void) {
	cfg_opt_t opts[] = {
		CFG_STR_LIST(OPTION_SUBSCRIBE, "{}", CFGF_NONE), // the list of subscribed users
		CFG_FLOAT_LIST(OPTION_EQUALIZER, "{}", CFGF_NONE),
		CFG_SIMPLE_STR(OPTION_CERT_PATH,   &cert_path),
		CFG_SIMPLE_STR(OPTION_CACHE_PATH,  &cache_path),
		CFG_SIMPLE_INT(OPTION_CACHE_LIMIT, &cache_limit),
		CFG_FUNC("map", config_map_command),
		CFG_END()
	};

	/* set default values for options */
	cert_path   = lstrdup(CERT_DEFAULT_PATH);
	cache_path  = lstrdup(CACHE_DEFAULT_PATH); // default subdir 'cache' in bin
	if(!cert_path || !cache_path) {
		return false;
	}

	cache_limit = -1; // default: no limit

	cfg_t *cfg = cfg_init(opts, CFGF_NOCASE);
	cfg_set_error_function(cfg, config_error_function);

	int parse_result = cfg_parse(cfg, SCTC_CONFIG_FILE);

	if(CFG_FILE_ERROR == parse_result) {
		_err("Failed to open config file `"SCTC_CONFIG_FILE"`");
		cfg_free(cfg);
		return false;
	} else if(CFG_PARSE_ERROR == parse_result) {
		_err("There was at least one parsing failure!");
	} else if(CFG_SUCCESS != parse_result) {
		_err("cfg_parse returned unexpected error code %i", parse_result);
		cfg_free(cfg);
		return false;
	}

	// verify required settings:
	// at least one subscription
	// at least one key mapped
	config_subscribe_count = cfg_size(cfg, OPTION_SUBSCRIBE);

	if(!config_subscribe_count) {
		_log("Have 0 subscriptions, to continue add at least one user");
	}

	if(!kcm_count) {
		_log("Have 0 keymappings, by default you want to have quite a bunch of keymappings...");
	}

	if(!config_subscribe_count || !kcm_count) {
		cfg_free(cfg);
		return false;
	}

	config_subscribe = lcalloc(config_subscribe_count, sizeof(char*));
	for(unsigned int i = 0; i < config_subscribe_count; i++) {
		config_subscribe[i] = lstrdup(cfg_getnstr(cfg, OPTION_SUBSCRIBE, i));
	}

	unsigned int config_equalizer_count = cfg_size(cfg, OPTION_EQUALIZER);
	if(EQUALIZER_SIZE == config_equalizer_count) {
		for(unsigned int i = 0; i < config_equalizer_count; i++) {
			config_equalizer[i] = cfg_getnfloat(cfg, OPTION_EQUALIZER, i);
		}
	} else {
		_log("invalid values for equalizer!");
	}

	cfg_free(cfg);

	_log("config initialized, have values:");
	_log("| subscriptions:");
	for(size_t i = 0; i < config_subscribe_count; i++) {
		_log("| * %s", config_subscribe[i]);
	}
	_log("| cache: `%s`, limit: %i", cache_path, cache_limit);

	if(atexit(config_finalize)) {
		_log("atexit: %s", strerror(errno));
	}

	return true;
}

/** \brief Cleanup data allocated during execution
 */
static void config_finalize(void) {
	for(size_t i = 0; i < config_subscribe_count; i++) {
		free(config_subscribe[i]);
	}
	free(config_subscribe);
	free(cache_path);
	free(cert_path);
}

command_func_ptr config_get_function(enum scope scope, int key) {
	assert(key < KEY_MAX);

	if(!key_command_mapping[scope][key].func) {
		return key_command_mapping[scope_global][key].func;
	}

	return key_command_mapping[scope][key].func;
}

const char* config_get_param(enum scope scope, int key) {
	assert(key < KEY_MAX);

	if(!key_command_mapping[scope][key].func) {
		return key_command_mapping[scope_global][key].param;
	}

	return key_command_mapping[scope][key].param;
}

char* config_get_subscribe(size_t id) {
	assert(id < config_subscribe_count && "ERROR: id >= config_subscribe_count");

	return config_subscribe[id];
}

size_t config_get_subscribe_count(void) { return config_subscribe_count; }
char*  config_get_cert_path(void)       { return cert_path; }
char*  config_get_cache_path(void)      { return cache_path; }
double config_get_equalizer(int band)   { return config_equalizer[band]; }
