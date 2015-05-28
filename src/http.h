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

#ifndef _HTTP_H
	#define _HTTP_H

	//\cond
	#include <stddef.h>                     // for size_t
	//\endcond

	struct http_response {
		struct network_conn *nwc; ///< A `new` network-connection (used if the server redirects)
		char  *buffer;            ///< A pointer to the raw buffer (most likely not usefull, primarily for http_response_destroy()
		char  *body;              ///< A pointer to the body (the actual data returned by the server)
		size_t header_length;     ///< The length of the header in Bytes
		size_t content_length;    ///< The length of the body (the actual data)
		int    http_status;       ///< The HTTP-Status, 200 in case of success
		char  *location;          ///< The location in case of a non-resolved redirect
	};

	/** \brief Send an HTTP request to host using nwc and reads the header.
	 *
	 *  http_request_get_only_header() expects `nwc` to be connected to the correct server.
	 *  The parameters `url` and `host` are only used for the request itself, as shown below:
	 *  ~~~
	 *  GET <url> HTTP/1.1\r\n
	 *  Host: <host>\r\n\r\n
	 *  ~~~
	 *
	 *  **Take care**: The returned http_response may contain a different nwc than the original passed one.
	 *  In this case the body needs to be read from the returned http_response::nwc.
	 *
	 *  \param nwc                    The network-connection to use
	 *  \param url                    The URL to request. 
	 *  \param host                   The host.
	 *  \param follow_redirect_steps  The number of redirects allowed (0 = disable rediects).
	 *  \return                       A pointer to an http_response, or NULL in case of error.
	 */
	struct http_response* http_request_get_only_header(struct network_conn *nwc, char *url, char *host, size_t follow_redirect_steps);

	/** \brief Send an HTTP request and read the header along with the body.
	 *
	 *  Behaves similar to http_request_get_only_header(), but with the following differences:
	 *   * follow at max. MAX_REDIRECT_STEPS
	 *   * read *everything* the server sends (header + body)
	 *
	 *  \param nwc   The network-connection to use
	 *  \param url   The URL to request.
	 *  \param host  The host.
	 *  \return      A pointer to an http_response, or NULL in case of error.
	 */
	struct http_response* http_request_get(struct network_conn *nwc, char *url, char *host);

	/** \brief Free any data associated to the http_response
	 *
	 *  This function only frees data, it does **not** free the connection `nwc` itself.
	 *
	 *  \param resp  The http_response to free
	 */
	void http_response_destroy(struct http_response *resp);

#endif /* _HTTP_H */
