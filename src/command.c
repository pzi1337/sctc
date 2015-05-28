#define _GNU_SOURCE

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

#define SERVER_PORT 443
#define SERVER_NAME "api.soundcloud.com"

#define TIME_BUFFER_SIZE 64

#define MIN(x,y) (x < y ? x : y)

extern int playing;

static void handle_textbox();
static bool handle_command(char *buffer, size_t buffer_size);

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
static void cmd_download(char *unused) { // TODO!
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
}

static void cmd_open_user(char *_user) {
	char *user = strstrp(_user);
	state_set_status(cline_default, smprintf("Info: Switch to "F_BOLD"%s"F_RESET"'s channel\n", user));

	struct network_conn *nwc = tls_connect(SERVER_NAME, SERVER_PORT);
	struct track_list *list = soundcloud_get_entries(nwc, user);
	nwc->disconnect(nwc);

	if(list->count) {
		list->name = list->entries[0].username;
		state_add_list(list);
	} else {
		state_set_status(cline_warning, smprintf("Info: Cannot switch to "F_BOLD"%s"F_RESET"'s channel: No tracks found!\n", user));
		track_list_destroy(list, true);
	}
}

static void cmd_write_playlist(char *_file) {
	char *file = strstrp(_file);
	struct track_list *list = state_get_list(state_get_current_list());

	state_set_status(cline_default, smprintf("Info: Writing to file "F_BOLD"%s"F_RESET" (type: JSPF)\n", file));
	jspf_write(file, list);
}

static void cmd_goto(char *hint) {
	char *target = strstrp(hint);

	if(!strcmp("", target)) {
		state_set_current_selected(playing - 1);
		state_set_status(cline_default, "");
	} else if(!strcmp("end", target)) {
		struct track_list *list = state_get_list(state_get_current_list());

		state_set_current_selected(list->count - 1);
		state_set_status(cline_default, "");
	} else if('+' == *target || '-' == *target) {
		int delta = 0;
		bool valid = false;

		if(NULL != strchr(target, '.')) {
			float pages = 0;
			valid = (1 == sscanf(target, " %f ", &pages));

			delta = pages * (LINES - 2);
		} else {
			valid = (1 == sscanf(target, " %d ", &delta));
		}
		if(valid) state_set_current_selected_rel(delta);
	} else {
		unsigned int pos;
		if(1 == sscanf(target, " %u ", &pos)) {
			state_set_current_selected(pos);
		}
	}
}

/** \brief Switch to playlist with `id`
 *
 *  \param id  The id of the playlist to switch to
 *  \return    `true` on success, otherwise false
 */
void switch_to_list(unsigned int id) {
	struct track_list *list = state_get_list(id);

	if(!list) {
		state_set_status(cline_warning, "Error: Not switching to list: No such list");
		return;
	}

	if(!list->count) {
		state_set_status(cline_warning, smprintf("Error: Not switching to "F_BOLD"%s"F_RESET": List is empty", list->name));
		return;
	}

	state_set_status(cline_default, smprintf("Info: Switching to "F_BOLD"%s"F_RESET, list->name));
	state_set_current_list(id);
}

/** \brief Switch to playlist specified in string `list`
 *
 *  \param list  The id of the playlist to switch to
 *  \return      `true` on success, otherwise false
 */
static void cmd_list(char *list) {
	unsigned int list_id;

	if(1 == sscanf(list, " %u ", &list_id)) {
		switch_to_list(list_id - 1);
	} else {
		_log("cannot switch to list %s", list);
	}
}

static void cmd_seek(char *time) {
	char *seekto = strstrp(time);
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
	} else {
		sound_seek(new_abs);
	}
}

/** \brief Exit SCTC
 *
 *  Writes the list of bookmarks to file and shuts down SCTC.
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return does not return
 */
static void cmd_exit(char *unused) {
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
}

static void search_direction(bool down) {
	struct track_list *list = state_get_list(state_get_current_list());
	const int step = down ? 1 : -1;

	for(int i = state_get_current_selected() + step; i >= 0 && i < list->count; i += step) {
		if(strcasestr(list->entries[i].name, state_get_input())) {
			state_set_current_selected(i);
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

static void cmd_search_next(char *unused) { search_direction(true);  }
static void cmd_search_prev(char *unused) { search_direction(false); }

static void cmd_search_start(char *unused) {
	state_set_status(cline_cmd_char, F_BOLD"/"F_RESET);
	handle_search(state_get_input(), 127);

	cmd_search_next(NULL);
}

static void cmd_bookmark(char *unused) {
	struct track_list *list = state_get_list(state_get_current_list());

	track_list_add(state_get_list(LIST_BOOKMARKS), &list->entries[state_get_current_selected()]);
	state_set_status(cline_default, smprintf("Info: Added "F_BOLD"%s"F_RESET" to bookmarks", list->entries[state_get_current_selected()].name));
}

static void cmd_command_input(char *unused) {
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

}

/** \brief Display 'Help' Dialog
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static void cmd_help(char *unused) {
	char *help_msg = LOGO_PART PARAGRAPH_PART DESCRIPTION_PART PARAGRAPH_PART ALPHA_PART PARAGRAPH_PART FEATURE_PART PARAGRAPH_PART NONFEATURE_PART PARAGRAPH_PART KNOWN_BUGS_PART PARAGRAPH_PART LICENSE_PART;

	state_set_tb("Help / About", help_msg);
	handle_textbox();
}

/** \brief Set repeat to 'none'
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static void cmd_repeat_none(char *unused) {
	state_set_repeat(rep_none);
	state_set_status(cline_default, "Info: Switched repeat to 'none'");
}

/** \brief Set repeat to 'one' (repeat single track)
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static void cmd_repeat_one(char *unused) {
	state_set_repeat(rep_one);
	state_set_status(cline_default, "Info: Switched repeat to 'one'");
}

/** \brief Set repeat to 'all' (repeat whole track_list)
 *
 *  \param unused  Unused parameter, required due to interface of cmd_* functions
 *  \return true
 */
static void cmd_repeat_all(char *unused) {
	state_set_repeat(rep_all);
	state_set_status(cline_default, "Info: Switched repeat to 'all'");
}

static void cmd_repeat_toggle(char *unused) {
	switch(state_get_repeat()) {
		case rep_none: cmd_repeat_one (NULL); break;
		case rep_one:  cmd_repeat_all (NULL); break;
		case rep_all:  cmd_repeat_none(NULL); break;
	}
}

static void cmd_yank(char *unused) {
	struct track_list *list = state_get_list(state_get_current_list());
	size_t current_selected = state_get_current_selected();

	yank(list->entries[current_selected].permalink_url);
	state_set_status(cline_default, smprintf("yanked "F_BOLD"%s"F_RESET, list->entries[current_selected].permalink_url));
}

static void cmd_details(char *unused) {
	struct track_list *list = state_get_list(state_get_current_list());
	size_t current_selected = state_get_current_selected();

	char *title = smprintf("%s by %s", list->entries[current_selected].name, list->entries[current_selected].username);
	state_set_tb(title, list->entries[current_selected].description);
	handle_textbox();
	free(title);
}

static void stop_playback(bool reset) {
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
	}
}

static void cmd_play(char *unused) {
	size_t current_selected = state_get_current_selected();
	struct track_list *list = state_get_list(state_get_current_list());

	stop_playback(false); // pause other playing track (if any)

	char time_buffer[TIME_BUFFER_SIZE];
	snprint_ftime(time_buffer, TIME_BUFFER_SIZE, list->entries[current_selected].duration);

	playing = current_selected;

	state_set_title(smprintf("Now playing "F_BOLD"%s"F_RESET" by "F_BOLD"%s"F_RESET" (%s)", list->entries[playing].name, list->entries[playing].username, time_buffer));

	list->entries[current_selected].flags = (list->entries[current_selected].flags & ~FLAG_PAUSED) | FLAG_PLAYING;

	tui_submit_action(update_list);

	sound_play(&list->entries[current_selected]);
}

static void cmd_pause(char *unused) { stop_playback(false); }
static void cmd_stop(char *unused)  { stop_playback(true);  }

/** \brief Issue a redraw of the whole screen
 *
 *  \return true
 */
static void cmd_redraw(char *unused) {
	tui_submit_action(redraw);
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
struct command commands[] = {
	{"bookmark",      cmd_bookmark,       "<none/ignored>",                "Add currently selected entry to booksmarks"},
	{"command-input", cmd_command_input,  "<none/ignored>",                "Open command input field"},
	{"details",       cmd_details,        "<none/ignored>",                "Show details for currently selected track"},
	{"download",      cmd_download,       "<none/ignored>",                "Download the currently selected entry to file"},
	{"exit",          cmd_exit,           "<none/ignored>",                "Terminate SCTC"},
	{"goto",          cmd_goto,           "<relative or absolute offset>", "Set selection to specific entry"},
	{"help",          cmd_help,           "<none/ignored>",                "Show help"},
	{"list",          cmd_list,           "<number of list>",              "Switch to specified playlist"},
	{"open",          cmd_open_user,      "<name of user>",                "Open a specific user's stream"},
	{"pause",         cmd_pause,          "<none/ignored>",                "Pause playback of current track"},
	{"play",          cmd_play,           "<none/ignored>",                "Start playback of currently selected track"},
	{"redraw",        cmd_redraw,         "<none/ignored>",                "Redraw the screen"},
	{"repeat-none",   cmd_repeat_none,    "<none/ignored>",                "Set repeat to 'none'"},
	{"repeat-one",    cmd_repeat_one,     "<none/ignored>",                "Set repeat to 'one'"},
	{"repeat-all",    cmd_repeat_all,     "<none/ignored>",                "Set repeat to 'all'"},
	{"repeat-toggle", cmd_repeat_toggle,  "<none/ignored>",                "Toggle repeat (none -> one -> all -> none)"},
	{"search-start",  cmd_search_start,   "<none/ignored>",                "Start searching (open input field)"},
	{"search-next",   cmd_search_next,    "<none/ignored>",                "Continue search downwards"},
	{"search-prev",   cmd_search_prev,    "<none/ignored>",                "Continue search upwards"},
	{"seek",          cmd_seek,           "<time to seek to>",             "Seek to specified time in current track"},
	{"stop",          cmd_stop,           "<none/ignored>",                "Stop playback of current track"},
	{"write",         cmd_write_playlist, "<filename>",                    "Write current playlist to file (.jspf)",  },
	{"yank",          cmd_yank,           "<none/ignored>",                "Copy URL of currently selected track to clipboard"},
	{NULL, NULL, NULL}
};
const size_t command_count = sizeof(commands) / sizeof(struct command) - 1;


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
	memcpy(matching_commands, commands, command_count + 1);

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

