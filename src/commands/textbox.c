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
#include "textbox.h"

#include <ncurses.h>                    // for LINES
//\cond
#include <stddef.h>                     // for NULL
#include <string.h>                     // for strchr
//\endcond

#include "../config.h"
#include "../helper.h"                  // for strstrp, astrdup
#include "../log.h"
#include "../state.h"                   // for state_set_tb, etc
#include "../_hard_config.h"            // for UNUSED

void cmd_tb_yank(const char *_hint) {
	astrdup(hint, _hint);
	char *selection = strstrp(hint);

	size_t playing = state_get_current_selected();
	struct track_list *list = state_get_list(state_get_current_list());

	if(streq("", selection)) {
		// copy the whole textbox if no parameter is supplied
		yank(TRACK(list, playing)->description);
	} else {
		_err("NYI");
	}
}

void cmd_tb_scroll(const char *pos) {
	astrdup(_pos, pos);
	char *target = strstrp(_pos);

	size_t cur_pos = state_get_tb_pos();
	size_t new_pos = parse_position(target, cur_pos, SIZE_MAX - 1, LINES - 8);

	if(SIZE_MAX != new_pos) {
		state_set_tb_pos(new_pos);
	} else {
		state_set_status(cline_warning, smprintf("Error: Position `%s` is not valid", pos));
	}
}

void cmd_tb_close(const char *unused UNUSED) {
	state_set_tb_pos(0);
	state_set_tb(NULL, NULL);
}

void cmd_tb_goto(const char *_hint) {
	astrdup(hint, _hint);
	char *target = strstrp(hint);

	if('+' == *target || '-' == *target) {
		int delta;
		if(1 == sscanf(target, " %16d ", &delta)) {
			size_t csel = state_get_tb_selected();
			size_t nsel = add_delta_within_limits(csel, delta, 555); // \todo TODO: limit
			state_set_tb_selected(nsel);
		}
	}
}

void cmd_tb_toggle(const char *unused UNUSED) {
	size_t csel = state_get_tb_selected();
	struct subscription *list = state_get_tb_items();
	_log("toggling selection for %s", list[csel].name);

	if(FLAG_SUBSCRIBED & list[csel].flags) list[csel].flags &= ~FLAG_SUBSCRIBED;
	else list[csel].flags |= FLAG_SUBSCRIBED;

	config_add_subscription(list[csel].name);
	state_set_tb_items(list);
}
