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

#ifndef _TUI_H
	#define _TUI_H

	#include <stdbool.h>                    // for bool, false, true

	enum color {
		cline_default = 1, cline_cmd_char, cline_warning,
		tbar_default, tbar_tab_selected, tbar_tab_nselected,
		inp_cursor,
		sbar_default,

		/* track-line colors */
		tline_status,           ///< **trackline**: the status ('>' or '=')

		tline_default,          ///< **trackline**: the default colors (title / empty parts)
		tline_default_selected, ///< **trackline**: the default colors (title / empty parts) [if track is selected]
		tline_default_played,   ///< **trackline**: the default colors (title / empty parts) [if part is already played]

		tline_date,             ///< **trackline**: the created_at date
		tline_date_selected,    ///< **trackline**: the created_at date [if track is selected]
		tline_date_played,      ///< **trackline**: the created_at date [if part is already played]

		tline_ctime,            ///< **trackline**: the current position of playback
		tline_ctime_selected,   ///< **trackline**: the current position of playback [if track is selected]
		tline_ctime_played,     ///< **trackline**: the current position of playback [if part is already played]

		tline_user,             ///< **trackline**: the username
		tline_user_selected,    ///< **trackline**: the username [if track is selected]
		tline_user_played,      ///< **trackline**: the username [if part is already played]

		tline_time,             ///< **trackline**: the total time
		tline_time_selected,    ///< **trackline**: the total time [if track is selected]
		tline_time_played,      ///< **trackline**: the total time [if part is already played]

		/* command-list colors */
		cmdlist_default,        ///< **commandlist**: the default color
		cmdlist_selected,       ///< **commandlist**: the name [if entry is selected]
		cmdlist_desc_selected,  ///< **commandlist**: the description [if entry is selected]
		cmdlist_desc            ///< **commandlist**: the description
	};

	enum tui_action_kind {
		none,                ///< "do nothing"
		redraw,              ///< redraw the whole screen
		textbox_modified,    ///< redraw the textbox with new data
		tabbar_modified,     ///< TODO
		titlebar_modified,   ///< TODO
		statusbar_modified,  ///< TODO
		list_modified,       ///< TODO
		update_list,         ///< set the supplied list as currently visible one
		set_sbar_time,       ///< update the time shown in the status bar
		set_suggestion_list, ///< set the suggestion list
		updown,              ///< move cursor up/down
		input_modify_text,   ///< set the currently visible input
		back_exit            ///< destroy current 'window'
	};

	#define F_BOLD  "\1"
	#define F_RESET "\2"

	void tui_submit_action(enum tui_action_kind kind);

	bool tui_init();
	void tui_finalize();
#endif /* _TUI_H */
