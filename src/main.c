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

#define _GNU_SOURCE

//#define NDEBUG

/** \mainpage SCTC - the soundcloud.com terminal client
 *
 *  \section intro_sec Introduction
 *
 *  This documentation documents the internals of SCTC.\n
 *  As STSC is not a library this documentation is not usefull if you do not want to
 *  modify / extend / bugfix SCTC.
 *
 *  \warning SCTC is, at this point, in a very early stage.
 *  Even if things seem to work fine there is no guarantee it behaves the way you want it to behave.\n
 *  Still a huge amount of work is required to achieve clean code ;)
 *
 *  \todo Fix the \\todos and the TODOs
 *
 *  \copyright Copyright 2015  Christian Eichler. All rights reserved.\n
 *  This project is released under the GNU General Public License version 3.
 */

//\cond
#include <ctype.h>                      // for isdigit
#include <locale.h>                     // for NULL, setlocale, LC_CTYPE
#include <stdbool.h>                    // for true, bool, false
#include <stddef.h>                     // for size_t
#include <stdio.h>                      // for sprintf, fclose, fopen, etc
#include <stdlib.h>                     // for atoi, free, exit, qsort, etc
#include <string.h>                     // for strlen, strncmp, memcpy, etc
#include <sys/time.h>                   // for CLOCK_MONOTONIC
#include <time.h>                       // for timespec, clock_gettime, etc
//\endcond

#include <ncurses.h>                    // for LINES, getch, KEY_DOWN, etc

#include "cache.h"                      // for cache_track_exists
#include "config.h"                     // for config_get_subscribe_count, etc
#include "helper.h"                     // for lcalloc, lmalloc, etc
#include "log.h"                        // for _log, log_close, log_init
#include "network/network.h"            // for network_conn
#include "network/tls.h"                // for tls_connect, tls_finalize, etc
#include "state.h"
#include "sound.h"                      // for sound_stop, etc
#include "soundcloud.h"                 // for soundcloud_get_entries
#include "track.h"                      // for track_list, track, etc
#include "tui.h"
#include "jspf.h"                       // for jspf_write, jspf_read

#define USE_UNICODE

#define BENCH_START(ID) \
	struct timespec bench_start##ID; \
	clock_gettime(CLOCK_MONOTONIC, &bench_start##ID);

#define BENCH_STOP(ID, DESC) { \
		struct timespec bench_end; \
		clock_gettime(CLOCK_MONOTONIC, &bench_end); \
		_log("%s took %dms", DESC, (bench_end.tv_sec - bench_start##ID.tv_sec) * 1000 + (bench_end.tv_nsec - bench_start##ID.tv_nsec) / (1000 * 1000)); \
	}

#define BOOKMARK_FILE ".bookmarks.jspf"

#define SERVER_PORT 443
#define SERVER_NAME "api.soundcloud.com"

#define TIME_BUFFER_SIZE 64

#define MIN(x,y) (x < y ? x : y)

static void handle_textbox();

static bool param_is_offline = false;

static int playing = -1; ///< the currently playing entry

/** \brief Update the current playback time.
 *
 *  If time equals -1, then the next track is selected (if any) and playback initiated. (\todo not yet working!)
 *
 *  \param time  The time in seconds to print to the screen
 */
void tui_update_time(int time) {
	struct track_list *list = state_get_list(state_get_current_list());

	if(-1 != time) {
		list->entries[playing].current_position = time;

		state_set_current_time(time);
		tui_submit_action(set_sbar_time);

		tui_submit_action(update_list);
	} else {
		list->entries[playing].current_position = 0;
		list->entries[playing].flags &= ~(FLAG_PAUSED | FLAG_PLAYING);

		switch(state_get_repeat()) {
			case rep_none:
				playing++;
				if(playing >= list->count) {
					playing = -1;
					return;
				}
				break;

			case rep_one:
				break;

			case rep_all: {
				playing++;
				if(playing >= list->count) {
					playing = 0;
				}
				break;
			}
		}

		list->entries[playing].flags = (list->entries[playing].flags & ~FLAG_PAUSED) | FLAG_PLAYING;

		char time_buffer[TIME_BUFFER_SIZE];
		snprint_ftime(time_buffer, TIME_BUFFER_SIZE, list->entries[playing].duration);

		state_set_title(smprintf("Now playing "F_BOLD"%s"F_RESET" by "F_BOLD"%s"F_RESET" (%s)", list->entries[playing].name, list->entries[playing].username, time_buffer));

		tui_submit_action(update_list);
		sound_play(&list->entries[playing]);
	}
}

/** \addtogroup cmd Command handling functions
 *
 *  These functions represent the individual commands action.\n
 *  For instance executing a `redraw` will result in calling the function cmd_redraw().
 *
 *  \todo Every possible action by the user should be accessible via command - and therefore
 *  for every action a cmd_action function is required.
 *
 *  \todo As soon as all the cmd_action functions are implemented we want to be able to map keys -> functions.
 *
 *  @{
 */

#define LOGO_PART "   ____________________\n"\
"  / __/ ___/_  __/ ___/\n"\
" _\\ \\/ /__  / / / /__  \n"\
"/___/\\___/ /_/  \\___/  \n"\
"  - the soundcloud.com terminal client"
#define DESCRIPTION_PART "SCTC is a curses based client for soundcloud.com.\nIts primary target is streaming - the most social media - like features are and will not (be) supported."
#define LICENSE_PART "SCTC is Free Software released under the GPL v3 or later license.\nFor details see LICENSE or visit https://www.gnu.org/copyleft/gpl.html"
#define ALPHA_PART "This version is NOT a stable release - expect bugs!"
#define FEATURE_PART "Implemented / functional features:\n - TUI (resizing, updating)\n - playback\n - caching\n - bookmarks"
#define NONFEATURE_PART "Planned features not yet implemented:\n - playlist management\n - logging in to soundcloud.com (OAuth)\n - reading playlists from soundcloud.com"
#define KNOWN_BUGS_PART "Known bugs:\n - track in bookmarks is not marked as 'currently playing'"
#define PARAGRAPH_PART "\n \n"

/** \todo a) does not belong here\n b) does not work */
static bool cmd_download(char *unused) { // TODO!
/*
	state_set_status(cline_default, smprintf("Info: Downloading "F_BOLD"%s"F_RESET"", list->entries[list->selected].name));

	char urlbuf[2048];
	sprintf(urlbuf, "%s?client_id=848ee866ea93c21373f6a8b61772b412", list->entries[list->selected].download_url);

	struct network_conn *nwc = tls_connect(SERVER_NAME, SERVER_PORT);
	struct http_response *resp = http_request_get(nwc, urlbuf, "api.soundcloud.com");
	nwc->disconnect(nwc);

	char errbuf[1024];
	yajl_val node = yajl_tree_parse((const char *) resp->body, errbuf, sizeof(errbuf));

	if(!node) {
		_log("%s", errbuf);
		_log("length of affected string: %d", strlen(resp->body));
		_log("affected string: %s", resp->body);
		return NULL;
	}

	char *download_url = json_get_string(node, "location", NULL);
	yajl_tree_free(node);

	http_response_destroy(resp);

	_log("actual download url: %s", download_url);

	struct url* url = url_parse_string(download_url);
	url_connect(url);

	struct http_response *dlresp = http_request_get(url->nwc, download_url, url->host);

	char fname[strlen(list->entries[list->selected].name) + 16];
	sprintf(fname, "%s.mp3", list->entries[list->selected].name);
	FILE *fh = fopen(fname, "w");
	fwrite(dlresp->body, dlresp->content_length, sizeof(char), fh);
	fclose(fh);

	url->nwc->disconnect(url->nwc);

	_log("finished downloading from SC, result: %i", resp->http_status);

	state_set_status(cline_default, strdup("Info: Download finished"));
*/

	return true;
}

static bool cmd_open_user(char *_user) {
	char *user = strstrp(_user);
	state_set_status(cline_default, smprintf("Info: Switch to "F_BOLD"%s"F_RESET"'s channel\n", user));

	struct network_conn *nwc = tls_connect(SERVER_NAME, SERVER_PORT);
	struct track_list *list = soundcloud_get_entries(nwc, user);
	nwc->disconnect(nwc);

	if(list->count) {
		list->name = list->entries[0].username;
		state_add_list(list);

		return true;
	}

	state_set_status(cline_warning, smprintf("Info: Cannot switch to "F_BOLD"%s"F_RESET"'s channel: No tracks found!\n", user));
	track_list_destroy(list, true);
	return false;
}

static bool cmd_write_playlist(char *file) {
	struct track_list *list = state_get_list(state_get_current_list());

	state_set_status(cline_default, smprintf("Info: Writing to file "F_BOLD"%s"F_RESET" (type: XSPF)\n", file));
	jspf_write(file, list);

	return true;
}

static bool cmd_goto(char *hint) {
	char *target = strstrp(hint);

	if(!strcmp("", target)) {
		state_set_current_selected(playing - 1);
		state_set_status(cline_default, "");
	}
	return true;
}

static bool switch_to_list(unsigned int id) {
	struct track_list *list = state_get_list(id);
	if(!list) {
		state_set_status(cline_warning, "Error: Not switching to list: No such list");
	} else if(list->count) {
		state_set_status(cline_default, smprintf("Info: Switching to "F_BOLD"%s"F_RESET, list->name));
		state_set_current_list(id);
		return true;
	} else {
		state_set_status(cline_warning, smprintf("Error: Not switching to "F_BOLD"%s"F_RESET": List is empty", list->name));
	}

	return false;
}

static bool cmd_list(char *list) {
	unsigned int list_id;

	if(1 == sscanf(list, " %u ", &list_id)) {
		return switch_to_list(list_id - 1);
	} else {
		_log("cannot switch to list %s", list);
		return false;
	}
}

static bool cmd_seek(char *time) {
	char *seekto = strstrp(time);

	if(!strcmp("", seekto)) {
		return false;
	} else {
		unsigned int new_abs = INVALID_TIME;

		if('+' == *seekto || '-' == *seekto) {
			// relative offset to current position
			unsigned int delta = parse_time_to_sec(seekto + 1);
			if(INVALID_TIME != delta) {
				new_abs = sound_get_current_pos();
				if('+' == *seekto) {
					new_abs += delta;
				} else {
					new_abs -= delta;
				}
			}
		} else {
			// absolute
			new_abs = parse_time_to_sec(seekto);
		}

		if(INVALID_TIME == new_abs) {
			_log("seeking aborted due to input error");
			return false;
		} else {
			sound_seek(new_abs);
		}
	}
	return true;
}

/** \brief Exit SCTC
 *
 *  Writes the list of bookmarks to file and shuts down SCTC.
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return does not return
 */
static bool cmd_exit(char *unused) {
	jspf_write(BOOKMARK_FILE, state_get_list(LIST_BOOKMARKS));

	track_list_destroy(state_get_list(LIST_STREAM), true);
	track_list_destroy(state_get_list(LIST_BOOKMARKS), true); // TODO: double free!
	sound_finalize();
	tls_finalize();
	config_finalize();
	tui_finalize();
	state_finalize();

	log_close();
	exit(EXIT_SUCCESS);

	return true;
}

static bool cmd_bookmark(char *unused) {
	struct track_list *list = state_get_list(state_get_current_list());

	track_list_add(state_get_list(LIST_BOOKMARKS), &list->entries[state_get_current_selected()]);
	state_set_status(cline_default, smprintf("Info: Added "F_BOLD"%s"F_RESET" to bookmarks", list->entries[state_get_current_selected()].name));

	return true;
}

/** \brief Display 'Help' Dialog
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static bool cmd_help(char *unused) {
	char *help_msg = LOGO_PART PARAGRAPH_PART DESCRIPTION_PART PARAGRAPH_PART ALPHA_PART PARAGRAPH_PART FEATURE_PART PARAGRAPH_PART NONFEATURE_PART PARAGRAPH_PART KNOWN_BUGS_PART PARAGRAPH_PART LICENSE_PART;

	state_set_tb("Help / About", help_msg);
	handle_textbox();
	return true;
}

/** \brief Set repeat to 'none'
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static bool cmd_repeat_none(char *unused) {
	state_set_repeat(rep_none);
	state_set_status(cline_default, "Info: Switched repeat to 'none'");
	return true;
}

/** \brief Set repeat to 'one' (repeat single track)
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static bool cmd_repeat_one(char *unused) {
	state_set_repeat(rep_one);
	state_set_status(cline_default, "Info: Switched repeat to 'one'");
	return true;
}

/** \brief Set repeat to 'all' (repeat whole track_list)
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static bool cmd_repeat_all(char *unused) {
	state_set_repeat(rep_all);
	state_set_status(cline_default, "Info: Switched repeat to 'all'");
	return true;
}

static bool stop_playback(bool reset) {
	struct track_list *list = state_get_list(state_get_current_list());

	if(-1 != playing) {
		sound_stop();

		list->entries[playing].flags &= ~FLAG_PLAYING;
		if(reset) {
			list->entries[playing].current_position = 0;
		} else {
			if(list->entries[playing].current_position) {
				list->entries[playing].flags |= FLAG_PAUSED;
			}

			list->entries[playing].current_position = sound_get_current_pos();
		}

		tui_submit_action(update_list);

		playing = -1;
		return true;
	}
	return false;
}

static bool cmd_pause(char *unused) {
	return stop_playback(false);
}

static bool cmd_stop(char *unused) {
	return stop_playback(true);
}

/** \brief Issue a redraw of the whole screen
 *
 *  \return true
 */
static bool cmd_redraw(char *unused) {
	tui_submit_action(redraw);
	return true;
}
/** @}*/

/** \brief Array of all commands supported by SCTC.
 *
 *  Includes a description, which is, for instance, shown in the commandwindow, and the function to call in
 *  order to execute the command.
 *
 *  \todo Step by step, 'everything' should be moved here.\n
 *  At some point in time we want to be able to bind keys via configfile.
 */
static struct command commands[] = {
	{"bookmark",    cmd_bookmark,       "<none/ignored>",                "Add currently selected entry to booksmarks"},
	{"open",        cmd_open_user,      "<name of user>",                "Open a specific user's stream"},
	{"download",    cmd_download,       "<none/ignored>",                "Download the currently selected entry to file"},
	{"redraw",      cmd_redraw,         "<none/ignored>",                "Redraw the screen"},
	{"repeat-none", cmd_repeat_none,    "<none/ignored>",                "Set repeat to 'none'"},
	{"repeat-one",  cmd_repeat_one,     "<none/ignored>",                "Set repeat to 'one'"},
	{"repeat-all",  cmd_repeat_all,     "<none/ignored>",                "Set repeat to 'all'"},
	{"stop",        cmd_stop,           "<none/ignored>",                "Stop playback of current track"},
	{"pause",       cmd_pause,          "<none/ignored>",                "Pause playback of current track"},
	{"seek",        cmd_seek,           "<time to seek to>",             "Seek to specified time in current track"},
	{"list",        cmd_list,           "<number of list>",              "Switch to specified playlist"},
	{"goto",        cmd_goto,           "<relative or absolute offset>", "Set selection to specific entry"},
	{"help",        cmd_help,           "<none/ignored>",                "Show help"},
	{"write",       cmd_write_playlist, "<filename>",                    "Write current playlist to file (.jspf)",  },
	{"exit",        cmd_exit,           "<none/ignored>",                "Terminate SCTC"},
	{NULL, NULL, NULL}
};
static const size_t command_count = sizeof(commands) / sizeof(struct command) - 1;

static int command_dispatcher(char *command) {
	const size_t input_size = strlen(command);
	for(int i = 0; commands[i].name; i++) {
		const size_t ci_size = strlen(commands[i].name);

		if(!strncmp(commands[i].name, command, MIN(ci_size, input_size))) {
			commands[i].func(command + strlen(commands[i].name) + 1);
			return -1;
		}
	}

	char *endptr;
	long val = strtol(command, &endptr, 10);
	if('\0' == *endptr) {
		state_set_status(0, smprintf("Info: Jumping to %i", val));
		return val;
	}

	_log("Unknown command '%s'", command);
	state_set_status(cline_warning, smprintf("Error: Unknown command "F_BOLD"%s"F_RESET, command));
	return -1;
}

/** \todo remove this early-day-hack */
static struct track_list* get_list() {
	struct network_conn *nwc = NULL;
	if(!param_is_offline) {
		state_set_status(cline_default, "Info: Connecting to soundcloud.com");
		nwc = tls_connect(SERVER_NAME, SERVER_PORT);
	}

	const size_t lists_size = config_get_subscribe_count() + 1;
	struct track_list *lists[lists_size];
	lists[lists_size - 1] = NULL;

	for(int i = 0; i < config_get_subscribe_count(); i++) {
		state_set_status(cline_default, smprintf("Info: Retrieving %i/%i lists from soundcloud.com: "F_BOLD"%s"F_RESET, i, config_get_subscribe_count(), config_get_subscribe(i)));
		lists[i] = soundcloud_get_entries(nwc, config_get_subscribe(i));
	}

	if(nwc) {
		nwc->disconnect(nwc);
	}

	BENCH_START(MP)
	struct track_list* list = track_list_merge(lists);
	track_list_sort(list);
	BENCH_STOP(MP, "Merging playlists")

	for(int i = 0; i < config_get_subscribe_count(); i++) {
		if(lists[i]) {
			track_list_destroy(lists[i], false);
		}
	}
	return list;
}

static void handle_textbox() {
	int c;
	while( (c = getch()) ) {
		switch(c) {
			case 'd':
			case 'q':
				state_set_tb_pos(0);
				state_set_tb(NULL, NULL);
				return;

			/* manual scrolling
			 *  -> single line up
			 *  -> single line down
			 *  -> page up
			 *  -> page down */
			case KEY_UP:    state_set_tb_pos_rel(-1);            break;
			case KEY_DOWN:  state_set_tb_pos_rel(+1);            break;
			case KEY_PPAGE: state_set_tb_pos_rel(- (LINES - 2)); break;
			case KEY_NPAGE: state_set_tb_pos_rel(+ (LINES - 2)); break;
		}
	}
}

/** \brief Generate a new list of commands using the filter `filter`.
 *
 *  Filtering equals to matching the first `strlen(filter)` chars against the individual commands' names.
 *  Matching is done case-insensitive.
 *
 *  \param filter  The filter to use for filtering the list of commands
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

static bool handle_command(char *buffer, size_t buffer_size) {
	struct command matching_commands[command_count + 1];
	memcpy(matching_commands, commands, sizeof(commands));

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

				for(int i = suggest_pos; matching_commands[i].name; i++) {
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

static bool handle_search(char *buffer, size_t buffer_size) {
	size_t pos = 0;
	int c;

	// `clear` the buffer on starting search
	buffer[0] = '\0';

	tui_submit_action(input_modify_text);

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

int main(int argc, char **argv) {
	log_init("sctc.log");

	for(size_t i = 1; i < argc; i++) {
		if(!strcmp("--offline", argv[i])) {
			param_is_offline = true;
			_log("switching to offline-mode due to parameter");
		}
	}

	setlocale(LC_CTYPE, "C-UTF-8");

	state_init();
	config_init();
	tls_init();
	tui_init();
	sound_init(tui_update_time);

	state_set_status(cline_default, "");
	state_set_title("");

	struct track_list *lists[4] = {NULL};
	lists[LIST_BOOKMARKS] = jspf_read(BOOKMARK_FILE);
	lists[LIST_BOOKMARKS]->name = "Bookmarks";

	lists[LIST_STREAM] = get_list();
	lists[LIST_STREAM]->name = "Stream";

	state_set_lists(lists);

	// TODO: speed
	BENCH_START(SB)
	for(size_t i = 0; i < lists[LIST_STREAM]->count; i++) {
		if(track_list_contains(lists[LIST_BOOKMARKS], lists[LIST_STREAM]->entries[i].permalink_url)) {
			_log("'%s' is bookmarked!", lists[LIST_STREAM]->entries[i].name);
			lists[LIST_STREAM]->entries[i].flags |= FLAG_BOOKMARKED;
		}

		if(cache_track_exists(&lists[LIST_STREAM]->entries[i])) {
			lists[LIST_STREAM]->entries[i].flags |= FLAG_CACHED;
		}
	}
	BENCH_STOP(SB, "Searching for bookmarks")

	state_set_lists(lists);

	// send new list to tui-thread
	state_set_current_list(LIST_STREAM);
	state_set_status(cline_default, smprintf("Info: "F_BOLD"%i elements"F_RESET" in %i subscriptions from soundcloud.com", lists[LIST_STREAM]->count, config_get_subscribe_count()));

	int c;
	while( (c = getch()) ) {
		struct track_list *list = state_get_list(state_get_current_list());
		size_t current_selected = state_get_current_selected();

		if(isdigit(c) && '0' != c) {
			switch_to_list(c - '1');
			continue;
		}

		switch(c) {
			case 'q': cmd_exit(NULL);        break;
			case '0': /** \todo show logs */ break;

			case '/':
				state_set_status(cline_cmd_char, F_BOLD"/"F_RESET);
				handle_search(state_get_input(), 127);
				_log("have search: %s", state_get_input());

				// fall through

			case 'n':
				for(int i = state_get_current_selected(); i < list->count; i++) {
					if(strcasestr(list->entries[i].name, state_get_input())) {
						state_set_current_selected(i);
						break;
					}
				}
				break;

			case 'N':
				for(int i = state_get_current_selected() - 1; i >= 0; i--) {
					if(strcasestr(list->entries[i].name, state_get_input())) {
						state_set_current_selected(i);
						break;
					}
				}
				break;

			case 'b': cmd_bookmark(NULL); break;

			case ':': {
				state_set_status(cline_cmd_char, strdup(F_BOLD":"F_RESET));

				char *buffer = state_get_input();

				if(handle_command(buffer, 127)) {
					int jump_target = command_dispatcher(buffer);
					if(-1 != jump_target) {
						state_set_current_selected(jump_target);
						state_set_status(cline_default, "");
					}
				} else {
					state_set_status(cline_default, "");
				}
				break;
			}

			case 'r':
				switch(state_get_repeat()) {
					case rep_none: cmd_repeat_one (NULL); break;
					case rep_one:  cmd_repeat_all (NULL); break;
					case rep_all:  cmd_repeat_none(NULL); break;
				}
				break;

			case 'd': {
				char *title = smprintf("%s by %s", list->entries[current_selected].name, list->entries[current_selected].username);
				state_set_tb(title, list->entries[current_selected].description);
				handle_textbox();
				free(title);
				break;
			}

			case 'c': cmd_pause(NULL); break; // pause
			case 's': cmd_stop(NULL);  break; // stop (= reset current position to 0)

			case 'y': /* copy url to selected entry */
				yank(list->entries[current_selected].permalink_url);
				state_set_status(cline_default, smprintf("yanked "F_BOLD"%s"F_RESET, list->entries[current_selected].permalink_url));
				break;

			/* jump to start/end of list */
			case 'g': state_set_current_selected(0);               break;
			case 'G': state_set_current_selected(list->count - 1); break;

			/* manual scrolling
			 *  -> single line up
			 *  -> single line down
			 *  -> page up
			 *  -> page down */
			case KEY_UP:    state_set_current_selected_rel(-1);            break;
			case KEY_DOWN:  state_set_current_selected_rel(+1);            break;
			case KEY_NPAGE: state_set_current_selected_rel(+ (LINES - 2)); break;
			case KEY_PPAGE: state_set_current_selected_rel(- (LINES - 2)); break;

			case 0x0A: // LF (aka 'enter')
			case KEY_ENTER: {
				stop_playback(false); // pause other playing track (if any)

				char time_buffer[TIME_BUFFER_SIZE];
				snprint_ftime(time_buffer, TIME_BUFFER_SIZE, list->entries[current_selected].duration);

				playing = current_selected;

				state_set_title(smprintf("Now playing "F_BOLD"%s"F_RESET" by "F_BOLD"%s"F_RESET" (%s)", list->entries[playing].name, list->entries[playing].username, time_buffer));

				list->entries[current_selected].flags = (list->entries[current_selected].flags & ~FLAG_PAUSED) | FLAG_PLAYING;

				tui_submit_action(update_list);

				sound_play(&list->entries[current_selected]);
				break;
			}

			default:
				state_set_status(cline_warning, smprintf("Error: got unkown keycode %x", c));
		}
	}

	// never reached, exit called in case of 'q'
	return EXIT_FAILURE;
}
