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


/** \file track.h
 *  \brief Definitions of the most important structures `track` and `track_list`
 */

#ifndef _TRACK_H
	#define _TRACK_H

	//\cond
	#include <stdbool.h>
	#include <stdint.h>
	#include <time.h>
	//\endcond

	/// this track is currently being played
	#define FLAG_PLAYING    1

	/// this track is currently pause
	#define FLAG_PAUSED     2

	/// this track is bookmarked
	#define FLAG_BOOKMARKED 4

	/// this track is 'new'
	#define FLAG_NEW        8

	/// this track is within the cache
	#define FLAG_CACHED    16

	struct track {
		char   *name;          ///< the tracks name
		char   *stream_url;    ///< The URL used for streaming this track

		/** The URL to the permanent location of this track.
		 *
		 *  This URL cannot be used for streaming without further actions.
		 */
		char   *permalink_url;

		char   *download_url; ///< The URL pointing to the download. Might be NULL, if the uploader does not provide a download.
		char   *username; ///< The username
		char   *description;
		struct tm created_at;
		int    duration; ///< duration in seconds
		int    bpm;      ///< beats per minute
		int    user_id;
		int    track_id;

		/* these members are used for handling the playlist, they are not part of the data received from sc.com */
		uint8_t flags;
		int     current_position; // current position
	};

	struct track_list {
		char   *name;
		size_t count;
		size_t position;
		size_t selected;
		struct track *entries;
	};

	// TODO: put to location it belongs to (where?)
	enum repeat {rep_none, rep_one, rep_all};

	bool track_list_add(struct track_list *list, struct track *track);

	/** \brief Merge two track_lists
	 *
	 *  The resulting list is not sorted, entries from list1 and list2 are simply concatinated.
	 *  Both input lists are not modified, call track_list_destroy() if they are no longer needed.
	 *  The struct tracks within the list are duplicated, the struct tracks' members are not - set free_trackdata to false!
	 *
	 *  \param list1  The first list to use for merging
	 *  \param list2  The second list to use for merging
	 *  \return a new list containing all tracks from list1 and list2
	 */
	struct track_list* track_list_merge(struct track_list *list1, struct track_list *list2);


	bool track_list_append(struct track_list *target, struct track_list *source);

	/**
	 *
	 */
	bool track_list_contains(struct track_list *list, char *permalink);

	/** \brief Free the memory occupied by a track_list.
	 *
	 *  Do not use the list after calling this function. Do not use any of the tracks within the list if `free_trackdata` is set to true.
	 *
	 *  \param list            the list to free
	 *  \param free_trackdata  if true every single track within the list will be freed too
	 */
	void track_list_destroy(struct track_list *list, bool free_trackdata);
#endif /* _TRACK_H */
