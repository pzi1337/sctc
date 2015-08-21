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

/** \file state.c
 *  \brief Implementation of internal state
 *
 *  This file contains all the internal state of SCTC.
 *  Everything required by several modules should be part of this state.
 */

#include "_hard_config.h"

#include "state.h"

//\cond
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
//\endcond

#include "helper.h"
#include "log.h"

void (*callbacks[callback_event_size])(void) = {NULL};
static void state_finalize(void);

#define CALL_CALLBACK(EVT) { if(callbacks[EVT]) {callbacks[EVT]();} }

static unsigned int _volume;

unsigned int state_get_volume(void) { return _volume; }
void state_set_volume(unsigned int volume) { _volume = volume; CALL_CALLBACK(cbe_tabs_modified); }

static enum repeat  _repeat     = rep_none; ///< the repeat state, one out of (none, one, all)
static char        *_title_text = NULL;
static char        *_input      = NULL;

enum repeat state_get_repeat(void)     { return _repeat; }
char*       state_get_title_text(void) { return _title_text; }
char*       state_get_input(void)      { return _input; }

void state_set_repeat(enum repeat repeat) {
	_repeat = repeat;
	CALL_CALLBACK(cbe_repeat_modified);
}

void state_set_title(char *text) {
	assert(text && "text must not be NULL");

	_title_text = text;

	CALL_CALLBACK(cbe_titlebar_modified);
}

/**************
* SUGGESTIONS *
**************/
struct command *_commands = NULL;
static size_t   _sugg_selected = 0;

struct command*  state_get_commands(void)      { return _commands; }
size_t           state_get_sugg_selected(void) { return _sugg_selected; }

void state_set_commands(struct command *commands) {
	_commands = commands;

	CALL_CALLBACK(cbe_sugg_modified);
}

void state_set_sugg_selected(size_t selected) {
	_sugg_selected = selected;

	CALL_CALLBACK(cbe_sugg_modified);
}

/**********
* TEXTBOX * 
**********/
static struct {
	char   *title; ///< Title of the currently visible textbox
	char   *text;  ///< Contents of the currently visible textbox
	size_t  pos;   ///< Position (uppermost line shown)
} textbox = { NULL, NULL, 0 };

char*  state_get_tb_text(void)  { return textbox.text;  }
char*  state_get_tb_title(void) { return textbox.title; }
size_t state_get_tb_pos(void)   { return textbox.pos;   }

void state_set_tb_pos(size_t pos) {
	textbox.pos = pos;
	CALL_CALLBACK(cbe_textbox_modified);
}

void state_set_tb_pos_rel(int delta) {
	textbox.pos = add_delta_within_limits(textbox.pos, delta, SIZE_MAX);
	CALL_CALLBACK(cbe_textbox_modified);
}

void state_set_tb(char *title, char *text) {
	textbox.title = title;
	textbox.text  = text;

	CALL_CALLBACK(cbe_textbox_modified);
}

/*************
* TRACK LIST *
*************/
static struct track_list_state {
	struct track_list *list;
	size_t old_selected;
	size_t selected;
	size_t position;
} lists[MAX_LISTS];

static size_t _current_list = 0; ///< the index in lists of the currently displayed list (default: 0)

size_t state_get_current_list(void)     { return _current_list; }
size_t state_get_current_selected(void) { return lists[_current_list].selected; }
size_t state_get_current_position(void) { return lists[_current_list].position; }
size_t state_get_old_selected(void)     { return lists[_current_list].old_selected; }

struct track_list* state_get_list(size_t id) {
	return id < MAX_LISTS ? lists[id].list : NULL;
}

void state_set_current_list(size_t list) {
	_current_list = list;

	// we need to redraw both tabs and the list
	// -> both callbacks called
	CALL_CALLBACK(cbe_tabs_modified);
	CALL_CALLBACK(cbe_list_modified);
}

void state_add_list(struct track_list *_list) {
	assert(_list && "_list must not be NULL");

	size_t pos;
	for(pos = 0; pos < MAX_LISTS && lists[pos].list; pos++);
	if(MAX_LISTS == pos) return;

	lists[pos].list         = _list;
	lists[pos].selected     = 0;
	lists[pos].old_selected = 0;
	lists[pos].position     = 0;

	CALL_CALLBACK(cbe_tabs_modified);
}

void state_set_current_selected(size_t selected) {
	lists[_current_list].old_selected = lists[_current_list].selected;
	lists[_current_list].selected = selected;

	CALL_CALLBACK(cbe_list_modified);
}

void state_set_current_position(size_t pos) {
	lists[_current_list].position = pos;
}

/*******************
* CURRENT PLAYBACK *
*******************/
static struct current_playback {
	size_t list;
	size_t track;
	size_t time;
} current_playback = {.list = (size_t) ~0l, .track = (size_t) ~0l, .time = (size_t) 0l};

void state_set_current_playback(size_t list, size_t track) {
	current_playback.list  = list;
	current_playback.track = track;
}

void state_set_current_time(size_t time) { current_playback.time = time; }

size_t state_get_current_playback_list(void)  { return current_playback.list;  }
size_t state_get_current_playback_track(void) { return current_playback.track; }
size_t state_get_current_playback_time(void)  { return current_playback.time;  }

/**************
* STATUS LINE *
**************/
static struct {
	char       *text;
	enum color  color;
} status_line = { NULL };

void state_set_status(enum color color, char *text) {
	assert(text && "text must not be NULL");

	status_line.text  = text;
	status_line.color = color;

	CALL_CALLBACK(cbe_statusbar_modified);
}

char*      state_get_status_text(void)  { return status_line.text;  }
enum color state_get_status_color(void) { return status_line.color; }

/*******
* MISC *
*******/
void state_register_callback(enum callback_event evt, void (*cb)(void)) {
	assert((evt < callback_event_size) && "ERROR: evt is not within valid range");
	assert(!callbacks[evt] && "ERROR: trying to reregister a callback for an event");
	callbacks[evt] = cb;
}

bool state_init(void) {
	_input = lcalloc(128, sizeof(char));

	if(atexit(state_finalize)) {
		_log("atexit: %s", strerror(errno));
	}

	return _input;
}

/** \brief Global finalization of the internal state of SCTC.
 *
 *  This function is required to be called prior to termination of SCTC.
 *  Do not use any state_* after calling this function.
 */
static void state_finalize(void) {
	free(_input);
}
