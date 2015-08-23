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

#include "../_hard_config.h"
#include "global.h"

//\cond
#include <stdbool.h>                    // for bool, false, true
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint8_t
#include <stdio.h>                      // for sscanf
#include <string.h>
//\endcond

#include "../helper.h"                  // for smprintf, streq, strstrp, etc
#include "../log.h"                     // for _log
#include "../sound.h"                   // for sound_change_volume, etc
#include "../state.h"                   // for state_set_repeat, etc
#include "../track.h"                   // for track, track_list, TRACK, etc
#include "../tui.h"                     // for tui_submit_action, F_BOLD, etc
#include "../_hard_config.h"            // for UNUSED

#define NO_TRACK ((size_t) ~0)

// TODO
static void update_flags_stop_playback(struct track_list *list, size_t tid) {
	if(list && NO_TRACK != tid) {
		TRACK(list, tid)->flags &= (uint8_t)~FLAG_PLAYING;
		if(TRACK(list, tid)->current_position) {
			TRACK(list, tid)->flags |= FLAG_PAUSED;
		}
	}
}


/** \brief Stop playback of currently playing track
 *
 *  Nothing happens in case no track is currently playing.
 *  If `reset` is set to `true` the position of the currently plaing
 *  track is reset to 0, if set to `false` the position will not be modified.
 *  The position can be used lateron to continue playing.
 *
 *  \param reset  Reset the current position of playback to 0
 */
static void stop_playback(bool reset) {
	size_t playing = state_get_current_playback_track();

	if(NO_TRACK != playing) {
		if(!sound_stop()) {
			_log("failed to stop playback: %s", sound_error());
		}

		struct track_list *list = state_get_list(state_get_current_playback_list());
		if(reset) {
			TRACK(list, playing)->current_position = 0;
		}
		update_flags_stop_playback(list, playing);

		tui_submit_action(update_list);

		state_set_current_playback(0, ~0);
	}
}

void cmd_pause(const char *unused UNUSED) { stop_playback(false); }
void cmd_stop (const char *unused UNUSED) { stop_playback(true);  }

void cmd_volume(const char *_hint) {
	astrdup(hint, _hint);
	char *delta_str = strstrp(hint);
	int delta;
	if(1 == sscanf(delta_str, " %4i ", &delta)) {
		unsigned int vol = sound_change_volume(delta);
		_log("volume now: %i, delta: %i", vol, delta);

		state_set_volume(vol);
	}
}

void cmd_redraw(const char *unused UNUSED) {
	tui_submit_action(redraw);
}

/** \brief Set repeat to 'rep'
 *
 *  \param rep  The type of repeat to use (one in {none,one,all})
 */
void cmd_repeat(const char *_rep) {
	astrdup(trep, _rep);
	char *rep = strstrp(trep);

	if(streq("", rep)) {
		switch(state_get_repeat()) {
			case rep_none: cmd_repeat("one");  break;
			case rep_one:  cmd_repeat("all");  break;
			case rep_all:  cmd_repeat("none"); break;

			/* no error-handling default case here, `enum repeat` only has 3 values */
			default: break;
		}
	} else {
		if     (streq("none", rep)) state_set_repeat(rep_none);
		else if(streq("one",  rep)) state_set_repeat(rep_one);
		else if(streq("all",  rep)) state_set_repeat(rep_all);
		else {
			state_set_status(cline_warning, smprintf("Error: "F_BOLD"%s"F_RESET" is not in {none,one,all}", rep));
			return;
		}
		state_set_status(cline_default, smprintf("Info: Switched repeat to '%s'", rep));
	}
}


