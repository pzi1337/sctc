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
		void (*func)(char*);
		char  *desc_param;
		char  *desc;
	};

	enum callback_event {
		cbe_textbox_modified = 0,
		cbe_repeat_modified, 
		cbe_tabs_modified,
		cbe_titlebar_modified,
		cbe_statusbar_modified,
		cbe_input_modified,
		cbe_sugg_modified,
		cbe_list_modified,
		callback_event_size
	};

	void state_add_list(struct track_list *_list);

	/** \addtogroup state_get State: Getter
	 *
	 *  @{
	 */
	/** \brief Get the id of the currently displayed list
	 *  \return id of the currently displayed list
	 */
	size_t             state_get_current_list();

	/** \brief Get the track_list for a specific id
	 *  \return The track_list for a specific id
	 */
	struct track_list* state_get_list(size_t id);

	/** \brief Get the current `repeat` state
	 *  \return The current `repeat` state
	 */
	enum repeat        state_get_repeat();

	/** \brief Get the current text in the title bar
	 *  \return The current text in the title bar
	 */
	char*              state_get_title_text();

	/** \brief Get the current text in the status bar
	 *  \return The current text in the status bar
	 */
	char*              state_get_status_text();

	/** \brief Get the current color of the text in the status bar
	 *  \return the current color of the text in the status bar
	 */
	enum color         state_get_status_color();

	/** \brief Get the text of the currently visible textbox
	 *  \return the text of the currently visible textbox, `NULL` if no textbox is shown
	 */
	char*              state_get_tb_text();

	/** \brief Get the title of the currently visible textbox
	 *  \return the title of the currently visible textbox, `NULL` if no textbox is shown
	 */
	char*              state_get_tb_title();
	char*              state_get_input();
	struct command*    state_get_commands();
	size_t             state_get_current_time();
	size_t             state_get_tb_old_pos();
	size_t             state_get_tb_pos();
	size_t             state_get_old_selected();
	size_t             state_get_current_selected();
	size_t             state_get_current_position();
	size_t             state_get_sugg_selected();
	/** @}*/

	/** \brief Global initialization of the internal state of SCTC.
	 *
	 *  This function is required to be called prior to the first call  to
	 *  any other state_* function.
	 *
	 *  \return   true in case of success, false otherwise
	 */
	bool state_init();

	/** \brief Global finalization of the internal state of SCTC.
	 *
	 *  This function is required to be called prior to termination of SCTC.
	 *  Do not use any state_* after calling this function.
	 *
	 *  \return   true in case of success, false otherwise
	 */
	bool state_finalize();

	/** \addtogroup state_set State: Setter
	 *
	 *  @{
	 */
	void state_set_commands    (struct command *commands);
	void state_set_current_list(size_t              list);
	void state_set_current_position(size_t pos);
	void state_set_current_selected(size_t selected);
	void state_set_current_selected_rel(int delta);
	void state_set_lists       (struct track_list **lists);
	void state_set_repeat      (enum   repeat       repeat);
	void state_set_title       (char *title_line_text);
	void state_set_status      (enum color color, char *text);
	void state_set_tb          (char *title, char *text);
	void state_set_current_time(size_t time);
	void state_set_sugg_selected(size_t selected);
	void state_set_tb_pos      (size_t pos);
	void state_set_tb_pos_rel  (int delta);
	/** @}*/


	void state_register_callback(enum callback_event evt, void (*cb)(void));

	#define NOTHING_SELECTED ((unsigned int) ~0)

#endif /* _STATE_H */
