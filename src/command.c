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
#include <stddef.h>                     // for NULL, size_t
#include <string.h>                     // for strlen, strncmp
//\endcond
#include "commands/global.h"            // for cmd_gl_pause, cmd_gl_redraw, etc
#include "commands/playlist.h"          // for cmd_pl_add, etc
#include "commands/textbox.h"           // for cmd_tb_close, cmd_tb_scroll, etc
#include "config.h"                     // for scope::scope_playlist, etc

/** \brief Array of all commands supported by SCTC.
 *
 *  Includes a description, which is, for instance, shown in the commandwindow, and the function to call in
 *  order to execute the command.
 *
 *  \todo At this point only the `listview` can be used via commands
 */
const struct command commands[] = {
	{"add",           cmd_pl_add,            scope_playlist, "<ID of list>",                  "Add currently selected track to playlist with provided ID"},
	{"command-input", cmd_pl_command_input,  scope_playlist, "<none/ignored>",                "Open command input field"},
	{"close",         cmd_tb_close,          scope_textbox,  "<none/ignored>",                "Close the currently visible textbox"},
	{"del",           cmd_pl_del,            scope_playlist, "<none/ignored>",                "Delete currently selected track from current playlist"},
	{"details",       cmd_pl_details,        scope_playlist, "<none/ignored>",                "Show details for currently selected track"},
	{"download",      cmd_pl_download,       scope_playlist, "<none/ignored>",                "Download the currently selected entry to file"},
	{"exit",          cmd_pl_exit,           scope_playlist, "<none/ignored>",                "Terminate SCTC"},
	{"goto",          cmd_pl_goto,           scope_playlist, "<relative or absolute offset>", "Set selection to specific entry"},
	{"help",          cmd_pl_help,           scope_playlist, "<none/ignored>",                "Show help"},
	{"list",          cmd_pl_list,           scope_playlist, "<number of list>",              "Switch to specified playlist"},
	{"list-new",      cmd_pl_list_new,       scope_playlist, "<name of list>",                "Create a new playlist with specified name"},
	{"open",          cmd_pl_open_user,      scope_playlist, "<name of user>",                "Open a specific user's stream"},
	{"pause",         cmd_gl_pause,          scope_global,   "<none/ignored>",                "Pause playback of current track"},
	{"play",          cmd_pl_play,           scope_playlist, "<none/ignored>",                "Start playback of currently selected track"},
	{"redraw",        cmd_gl_redraw,         scope_global,   "<none/ignored>",                "Redraw the screen"},
	{"repeat",        cmd_gl_repeat,         scope_global,   "{,none,one,all}",               "Set/Toggle repeat"},
	{"scroll",        cmd_tb_scroll,         scope_textbox,  "<relative or absolute offset>", "Scroll a textbox up/down"},
	{"search-start",  cmd_pl_search_start,   scope_playlist, "<none/ignored>",                "Start searching (open input field)"},
	{"search-next",   cmd_pl_search_next,    scope_playlist, "<none/ignored>",                "Continue search downwards"},
	{"search-prev",   cmd_pl_search_prev,    scope_playlist, "<none/ignored>",                "Continue search upwards"},
	{"seek",          cmd_gl_seek,           scope_global,   "<time to seek to>",             "Seek to specified time in current track"},
	{"stop",          cmd_gl_stop,           scope_global,   "<none/ignored>",                "Stop playback of current track"},
	{"vol",           cmd_gl_volume,         scope_global,   "<delta (in percent)>",          "modify playback volume by given percentage"},
	{"write",         cmd_pl_write_playlist, scope_playlist, "<filename>",                    "Write current playlist to file (.jspf)"},
	{"yank",          cmd_tb_yank,           scope_textbox,  "<none/ignored>",                "MISSING"},
	{"yank",          cmd_pl_yank,           scope_playlist, "<none/ignored>",                "Copy URL of currently selected track to clipboard"},
	{NULL, NULL, 0, NULL, NULL}
};
const size_t command_count = sizeof(commands) / sizeof(struct command) - 1;

const struct command* command_search(const char *input, enum scope scope) {
	const size_t in_len = strlen(input);

	for(size_t i = 0; commands[i].name; i++) {
		if(scope != commands[i].valid_scope && scope_global != commands[i].valid_scope) {
			continue;
		}

		const size_t cmd_len = strlen(commands[i].name);

		if(in_len >= cmd_len) {
			if(!strncmp(commands[i].name, input, cmd_len)) {
				return &commands[i];
			}
		}
	}
	return NULL;
}
