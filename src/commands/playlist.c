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

#include "../_hard_config.h"

#include "playlist.h"
#include <ncurses.h>                    // for KEY_BACKSPACE, KEY_ENTER, etc

//\cond
#include <stdbool.h>                    // for false, bool, true
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint8_t
#include <stdio.h>                      // for sscanf, sprintf
#include <stdlib.h>                     // for NULL, free, exit, strtol, etc
#include <string.h>                     // for strlen, strncmp, memcpy, etc
//\endcond
#include "textbox.h"
#include "../command.h"                 // for command, commands, etc
#include "../jspf.h"                    // for jspf_write, jspf_error
#include "../log.h"                     // for _log
#include "../network/network.h"         // for network_conn
#include "../network/tls.h"             // for tls_connect
#include "../sound.h"                   // for sound_play, sound_seek
#include "../soundcloud.h"              // for soundcloud_get_entries
#include "../state.h"                   // for state_set_status, etc
#include "../config.h"                  // for config_get_cache_path
#include "../helper.h"                  // for smprintf, strstrp, astrdup, etc
#include "../track.h"                   // for track, track_list, TRACK, etc
#include "../tui.h"                     // for tui_submit_action, F_BOLD, etc

#define NONE ((unsigned int) ~0)
#define TIME_BUFFER_SIZE 64
#define NO_TRACK ((size_t) ~0)

static void switch_to_list(unsigned int id);
static bool handle_search(char *buffer, size_t buffer_size);
static void search_direction(bool down);
static void command_dispatcher(char *command);
static bool handle_command(char *buffer, size_t buffer_size);
static size_t submit_updated_suggestion_list(struct command *buffer, char *filter);
static void handle_textbox(void);

/** \todo a) does not belong here\n b) does not work */
void cmd_pl_download(const char *unused UNUSED) { // TODO!
	state_set_status(cline_warning, "Error: Downloading not yet implemented");
}

void cmd_pl_list_new(const char *_name) {
	astrdup(tname, _name);
	char *name = strstrp(tname);

	struct track_list *list = track_list_create(name);
	if(!list) {
		state_set_status(cline_warning, "Error: Failed to allocate memory for new list!");
	} else {
		state_add_list(list);
	}
}

void cmd_pl_write_playlist(const char *_file) {
	astrdup(tfile, _file);
	char *file = strstrp(tfile);
	struct track_list *list = state_get_list(state_get_current_list());

	state_set_status(cline_default, smprintf("Info: Writing to file "F_BOLD"%s"F_RESET" (type: JSPF)\n", file));
	if(!jspf_write(file, list)) {
		state_set_status(cline_warning, smprintf("Error: Writing to file "F_BOLD"%s"F_RESET" failed: "F_BOLD"%s"F_RESET"\n", file, jspf_error()));
	}
}

/** \brief Exit SCTC
 *
 *  Writes the list of bookmarks to file and shuts down SCTC.
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 */
void cmd_pl_exit(const char *unused UNUSED) {
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

/** \brief Display 'Help' Dialog
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 */
void cmd_pl_help(const char *unused UNUSED) {
	char *help_msg = LOGO_PART PARAGRAPH_PART DESCRIPTION_PART PARAGRAPH_PART ALPHA_PART PARAGRAPH_PART FEATURE_PART PARAGRAPH_PART NONFEATURE_PART PARAGRAPH_PART KNOWN_BUGS_PART PARAGRAPH_PART LICENSE_PART;

	state_set_tb("Help / About", help_msg);
	handle_textbox();
}

void cmd_pl_yank(const char *unused UNUSED) {
	struct track_list *list = state_get_list(state_get_current_list());
	size_t current_selected = state_get_current_selected();

	yank(TRACK(list, current_selected)->permalink_url);
	state_set_status(cline_default, smprintf("yanked "F_BOLD"%s"F_RESET, TRACK(list, current_selected)->permalink_url));
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
void cmd_pl_list(const char *list) {
	unsigned int list_id;

	if(1 == sscanf(list, " %8u ", &list_id)) {
		switch_to_list(list_id - 1);
	} else {
		state_set_status(cline_warning, smprintf("Error: Not switching to list "F_BOLD"%s"F_RESET": Expecting numeric ID", list));
	}
}

void cmd_pl_add(const char *_list) {
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

void cmd_pl_open_user(const char *_user) {
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


/** \brief Initiate a command input
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 */
void cmd_pl_command_input(const char *unused UNUSED) {
	state_set_status(cline_cmd_char, strdup(F_BOLD":"F_RESET));

	char *buffer = state_get_input();

	if(handle_command(buffer, 127)) {
		command_dispatcher(buffer);
	} else {
		state_set_status(cline_default, "");
	}

}

// TODO
static void update_flags_stop_playback(struct track_list *list, size_t tid) {
	if(list && NO_TRACK != tid) {
		TRACK(list, tid)->flags &= (uint8_t)~FLAG_PLAYING;
		if(TRACK(list, tid)->current_position) {
			TRACK(list, tid)->flags |= FLAG_PAUSED;
		}
	}
}

void cmd_pl_goto(const char *_hint) {
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

void cmd_pl_del(const char *unused UNUSED) {
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

void cmd_pl_play(const char *unused UNUSED) {
	size_t current_selected = state_get_current_selected();
	struct track_list *list = state_get_list(state_get_current_list());

	struct track *track = TRACK(list, current_selected);
	if(track->stream_url) {
		char time_buffer[TIME_BUFFER_SIZE];
		snprint_ftime(time_buffer, TIME_BUFFER_SIZE, track->duration);

		update_flags_stop_playback(state_get_list(state_get_current_playback_list()), state_get_current_playback_track());

		state_set_current_playback(state_get_current_list(), current_selected);

		state_set_title(smprintf("Now playing "F_BOLD"%s"F_RESET" by "F_BOLD"%s"F_RESET" (%s)", track->name, track->username, time_buffer));
		track->flags = (uint8_t) ( (track->flags & ~FLAG_PAUSED) | FLAG_PLAYING );

		tui_submit_action(update_list);

		sound_play(track);
	} else {
		state_set_status(cline_warning, smprintf("Error: Can't play "F_BOLD"%s"F_RESET" by "F_BOLD"%s"F_RESET": Missing Stream URL", track->name, track->username));
	}
}

static void search_direction(bool down) {
	struct track_list *list = state_get_list(state_get_current_list());
	const int step = down ? 1 : -1;

	size_t current = state_get_current_selected();
	while( (down && current < list->count - 1) || (!down && current > 0) ) {
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

void cmd_pl_search_next(const char *unused UNUSED) { search_direction(true);  }
void cmd_pl_search_prev(const char *unused UNUSED) { search_direction(false); }

void cmd_pl_search_start(const char *unused UNUSED) {
	state_set_status(cline_cmd_char, F_BOLD"/"F_RESET);
	handle_search(state_get_input(), 127);

	cmd_pl_search_next(NULL);
}

void cmd_pl_details(const char *unused UNUSED) {
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
		cmd_pl_goto(command);
		return;
	}

	state_set_status(cline_warning, smprintf("Error: Unknown command "F_BOLD"%s"F_RESET, command));
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

/** \brief Handle input for a textbox
 *
 *  Handles user's input for a textbox, such as `scroll up`, `scroll down`
 *  and `close textbox`.
 */
void handle_textbox(void) {
	int c;
	while( (c = getch()) ) {
		command_func_ptr func = config_get_function(scope_textbox, c);
		if(func) {
			func(config_get_param(scope_textbox, c));
			if(func == cmd_tb_close) {
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
