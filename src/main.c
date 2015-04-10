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
#include "http.h"                       // for http_response, etc
#include "log.h"                        // for _log, log_close, log_init
#include "m3u.h"                        // for m3u_write
#include "network/network.h"            // for network_conn
#include "network/tls.h"                // for tls_connect, tls_finalize, etc
#include "sound.h"                      // for sound_stop, etc
#include "soundcloud.h"                 // for soundcloud_get_entries
#include "track.h"                      // for track_list, track, etc
#include "tui.h"
#include "url.h"                        // for url, url_connect, etc
#include "xspf.h"                       // for xspf_write, xspf_read

#define USE_UNICODE

#define BENCH_START(ID) \
	struct timespec bench_start##ID; \
	clock_gettime(CLOCK_MONOTONIC, &bench_start##ID);

#define BENCH_STOP(ID, DESC) { \
		struct timespec bench_end; \
		clock_gettime(CLOCK_MONOTONIC, &bench_end); \
		_log("%s took %dms", DESC, (bench_end.tv_sec - bench_start##ID.tv_sec) * 1000 + (bench_end.tv_nsec - bench_start##ID.tv_nsec) / (1000 * 1000)); \
	}

#define BOOKMARK_FILE ".bookmarks.xspf"

#define SERVER_PORT 443
#define SERVER_NAME "api.soundcloud.com"

#define TIME_BUFFER_SIZE 64

static void handle_textbox();

static bool param_is_offline = false;

static struct track_list *list = NULL;
static int playing = -1; ///< the currently playing entry
static enum repeat repeat = rep_none;
struct track_list *playlists[4] = { NULL };

#define LIST_STREAM     0
#define LIST_BOOKMARKS  1
#define LIST_USER1      2

/** \brief Update the current playback time.
 *
 *  If time equals -1, then the next track is selected (if any) and playback initiated. (\todo not yet working!)
 *
 *  \param time  The time in seconds to print to the screen
 */
void tui_update_time(int time) {

	if(-1 != time) {
		list->entries[playing].current_position = time;
		tui_submit_int_action(set_sbar_time, time);
		tui_submit_set_list_action(list);
	} else {
		list->entries[playing].current_position = 0;

		_log("playback done, going to next track in playlist");

		size_t next_playing = playing + 1;
		switch(repeat) {
			case rep_none:
				if(next_playing >= list->count) {
					playing = -1;
					return;
				}
				break;

			case rep_one:
				next_playing = playing;
				break;

			case rep_all: {
				if(next_playing >= list->count) {
					next_playing = 0;
				}
				break;
			}
		}

		list->entries[next_playing].flags = (list->entries[next_playing].flags & ~FLAG_PAUSED) | FLAG_PLAYING;

		playing = next_playing;

		char time_buffer[TIME_BUFFER_SIZE];
		snprint_ftime(time_buffer, TIME_BUFFER_SIZE, list->entries[playing].duration);

		tui_submit_title_line_print("Now playing "F_BOLD"%s"F_RESET" by "F_BOLD"%s"F_RESET" (%s)", list->entries[playing].name, list->entries[playing].username, time_buffer);

		tui_submit_set_list_action(list);

		sound_play(&list->entries[next_playing]);
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
	_log("cmd_download for '%s' (url: '%s')", list->entries[list->selected].name, list->entries[list->selected].download_url);
/*
	tui_submit_status_line_print(cline_default, "Info: Downloading "F_BOLD"%s"F_RESET"", list->entries[list->selected].name);

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

	tui_submit_status_line_print(cline_default, "Info: Download finished");
*/

	return true;
}

static bool cmd_open_user(char *user) {
	tui_submit_status_line_print(cline_default, "Info: Switch to %s's channel\n", user);

	struct network_conn *nwc = tls_connect(SERVER_NAME, SERVER_PORT);
	list = soundcloud_get_entries(nwc, user);
	nwc->disconnect(nwc);

	list->position = 0;

	tui_submit_set_list_action(list);

	return true;
}

static bool cmd_write_playlist(char *file) {
	char *ext = file + strlen(file)  - 4;

	if(!strcmp(ext, ".m3u")) {
		tui_submit_status_line_print(cline_default, "Info: Writing to file "F_BOLD"%s"F_RESET" (type: M3U)\n", file);
		m3u_write(file, list);
	} else {
		tui_submit_status_line_print(cline_default, "Info: Writing to file "F_BOLD"%s"F_RESET" (type: XSPF)\n", file);
		xspf_write(file, list);
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
	xspf_write(BOOKMARK_FILE, playlists[LIST_BOOKMARKS]);

	track_list_destroy(playlists[LIST_STREAM], true);
	track_list_destroy(playlists[LIST_BOOKMARKS], true); // TODO: double free!
	free(playlists[LIST_USER1]); // TODO
	sound_finalize();
	tls_finalize();
	config_finalize();

	tui_finalize();

	log_close();
	exit(EXIT_SUCCESS);

	return true;
}

static bool cmd_bookmark(char *unused) {
	track_list_add(playlists[LIST_BOOKMARKS], &list->entries[list->selected]);
	tui_submit_status_line_print(cline_default, "Info: Added "F_BOLD"%s"F_RESET" to bookmarks", list->entries[list->selected].name);

	return true;
}

/** \brief Display 'Help' Dialog
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static bool cmd_help(char *unused) {
	// TODO
	char *help_msg = LOGO_PART PARAGRAPH_PART DESCRIPTION_PART PARAGRAPH_PART ALPHA_PART PARAGRAPH_PART FEATURE_PART PARAGRAPH_PART NONFEATURE_PART PARAGRAPH_PART KNOWN_BUGS_PART PARAGRAPH_PART LICENSE_PART;

	struct tui_action *action = tui_action_init(show_textbox);
	action->tt.title = strdup("Help / About");
	action->tt.text  = strdup(help_msg);
	tui_submit_action(action);

	handle_textbox();
	return true;
}

/** \brief Set repeat to 'none'
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static bool cmd_repeat_none(char *unused) {
	repeat = rep_none;
	tui_submit_update_tab_bar(playlists, 0, repeat);
	tui_submit_status_line_print(cline_default, "Info: Switched repeat to 'none'");
	return true;
}

/** \brief Set repeat to 'one' (repeat single track)
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static bool cmd_repeat_one(char *unused) {
	repeat = rep_one;
	tui_submit_update_tab_bar(playlists, 0, repeat);
	tui_submit_status_line_print(cline_default, "Info: Switched repeat to 'one'");
	return true;
}

/** \brief Set repeat to 'all' (repeat whole track_list)
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static bool cmd_repeat_all(char *unused) {
	repeat = rep_all;
	tui_submit_update_tab_bar(playlists, 0, repeat);
	tui_submit_status_line_print(cline_default, "Info: Switched repeat to 'all'");
	return true;
}

/** \brief Issue a redraw of the whole screen
 *
 *  \return true
 */
static bool cmd_redraw(char *unused) {
	tui_submit_action(tui_action_init(redraw));
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
	{"bookmark",    "Add currently selected entry to booksmarks",  cmd_bookmark},
	{"open",        "Open a specific user's stream",  cmd_open_user},
	{"download",    "Download the currently selected entry to file",  cmd_download},
	{"redraw",      "Redraw the screen",     cmd_redraw},
	{"repeat-none", "Set repeat to 'none'",  cmd_repeat_none},
	{"repeat-one",  "Set repeat to 'one'",   cmd_repeat_one},
	{"repeat-all",  "Set repeat to 'all'",   cmd_repeat_all},
	{"help",        "Show help",             cmd_help},
	{"write",       "Write current playlist to file (either .xspf (recommend) or .m3u)",  cmd_write_playlist},
	{"exit",        "Terminate SCTC",        cmd_exit},
	{NULL,          NULL, NULL}
};

static bool is_valid_int(char *str) {
	for(int i = 0; str[i] != '\0'; i++) {
		if(!isdigit(str[i])) return false;
	}
	return true;
}

static int command_dispatcher(char *command) {
	for(int i = 0; commands[i].name; i++) {
		if(!strncmp(commands[i].name, command, strlen(commands[i].name))) {
			commands[i].func(command + strlen(commands[i].name) + 1);
			return -1;
		}
	}

	if(is_valid_int(command)) {
		_log("jumping to %i", atoi(command));
		tui_submit_status_line_print(0, "Info: Jumping to %i", atoi(command));
		return atoi(command);
	}

	_log("Unknown command '%s'", command);
	tui_submit_status_line_print(cline_warning, "Error: Unknown command "F_BOLD"%s"F_RESET, command);
	return -1;
}

/** \todo remove this early-day-hack */
static struct track_list* get_list() {
	struct network_conn *nwc = NULL;
	if(!param_is_offline) {
		tui_submit_status_line_print(cline_default, "Info: Connecting to soundcloud.com");
		nwc = tls_connect(SERVER_NAME, SERVER_PORT);
	}

	struct track_list **lists = lcalloc(config_get_subscribe_count() + 1, sizeof(struct track_list*));
	if(!lists) return NULL;

	for(int i = 0; i < config_get_subscribe_count(); i++) {
		tui_submit_status_line_print(cline_default, "Info: Retrieving %i/%i lists from soundcloud.com: "F_BOLD"%s"F_RESET, i, config_get_subscribe_count(), config_get_subscribe(i));
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
	free(lists);

	// TODO: speed
	BENCH_START(SB)
	for(size_t i = 0; i < list->count; i++) {
		if(track_list_contains(playlists[1], list->entries[i].permalink_url)) {
			_log("'%s' is bookmarked!", list->entries[i].name);
			list->entries[i].flags |= FLAG_BOOKMARKED;
		}

		if(cache_track_exists(&list->entries[i])) {
			list->entries[i].flags |= FLAG_CACHED;
		}
	}
	BENCH_STOP(SB, "Searching for bookmarks")

	return list;
}

static void handle_textbox() {
	int c;
	while( (c = getch()) ) {
		switch(c) {
			case 'd':
			case 'q':
				tui_submit_int_action(back_exit, 0);
				return;
				break;

			/* manual scrolling
			 *  -> single line up
			 *  -> single line down
			 *  -> page up
			 *  -> page down */
			case KEY_UP:    tui_submit_int_action(updown, -1);            break;
			case KEY_DOWN:  tui_submit_int_action(updown, 1);             break;
			case KEY_PPAGE: tui_submit_int_action(updown, - (LINES - 2)); break;
			case KEY_NPAGE: tui_submit_int_action(updown, LINES - 2);     break;
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
static void submit_updated_suggestion_list(char *filter) {
	const size_t command_count = sizeof(commands) / sizeof(struct command) - 1;
	struct command *matching = lcalloc(command_count + 1, sizeof(struct command));

	size_t matching_pos = 0;
	for(size_t i = 0; i < command_count; i++) {
		if(!strncmp(commands[i].name, filter, strlen(filter))) {
			memcpy(&matching[matching_pos], &commands[i], sizeof(struct command));
			matching_pos++;
		}
	}

	struct tui_action *action = tui_action_init(set_suggestion_list);
	action->sl_commands = matching;

	_log("submit: set_suggestion_list, %i elements", matching_pos);
	tui_submit_action(action);
}

static bool handle_command(char *buffer, size_t buffer_size) {

	const size_t command_count = sizeof(commands) / sizeof(struct command) - 1;

	size_t pos = 0;
	int c;

	size_t suggest_pos = 0;
	size_t pos_prior_suggest = 0;

	/* the currently selected entry, -1 if "nothing" is selected */
	size_t sl_selected = 0;

	struct tui_action *action = tui_action_init(input_modify_text);
	action->input = buffer;
	tui_submit_action(action);

	action = tui_action_init(set_suggestion_list);
	action->sl_commands = commands;
	tui_submit_action(action);

	tui_submit_int_action(updown_absolute, -1);

	while( (c = getch()) ) {
		switch(c) {
			case KEY_EXIT: // ESC
				return false;

			case 0x0A: // LF (aka 'enter')
			case KEY_ENTER: {
				if(sl_selected) {
					snprintf(buffer, buffer_size, "%s", commands[sl_selected - 1].name);
				}

				// disable the suggestion lsit
				tui_submit_action(tui_action_init(back_exit));

				_log("sl_selected = %i -> buffer = %s", sl_selected - 1, buffer);
				return true;
			}

			case KEY_UP: {
				if(sl_selected > 0) {
					sl_selected--;
					tui_submit_int_action(updown_absolute, sl_selected - 1);
				}
				break;
			}

			case KEY_DOWN: {
				if(sl_selected < command_count) {
					sl_selected++;
					tui_submit_int_action(updown_absolute, sl_selected - 1);
				}
				break;
			}

			case 0x09: { // TAB
				if(-1 == pos_prior_suggest) {
					pos_prior_suggest = pos;
				} else {
					buffer[pos_prior_suggest] = '\0';
				}

				for(int i = suggest_pos; commands[i].name; i++) {
					if(!strncmp(commands[i].name, buffer, strlen(buffer))) {
						sprintf(buffer, "%s", commands[i].name);
						tui_submit_input_action(buffer);
						suggest_pos = i + 1;
						break;
					}
				}

				break;
			}


			case KEY_BACKSPACE: {
				if(pos) {
					pos--;
					buffer[pos] = '\0';
					tui_submit_input_action(buffer);

					submit_updated_suggestion_list(buffer);
				} else {
					return false;
				}

				break;
			}

			default: {
				buffer[pos] = c;
				buffer[pos + 1] = '\0';
				tui_submit_input_action(buffer);

				submit_updated_suggestion_list(buffer);

				pos++;

				if(pos == buffer_size) {
					return true;
				}
			}
		}
	}

	return false; // never reached
}

static bool handle_search(char *buffer, size_t buffer_size) {
	size_t pos = 0;
	int c;

	tui_submit_input_action(buffer);

	while( (c = getch()) ) {
		switch(c) {
			case KEY_EXIT: // ESC
				return false;

			case 0x0A: // LF (aka 'enter')
			case KEY_ENTER:
				return true;

			case KEY_BACKSPACE: {
				if(pos) {
					buffer[pos] = '\0';
					tui_submit_input_action(buffer);
					pos--;
				} else {
					return false;
				}

				break;
			}

			default: {
				buffer[pos] = c;
				buffer[pos + 1] = '\0';
				tui_submit_input_action(buffer);
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
		_log("param: %s", argv[i]);

		if(!strcmp("--offline", argv[i])) {
			param_is_offline = true;
			_log("switching to offline-mode due to parameter");
		}
	}

	setlocale(LC_CTYPE, "C-UTF-8");

	config_init();
	tls_init();
	tui_init();
	sound_init(tui_update_time);

	tui_submit_status_line_print(cline_default, "");
	tui_submit_title_line_print("");

	playlists[LIST_BOOKMARKS] = xspf_read(BOOKMARK_FILE);
	playlists[LIST_BOOKMARKS]->name = "Bookmarks";

	playlists[LIST_USER1] = lcalloc(1, sizeof(struct track_list));
	playlists[LIST_USER1]->name = "User List 1";

	list = get_list();
	list->name = "Stream";

	// send new list to tui-thread
	tui_submit_set_list_action(list);

	playlists[LIST_STREAM] = list;

	tui_submit_update_tab_bar(playlists, 0, repeat);
	tui_submit_status_line_print(cline_default, "Info: "F_BOLD"%i elements"F_RESET" in %i subscriptions from soundcloud.com", list->count, config_get_subscribe_count());

	tui_submit_set_list_action(list);

	int c;

	char searchbuf[128] = {0};
	while( (c = getch()) ) {
		switch(c) {
			case 'q':
				cmd_exit(NULL);
				break;

			case '0': /** \todo show logs */
				break;

			case '1':
			case '2':
			case '3': {
				if(playlists[c - '1']->count) {
					list = playlists[c - '1'];
					list->position = 0;

					tui_submit_set_list_action(list);
					tui_submit_status_line_print(cline_default, "Info: Switching to "F_BOLD"%s"F_RESET, playlists[c - '1']->name);
					tui_submit_update_tab_bar(playlists, c - '1', repeat);
				} else {
					tui_submit_status_line_print(cline_warning, "Error: Not switching to "F_BOLD"%s"F_RESET": List is empty", playlists[c - '1']->name);
				}
				break;
			}

			case '/':
				tui_submit_status_line_print(cline_cmd_char, F_BOLD"/"F_RESET);
				handle_search(searchbuf, 127);
				_log("have search: %s", searchbuf);

				// fall through

			case 'n':
				if(list->selected >= list->count) list->selected = 0;
				for(int i = list->selected + 1; i < list->count; i++) {
					if(strcasestr(list->entries[i].name, searchbuf)) {
						tui_submit_int_action(updown_absolute, i);
						break;
					}
				}

				break;

			case 'N':
				if(list->selected >= list->count) list->selected = 0;
				for(int i = list->selected - 1; i >= 0; i--) {
					if(strcasestr(list->entries[i].name, searchbuf)) {
						tui_submit_int_action(updown_absolute, i);
						break;
					}
				}

				break;

			case 'b':
				cmd_bookmark(NULL);
				break;

			case ':': {
				tui_submit_status_line_print(cline_cmd_char, F_BOLD":"F_RESET);

				char buffer[128] = {0};
				if(handle_command(buffer, 127)) {
					int jump_target = command_dispatcher(buffer);
					if(-1 != jump_target) {
						list->selected = jump_target;

						tui_submit_set_list_action(list);
						tui_submit_status_line_print(cline_default, "");
					}
				} else {
					tui_submit_status_line_print(cline_default, "");
				}
				break;
			}

			case 'r': {
				switch(repeat) {
					case rep_none: cmd_repeat_one (NULL); break;
					case rep_one:  cmd_repeat_all (NULL); break;
					case rep_all:  cmd_repeat_none(NULL); break;
				}
				break;
			}

			case 'd': {
				struct tui_action *action = tui_action_init(show_textbox);

				const char *format = "%s by %s";
				action->tt.title = lmalloc(strlen(format) + strlen(list->entries[list->selected].name) + strlen(list->entries[list->selected].username));
				sprintf(action->tt.title, format, list->entries[list->selected].name, list->entries[list->selected].username);
				action->tt.text  = strdup(list->entries[list->selected].description);
				tui_submit_action(action);

				handle_textbox();
				break;
			}

			case 'c': { // pause / continue
				if(-1 != playing) {
					sound_stop();
					list->entries[playing].flags = (list->entries[playing].flags & ~FLAG_PLAYING) | FLAG_PAUSED;
					list->entries[playing].current_position = sound_get_current_pos();

					tui_submit_set_list_action(list);

					playing = -1;
				}
				break;
			}

			case 's': { // stop (= reset current position to 0)
				if(-1 != playing) {
					sound_stop();
					list->entries[playing].flags &= ~FLAG_PLAYING;
					list->entries[playing].current_position = 0;

					tui_submit_set_list_action(list);

					playing = -1;
				}
				break;
			}

			case 'y': /* copy url to selected entry */
				yank(list->entries[list->selected].permalink_url);
				tui_submit_status_line_print(cline_default, "yanked "F_BOLD"%s"F_RESET, list->entries[list->selected].permalink_url);
				break;

			/* jump to start/end of list */
			case 'g': tui_submit_int_action(updown_absolute, 0);               break;
			case 'G': tui_submit_int_action(updown_absolute, list->count - 1); break;

			/* manual scrolling
			 *  -> single line up
			 *  -> single line down
			 *  -> page up
			 *  -> page down */
			case KEY_UP:    tui_submit_int_action(updown, -1);            break;
			case KEY_DOWN:  tui_submit_int_action(updown, 1);             break;
			case KEY_PPAGE: tui_submit_int_action(updown, - (LINES - 2)); break;
			case KEY_NPAGE: tui_submit_int_action(updown, LINES - 2);     break;

			case 0x0A: // LF (aka 'enter')
			case KEY_ENTER: {
				if(-1 != playing) {
					sound_stop();
					list->entries[playing].current_position = sound_get_current_pos();
					list->entries[playing].flags = (list->entries[playing].flags & ~FLAG_PLAYING);
					if(list->entries[playing].current_position) {
						list->entries[playing].flags |= FLAG_PAUSED;
					}

					tui_submit_set_list_action(list);
				}

				char time_buffer[TIME_BUFFER_SIZE];
				snprint_ftime(time_buffer, TIME_BUFFER_SIZE, list->entries[list->selected].duration);

				tui_submit_title_line_print("Now playing "F_BOLD"%s"F_RESET" by "F_BOLD"%s"F_RESET" (%s)", list->entries[list->selected].name, list->entries[list->selected].username, time_buffer);

				playing = list->selected;
				list->entries[list->selected].flags = (list->entries[list->selected].flags & ~FLAG_PAUSED) | FLAG_PLAYING;

				tui_submit_set_list_action(list);

				sound_play(&list->entries[list->selected]);
				break;
			}

			default:
				tui_submit_status_line_print(cline_warning, "Error: got unkown keycode %x", c);
		}
	}

	// never reached, exit called in case of 'q'
	return EXIT_FAILURE;
}
