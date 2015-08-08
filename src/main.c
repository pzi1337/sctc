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
#include <dirent.h>
#include <locale.h>                     // for setlocale, LC_CTYPE
#include <stddef.h>                     // for NULL, size_t
#include <stdlib.h>                     // for EXIT_FAILURE
#include <string.h>
#include <time.h>                       // for timespec, clock_gettime
//\endcond

#include <ncurses.h>                    // for getch

#include "cache.h"                      // for cache_track_exists
#include "command.h"                    // for command_func_ptr
#include "config.h"                     // for config_get_subscribe_count, etc
#include "downloader.h"
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

/** \brief Update the current playback time.
 *
 *  If time equals -1, then the next track is selected (if any) and playback initiated. (\todo not yet working!)
 *
 *  \param time  The time in seconds to print to the screen
 */
static void tui_update_time(int time) {
	struct track_list *list = state_get_list(state_get_current_playback_list());
	size_t playing = state_get_current_playback_track();

	if(-1 != time) {
		TRACK(list, playing)->current_position = time;

		state_set_current_time(time);
		tui_submit_action(set_sbar_time);

		tui_submit_action(update_list);
	} else {
		_log("playback of current track done, switching to next one...");
		TRACK(list, playing)->current_position = 0;
		TRACK(list, playing)->flags &= ~(FLAG_PAUSED | FLAG_PLAYING);

		// select track based on `repeat` state
		enum repeat rep = state_get_repeat();
		if(rep_one != rep) {
			playing++;
			if(playing >= list->count) {
				if(rep_none == rep) return;
				playing = 0;
			}
		}

		TRACK(list, playing)->flags = (TRACK(list, playing)->flags & ~FLAG_PAUSED) | FLAG_PLAYING;

		char time_buffer[TIME_BUFFER_SIZE];
		snprint_ftime(time_buffer, TIME_BUFFER_SIZE, TRACK(list, playing)->duration);

		state_set_title(smprintf("Now playing "F_BOLD"%s"F_RESET" by "F_BOLD"%s"F_RESET" (%s)", TRACK(list, playing)->name, TRACK(list, playing)->username, time_buffer));

		state_set_current_playback(state_get_current_playback_list(), playing);
		tui_submit_action(update_list);
		sound_play(TRACK(list, playing));
	}
}

int main(int argc UNUSED, char **argv UNUSED) {
	// initialize the modules
	log_init("sctc.log");

	setlocale(LC_CTYPE, "C-UTF-8");

	state_init();
	config_init();
	tls_init();
	tui_init();
	downloader_init();
	sound_init(tui_update_time);

	// start drawing on screen
	state_set_status(cline_default, "");
	state_set_title("");

	struct track_list *list_stream = soundcloud_get_stream();
	list_stream->name = strdup("Stream");
	state_add_list(list_stream);

#ifndef NDEBUG
	track_list_dump_mem_usage(list_stream);
#endif

	struct track_list *list_bookmark = jspf_read(BOOKMARK_FILE);
	list_bookmark->name = strdup("Bookmarks");
	track_list_href_to(list_bookmark, list_stream);

	BENCH_START(SB)
	for(size_t i = 0; i < list_stream->count; i++) {
		struct track *btrack = track_list_get(list_bookmark, list_stream->entries[i].permalink_url);
		if(NULL != btrack) list_stream->entries[i].flags |= FLAG_BOOKMARKED;

		if(cache_track_exists(&list_stream->entries[i])) {
			list_stream->entries[i].flags |= FLAG_CACHED;
		}
	}
	BENCH_STOP(SB, "Searching for bookmarks")

	state_add_list(list_bookmark);

	char *cache_path = config_get_cache_path();
	char userlist_folder[strlen(cache_path) + 1 + strlen(USERLIST_FOLDER) + 1];
	sprintf(userlist_folder, "%s/"USERLIST_FOLDER"/", cache_path);

	DIR *d = opendir(userlist_folder);
	struct dirent *e;
	while( (e = readdir(d)) ) {
		if(strcmp(".", e->d_name) && strcmp("..", e->d_name)) {
			char userlist_file[strlen(userlist_folder) + 1 + strlen(e->d_name)];
			sprintf(userlist_file, "%s/%s", userlist_folder, e->d_name);

			struct track_list *list = jspf_read(userlist_file);

			e->d_name[strlen(e->d_name) - 5] = '\0';
			list->name = strdup(e->d_name);

			track_list_href_to(list, list_stream);
			state_add_list(list);
		}
	}
	closedir(d);

	// send new list to tui-thread
	state_set_current_list(LIST_STREAM);
	state_set_status(cline_default, smprintf("Info: "F_BOLD"%zu elements"F_RESET" in %zu subscriptions from soundcloud.com", list_stream->count, config_get_subscribe_count()));

	// and enter the main `message` loop
	int c;
	while( (c = getch()) ) {
		// then try to read the
		// corresponding function from configuration and
		// execute it
		command_func_ptr func = config_get_function(c);
		if(func) func(config_get_param(c));
	}

	// never reached, exit called in case of 'cmd_exit'
	return EXIT_FAILURE;
}
