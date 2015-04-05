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

/** \file plain.h
 *  \brief Exported functions required for handling plain TCP/IP connections.
 */

#ifndef _PLAIN_H
	#define _PLAIN_H
	#include "network.h"

	/** \brief Connect a plain TCP/IP socket to server:port.
	 *
	 *  \param server  The server to connect to, either an IP or a hostname.
	 *  \param port    The servers port (\f$ 0 \le port \le 65536 \f$)
	 *  \return        Pointer to a network_conn struct, or NULL in case of an error
	 */
	struct network_conn* plain_connect(char *server, int port);

#endif /* _PLAIN_H */
