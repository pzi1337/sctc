#ifndef _CURSES_HELPER_H
	#define _CURSES_HELPER_H

	#include <ncurses.h>
	#include "../tui.h"

	#define F_BOLD       "\1"
	#define F_UNDERLINE  "\2"
	#define F_RESET      "\3"
	#define F_COLOR      "\4"

	#define wcsps(S) mbstowcs(NULL, S, 0)

	void hc_print(WINDOW *win, char *format, ...);
	void hc_print_va(WINDOW *win, char *format, va_list va);
	void hc_print_mv(WINDOW *win, int x, int y, char *format, ...);
	size_t hc_print_2c_va(size_t chars_c1, enum color color1, enum color color2, char *format, va_list va);
	WINDOW* hc_pad_print(char *text, const unsigned int max_width);
#endif /* _CURSES_HELPER_H */
