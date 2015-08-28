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

#ifndef _COMMANDS_GLOBAL_H
	#define _COMMANDS_GLOBAL_H

	#include "../_hard_config.h"

	void cmd_gl_volume(const char *_hint) ATTR(nonnull);
	void cmd_gl_pause(const char *unused UNUSED);
	void cmd_gl_stop(const char *unused UNUSED);

	/** \brief Issue a redraw of the whole screen
	 */
	void cmd_gl_redraw(const char *unused UNUSED);

	/** \brief Set repeat to 'rep'
	 *
	 *  \param rep  The type of repeat to use (one in {none,one,all})
	 */
	void cmd_gl_repeat(const char *_rep);
#endif
