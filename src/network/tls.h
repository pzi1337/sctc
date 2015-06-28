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

/** \file tls.h
 *  \brief Exported functions required for handling encrypted (TLS) TCP/IP connections.
 */

#ifndef _TLS_H
	#define _TLS_H
	//\cond
	#include <stdbool.h>
	//\endcond

	/** \brief Global initialization of TLS.
	 *
	 *  This function is required to be called prior to the first call tls_connect().
	 *  Global initialization, such as loading the list of trusted CAs is performed here.
	 *
	 *  If the initialization failes - that is `false` is returned - the TLS module is
	 *  not initialized correctly and may not be used!
	 *
	 *  \return `true` in case of success, `false` otherwise
	 */
	bool tls_init();

	/** \brief Connect an encrypted TCP/IP socket to server:port.
	 *
	 *  \param server  The server to connect to, either an IP or a hostname.
	 *  \param port    The servers port (0 <= port <= 65536)
	 *  \return        Pointer to a network_conn struct, or NULL in case of an error
	 */
	struct network_conn* tls_connect(char *server, int port);
#endif /* _TLS_H */
