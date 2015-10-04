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
	#include "soundcloud.h"
	#include "generic/rc_string.h"

	#define LIST_STREAM    0
	#define LIST_BOOKMARKS 1

	/** \brief Enumeration holding the currently provided events callbacks can be registered for.
	 *
	 *  Whenever the internal state is modified (by calling one of the `state_set_*()` functions,
	 *  any callbacks associated to the modified part of the state are called (if any).
	 *
	 *  In the most cases these functions do redrawing / updating the current screen in order to 
	 *  provide the updated information.
	 *
	 *  \see `state_register_callback()`
	 */
	enum callback_event {
		cbe_textbox_modified = 0, ///< Event triggerd if a **textbox** was modified
		cbe_textbox_items_modified,
		cbe_repeat_modified,      ///< Event triggerd if the **repeat mode** was modified 
		cbe_tabs_modified,        ///< Event triggerd if the **tabs** were modified (p.x. a new list, a list removed)
		cbe_titlebar_modified,    ///< Event triggerd if the **titlebar** was modified
		cbe_statusbar_modified,   ///< Event triggerd if the **statusbar** was modified
		cbe_input_modified,       ///< Event triggerd if the **input** was modified
		cbe_sugg_modified,        ///< Event triggerd if the **list of suggestions** was modified
		cbe_list_modified,        ///< Event triggerd if the **contents of a list** were modified was modified
		callback_event_size       ///< The number of elements within this enumeration
	};

	enum repeat {
		rep_none, ///< Repeating is disabled
		rep_one,  ///< Repeating the current track
		rep_all   ///< Repeating the whole list
	};

	/** \brief Getter functions for the internal state.
	 *  \name Getter
	 *
	 *  Functions returning the internal state, do not insist modifying memory returned by any of those functions.
	 */
	///@{
	/** \brief Get the id of the currently displayed list
	 *  \return id of the currently displayed list
	 */
	size_t             state_get_current_list(void);

	/** \brief Get the track_list for a specific id
	 *  \return The track_list for a specific id
	 */
	struct track_list* state_get_list(size_t id);

	/** \brief Get the current `repeat` state
	 *  \return The current `repeat` state
	 */
	enum repeat        state_get_repeat(void);

	/** \brief Get the current text in the title bar
	 *  \return The current text in the title bar
	 */
	struct rc_string*  state_get_title_text(void);

	/** \brief Get the current text in the status bar
	 *  \return The current text in the status bar
	 */
	char*              state_get_status_text(void);

	/** \brief Get the current color of the text in the status bar
	 *  \return the current color of the text in the status bar
	 */
	enum color         state_get_status_color(void);

	/** \brief Get the text of the currently visible textbox
	 *  \return the text of the currently visible textbox, `NULL` if no textbox is shown
	 */
	char*              state_get_tb_text(void);

	/** \brief Get the title of the currently visible textbox
	 *  \return the title of the currently visible textbox, `NULL` if no textbox is shown
	 */
	char*              state_get_tb_title(void);
	struct subscription* state_get_tb_items(void);
	size_t             state_get_tb_selected(void);
	void               state_set_tb_selected(size_t selected);
	char*              state_get_input(void);
	struct command*    state_get_commands(void);
	size_t             state_get_current_playback_time(void);
	size_t             state_get_tb_old_pos(void);
	size_t             state_get_tb_pos(void);
	size_t             state_get_old_selected(void);
	size_t             state_get_current_selected(void);
	size_t             state_get_current_position(void);
	size_t             state_get_sugg_selected(void);
	size_t             state_get_current_playback_list(void);
	size_t             state_get_current_playback_track(void);
	unsigned int       state_get_volume(void);
	///@}

	/** \brief Global initialization of the internal state of SCTC.
	 *
	 *  This function is required to be called prior to the first call  to
	 *  any other state_* function.
	 *
	 *  \return   true in case of success, false otherwise
	 */
	bool state_init(void);

	/** \brief Setter functions for the internal state.
	 *  \name Setter
	 *
	 *  These functions automatically call the associated callback, if registered via `state_register_callback()`.
	 *  \see `enum callback_event`
	 *  \see `state_register_callback()`
	 */
	///@{

	/** \brief Add a new tracklist to the currently visible lists
	 *
	 *  \warning This function may fail silently if already `MAX_LISTS` are shown.
	 *  \see _hard_config.h
	 *  \see `MAX_LISTS`
	 *
	 *  \todo Silent failing most probably no good idea...
	 *
	 *  \param _list  The list to add to the list of currently visible lists (may not be `NULL`)
	 *
	 *  \remark Triggers `cbe_tabs_modified`
	 *  \see `state_register_callback()`
	 *  \see `enum callback_event`
	 */
	void state_add_list(struct track_list *_list) ATTR(nonnull);

	/** \brief Set a list of currently applicable commands
	 *
	 *  \param commands  The list of commands, may be `NULL` if nothing should be shown
	 *
	 *  \remark Triggers `cbe_sugg_modified`
	 *  \see `state_register_callback()`
	 *  \see `enum callback_event`
	 */
	void state_set_commands(struct command *commands);

	/** \brief Set the id of the currently visible list
	 *
	 *  \param list  The id of the currently visible list
	 *
	 *  \remark Triggers `cbe_tabs_modified` **and** `cbe_list_modified`
	 *  \see `state_register_callback()`
	 *  \see `enum callback_event`
	 */
	void state_set_current_list(size_t list);

	/** \brief Sets the position (= the first track shown) within the current list
	 *
	 *  \param pos  The position within the list
	 *
	 *  \remark Triggers **nothing**
	 *  \see `state_register_callback()`
	 *  \see `enum callback_event`
	 */
	void state_set_current_position(size_t pos);

	/** \brief Sets the currently selected track and updates the old_selected track
	 *
	 *  \param selected  The id of the currently seelcted track
	 *
	 *  \remark Triggers `cbe_list_modified`
	 *  \see `state_register_callback()`
	 *  \see `enum callback_event`
	 */
	void state_set_current_selected(size_t selected);

	/** \brief Set the current repeat state
	 *
	 *  \param repeat  The new repeat state
	 *
	 *  \remark Triggers `cbe_repeat_modified`
	 *  \see `state_register_callback()`
	 *  \see `enum callback_event`
	 */
	void state_set_repeat(enum repeat repeat);

	/** \brief Set the text shown in the titlebar
	 *
	 *  \param text  The new text to be shown in the titlebar (must not be `NULL`)
	 *
	 *  \remark Triggers `cbe_titlebar_modified`
	 *  \see `state_register_callback()`
	 *  \see `enum callback_event`
	 */
	void state_set_title(struct rc_string *text) ATTR(nonnull);

	/** \brief Set the text shown in the statusbar
	 *
	 *  \param color  The color of the new text
	 *  \param text   The new text to be shown in the titlebar (must not be `NULL`)
	 *
	 *  \remark Triggers `cbe_statusbar_modified`
	 *  \see `state_register_callback()`
	 *  \see `enum callback_event`
	 */
	void state_set_status(enum color color, char *text);

	/** \brief Set the current playback time (of the current track)
	 *
	 *  \param time  The time
	 *
	 *  \remark Triggers **nothing**
	 *  \see `state_register_callback()`
	 *  \see `enum callback_event`
	 */
	void state_set_current_time(size_t time);

	void state_set_sugg_selected(size_t selected);

	void state_set_tb          (char *title, char *text);
	void state_set_tb_pos      (size_t pos);
	void state_set_tb_pos_rel  (int delta);
	void state_set_current_playback(size_t list, size_t track);
	void state_set_volume(unsigned int volume);
	void state_set_tb_items(struct subscription *items);
	///@}

	/** \brief Register a callback for a specific event.
	 *
	 *  This function allows registration of a callback function for any callback events specified in
	 *  `enum callback_event`.
	 *
	 *  **Do not try to call `state_register_callback()` a second time with the same callback event**
	 *
	 *  Once a callback is registered it cannot be modified or deleted.
	 *
	 *  \param evt  The event to register the callback for (`callback_event_size` is not valid here)
	 *  \param cb   The function to be called in case of event
	 */
	void state_register_callback(enum callback_event evt, void (*cb)(void));

	#define NOTHING_SELECTED ((unsigned int) ~0)

#endif /* _STATE_H */
