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

/** \file network.h
 *  \brief Basic networking interface
 *
 *  This header only provides a basic networking interface, which should be used by every implementation of networking.
 *  By doing so any new method of networking can be integrated easily and without further modification of the most other code.
 */

#ifndef _NETWORK_H
	#define _NETWORK_H
	//\cond
	#include <stdarg.h>
	#include <stdbool.h> /* bool */
	#include <stdlib.h>
	//\endcond

	/** A struct containing all the information required to send/recv data from a remote server.\n
	 *  This struct is used to hide the actual implementation used for communication, such as a 'normal' TCP connection (see plain.c)
	 *  or a encrypted TCP connection (see tls.c).
	 *
	 *  Conceptually only the functions supplied by network_conn may be used for communication.
	 *  **Directly calling a implementation-specific function, such as plain_send() / tls_send() is an error and might lead to an abort.**
	 */
	struct network_conn {
		void *mdata; ///< Pointer to data to be used by the implemention handling the actual connection

		/// Send data (buffer_len Bytes) from buffer
		bool  (*send)      (struct network_conn *nwc, char *buffer, size_t buffer_len);

		/// Send formatted data (string; see 'man 3 printf')
		bool  (*send_fmt)  (struct network_conn *nwc, char *fmt, ...);

		/// Recv data (at max. buffer_len Bytes) into buffer
		int   (*recv)      (struct network_conn *nwc, char *buffer, size_t buffer_len);

		/** Recv a single byte */
		int   (*recv_byte) (struct network_conn *nwc);

		/** Return a descriptive message for a previously error
		 * the returned value may NOT be freed as it is handled internally
		 */
		char* (*get_error_str) (struct network_conn *nwc);

		/** Close the connection to the remote server
		 * a call to disconnect frees the nwc-struct, consequently it may no longer
		 * be accessed at all */
		void  (*disconnect)(struct network_conn *nwc);
	};
#endif /* _NETWORK_H */
