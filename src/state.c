#include "state.h"

//\cond
#include <assert.h>
#include <stdlib.h>
//\endcond

#include "helper.h"
#include "log.h"

#define MAX_LISTS 16

struct track_list_state {
	struct track_list *list;
	size_t old_selected;
	size_t selected;
	size_t position;
} lists[MAX_LISTS];

void (*callbacks[callback_event_size])(void) = {NULL};

#define CALL_CALLBACK(EVT) { if(callbacks[EVT]) {callbacks[EVT]();} }

static struct current_playback {
	size_t list;
	size_t track;
} current_playback = {.list = ~0, .track = ~0};

static size_t       _current_list = 0;        ///< the index in lists of the currently displayed list (default: 0)
static enum repeat  _repeat       = rep_none; ///< the repeat state, one out of (none, one, all)
static char        *_title_text   = NULL;
static char        *_status_text  = NULL;
static enum color   _status_color;
static size_t       _current_time = 0;

static char        *_tb_title = NULL;
static char        *_tb_text  = NULL;
static size_t       _tb_pos   = 0;
static size_t       _tb_old_pos   = 0;

static size_t       _sugg_selected = 0;

static char        *_input = NULL;
struct command     *_commands = NULL;

size_t             state_get_current_list()  { return _current_list; }
size_t             state_get_current_selected() { return lists[_current_list].selected; }
size_t             state_get_current_position() { return lists[_current_list].position; }
size_t             state_get_old_selected() { return lists[_current_list].old_selected; }

struct track_list* state_get_list(size_t id) {
	return id < MAX_LISTS ? lists[id].list : NULL;
}

enum   repeat      state_get_repeat()        { return _repeat; }
char*              state_get_title_text()    { return _title_text; }
char*              state_get_status_text()   { return _status_text; }
enum color         state_get_status_color()  { return _status_color; }
char*              state_get_tb_text()       { return _tb_text; }
char*              state_get_tb_title()      { return _tb_title; }
size_t             state_get_tb_pos()        { return _tb_pos; }
char*              state_get_input()         { return _input; }
struct command*    state_get_commands()      { return _commands; }
size_t             state_get_current_time()  { return _current_time; }
size_t             state_get_sugg_selected() { return _sugg_selected; }

void state_set_commands(struct command *commands) { _commands = commands; CALL_CALLBACK(cbe_sugg_modified); }

void state_set_current_list(size_t list) {
	_current_list = list;

	// we need to redraw both tabs and the list
	// -> both callbacks called
	CALL_CALLBACK(cbe_tabs_modified);
	CALL_CALLBACK(cbe_list_modified);
}

void state_set_lists(struct track_list **_lists) {
	for(size_t i = 0; _lists[i] && i < MAX_LISTS; i++) {
		lists[i].list         = _lists[i];
		lists[i].selected     = 0;
		lists[i].old_selected = 0;
		lists[i].position     = 0;
	}
	CALL_CALLBACK(cbe_tabs_modified);
}

void state_add_list(struct track_list *_list) {
	// TODO \todo check MAX_LISTS!
	size_t pos;
	for(pos = 0; pos < MAX_LISTS && lists[pos].list; pos++);

	lists[pos].list         = _list;
	lists[pos].selected     = 0;
	lists[pos].old_selected = 0;
	lists[pos].position     = 0;

	CALL_CALLBACK(cbe_tabs_modified);
}

void state_set_repeat(enum repeat repeat)       { _repeat       = repeat; CALL_CALLBACK(cbe_repeat_modified); }
void state_set_title(char *text)                { _title_text   = text;   CALL_CALLBACK(cbe_titlebar_modified); }
void state_set_current_time(size_t time)        { _current_time = time;   }

void state_set_sugg_selected(size_t selected) {
	_sugg_selected = selected;

	CALL_CALLBACK(cbe_sugg_modified);
}

void state_set_tb_pos(size_t pos) {
	_tb_old_pos = _tb_pos;
	_tb_pos = pos;

	CALL_CALLBACK(cbe_textbox_modified);
}

void state_set_current_playback(size_t list, size_t track) {
	current_playback.list  = list;
	current_playback.track = track;
}

void state_set_current_selected(size_t selected) {
	lists[_current_list].old_selected = lists[_current_list].selected;
	lists[_current_list].selected = selected;

	CALL_CALLBACK(cbe_list_modified);
}

void state_set_current_position(size_t pos) {
	lists[_current_list].position = pos;
}

void state_set_current_selected_rel(int delta) {
	lists[_current_list].old_selected = lists[_current_list].selected;
	if(delta < 0) {
		lists[_current_list].selected = lists[_current_list].selected < -delta ? 0 : lists[_current_list].selected + delta;
	} else {
		lists[_current_list].selected += delta;
		if(lists[_current_list].selected >= lists[_current_list].list->count) {
			lists[_current_list].selected = lists[_current_list].list->count - 1;
		}
	}

	CALL_CALLBACK(cbe_list_modified);
}

void state_set_tb_pos_rel(int delta) {
	_tb_old_pos = _tb_pos;
	if(delta < 0) {
		_tb_pos = _tb_pos < -delta ? 0 : _tb_pos + delta;
	} else {
		_tb_pos += delta;
	}

	CALL_CALLBACK(cbe_textbox_modified);
}

void state_set_status(enum color color, char *text) {
	_status_text  = text;
	_status_color = color;

	CALL_CALLBACK(cbe_statusbar_modified);
}

void state_set_tb(char *title, char *text) {
	_tb_title = title;
	_tb_text  = text;

	CALL_CALLBACK(cbe_textbox_modified);
}

void state_register_callback(enum callback_event evt, void (*cb)(void)) {
	assert(!callbacks[evt] && "ERROR: trying to reregister a callback for an event");
	callbacks[evt] = cb;
}

bool state_init() {
	_input = lcalloc(128, sizeof(char));

	return _input;
}

bool state_finalize() {
	free(_input);

	return true;
}
