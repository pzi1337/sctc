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

	#include "_hard_config.h"

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

	#define URL_COUNT_UNINITIALIZED ( (size_t) -1 )

	#define TRACK(LST, ID) (LST->entries[ID].name ? &LST->entries[ID] : LST->entries[ID].href)

	/** \brief The basic datastructure representing a single track
	 *
	 *  SCTC knows two different kind of tracks:
	 *   1. **The `normal track`**
	 *
	 *      The normal track behaves just like one would expect.
	 *      All the entries are filled with (valid) values (except href).
	 *
	 *   2. **The `reference track`**
	 *
	 *      The reference track, in contrast, is very different:
	 *      As soon as name contains NULL a track is a reference track.
	 *      Therefore the only usable value is href, a pointer to a normal track containing
	 *      the actual data.
	 *      \warning For a reference track, you must not try to access any members, except from
	 *               `name` (which is `NULL`) and `href` (a valid ptr to a normal track)
	 */
	struct track {
		char   *name; ///< the tracks name
		union {
			struct {
				char   *stream_url;    ///< The URL used for streaming this track

				/** The URL to the permanent location of this track.
				 *
				 *  This URL cannot be used for streaming without further actions.
				 */
				char   *permalink_url;

				char   *download_url; ///< The URL pointing to the download. Might be NULL, if the uploader does not provide a download.
				char   *username; ///< The username
				char   *description;
				time_t created_at;
				int    duration; ///< duration in seconds
				int    user_id;
				int    track_id;
				char **urls;
				size_t url_count;

				/* these members are used for handling the playlist, they are not part of the data received from sc.com */
				uint8_t flags;
				unsigned int current_position; ///< current position
			};
			struct track *href; ///< Pointer to a normal track (only valid if `name == NULL`)
		};
	};

	struct track_list {
		char   *name;
		size_t count;
		struct track *entries;
	};

	struct track_list* track_list_create(char *name) ATTR(nonnull);

	/** \brief Add a single track to an existing track_list
	 *
	 *  \param list   The list receiving the new track
	 *  \param track  The track, which will be added to list
	 */
	bool track_list_add(struct track_list *list, struct track *track);

	/** \brief Merge an array of track_lists
	 *
	 *  The resulting list is not sorted, entries from all lists are simply concatinated.
	 *  Both input lists are not modified, call track_list_destroy() if they are no longer needed.
	 *  The struct tracks within the list are duplicated, the struct tracks' members are not - set free_trackdata to false!
	 *
	 *  \param lists  A NULL-terminated array containing the track_lists to be merged into one
	 *  \return a new list containing all tracks all lists
	 */
	struct track_list* track_list_merge(struct track_list **lists);

	/** \brief Sort a track_list by creation time
	 *
	 *  \param list  The list to be sorted
	 */
	void track_list_sort(struct track_list *list);

	/** \brief Append a list to another list
	 *
	 *  \todo remove in favour of track_list_merge()
	 */
	bool track_list_append(struct track_list *target, struct track_list *source);

	/** \brief Check if list contains a track, identified by its permalink and return it
	 *
	 *  \param list       The list to search in
	 *  \param permalink  The permalink to use for searching
	 *  \return           The track, if found, `NULL` otherwise
	 */
	struct track* track_list_get(struct track_list *list, char *permalink);

	/** \brief Free the memory occupied by a track_list.
	 *
	 *  Do not use the list after calling this function. Do not use any of the tracks within the list if `free_trackdata` is set to true.
	 *
	 *  \param list            the list to free
	 *  \param free_trackdata  if true every single track within the list will be freed too
	 */
	void track_list_destroy(struct track_list *list, bool free_trackdata);

	void track_list_href_to(struct track_list *list, struct track_list *target);
	bool track_list_del(struct track_list *list, size_t track_id);
	void track_destroy(struct track *track);

#ifndef NDEBUG
	void track_list_dump_mem_usage(struct track_list *list);
#endif
#endif /* _TRACK_H */
