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

#define _XOPEN_SOURCE 500

//\cond
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
//\endcond

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include "track.h"
#include "log.h"
#include "helper.h"

#define MY_ENCODING "utf-8"

static void write_xspf_track(xmlTextWriterPtr writer, struct track *track) {
	xmlTextWriterStartElement(writer, BAD_CAST "track");

	xmlTextWriterWriteFormatElement(writer, BAD_CAST "title",      "%s", track->name);
	xmlTextWriterWriteFormatElement(writer, BAD_CAST "location",   "%s", track->stream_url);
	xmlTextWriterWriteFormatElement(writer, BAD_CAST "creator",    "%s", track->username);
	xmlTextWriterWriteFormatElement(writer, BAD_CAST "identifier", "%s", track->permalink_url);

	xmlTextWriterWriteFormatElement(writer, BAD_CAST "annotation", "%s", track->description);

	xmlTextWriterWriteFormatElement(writer, BAD_CAST "duration",   "%i", track->duration);

	/* use meta-tags to save 'bpm' (beats per minute) and date of creation */
	
	xmlTextWriterStartElement(writer, BAD_CAST "meta");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "rel", BAD_CAST "http://scc.narbo.de/bpm");
	xmlTextWriterWriteFormatString(writer, "%u", track->bpm);
	xmlTextWriterEndElement(writer); // end bpm

	xmlTextWriterStartElement(writer, BAD_CAST "meta");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "rel", BAD_CAST "http://scc.narbo.de/user_id");
	xmlTextWriterWriteFormatString(writer, "%u", track->user_id);
	xmlTextWriterEndElement(writer); // end user_id

	xmlTextWriterStartElement(writer, BAD_CAST "meta");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "rel", BAD_CAST "http://scc.narbo.de/track_id");
	xmlTextWriterWriteFormatString(writer, "%u", track->track_id);
	xmlTextWriterEndElement(writer); // end track_id

	char time_buffer[256];
	strftime(time_buffer, sizeof(time_buffer), "%Y/%m/%d %H:%M:%S %z", &track->created_at);
	xmlTextWriterStartElement(writer, BAD_CAST "meta");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "rel", BAD_CAST "http://scc.narbo.de/created_at");
	xmlTextWriterWriteFormatString(writer, "%s", time_buffer);
	xmlTextWriterEndElement(writer); // end creation time

	xmlTextWriterEndElement(writer); // end track
}

bool xspf_write(char *file, struct track_list *list) {
	xmlTextWriterPtr writer = xmlNewTextWriterFilename(file, false);

	xmlTextWriterStartDocument(writer, NULL, MY_ENCODING, NULL);

	xmlTextWriterStartElement(writer, BAD_CAST "playlist");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "version", BAD_CAST "1");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "xmlns", BAD_CAST "http://xspf.org/ns/0/");

	xmlTextWriterStartElement(writer, BAD_CAST "trackList");

	for(int i = 0; i < list->count; i++) {
		write_xspf_track(writer, &list->entries[i]);
	}

	xmlTextWriterEndElement(writer); // end trackList
	xmlTextWriterEndElement(writer); // end playlist

	xmlTextWriterEndDocument(writer);
	xmlFreeTextWriter(writer);

	xmlCleanupCharEncodingHandlers();
	xmlCleanupParser();

	return true;
}

static struct track* xspf_read_track(xmlNode *node) {
	struct track* track = lcalloc(1, sizeof(struct track));
	if(!track) return NULL;

	for(xmlNode *sub = node->children; sub; sub = sub->next) {
		if(sub->children && xmlStrEqual(BAD_CAST "title",      sub->name)) track->name          = lstrdup((const char*) sub->children->content);
		if(sub->children && xmlStrEqual(BAD_CAST "location",   sub->name)) track->stream_url    = lstrdup((const char*) sub->children->content);
		if(sub->children && xmlStrEqual(BAD_CAST "creator",    sub->name)) track->username      = lstrdup((const char*) sub->children->content);
		if(sub->children && xmlStrEqual(BAD_CAST "identifier", sub->name)) track->permalink_url = lstrdup((const char*) sub->children->content);
		if(sub->children && xmlStrEqual(BAD_CAST "annotation", sub->name)) track->description   = lstrdup((const char*) sub->children->content);
		if(sub->children && xmlStrEqual(BAD_CAST "duration",   sub->name)) track->duration      = atoi((const char*) sub->children->content);

		if(sub->children && xmlStrEqual(BAD_CAST "meta", sub->name)) {
			xmlChar *relProp = xmlGetProp(sub, BAD_CAST "rel");
			if(sub->children && xmlStrEqual(BAD_CAST "http://scc.narbo.de/bpm", relProp))
				track->bpm = atoi((char*) sub->children->content);

			if(sub->children && xmlStrEqual(BAD_CAST "http://scc.narbo.de/track_id", relProp))
				track->track_id = atoi((char*) sub->children->content);

			if(sub->children && xmlStrEqual(BAD_CAST "http://scc.narbo.de/user_id", relProp))
				track->user_id = atoi((char*) sub->children->content);

			if(sub->children && xmlStrEqual(BAD_CAST "http://scc.narbo.de/created_at", relProp)) {
				strptime((const char *) sub->children->content, "%Y/%m/%d %H:%M:%S %z", &track->created_at);
			}
			xmlFree(relProp);
		}
	}

	return track;
}

struct track_list* xspf_read(char *file) {
	//_log("reading file '%s'", file);

	xmlDocPtr doc = xmlReadFile(file, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET);
	struct track_list *list = lcalloc(1, sizeof(struct track_list));
	if(!list) return NULL;

	if(!doc) {
		xmlErrorPtr error = xmlGetLastError();
		_log("reading '%s' failed: %s", file, error->message);

		if(XML_IO_LOAD_ERROR != error->code) {
			doc = xmlRecoverFile(file);
		}
	}

	if(doc) {
		xmlNode *playlist_node = xmlDocGetRootElement(doc)->children->children;

		for(xmlNode *node = playlist_node; node; node = node->next) {
			assert(!strcmp("track", (char *) node->name));

			struct track* track = xspf_read_track(node);
			track_list_add(list, track);
			free(track);
		}

		xmlFreeDoc(doc);
	} else {
		_log("failed to open '%s'", file);
		//exit(EXIT_FAILURE);
	}

	xmlCleanupParser();

	return list;
}


