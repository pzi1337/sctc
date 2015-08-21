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

/** \file command.c
 *  \brief Implementation of all the commands supported by SCTC
 *
 *  This file contains the actual implementation of the all cmd_* functions.
 *  These function contain the actual behaviour in case of user input.
 *  Any command may be mapped to a key via configfile.
 *  \see config.c
 */

#include "_hard_config.h"
#include "command.h"

//\cond
#include <stdbool.h>                    // for false, true, bool
#include <stddef.h>                     // for size_t, NULL
#include <stdio.h>                      // for sscanf, sprintf
#include <stdlib.h>                     // for exit, free, strtol, etc
#include <string.h>                     // for strlen, strncmp, memcpy, etc
//\endcond

#include <ncurses.h>                    // for LINES, getch, KEY_BACKSPACE, etc

#include "config.h"                     // for config_finalize
#include "helper.h"                     // for smprintf, strstrp, etc
#include "jspf.h"                       // for jspf_write
#include "log.h"                        // for _log, log_close
#include "network/network.h"            // for network_conn
#include "network/tls.h"                // for tls_connect, tls_finalize
#include "sound.h"                      // for sound_get_current_pos, etc
#include "soundcloud.h"                 // for soundcloud_get_entries
#include "state.h"                      // for state_set_status, etc
#include "track.h"                      // for track, track_list, etc
#include "tui.h"                        // for tui_submit_action, F_BOLD, etc

#define NONE ((unsigned int) ~0)
#define NO_TRACK ((size_t) ~0)

#define TIME_BUFFER_SIZE 64

#define MIN(x,y) (x < y ? x : y)

static void command_dispatcher(char *command);
static void search_direction(bool down);
static void switch_to_list(unsigned int id);
static bool handle_search(char *buffer, size_t buffer_size);
static void stop_playback(bool reset);
static void handle_textbox(void);
static size_t submit_updated_suggestion_list(struct command *buffer, char *filter);
static bool handle_command(char *buffer, size_t buffer_size);

static void cmd_open_user     (const char *_user) ATTR(nonnull);
static void cmd_add           (const char *_list) ATTR(nonnull);
static void cmd_list_new      (const char *_name) ATTR(nonnull);
static void cmd_write_playlist(const char *_file) ATTR(nonnull);
static void cmd_goto          (const char *_hint) ATTR(nonnull);
static void cmd_list          (const char *list)  ATTR(nonnull);
static void cmd_seek          (const char *time)  ATTR(nonnull);
static void cmd_volume        (const char *_hint) ATTR(nonnull);
static void cmd_repeat        (const char *rep)   ATTR(nonnull);

static void cmd_download      (const char *unused UNUSED);
static void cmd_close         (const char *unused UNUSED);
static void cmd_del           (const char *unused UNUSED);
static void cmd_exit          (const char *unused UNUSED) ATTR(noreturn);
static void cmd_scroll        (const char *_hint);
static void cmd_search_next   (const char *unused UNUSED);
static void cmd_search_prev   (const char *unused UNUSED);
static void cmd_search_start  (const char *unused UNUSED);
static void cmd_command_input (const char *unused UNUSED);
static void cmd_help          (const char *unused UNUSED);
static void cmd_yank          (const char *unused UNUSED);
static void cmd_details       (const char *unused UNUSED);
static void cmd_play          (const char *unused UNUSED);
static void cmd_pause         (const char *unused UNUSED);
static void cmd_stop          (const char *unused UNUSED);
static void cmd_redraw        (const char *unused UNUSED);

/** \brief Array of all commands supported by SCTC.
 *
 *  Includes a description, which is, for instance, shown in the commandwindow, and the function to call in
 *  order to execute the command.
 *
 *  \todo At this point only the `listview` can be used via commands
 */
const struct command commands[] = {
	{"add",           cmd_add,            scope_playlist, "<ID of list>",                  "Add currently selected track to playlist with provided ID"},
	{"command-input", cmd_command_input,  scope_playlist, "<none/ignored>",                "Open command input field"},
	{"close",         cmd_close,          scope_textbox,  "<none/ignored>",                "Close the currently visible textbox"},
	{"del",           cmd_del,            scope_playlist, "<none/ignored>",                "Delete currently selected track from current playlist"},
	{"details",       cmd_details,        scope_playlist, "<none/ignored>",                "Show details for currently selected track"},
	{"download",      cmd_download,       scope_playlist, "<none/ignored>",                "Download the currently selected entry to file"},
	{"exit",          cmd_exit,           scope_playlist, "<none/ignored>",                "Terminate SCTC"},
	{"goto",          cmd_goto,           scope_playlist, "<relative or absolute offset>", "Set selection to specific entry"},
	{"help",          cmd_help,           scope_playlist, "<none/ignored>",                "Show help"},
	{"list",          cmd_list,           scope_playlist, "<number of list>",              "Switch to specified playlist"},
	{"list-new",      cmd_list_new,       scope_playlist, "<name of list>",                "Create a new playlist with specified name"},
	{"open",          cmd_open_user,      scope_playlist, "<name of user>",                "Open a specific user's stream"},
	{"pause",         cmd_pause,          scope_global,   "<none/ignored>",                "Pause playback of current track"},
	{"play",          cmd_play,           scope_playlist, "<none/ignored>",                "Start playback of currently selected track"},
	{"redraw",        cmd_redraw,         scope_global,   "<none/ignored>",                "Redraw the screen"},
	{"repeat",        cmd_repeat,         scope_global,   "{,none,one,all}",               "Set/Toggle repeat"},
	{"scroll",        cmd_scroll,         scope_textbox,  "<relative or absolute offset>", "Scroll a textbox up/down"},
	{"search-start",  cmd_search_start,   scope_playlist, "<none/ignored>",                "Start searching (open input field)"},
	{"search-next",   cmd_search_next,    scope_playlist, "<none/ignored>",                "Continue search downwards"},
	{"search-prev",   cmd_search_prev,    scope_playlist, "<none/ignored>",                "Continue search upwards"},
	{"seek",          cmd_seek,           scope_playlist, "<time to seek to>",             "Seek to specified time in current track"},
	{"stop",          cmd_stop,           scope_global,   "<none/ignored>",                "Stop playback of current track"},
	{"vol",           cmd_volume,         scope_global,   "<delta (in percent)>",          "modify playback volume by given percentage"},
	{"write",         cmd_write_playlist, scope_playlist, "<filename>",                    "Write current playlist to file (.jspf)"},
	{"yank",          cmd_yank,           scope_playlist, "<none/ignored>",                "Copy URL of currently selected track to clipboard"},
	{NULL, NULL, 0, NULL, NULL}
};
const size_t command_count = sizeof(commands) / sizeof(struct command) - 1;

/**
 *  These functions represent the individual commands action.\n
 *  For instance executing a `redraw` will result in calling the function cmd_redraw().
 *
 *  \todo Commands are, at this point, only usefull for the main interface (the list).
 *        As soon as we open any other window everything is hardcoded again!
 */
static void command_dispatcher(char *command) {

	// try to find the command to execute within the list of known commands
	for(int i = 0; commands[i].name; i++) {
		const size_t ci_size = strlen(commands[i].name);

		if(!strncmp(commands[i].name, command, ci_size)) {
			if('\0' == command[ci_size] || ' ' == command[ci_size]) {
				commands[i].func(&command[ci_size + 1]);
				return;
			}
		}
	}

	// the shortcut `:<number>` as known by px. vim
	// strtol only used for check if :... is numeric, execution is done in cmd_goto
	char *endptr;
	(void) strtol(command, &endptr, 10);
	if('\0' == *endptr) {
		cmd_goto(command);
		return;
	}

	state_set_status(cline_warning, smprintf("Error: Unknown command "F_BOLD"%s"F_RESET, command));
}

/** \todo a) does not belong here\n b) does not work */
static void cmd_download(const char *unused UNUSED) { // TODO!
	state_set_status(cline_warning, "Error: Downloading not yet implemented");
}

static void cmd_open_user(const char *_user) {
	astrdup(tuser, _user);
	char *user = strstrp(tuser);
	state_set_status(cline_default, smprintf("Info: Switching to "F_BOLD"%s"F_RESET"'s channel\n", user));

	struct network_conn *nwc = tls_connect(SERVER_NAME, SERVER_PORT);
	struct track_list *list = soundcloud_get_entries(nwc, user);
	nwc->disconnect(nwc);

	if(list->count) {
		list->name = TRACK(list, 0)->username;
		state_add_list(list);
	} else {
		state_set_status(cline_warning, smprintf("Info: Cannot switch to "F_BOLD"%s"F_RESET"'s channel: No tracks found!\n", user));
		track_list_destroy(list, true);
	}
}

static void cmd_add(const char *_list) {
	unsigned int list_id = NONE;
	if(1 != sscanf(_list, " %8u ", &list_id)) {
		state_set_status(cline_warning, smprintf("Error: "F_BOLD"%s"F_RESET" is not numeric, expecting ID of playlist", _list));
		return;
	}

	if(1 == list_id) {
		state_set_status(cline_warning, "Error: Cannot add tracks to "F_BOLD"Stream"F_RESET);
		return;
	}

	struct track_list *list = state_get_list(list_id - 1);
	if(!list) {
		state_set_status(cline_warning, smprintf("Error: No such list "F_BOLD"%s"F_RESET, _list));
		return;
	}

	struct track_list *clist = state_get_list(state_get_current_list());
	if(2 == list_id) {
		TRACK(clist, state_get_current_selected())->flags |= FLAG_BOOKMARKED;
		tui_submit_action(update_list);
	}

	struct track track = {
		.name = NULL,
		.href = TRACK(clist, state_get_current_selected())
	};
	track_list_add(list, &track);

	// update the view, if we just added a track from to current list to the current list
	if(state_get_current_list() == list_id - 1) {
		tui_submit_action(update_list);
	}
	state_set_status(cline_default, smprintf("Info: Added "F_BOLD"%s"F_RESET" to %s", TRACK(clist, state_get_current_selected())->name, list->name));
}

static void cmd_del(const char *unused UNUSED) {
	if(!state_get_current_list()) {
		state_set_status(cline_warning, "Error: Cannot delete tracks from "F_BOLD"Stream"F_RESET);
		return;
	}

	// ensure we do not keep an element selected, which is not part
	// of the list (e.g. the list is empty, or we are pointing `behind` the list)
	size_t current_selected = state_get_current_selected();
	struct track_list *list = state_get_list(state_get_current_list());

	if(1 == list->count) {
		state_set_current_list(LIST_STREAM);
	} else if(current_selected >= list->count - 1) {
		state_set_current_selected(list->count - 2);
	}

	track_list_del(list, current_selected);
	tui_submit_action(update_list);
}

static void cmd_list_new(const char *_name) {
	astrdup(tname, _name);
	char *name = strstrp(tname);

	struct track_list *list = track_list_create(name);
	if(!list) {
		state_set_status(cline_warning, "Error: Failed to allocate memory for new list!");
	} else {
		state_add_list(list);
	}
}

static void cmd_write_playlist(const char *_file) {
	astrdup(tfile, _file);
	char *file = strstrp(tfile);
	struct track_list *list = state_get_list(state_get_current_list());

	state_set_status(cline_default, smprintf("Info: Writing to file "F_BOLD"%s"F_RESET" (type: JSPF)\n", file));
	if(!jspf_write(file, list)) {
		state_set_status(cline_warning, smprintf("Error: Writing to file "F_BOLD"%s"F_RESET" failed: "F_BOLD"%s"F_RESET"\n", file, jspf_error()));
	}
}

static void cmd_goto(const char *_hint) {
	astrdup(hint, _hint);
	char *target = strstrp(hint);

	if(streq("", target)) {
		if(state_get_current_playback_list() == state_get_current_list()) {
			state_set_current_selected(state_get_current_playback_track());
		}
		state_set_status(cline_default, "");
	} else if(streq("end", target)) {
		struct track_list *list = state_get_list(state_get_current_list());

		state_set_current_selected(list->count - 1);
		state_set_status(cline_default, "");
	} else if('+' == *target || '-' == *target) {
		int delta = 0;
		bool valid = false;
		struct track_list *list = state_get_list(state_get_current_list());

		if(NULL != strchr(target, '.')) {
			float pages = 0;
			valid = (1 == sscanf(target, " %16f ", &pages));

			delta = (int) (pages * (float) (LINES - 2));
		} else {
			valid = (1 == sscanf(target, " %16d ", &delta));
		}
		if(valid) {
			size_t csel = state_get_current_selected();
			state_set_current_selected(add_delta_within_limits(csel, delta, list->count - 1));
		}
	} else {
		unsigned int pos;
		if(1 == sscanf(target, " %8u ", &pos)) {
			state_set_current_selected(pos);
		}
	}
}

/** \brief Switch to playlist with `id`
 *
 *  \param id  The id of the playlist to switch to
 */
static void switch_to_list(unsigned int id) {
	struct track_list *list = state_get_list(id);

	if(list) {
		if(list->count) {
			state_set_current_list(id);
		} else {
			state_set_status(cline_warning, smprintf("Error: Not switching to "F_BOLD"%s"F_RESET": List is empty", list->name));
		}
	} else {
		state_set_status(cline_warning, smprintf("Error: Not switching to list %u: No such list", (id + 1)));
	}
}

/** \brief Switch to playlist specified in string `list`
 *
 *  \param list  The id of the playlist to switch to
 */
static void cmd_list(const char *list) {
	unsigned int list_id;

	if(1 == sscanf(list, " %8u ", &list_id)) {
		switch_to_list(list_id - 1);
	} else {
		state_set_status(cline_warning, smprintf("Error: Not switching to list "F_BOLD"%s"F_RESET": Expecting numeric ID", list));
	}
}

static void cmd_scroll(const char *_hint) {
	astrdup(hint, _hint);
	char *target = strstrp(hint);

	if('+' == *target || '-' == *target) {
		int delta = 0;
		bool valid = false;

		if(NULL != strchr(target, '.')) {
			float pages = 0;
			valid = (1 == sscanf(target, " %16f ", &pages));

			delta = (int) (pages * (float) (LINES - 8));
		} else {
			valid = (1 == sscanf(target, " %16d ", &delta));
		}
		if(valid) state_set_tb_pos_rel(delta);
	}
}

static void cmd_seek(const char *_time) {
	astrdup(time, _time);
	char *seekto = strstrp(time);
	unsigned int new_abs = INVALID_TIME;

	if('+' == *seekto || '-' == *seekto) {
		// relative offset to current position
		unsigned int delta = parse_time_to_sec(seekto + 1);
		if(INVALID_TIME != delta) {
			new_abs = state_get_current_playback_time();
			if('+' == *seekto) {
				new_abs += delta;
			} else {
				new_abs = delta < new_abs ? (new_abs - delta) : 0;
			}
		}
	} else {
		// absolute
		new_abs = parse_time_to_sec(seekto);
	}

	if(INVALID_TIME == new_abs) {
		_log("seeking aborted due to input error");
	} else {
		sound_seek(new_abs);
	}
}

/** \brief Exit SCTC
 *
 *  Writes the list of bookmarks to file and shuts down SCTC.
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 */
static void cmd_exit(const char *unused UNUSED) {
	jspf_write(BOOKMARK_FILE, state_get_list(LIST_BOOKMARKS));

	char *cache_path = config_get_cache_path();
	struct track_list *list;
	for(size_t list_id = 2; (list = state_get_list(list_id)); list_id++) {
		char target_file[strlen(cache_path) + 1 + strlen(USERLIST_FOLDER) + 1 + strlen(list->name) + strlen(USERLIST_EXT) + 1];
		sprintf(target_file, "%s/"USERLIST_FOLDER"/%s"USERLIST_EXT, cache_path, list->name);

		jspf_write(target_file, list);
	}

	for(size_t i = 0; i < MAX_LISTS; i++) {
		if( (list = state_get_list(i)) ) {
			track_list_destroy(list, true);
		}
	}

	ONLY_DEBUG( dump_alloc_counter(); )

	exit(EXIT_SUCCESS);
}

static void search_direction(bool down) {
	struct track_list *list = state_get_list(state_get_current_list());
	const int step = down ? 1 : -1;

	size_t current = state_get_current_selected();
	while(current > 0 && current < list->count - 1) {
		current += step;

		if(strcasestr(TRACK(list, current)->name, state_get_input())) {
			state_set_current_selected(current);
			return;
		}
	}
	state_set_status(cline_warning, smprintf("search hit %s", down ? "BOTTOM" : "TOP"));
}

static bool handle_search(char *buffer, size_t buffer_size) {
	// `clear` the buffer on starting search
	buffer[0] = '\0';

	tui_submit_action(input_modify_text);

	size_t pos = 0;
	int c;
	while( (c = getch()) ) {
		switch(c) {
			case KEY_EXIT: // ESC
			case 0x1B:
				return false;

			case 0x0A: // LF (aka 'enter')
			case KEY_ENTER:
				return true;

			case KEY_BACKSPACE:
				if(pos) {
					pos--;
					buffer[pos] = '\0';
					tui_submit_action(input_modify_text);
				} else {
					return false;
				}
				break;

			default: {
				buffer[pos] = c;
				buffer[pos + 1] = '\0';
				tui_submit_action(input_modify_text);
				pos++;

				if(pos == buffer_size) {
					return true;
				}
			}
		}
	}

	return false; // never reached
}

static void cmd_search_next(const char *unused UNUSED) { search_direction(true);  }
static void cmd_search_prev(const char *unused UNUSED) { search_direction(false); }

static void cmd_search_start(const char *unused UNUSED) {
	state_set_status(cline_cmd_char, F_BOLD"/"F_RESET);
	handle_search(state_get_input(), 127);

	cmd_search_next(NULL);
}

/** \brief Initiate a command input
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 */
static void cmd_command_input(const char *unused UNUSED) {
	state_set_status(cline_cmd_char, strdup(F_BOLD":"F_RESET));

	char *buffer = state_get_input();

	if(handle_command(buffer, 127)) {
		command_dispatcher(buffer);
	} else {
		state_set_status(cline_default, "");
	}

}

/** \brief Display 'Help' Dialog
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 */
static void cmd_help(const char *unused UNUSED) {
	char *help_msg = LOGO_PART PARAGRAPH_PART DESCRIPTION_PART PARAGRAPH_PART ALPHA_PART PARAGRAPH_PART FEATURE_PART PARAGRAPH_PART NONFEATURE_PART PARAGRAPH_PART KNOWN_BUGS_PART PARAGRAPH_PART LICENSE_PART;

	state_set_tb("Help / About", help_msg);
	handle_textbox();
}

/** \brief Set repeat to 'rep'
 *
 *  \param rep  The type of repeat to use (one in {none,one,all})
 */
static void cmd_repeat(const char *_rep) {
	astrdup(trep, _rep);
	char *rep = strstrp(trep);

	if(streq("", rep)) {
		switch(state_get_repeat()) {
			case rep_none: cmd_repeat("one");  break;
			case rep_one:  cmd_repeat("all");  break;
			case rep_all:  cmd_repeat("none"); break;

			/* no error-handling default case here, `enum repeat` only has 3 values */
			default: break;
		}
	} else {
		if     (streq("none", rep)) state_set_repeat(rep_none);
		else if(streq("one",  rep)) state_set_repeat(rep_one);
		else if(streq("all",  rep)) state_set_repeat(rep_all);
		else {
			state_set_status(cline_warning, smprintf("Error: "F_BOLD"%s"F_RESET" is not in {none,one,all}", rep));
			return;
		}
		state_set_status(cline_default, smprintf("Info: Switched repeat to '%s'", rep));
	}
}

static void cmd_yank(const char *unused UNUSED) {
	struct track_list *list = state_get_list(state_get_current_list());
	size_t current_selected = state_get_current_selected();

	yank(TRACK(list, current_selected)->permalink_url);
	state_set_status(cline_default, smprintf("yanked "F_BOLD"%s"F_RESET, TRACK(list, current_selected)->permalink_url));
}

static void cmd_details(const char *unused UNUSED) {
	struct track_list *list = state_get_list(state_get_current_list());
	size_t current_selected = state_get_current_selected();

	size_t url_count = TRACK(list, current_selected)->url_count;
	char **urls      = TRACK(list, current_selected)->urls;

	if(URL_COUNT_UNINITIALIZED == url_count) {
		url_count = string_find_urls(TRACK(list, current_selected)->description, &urls);
		TRACK(list, current_selected)->url_count = url_count;
		TRACK(list, current_selected)->urls = urls;

		char *old_desc = TRACK(list, current_selected)->description;
		char *new_desc = string_prepare_urls_for_display(old_desc, url_count);
		free(old_desc);
		TRACK(list, current_selected)->description = new_desc;
	}

	char *title = smprintf("%s by %s", TRACK(list, current_selected)->name, TRACK(list, current_selected)->username);
	state_set_tb(title, TRACK(list, current_selected)->description);
	handle_textbox();
	free(title);
}

static void update_flags_stop_playback(struct track_list *list, size_t tid) {
	if(list && NO_TRACK != tid) {
		TRACK(list, tid)->flags &= (uint8_t)~FLAG_PLAYING;
		if(TRACK(list, tid)->current_position) {
			TRACK(list, tid)->flags |= FLAG_PAUSED;
		}
	}
}

/** \brief Stop playback of currently playing track
 *
 *  Nothing happens in case no track is currently playing.
 *  If `reset` is set to `true` the position of the currently plaing
 *  track is reset to 0, if set to `false` the position will not be modified.
 *  The position can be used lateron to continue playing.
 *
 *  \param reset  Reset the current position of playback to 0
 */
static void stop_playback(bool reset) {
	size_t playing = state_get_current_playback_track();

	if(NO_TRACK != playing) {
		if(!sound_stop()) {
			_log("failed to stop playback: %s", sound_error());
		}

		struct track_list *list = state_get_list(state_get_current_playback_list());
		if(reset) {
			TRACK(list, playing)->current_position = 0;
		}
		update_flags_stop_playback(list, playing);

		tui_submit_action(update_list);

		state_set_current_playback(0, ~0);
	}
}

static void cmd_play(const char *unused UNUSED) {
	size_t current_selected = state_get_current_selected();
	struct track_list *list = state_get_list(state_get_current_list());

	char time_buffer[TIME_BUFFER_SIZE];
	snprint_ftime(time_buffer, TIME_BUFFER_SIZE, TRACK(list, current_selected)->duration);

	update_flags_stop_playback(state_get_list(state_get_current_playback_list()), state_get_current_playback_track());

	state_set_current_playback(state_get_current_list(), current_selected);
	size_t playing = state_get_current_playback_track();

	state_set_title(smprintf("Now playing "F_BOLD"%s"F_RESET" by "F_BOLD"%s"F_RESET" (%s)", TRACK(list, playing)->name, TRACK(list, playing)->username, time_buffer));
	TRACK(list, current_selected)->flags = (uint8_t) ( (TRACK(list, current_selected)->flags & ~FLAG_PAUSED) | FLAG_PLAYING );

	tui_submit_action(update_list);

	sound_play(TRACK(list, current_selected));
}

static void cmd_pause(const char *unused UNUSED) { stop_playback(false); }
static void cmd_stop (const char *unused UNUSED) { stop_playback(true);  }

static void cmd_volume(const char *_hint) {
	astrdup(hint, _hint);
	char *delta_str = strstrp(hint);
	int delta;
	if(1 == sscanf(delta_str, " %4i ", &delta)) {
		unsigned int vol = sound_change_volume(delta);
		_log("volume now: %i, delta: %i", vol, delta);

		state_set_volume(vol);
	}
}

/** \brief Issue a redraw of the whole screen
 */
static void cmd_redraw(const char *unused UNUSED) {
	tui_submit_action(redraw);
}

static void cmd_close(const char *unused UNUSED) {
	state_set_tb_pos(0);
	state_set_tb(NULL, NULL);
}

/** \brief Handle input for a textbox
 *
 *  Handles user's input for a textbox, such as `scroll up`, `scroll down`
 *  and `close textbox`.
 */
static void handle_textbox(void) {
	int c;
	while( (c = getch()) ) {
		command_func_ptr func = config_get_function(scope_textbox, c);
		if(func) {
			func(config_get_param(scope_textbox, c));
			if(func == cmd_close) {
				return;
			}
		} else {
			if('0' <= c && c <= '9') {
				unsigned int idx = c - '0';
				struct track_list *list = state_get_list(state_get_current_list());
				size_t current_selected = state_get_current_selected();

				size_t url_count = TRACK(list, current_selected)->url_count;
				char **urls = TRACK(list, current_selected)->urls;

				if(idx < url_count) {
					fork_and_run("xdg-open", urls[idx]);
				}
			}
		}
	}
}


/** \brief Generate a new list of commands using the filter `filter`.
 *
 *  Filtering equals to matching the first `strlen(filter)` chars against the individual commands' names.
 *  Matching is done case-insensitive.
 *
 *  \param[out] buffer  A pointer to a buffer holding the commands (is expected to be of size `command_count + 1) * sizeof(struct command)`)
 *  \param filter       The filter to use for filtering the list of commands
 *  \return             The number of elements matching the filter
 */
static size_t submit_updated_suggestion_list(struct command *buffer, char *filter) {
	size_t matching_pos = 0;
	for(size_t i = 0; i < command_count; i++) {
		if(!strncmp(commands[i].name, filter, strlen(filter))) {
			memcpy(&buffer[matching_pos], &commands[i], sizeof(struct command));
			matching_pos++;
		}
	}
	bzero(&buffer[matching_pos], sizeof(struct command));

	tui_submit_action(sugg_modified);
	return matching_pos;
}

/** \brief Handle input for a command
 *
 *  Note that `buffer` is modified even if `handle_command()` returns `false`.
 *
 *  \param[out] buffer  Pointer to a buffer receiving the user's input
 *  \param buffer_size  Size of buffer (in Bytes)
 *  \return             `true` if a command is in `buffer`, false otherwise
 */
static bool handle_command(char *buffer, size_t buffer_size) {
	struct command matching_commands[command_count + 1];
	memcpy(matching_commands, commands, (command_count + 1) * sizeof(struct command));

	// `clear` the buffer on starting search
	buffer[0] = '\0';

	size_t shown_commands = command_count;

	size_t pos = 0; // the position in buffer
	size_t suggest_pos = 0;
	size_t pos_prior_suggest = NOTHING_SELECTED;

	bool retab = false;

	tui_submit_action(input_modify_text);

	state_set_commands(matching_commands);
	state_set_sugg_selected(NOTHING_SELECTED);

	int c;
	while( (c = getch()) ) {
		size_t selected = state_get_sugg_selected();

		switch(c) {
			case KEY_EXIT: // ESC
			case 0x1B:
				state_set_commands(NULL);
				return false;

			case 0x0A: // LF (aka 'enter')
			case KEY_ENTER:
				// disable the suggestion list
				state_set_commands(NULL);
				return true;

			case KEY_UP:
				if(NOTHING_SELECTED == selected) {
					state_set_sugg_selected(shown_commands - 1);
				} else if(selected > 0) {
					state_set_sugg_selected(selected - 1);
				} else {
					state_set_sugg_selected(NOTHING_SELECTED);
				}
				retab = false;
				break;

			case KEY_DOWN:
				if(NOTHING_SELECTED == selected) {
					state_set_sugg_selected(0);
				} else if(selected < shown_commands) {
					state_set_sugg_selected(selected + 1);
				} else {
					state_set_sugg_selected(NOTHING_SELECTED);
				}
				retab = false;
				break;

			case 0x09: // TAB
				if(NOTHING_SELECTED == pos_prior_suggest) {
					pos_prior_suggest = pos;
				} else {
					buffer[pos_prior_suggest] = '\0';
				}

				if(!retab && NOTHING_SELECTED != selected) {
					suggest_pos = selected;
				}

				retab = true;

				if(suggest_pos >= shown_commands) {
					suggest_pos = 0;
					buffer[pos_prior_suggest] = '\0';
					pos = pos_prior_suggest;
					pos_prior_suggest = NOTHING_SELECTED;
					tui_submit_action(input_modify_text);
					state_set_sugg_selected(NOTHING_SELECTED);
					break;
				}

				for(size_t i = suggest_pos; matching_commands[i].name; i++) {
					if(!strncmp(matching_commands[i].name, buffer, strlen(buffer))) {
						sprintf(buffer, "%s ", matching_commands[i].name);
						pos = strlen(matching_commands[i].name) + 1;
						tui_submit_action(input_modify_text);
						state_set_sugg_selected(i);
						suggest_pos = i + 1;
						break;
					}
				}
				break;

			case KEY_BACKSPACE:
				if(pos) {
					pos--;
					buffer[pos] = '\0';

					tui_submit_action(input_modify_text);

					shown_commands = submit_updated_suggestion_list(matching_commands, buffer);
					retab = false;
				} else {
					state_set_commands(NULL);
					return false;
				}
				break;

			default:
				buffer[pos] = c;
				buffer[pos + 1] = '\0';

				tui_submit_action(input_modify_text);
				shown_commands = submit_updated_suggestion_list(matching_commands, buffer);

				pos++;

				if(pos == buffer_size) {
					state_set_commands(NULL);
					return true;
				}
				retab = false;
		}
	}

	return false; // never reached
}

