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

#ifndef _URL_H
	#define _URL_H

	struct url {
		char *scheme; /// the scheme, currently 'only' "http" and "https"
		char *user;   /// the user
		char *pass;   /// the corresponding password
		char *host;   /// the host
		int port;     /// the port
		char *request; /// the request itself, without scheme://[user[:pass]@]host[:port]

		struct network_conn *nwc;
	};

	struct url* url_parse_string(char *str);
	bool url_connect(struct url *u);
	void url_destroy(struct url *u);

#endif /* _URL_H */
