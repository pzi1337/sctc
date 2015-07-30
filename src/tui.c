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

#include "_hard_config.h"
#include "tui.h"

//\cond
#include <assert.h>                     // for assert
#include <errno.h>                      // for errno
#include <pthread.h>                    // for pthread_create, etc
#include <semaphore.h>                  // for sem_post, sem_init, etc
#include <signal.h>                     // for SIGWINCH, signal, SIG_ERR
#include <stdarg.h>                     // for va_start, va_end, va_list
#include <stddef.h>                     // for size_t
#include <stdio.h>                      // for vsnprintf
#include <stdlib.h>                     // for NULL, mbstowcs, free
#include <string.h>                     // for strlen, strtok, strdup, etc
#include <time.h>                       // for strftime
//\endcond

#include <ncurses.h>                    // for init_pair, ERR, LINES, etc
#include <term.h>                       // for del_curterm, cur_term

#include "helper.h"                     // for snprint_ftime, strcrep
#include "log.h"                        // for _log
#include "state.h"                      // for state_get_current_list, etc
#include "track.h"                      // for track, track_list, etc
#include "command.h"

/* user defined colors */
#define COLOR_SHELL  -1 /* the default color used by the shell, required to be -1 */
#define COLOR_GRAY    78 /* user defined color 'gray' */
#define COLOR_DGRAY   79 /* user defined color 'dark gray' */
#define COLOR_DGREEN  80

#define TIME_BUFFER_SIZE 64

static void tui_track_print_line(struct track* entry, bool selected, int line);

static void tui_print(char *fmt, ...);
static void tui_track_list_print();
static size_t tui_track_focus();
static void tui_update_suggestion_list();

static void tui_update_textbox();

/* functions handling (re)drawing of (parts of) screen */
static void tui_draw_title_line();
static void tui_draw_tab_bar();
static void tui_draw_status_line();

static void tui_finalize();

#define wcsps(S) mbstowcs(NULL, S, 0)

static pthread_t thread_tui;
static bool terminate = false;

static bool whole_redraw_required = false;

static struct textbox_window {
	WINDOW *win;
	WINDOW *pad;
} textbox_window = { NULL };

static WINDOW *suggestion_window = NULL;

static sem_t sem_have_action;
static sem_t sem_wait_action;

static enum tui_action_kind action;

static void* _thread_tui_function(void *unused) {
	do {
		int ret = sem_wait(&sem_have_action);
		if(whole_redraw_required) {
			whole_redraw_required = false;

			endwin();
			refresh();
			clear();

			_log("doing the redraw, sem_wait returned %i, LINES = %d", ret, LINES);

			tui_draw_title_line();  // redraw the title line
			tui_draw_tab_bar();     // redraw the tab bar

			tui_track_list_print();

			if(state_get_tb_title()) {
				tui_update_textbox();
			}

			tui_draw_status_line(); // redraw the status line

		} else {
			switch(action) {
				case none: break; // FIXME

				case set_sbar_time: {
					color_set(sbar_default, NULL);

					char time_buffer[TIME_BUFFER_SIZE];
					int time_len = snprint_ftime(time_buffer, TIME_BUFFER_SIZE, state_get_current_playback_time());
					mvprintw(0, COLS - time_len, "%s", time_buffer);

					refresh();
					break;
				}

				case input_modify_text: {
					const int x = 1;
					const int y = LINES - 1;

					color_set(cline_default, NULL);
					move(y, x);
					tui_print("%s", state_get_input());

					color_set(inp_cursor, NULL);
					printw(" ");

					color_set(cline_default, NULL);
					mvprintw(y, x + wcsps(state_get_input()) + 1, "%0*c", COLS - (x + wcsps(state_get_input()) + 1), ' ');

					break;
				}

				case update_list:        tui_track_list_print();       break;
				case titlebar_modified:  tui_draw_title_line();        break;
				case tabbar_modified:    tui_draw_tab_bar();           break;
				case textbox_modified:   tui_update_textbox();         break;
				case sugg_modified:      tui_update_suggestion_list(); break;
				case statusbar_modified: tui_draw_status_line();       break;

				case list_modified:
					tui_track_focus();
					tui_track_list_print();
					break;

				default:
					_log("currently not implemented action-kind %d", action);
			}
			action = none;
		}
		refresh();

		sem_post(&sem_wait_action);
	} while(!terminate);

	return NULL;
}

static void tui_draw_title_line() {
	color_set(sbar_default, NULL);

	move(0, 0);
	tui_print("%s%0*c", state_get_title_text(), COLS - strlen(state_get_title_text()), ' ');
	refresh();
}

static void tui_draw_status_line() {
	color_set(state_get_status_color(), NULL);

	move(LINES - 1, 0);
	tui_print("%s%0*c", state_get_status_text(), COLS - strlen(state_get_status_text()), ' ');
	refresh();
}

static void tui_update_suggestion_list() {
	struct command* commands = state_get_commands();

	if(!commands) {
		assert(suggestion_window);
		delwin(suggestion_window);
		suggestion_window = NULL;

		touchwin(stdscr);
		refresh();
	} else {
		if(!suggestion_window) {
			suggestion_window = newwin(SUGGESTION_LIST_HEIGHT, COLS, LINES - SUGGESTION_LIST_HEIGHT - 1, 0);
			wrefresh(suggestion_window);
		}

		size_t start = 0;
		size_t sugg_selected = state_get_sugg_selected();
		if(NOTHING_SELECTED != sugg_selected) {
			start = sugg_selected - sugg_selected % SUGGESTION_LIST_HEIGHT;
		}

		size_t line;
		for(line = 0; line < SUGGESTION_LIST_HEIGHT && commands[start + line].name; line++) {
			wcolor_set(suggestion_window, line + start == sugg_selected ? cmdlist_selected : cmdlist_default, NULL);
			mvwprintw (suggestion_window, line, 0, "%0*c", COLS, ' ');
			mvwprintw (suggestion_window, line, 1, "%s", commands[start + line].name);

			wcolor_set(suggestion_window, line + start == sugg_selected ? cmdlist_descparam_selected : cmdlist_descparam, NULL);
			mvwprintw (suggestion_window, line, 20, "%s", commands[start + line].desc_param);

			wcolor_set(suggestion_window, line + start == sugg_selected ? cmdlist_desc_selected : cmdlist_desc, NULL);
			mvwprintw (suggestion_window, line, COLS / 2, commands[start + line].desc);
		}

		wcolor_set(suggestion_window, cmdlist_default, NULL);
		for(; line < SUGGESTION_LIST_HEIGHT; line++) {
			mvwprintw(suggestion_window, line, 0, "%0*c", COLS, ' ');
		}

		wrefresh(suggestion_window);
	}
}

static size_t tui_track_focus() {
	struct track_list *list = state_get_list(state_get_current_list());
	size_t current_list_pos = state_get_current_position();

	size_t old_selected = state_get_old_selected();
	size_t new_selected = state_get_current_selected();

	// check if new selection is currently visible
	if(new_selected >= current_list_pos && new_selected < current_list_pos + LINES - 4) {
		// the current selection is always visible, therefore we can modify the line without
		// any further checking
		tui_track_print_line(TRACK(list, old_selected), false, old_selected - current_list_pos + 2);
		tui_track_print_line(TRACK(list, new_selected), true,  new_selected - current_list_pos + 2);
	} else {
		// we need to scroll -> redraw 'everything'
		// obviously one of the two possibilities is sensible ;)

		// try to scroll up
		while(new_selected < current_list_pos) {
			if(current_list_pos >= LINES / 4) {
				current_list_pos -= LINES / 4;
			} else {
				current_list_pos = 0;
			}
		}

		// try to scroll down
		while(new_selected >= current_list_pos + LINES - 4) {
			if(current_list_pos < list->count - LINES / 2) {
				current_list_pos += LINES / 4;
			} else {
				current_list_pos = list->count - LINES / 2;
				break; // GNAA TODO
			}
		}

		// do the actual redrawing
		state_set_current_position(current_list_pos);
		tui_track_list_print();
	}

	state_set_current_position(current_list_pos);
	return current_list_pos;
}

static void tui_draw_tab_bar() {
	color_set(tbar_default, NULL);
	mvprintw(1, 0, "%0*c", COLS, ' ');

	move(1, 0);
	for(size_t i = 0; state_get_list(i); i++) {
		color_set(i == state_get_current_list() ? tbar_tab_selected : tbar_tab_nselected, NULL);

		if(i == state_get_current_list()) attron(A_BOLD);
		printw(" [%i] %s ", i + 1, state_get_list(i)->name);
		if(i == state_get_current_list()) attroff(A_BOLD);
	}

	move(1, COLS - 3);
	color_set(tbar_default, NULL);
	switch(state_get_repeat()) {
		case rep_none:                        break;
		case rep_one: addstr("\u21BA1");      break;
		case rep_all: addstr("\u21BA\u221E"); break;
	}
}

static size_t tui_track_print_played(size_t remaining, bool selected, enum color color, enum color color_played, enum color color_selected, char *format, ...) {
	if(selected) {
		color        = color_selected;
		color_played = color_selected;
	}

	va_list va;
	va_start(va, format);
	int required_buffer = vsnprintf(NULL, 0, format, va);
	va_end(va);

	va_start(va, format);
	char string[required_buffer + 1];
	vsnprintf(string, required_buffer + 1, format, va);
	va_end(va);

	if(remaining) {
		// print min(remaining, strlen(string)) using 'color_played'
		color_set(color_played, NULL);

		int printed = 0;
		printw("%.*s%n", remaining, string, &printed);
		remaining -= printed;

		if(!remaining) {
			color_set(color, NULL);
			printw("%s", string + printed);
		}
	} else {
		// no 'played' chars remaining, just print the string using 'color'
		color_set(color, NULL);
		printw("%s", string);
	}

	return remaining;
}

static void tui_track_print_line(struct track* entry, bool selected, int line) {
	move(line, 0);
	if(entry->flags & FLAG_PLAYING) {
		color_set(tline_status, NULL);
		addstr("\u25B8");
	} else if(entry->flags & FLAG_PAUSED) {
		color_set(tline_status, NULL);
		attron(A_BOLD);
		addch('=');
		attroff(A_BOLD);
	} else {
		color_set(tline_default, NULL);
		addch(' ');
	}

	int played_chars = 0;
	if(entry->current_position) {
		float rel = (float)entry->current_position / entry->duration;
		played_chars = rel * COLS;
	}

	char date_buffer[256];
	int date_buffer_used = strftime(date_buffer, sizeof(date_buffer), "%Y/%m/%d %H:%M:%S", &entry->created_at);
	played_chars = tui_track_print_played(played_chars, selected, tline_date,    tline_date_played,    tline_date_selected,    " %s",    date_buffer);
	played_chars = tui_track_print_played(played_chars, selected, tline_default, tline_default_played, tline_default_selected, " %s",    entry->name);
	played_chars = tui_track_print_played(played_chars, selected, tline_user,    tline_user_played,    tline_user_selected,    " by %s", entry->username);
	played_chars = tui_track_print_played(played_chars, selected, tline_default, tline_default_played, tline_default_selected, "%0*c",   COLS - wcsps(entry->name) - wcsps(entry->username) - 4 - date_buffer_used - 8 - 11 - 12, ' ');

	if(entry->current_position) {
		char time_buffer[TIME_BUFFER_SIZE];
		int time_len = snprint_ftime(time_buffer, TIME_BUFFER_SIZE, entry->current_position);

		played_chars = tui_track_print_played(played_chars, selected, tline_ctime, tline_ctime_played, tline_ctime_selected, "%0*c%s", 12 - time_len, ' ', time_buffer);
	}
	played_chars = tui_track_print_played(played_chars, selected, tline_default, tline_default_played, tline_default_selected, "%0*c", (entry->current_position ? 0 : 12) + 3, ' ');

	played_chars = tui_track_print_played(played_chars, selected, tline_default, tline_default_played, tline_default_selected, "%s%s%s",
		FLAG_CACHED     & entry->flags ? "C"      : " ",
		FLAG_NEW        & entry->flags ? "\u27EA" : " ",
		FLAG_BOOKMARKED & entry->flags ? "\u2661" : " ");

	char time_buffer[TIME_BUFFER_SIZE];
	int time_len = snprint_ftime(time_buffer, TIME_BUFFER_SIZE, entry->duration);
	tui_track_print_played(played_chars, selected, tline_default, tline_default_played, tline_time_selected, "%0*c%s", 9 - time_len, ' ', time_buffer);
}

static void tui_track_list_print() {
	struct track_list *list = state_get_list(state_get_current_list());

	// just abort if no list was set yet (px. during the initial update of the stream)
	if(!list) {
		return;
	}

	// if a textbox is shown, then do not write over it
	int y = 2;
	for(int i = state_get_current_position(); i < list->count && y < LINES - 2; i++) {
		if( (y < 4 || y > LINES - 4) || !textbox_window.win ) {
			if( y < LINES - SUGGESTION_LIST_HEIGHT - 1 || !suggestion_window) {
				tui_track_print_line(TRACK(list, i), i == state_get_current_selected(), y);
			}
		}
		y++;
	}

	for(; y < LINES - 2; y++) {
		if( (y < 4 || y > LINES - 4) || !textbox_window.win ) {
			if( y < LINES - SUGGESTION_LIST_HEIGHT - 1 || !suggestion_window) {
				move(y, 0);
				clrtoeol();
			}
		}
	}

	refresh();
}

static void tui_print(char *fmt, ...) {
	va_list va;
	va_start(va, fmt);

	char buffer[512];
	vsnprintf(buffer, sizeof(buffer), fmt, va);

	unsigned int i;
	for(i = 0; buffer[i]; i++) {
		switch(buffer[i]) {
			case '\1':
				attron(A_BOLD);
				break;
			case '\2':
				attroff(A_BOLD);
				break;

			default:
				addch(buffer[i]);
				break;
		}
	}

	va_end(va);
}

// TODO !!!
static size_t tui_draw_text(WINDOW *win, char *text, const int max_width) {
	text = strdup(text);

	strcrep(text, '\r', ' ');

	size_t y = 0;
	char *tok = strtok(text, "\n");
	while(tok) {
		while(wcsps(tok) > max_width) {

			// search for any space to avoid breaking within words
			int line_len = max_width;
			for(int i = 0; i < max_width / 8; i++) {
				if(' ' == tok[max_width - 1 - i]) {
					line_len = max_width - i;
					break;
				}
			}

			if(win) {
				mvwprintw(win, y, 2, "%.*s", line_len, tok);
			}
			y++;
			tok += line_len;
		}

		if(win) {
			mvwprintw(win, y, 2, "%s%0*c", tok, max_width - wcsps(tok), ' ');
		}
		y++;
		tok = strtok(NULL, "\n");
	}

	free(text);
	return y;
}

static void tui_update_textbox() {
	const int height = LINES - 8;
	const int width = COLS - 8;

	/* create new textbox if there is none, but a title was supplied */
	if(!textbox_window.win && state_get_tb_title()) {
		const size_t pad_height = tui_draw_text(NULL, state_get_tb_text(), width - 4);

		textbox_window.win = newwin(LINES - 8, width, 4, 4);
		box(textbox_window.win, 0, 0);

		wattron(textbox_window.win, A_BOLD);
		mvwprintw(textbox_window.win, 0, 2, " %s ", state_get_tb_title());
		wattroff(textbox_window.win, A_BOLD);

		textbox_window.pad = newpad(pad_height + LINES, width - 4);

		int start_line = 0;
		tui_draw_text(textbox_window.pad, state_get_tb_text(), width - 10);
		wrefresh(textbox_window.win);
		prefresh(textbox_window.pad, start_line, 0, 5, 5, start_line + LINES - 8 - 1, width - 1);

	} else if(textbox_window.win && !state_get_tb_title()) {
		delwin(textbox_window.pad);
		delwin(textbox_window.win);
		textbox_window.win = NULL;

		touchwin(stdscr);
		refresh();
	} else {
		prefresh(textbox_window.pad, state_get_tb_pos(), 0, 5, 5, height - 1, width - 1);
	}
}

/* tui_submit_* functions

   these functions are the only exported ones, which may be called by different threads (and thus are synchronized).
   tui_init / tui_finalize each may only be called once (at the very beginning and the very end)
*/
void tui_submit_action(enum tui_action_kind _action) {
	sem_wait(&sem_wait_action);
	action = _action;
	sem_post(&sem_have_action);

	// TODO: this blocks everything
	sem_wait(&sem_wait_action);
	sem_post(&sem_wait_action);
}

static void tui_callback_textbox_modifed()    { tui_submit_action(textbox_modified);   }
static void tui_callback_tabbar_modified()    { tui_submit_action(tabbar_modified);    }
static void tui_callback_titlebar_modified()  { tui_submit_action(titlebar_modified);  }
static void tui_callback_statusbar_modified() { tui_submit_action(statusbar_modified); }
static void tui_callback_list_modified()      { tui_submit_action(list_modified);      }
static void tui_callback_sugg_modified()      { tui_submit_action(sugg_modified);      }

/* signal handler, exectued in case of resize of terminal
   to avoid race conditions no drawing is done here */
static void tui_terminal_resized(int signum) {
	assert(SIGWINCH == signum);

	whole_redraw_required = true;
	sem_post(&sem_have_action);
}

bool tui_init() {
	sem_init(&sem_have_action, 0, 0);
	sem_init(&sem_wait_action, 0, 1);

	if(SIG_ERR == signal(SIGWINCH, tui_terminal_resized)) {
		_log("installing sighandler for SIGWINCH failed: %s", strerror(errno));
		_log("resizing terminal will not cause an automatic update of screen");
	}

	initscr();
	curs_set(0);

	cbreak();
	noecho();
	keypad(stdscr, true);

	if(has_colors()) {
		start_color();

		use_default_colors();

		if(can_change_color()) {
			_log("color id in [0; %i]", COLORS);

		#define INIT_COLOR(C, R, G, B) if(ERR == init_color(C, R, G, B)) { _log("init_color("#C", %d, %d, %d) failed", R, G, B); }
			INIT_COLOR(COLOR_GRAY,   280, 280, 280);
			INIT_COLOR(COLOR_DGRAY,  200, 200, 200);
			INIT_COLOR(COLOR_DGREEN, 100, 500, 100);
		} else {
			_log("terminal does not allow changing colors");
		}

		_log("color pair id in [1; %d]", COLOR_PAIRS - 1);

	#define INIT_PAIR(CP, CF, CB) if(ERR == init_pair(CP, CF, CB)) { _log("init_pair("#CP", "#CF", "#CB") failed"); }
		INIT_PAIR(sbar_default,                COLOR_WHITE,  COLOR_BLUE);

		INIT_PAIR(cline_default,               COLOR_WHITE,  COLOR_BLUE);
		INIT_PAIR(cline_cmd_char,              COLOR_RED,    COLOR_BLUE);
		INIT_PAIR(cline_warning,               COLOR_RED,    COLOR_BLUE);

		INIT_PAIR(tbar_default,                COLOR_BLACK,  COLOR_WHITE);

		INIT_PAIR(tbar_tab_selected,           COLOR_WHITE,  COLOR_BLACK);
		INIT_PAIR(tbar_tab_nselected,          COLOR_WHITE,  COLOR_GRAY);

		INIT_PAIR(inp_cursor,                  COLOR_GRAY,   COLOR_GRAY);

		INIT_PAIR(cmdlist_default,             COLOR_WHITE,  COLOR_BLACK);
		INIT_PAIR(cmdlist_selected,            COLOR_WHITE,  COLOR_GRAY);
		INIT_PAIR(cmdlist_desc,                COLOR_GREEN,  COLOR_BLACK);
		INIT_PAIR(cmdlist_desc_selected,       COLOR_GREEN,  COLOR_GRAY);
		INIT_PAIR(cmdlist_descparam,           COLOR_DGRAY,  COLOR_BLACK);
		INIT_PAIR(cmdlist_descparam_selected,  COLOR_DGRAY,  COLOR_GRAY);

		INIT_PAIR(tline_default,               COLOR_WHITE,  COLOR_SHELL);
		INIT_PAIR(tline_default_selected,      COLOR_BLACK,  COLOR_WHITE);
		INIT_PAIR(tline_default_played,        COLOR_WHITE,  COLOR_DGRAY);

		INIT_PAIR(tline_status,                COLOR_RED,    COLOR_SHELL);

		INIT_PAIR(tline_date,                  COLOR_CYAN,   COLOR_SHELL);
		INIT_PAIR(tline_date_played,           COLOR_CYAN,   COLOR_DGRAY);
		INIT_PAIR(tline_date_selected,         COLOR_BLACK,  COLOR_CYAN);

		INIT_PAIR(tline_user,                  COLOR_GREEN,  COLOR_SHELL);
		INIT_PAIR(tline_user_played,           COLOR_GREEN,  COLOR_DGRAY);
		INIT_PAIR(tline_user_selected,         COLOR_DGREEN, COLOR_WHITE);

		INIT_PAIR(tline_ctime,                 COLOR_YELLOW, COLOR_SHELL);
		INIT_PAIR(tline_ctime_played,          COLOR_YELLOW, COLOR_DGRAY);
		INIT_PAIR(tline_ctime_selected,        COLOR_YELLOW, COLOR_WHITE);

		INIT_PAIR(tline_time,                  COLOR_WHITE,  COLOR_SHELL);
		INIT_PAIR(tline_time_played,           COLOR_WHITE,  COLOR_DGRAY);
		INIT_PAIR(tline_time_selected,         COLOR_BLACK,  COLOR_WHITE);
	} else {
		_log("terminal does not support colors at all(!)");
	}

	state_register_callback(cbe_textbox_modified,   tui_callback_textbox_modifed);
	state_register_callback(cbe_repeat_modified,    tui_callback_tabbar_modified);
	state_register_callback(cbe_tabs_modified,      tui_callback_tabbar_modified);
	state_register_callback(cbe_titlebar_modified,  tui_callback_titlebar_modified);
	state_register_callback(cbe_statusbar_modified, tui_callback_statusbar_modified);
	state_register_callback(cbe_list_modified,      tui_callback_list_modified);
	state_register_callback(cbe_sugg_modified,      tui_callback_sugg_modified);

	pthread_create(&thread_tui, NULL, _thread_tui_function, NULL);

	if(atexit(tui_finalize)) {
		_log("atexit: %s", strerror(errno));
	}

	return true;
}

static void tui_finalize() {
	action = none;
	terminate = true;
	sem_post(&sem_have_action);

	pthread_join(thread_tui, NULL);

	delwin(stdscr);
	endwin();
	del_curterm(cur_term);
}
