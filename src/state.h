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

#ifndef _STATE_H
	#define _STATE_H

	#include "track.h"
	#include "tui.h"

	#define LIST_STREAM    0
	#define LIST_BOOKMARKS 1
	#define LIST_USER1     2

	struct command {
		char  *name;
		char  *desc;
		bool (*func)(char*);
	};

	size_t              state_get_current_list();
	struct track_list*  state_get_list(size_t id);
	enum   repeat       state_get_repeat();
	char*               state_get_title_text();
	char*               state_get_status_text();
	enum color          state_get_status_color();
	char*               state_get_tb_text();
	char*               state_get_tb_title();
	char*               state_get_input();
	struct command*     state_get_commands();
	size_t              state_get_current_time();
	size_t              state_get_tb_old_pos();
	size_t              state_get_tb_pos();

	void state_init();
	void state_finalize();

	void state_set_commands    (struct command *commands);
	void state_set_current_list(size_t              list);
	void state_set_lists       (struct track_list **lists);
	void state_set_repeat      (enum   repeat       repeat);
	void state_set_title       (char *title_line_text);
	void state_set_status      (char *text, enum color color);
	void state_set_tb          (char *title, char *text);
	void state_set_current_time(size_t time);
	void state_set_tb_pos      (size_t pos);
#endif /* _STATE_H */
