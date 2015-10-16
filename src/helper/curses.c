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

#include "curses.h"

//\cond
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
//\endcond

#include "../helper.h"
#include "../log.h"

size_t hc_print_2c_va(size_t chars_c1, enum color color1, enum color color2, char *format, va_list va) {

	char buffer[512];
	vsnprintf(buffer, sizeof(buffer), format, va);

	int printed = 0;
	if(chars_c1) {
		// print min(remaining, strlen(string)) using 'color1'
		color_set(color1, NULL);
		printw("%.*s%n", chars_c1, buffer, &printed);
		chars_c1 -= printed;
	}

	// no chars for 'color1' remaining, just print the string using 'color2'
	if(!chars_c1) {
		color_set(color2, NULL);
		printw("%s", buffer + printed);
	}

	return chars_c1;
}

static size_t next_control_char(char *string) {
	size_t idx;
	for(idx = 0; string[idx]; idx++) {
		if(F_BOLD[0]      == string[idx]
		|| F_UNDERLINE[0] == string[idx]
		|| F_RESET[0]     == string[idx]
		|| F_COLOR[0]     == string[idx]) {
			return idx;
		}
	}
	return idx;
}

void hc_print_va(WINDOW *win, char *format, va_list va) {

	char buffer[512];
	vsnprintf(buffer, sizeof(buffer), format, va);

	char* buf = buffer;

	size_t cc_pos = next_control_char(buf);
	while('\0' != buf[cc_pos]) {
		char cc = buf[cc_pos];

		buf[cc_pos] = '\0';
		wprintw(win, "%s", buf);

		if(F_BOLD[0] == cc) {
			wattron(win, A_BOLD);
		} else if(F_UNDERLINE[0] == cc) {
			wattron(win, A_UNDERLINE);
		} else if(F_RESET[0] == cc) {
			wattroff(win, A_BOLD);
			wattroff(win, A_UNDERLINE);
		} else if(F_COLOR[0] == cc) {
			wcolor_set(win, va_arg(va, enum color), NULL);
		}

		buf = &buf[cc_pos + 1];
		cc_pos = next_control_char(buf);
	}

	wprintw(win, "%s", buf);
}

void hc_print(WINDOW *win, char *format, ...) {
	va_list va;
	va_start(va, format);

	hc_print_va(win, format, va);

	va_end(va);
}

void hc_print_mv(WINDOW *win, int x, int y, char *format, ...) {
	va_list va;
	va_start(va, format);

	wmove(win, y, x);
	hc_print_va(win, format, va);

	va_end(va);
}

WINDOW* hc_pad_print(char *text, const unsigned int max_width) {
	text = strdup(text);
	strcrep(text, '\r', ' ');

	size_t pad_height = 1;
	WINDOW *pad = newpad(pad_height, max_width);

	size_t y = 0;
	char *tok = strtok(text, "\n");
	while(tok) {
		// split the current line, if it is too long
		while(wcsps(tok) > max_width) {
			// search for any space to avoid breaking within words
			int line_len = max_width;
			for(size_t i = 0; i < max_width / 8; i++) {
				if(' ' == tok[max_width - 1 - i]) {
					line_len = max_width - i;
					break;
				}
			}

			hc_print_mv(pad, 2, y, "%.*s", line_len, tok);
			wresize(pad, ++pad_height, max_width);

			y++;
			tok += line_len;
		}

		hc_print_mv(pad, 2, y, "%s%0*c", tok, max_width - wcsps(tok), ' ');
		wresize(pad, ++pad_height, max_width);

		y++;
		tok = strtok(NULL, "\n");
	}

	free(text);
	return pad;
}

