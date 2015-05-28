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

#define _GNU_SOURCE

#include "_hard_config.h"

//#define NDEBUG

/** \mainpage SCTC - the soundcloud.com terminal client
 *
 *  \section intro_sec Introduction
 *
 *  This documentation documents the internals of SCTC.\n
 *  As STSC is not a library this documentation is not usefull if you do not want to
 *  modify / extend / bugfix SCTC.
 *
 *  \warning SCTC is, at this point, in a very early stage.
 *  Even if things seem to work fine there is no guarantee it behaves the way you want it to behave.\n
 *  Still a huge amount of work is required to achieve clean code ;)
 *
 *  \todo Fix the \\todos and the TODOs
 *
 *  \copyright Copyright 2015  Christian Eichler. All rights reserved.\n
 *  This project is released under the GNU General Public License version 3.
 */

//\cond
#include <ctype.h>                      // for isdigit
#include <locale.h>                     // for setlocale, LC_CTYPE
#include <stdbool.h>                    // for false, bool, true
#include <stddef.h>                     // for NULL, size_t
#include <stdlib.h>                     // for EXIT_FAILURE
#include <string.h>                     // for strcmp
#include <time.h>                       // for timespec, clock_gettime
//\endcond

#include <ncurses.h>                    // for getch

#include "cache.h"                      // for cache_track_exists
#include "command.h"                    // for command_func_ptr
#include "config.h"                     // for config_get_subscribe_count, etc
#include "helper.h"                     // for smprintf, snprint_ftime
#include "jspf.h"                       // for jspf_read
#include "log.h"                        // for _log, log_init
#include "network/tls.h"                // for tls_connect, tls_init
#include "sound.h"                      // for sound_init, sound_play
#include "soundcloud.h"                 // for soundcloud_get_entries
#include "state.h"                      // for LIST_STREAM, etc
#include "track.h"                      // for track, track_list, etc
#include "tui.h"                        // for tui_submit_action, F_BOLD, etc

#define TIME_BUFFER_SIZE 64

static bool param_is_offline = false;

/** \brief Update the current playback time.
 *
 *  If time equals -1, then the next track is selected (if any) and playback initiated. (\todo not yet working!)
 *
 *  \param time  The time in seconds to print to the screen
 */
void tui_update_time(int time) {
	struct track_list *list = state_get_list(state_get_current_playback_list());
	size_t playing = state_get_current_playback_track();

	if(-1 != time) {
		list->entries[playing].current_position = time;

		state_set_current_time(time);
		tui_submit_action(set_sbar_time);

		tui_submit_action(update_list);
	} else {
		list->entries[playing].current_position = 0;
		list->entries[playing].flags &= ~(FLAG_PAUSED | FLAG_PLAYING);

		switch(state_get_repeat()) {
			case rep_none:
				playing++;
				if(playing >= list->count) {
					playing = ~0;
					return;
				}
				break;

			case rep_one:
				break;

			case rep_all: {
				playing++;
				if(playing >= list->count) {
					playing = 0;
				}
				break;
			}
		}

		list->entries[playing].flags = (list->entries[playing].flags & ~FLAG_PAUSED) | FLAG_PLAYING;

		char time_buffer[TIME_BUFFER_SIZE];
		snprint_ftime(time_buffer, TIME_BUFFER_SIZE, list->entries[playing].duration);

		state_set_title(smprintf("Now playing "F_BOLD"%s"F_RESET" by "F_BOLD"%s"F_RESET" (%s)", list->entries[playing].name, list->entries[playing].username, time_buffer));

		state_set_current_playback(state_get_current_playback_list(), playing);
		tui_submit_action(update_list);
		sound_play(&list->entries[playing]);
	}
}

int main(int argc, char **argv) {
	log_init("sctc.log");

	for(size_t i = 1; i < argc; i++) {
		if(!strcmp("--offline", argv[i])) {
			param_is_offline = true;
			_log("switching to offline-mode due to parameter");
		}
	}

	setlocale(LC_CTYPE, "C-UTF-8");

	state_init();
	config_init();
	tls_init();
	tui_init();
	sound_init(tui_update_time);

	state_set_status(cline_default, "");
	state_set_title("");

	struct track_list *lists[4] = {NULL};
	lists[LIST_BOOKMARKS] = jspf_read(BOOKMARK_FILE);
	lists[LIST_BOOKMARKS]->name = "Bookmarks";

	lists[LIST_STREAM] = soundcloud_get_stream();
	lists[LIST_STREAM]->name = "Stream";

	state_set_lists(lists);

	// TODO: speed
	BENCH_START(SB)
	for(size_t i = 0; i < lists[LIST_STREAM]->count; i++) {
		if(track_list_contains(lists[LIST_BOOKMARKS], lists[LIST_STREAM]->entries[i].permalink_url)) {
			_log("'%s' is bookmarked!", lists[LIST_STREAM]->entries[i].name);
			lists[LIST_STREAM]->entries[i].flags |= FLAG_BOOKMARKED;
		}

		if(cache_track_exists(&lists[LIST_STREAM]->entries[i])) {
			lists[LIST_STREAM]->entries[i].flags |= FLAG_CACHED;
		}
	}
	BENCH_STOP(SB, "Searching for bookmarks")

	state_set_lists(lists);

	// send new list to tui-thread
	state_set_current_list(LIST_STREAM);
	state_set_status(cline_default, smprintf("Info: "F_BOLD"%i elements"F_RESET" in %i subscriptions from soundcloud.com", lists[LIST_STREAM]->count, config_get_subscribe_count()));

	int c;
	while( (c = getch()) ) {
		if(isdigit(c) && '0' != c) {
			switch_to_list(c - '1');
			continue;
		}

		command_func_ptr func = config_get_function(c);
		if(func) {
			func((char*)config_get_param(c));
		} else {
			state_set_status(cline_warning, smprintf("Error: got non-mapped keycode %x", c));
		}
	}

	// never reached, exit called in case of 'q'
	return EXIT_FAILURE;
}
