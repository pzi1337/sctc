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

#include <ncurses.h>                    // for getch
//\cond
#include <stddef.h>                     // for NULL, size_t
//\endcond

#include "commands/global.h"            // for cmd_pause, cmd_redraw, etc
#include "commands/playlist.h"          // for cmd_add, cmd_command_input, etc
#include "commands/textbox.h"           // for cmd_close, cmd_scroll
#include "config.h"                     // for scope::scope_playlist, etc
#include "helper.h"                     // for fork_and_run
#include "state.h"                      // for state_get_current_list, etc
#include "track.h"                      // for track, track_list, TRACK, etc

void handle_textbox(void); // TODO

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

/** \brief Handle input for a textbox
 *
 *  Handles user's input for a textbox, such as `scroll up`, `scroll down`
 *  and `close textbox`.
 */
void handle_textbox(void) { // TODO
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

