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

#ifndef _SOUND_H
	#define _SOUND_H

	//\cond
	#include <stdbool.h>                    // for bool
	#include <sys/types.h>
	//\endcond
	#include "track.h"

	/** \brief Global initialization of sound.
	 *
	 *  This function is required to be called prior to the first call sound_play() and does several internal
	 *  initializations.
	 *
	 *  \param time_callback  callback function receiving the elapsed seconds as parameter
	 *  \return               true in case of success, false otherwise
	 */
	bool sound_init(void (*time_callback)(int));


	/** \brief Start playback of track
	 *
	 *  This function is required to be called prior to the first call sound_play() and does several internal
	 *  initializations.
	 *
	 *  \param track       the track to play
	 *  \return            true in case of success, false otherwise
	 */
	bool sound_play(struct track *track);

	void sound_seek(unsigned int pos);

	int sound_change_volume(off_t delta);

	/** \brief Stop playback of current track
	 *
	 *  \return true in case of success, false otherwise
	 */
	bool sound_stop(void);

#endif /* _SOUND_H */
