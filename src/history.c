/**************************************************************************
 *   history.c  --  This file is part of GNU nano.                        *
 *                                                                        *
 *   Copyright (C) 2003-2011, 2013-2026 Free Software Foundation, Inc.    *
 *   Copyright (C) 2016, 2017, 2019, 2025 Benno Schulenberg               *
 *                                                                        *
 *   GNU nano is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published    *
 *   by the Free Software Foundation, either version 3 of the License,    *
 *   or (at your option) any later version.                               *
 *                                                                        *
 *   GNU nano is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see https://gnu.org/licenses/.     *
 *                                                                        *
 **************************************************************************/

#include "prototypes.h"

#ifdef ENABLE_HISTORIES

#include <errno.h>
#include <string.h>

#ifndef SEARCH_HISTORY
#define SEARCH_HISTORY  "search_history"
#endif

#ifndef POSITION_HISTORY
#define POSITION_HISTORY  "filepos_history"
#endif

static bool history_changed = FALSE;
		/* Whether any of the history lists has changed. */
static char *registername = NULL;
		/* The name of the positions-register file. */
static time_t latest_timestamp = 942927132;
		/* The last time the positions-register file was written. */
static positionstruct *positions_register = NULL;
		/* A list of recently opened files with their last cursor position. */

/* Initialize the lists of historical search and replace strings
 * and the list of historical executed commands. */
void history_init(void)
{
	search_history = make_new_node(NULL);
	search_history->data = copy_of("");
	searchtop = search_history;
	searchbot = search_history;

	replace_history = make_new_node(NULL);
	replace_history->data = copy_of("");
	replacetop = replace_history;
	replacebot = replace_history;

	execute_history = make_new_node(NULL);
	execute_history->data = copy_of("");
	executetop = execute_history;
	executebot = execute_history;
}

/* Reset the pointer into the history list that contains item to the bottom. */
void reset_history_pointer_for(const linestruct *item)
{
	if (item == search_history)
		search_history = searchbot;
	else if (item == replace_history)
		replace_history = replacebot;
	else if (item == execute_history)
		execute_history = executebot;
}

/* Return from the history list that starts at start and ends at end
 * the first node that contains the first len characters of the given
 * text, or NULL if there is no such node. */
linestruct *find_in_history(const linestruct *start, const linestruct *end,
		const char *text, size_t len)
{
	const linestruct *item;

	for (item = start; item != end->prev && item != NULL; item = item->prev) {
		if (strncmp(item->data, text, len) == 0)
			return (linestruct *)item;
	}

	return NULL;
}

/* Update a history list (the one in which item is the current position)
 * with a fresh string text.  That is: add text, or move it to the end. */
void update_history(linestruct **item, const char *text, bool avoid_duplicates)
{
	linestruct **htop = NULL, **hbot = NULL;
	linestruct *thesame = NULL;

	if (*item == search_history) {
		htop = &searchtop;
		hbot = &searchbot;
	} else if (*item == replace_history) {
		htop = &replacetop;
		hbot = &replacebot;
	} else if (*item == execute_history) {
		htop = &executetop;
		hbot = &executebot;
	}

	/* When requested, check if the string is already in the history. */
	if (avoid_duplicates)
		thesame = find_in_history(*hbot, *htop, text, HIGHEST_POSITIVE);

	/* If an identical string was found, delete that item. */
	if (thesame) {
		linestruct *after = thesame->next;

		/* If the string is at the head of the list, move the head. */
		if (thesame == *htop)
			*htop = after;

		unlink_node(thesame);
		renumber_from(after);
	}

	/* If the history is full, delete the oldest item (the one at the
	 * head of the list), to make room for a new item at the end. */
	if ((*hbot)->lineno == MAX_SEARCH_HISTORY + 1) {
		linestruct *oldest = *htop;

		*htop = (*htop)->next;
		unlink_node(oldest);
		renumber_from(*htop);
	}

	/* Store the fresh string in the last item, then create a new item. */
	(*hbot)->data = mallocstrcpy((*hbot)->data, text);
	splice_node(*hbot, make_new_node(*hbot));
	*hbot = (*hbot)->next;
	(*hbot)->data = copy_of("");

	/* Indicate that the history needs to be saved on exit. */
	history_changed = TRUE;

	/* Set the current position in the list to the bottom. */
	*item = *hbot;
}

#ifdef ENABLE_TABCOMP
/* Go backward through one of three history lists, starting at item *here,
 * searching for a string that is a tab completion of the given string,
 * looking at only its first len characters.  When found, make *here point
 * at the item and return its string; otherwise, just return the string. */
char *get_history_completion(linestruct **here, char *string, size_t len)
{
	linestruct *htop = NULL, *hbot = NULL;
	linestruct *item;

	if (*here == search_history) {
		htop = searchtop;
		hbot = searchbot;
	} else if (*here == replace_history) {
		htop = replacetop;
		hbot = replacebot;
	} else if (*here == execute_history) {
		htop = executetop;
		hbot = executebot;
	}

	/* First search from the current position to the top of the list
	 * for a match of len characters.  Skip over an exact match. */
	item = find_in_history((*here)->prev, htop, string, len);

	while (item != NULL && strcmp(item->data, string) == 0)
		item = find_in_history(item->prev, htop, string, len);

	if (item) {
		*here = item;
		return mallocstrcpy(string, item->data);
	}

	/* Now search from the bottom of the list to the original position. */
	item = find_in_history(hbot, *here, string, len);

	while (item != NULL && strcmp(item->data, string) == 0)
		item = find_in_history(item->prev, *here, string, len);

	if (item) {
		*here = item;
		return mallocstrcpy(string, item->data);
	}

	/* When no useful match was found, simply return the given string. */
	return (char *)string;
}
#endif /* ENABLE_TABCOMP */

/* Check whether we have or could make a directory for history files. */
bool have_statedir(void)
{
	const char *xdgdatadir;
	struct stat dirinfo;

	get_homedir();

	if (homedir != NULL) {
		statedir = concatenate(homedir, "/.nano/");

		if (stat(statedir, &dirinfo) == 0 && S_ISDIR(dirinfo.st_mode)) {
			registername = concatenate(statedir, POSITION_HISTORY);
			return TRUE;
		}
	}

	free(statedir);
	xdgdatadir = getenv("XDG_DATA_HOME");

	if (homedir == NULL && xdgdatadir == NULL)
		return FALSE;

	if (xdgdatadir != NULL)
		statedir = concatenate(xdgdatadir, "/nano/");
	else
		statedir = concatenate(homedir, "/.local/share/nano/");

	if (stat(statedir, &dirinfo) == -1) {
		if (xdgdatadir == NULL) {
			char *statepath = concatenate(homedir, "/.local");
			mkdir(statepath, S_IRWXU);
			free(statepath);
			statepath = concatenate(homedir, "/.local/share");
			mkdir(statepath, S_IRWXU);
			free(statepath);
		}
		if (mkdir(statedir, S_IRWXU) == -1) {
			jot_error(N_("Unable to create directory %s: %s\n"
								"It is required for saving/loading "
								"search history or cursor positions.\n"),
								statedir, strerror(errno));
			return FALSE;
		}
	} else if (!S_ISDIR(dirinfo.st_mode)) {
		jot_error(N_("Path %s is not a directory and needs to be.\n"
								"Nano will be unable to load or save "
								"search history or cursor positions.\n"),
								statedir);
		return FALSE;
	}

	registername = concatenate(statedir, POSITION_HISTORY);
	return TRUE;
}

/* Load the histories for Search, Replace With, and Execute Command. */
void load_history(void)
{
	char *historyname = concatenate(statedir, SEARCH_HISTORY);
	FILE *histories = fopen(historyname, "rb");

	/* If reading an existing file failed, don't save history when we quit. */
	if (histories == NULL && errno != ENOENT) {
		jot_error(N_("Error reading %s: %s"), historyname, strerror(errno));
		UNSET(HISTORYLOG);
	}

	if (histories == NULL) {
		free(historyname);
		return;
	}

	linestruct **list = &search_history;
	char *stanza = NULL;
	size_t dummy = 0;
	ssize_t read;

	/* Load the three history lists (first search, then replace, then execute)
	 * from oldest entry to newest.  Between two lists there is an empty line. */
	while ((read = getline(&stanza, &dummy, histories)) > 0) {
		stanza[--read] = '\0';
		if (read > 0) {
			recode_NUL_to_LF(stanza, read);
			update_history(list, stanza, IGNORE_DUPLICATES);
		} else if (list == &search_history)
			list = &replace_history;
		else
			list = &execute_history;
	}

	if (fclose(histories) == EOF)
		jot_error(N_("Error reading %s: %s"), historyname, strerror(errno));

	free(historyname);
	free(stanza);

	/* Reading in the lists has marked them as changed; undo this side effect. */
	history_changed = FALSE;
}

/* Write the lines of a history list, starting at head, from oldest to newest,
 * to the given file.  Return TRUE if writing succeeded, and FALSE otherwise. */
bool write_list(const linestruct *head, FILE *histories)
{
	const linestruct *item;

	for (item = head; item != NULL; item = item->next) {
		/* Decode 0x0A bytes as embedded NULs. */
		size_t length = recode_LF_to_NUL(item->data);

		if (fwrite(item->data, 1, length, histories) < length)
			return FALSE;
		if (putc('\n', histories) == EOF)
			return FALSE;
	}

	return TRUE;
}

/* Save the histories for Search, Replace With, and Execute Command. */
void save_history(void)
{
	/* If the histories are unchanged, don't bother saving them. */
	if (!history_changed)
		return;

	char *historyname = concatenate(statedir, SEARCH_HISTORY);
	FILE *histories = fopen(historyname, "wb");

	if (histories == NULL) {
		jot_error(N_("Error writing %s: %s"), historyname, strerror(errno));
		free(historyname);
		return;
	}

	/* Don't allow others to read or write the history file. */
	if (chmod(historyname, S_IRUSR | S_IWUSR) < 0)
		jot_error(N_("Cannot limit permissions on %s: %s"), historyname, strerror(errno));

	if (!write_list(searchtop, histories) || !write_list(replacetop, histories) ||
											!write_list(executetop, histories))
		jot_error(N_("Error writing %s: %s"), historyname, strerror(errno));

	if (fclose(histories) == EOF)
		jot_error(N_("Error writing %s: %s"), historyname, strerror(errno));

	free(historyname);
}

/* Return as a string... the line numbers of the lines with an anchor. */
char *stringify_anchors(void)
{
	char *string = copy_of("");
#ifndef NANO_TINY
	char number[24];

	for (linestruct *line = openfile->filetop; line != NULL; line = line->next)
		if (line->has_anchor) {
			sprintf(number, "%zi ", line->lineno);
			string = nrealloc(string, strlen(string) + strlen(number) + 1);
			strcat(string, number);
		}
#endif
	return string;
}

/* Set an anchor for each line number in the given string. */
void restore_anchors(char *string)
{
#ifndef NANO_TINY
	linestruct *line = openfile->filetop;
	ssize_t number;
	char *space;

	while (*string) {
		if ((space = strchr(string, ' ')) == NULL)
			return;
		*space = '\0';
		number = atoi(string);
		string = space + 1;

		while (line->lineno < number)
			if ((line = line->next) == NULL)
				return;

		line->has_anchor = TRUE;
	}
#endif
}

/* Load the recorded cursor positions for files that were opened. */
void load_positions_register(void)
{
	FILE *registry = fopen(registername, "rb");

	/* If reading an existing file failed, don't save the register when we quit. */
	if (registry == NULL && errno != ENOENT) {
		jot_error(N_("Error reading %s: %s"), registername, strerror(errno));
		UNSET(POSITIONLOG);
	}

	if (registry == NULL)
		return;

	positionstruct *lastitem = NULL;
	positionstruct *newitem;
	char *stanza, *lineptr, *columnptr;
	char *phrase = NULL;
	struct stat fileinfo;
	size_t dummy = 0;
	int count = 0;
	ssize_t length;

	/* Read and parse each line, and store the extracted data. */
	while (count++ < 200 && (length = getline(&phrase, &dummy, registry)) > 1) {
		stanza = strchr(phrase, '/');
		length -= (stanza ? stanza - phrase : 0);

		/* Decode NULs as embedded newlines. */
		recode_NUL_to_LF(stanza, length);

		/* Find the spaces before column number and line number. */
		columnptr = revstrstr(stanza, " ", stanza + length - 3);
		if (columnptr == NULL)
			continue;
		lineptr = revstrstr(stanza, " ", columnptr - 2);
		if (lineptr == NULL)
			continue;

		/* Now separate the three elements of the line. */
		*(columnptr++) = '\0';
		*(lineptr++) = '\0';

		/* Create a new position record. */
		newitem = nmalloc(sizeof(positionstruct));
		newitem->filename = copy_of(stanza);
		newitem->linenumber = atoi(lineptr);
		newitem->columnnumber = atoi(columnptr);
		newitem->anchors = (phrase == stanza) ? NULL : measured_copy(phrase, stanza - phrase);
		newitem->next = NULL;

		/* Add the record to the list. */
		if (positions_register == NULL)
			positions_register = newitem;
		else
			lastitem->next = newitem;

		lastitem = newitem;
	}

	if (fclose(registry) == EOF)
		jot_error(N_("Error reading %s: %s"), registername, strerror(errno));

	free(phrase);

	if (stat(registername, &fileinfo) == 0)
		latest_timestamp = fileinfo.st_mtime;
}

/* Save the recorded cursor positions for files that were opened. */
void save_positions_register(void)
{
	FILE *registry = fopen(registername, "wb");
	struct stat fileinfo;
	positionstruct *item;
	int count = 0;

	if (registry == NULL) {
		jot_error(N_("Error writing %s: %s"), registername, strerror(errno));
		return;
	}

	/* Don't allow others to read or write the positions-register file. */
	if (chmod(registername, S_IRUSR | S_IWUSR) < 0)
		jot_error(N_("Cannot limit permissions on %s: %s"), registername, strerror(errno));

	for (item = positions_register; item != NULL && count++ < 200; item = item->next) {
		char *path_and_place;
		size_t length = (item->anchors == NULL) ? 0 : strlen(item->anchors);

		/* First write the string of line numbers with anchors, if any. */
		if (length && fwrite(item->anchors, 1, length, registry) < length)
			jot_error(N_("Error writing %s: %s"), registername, strerror(errno));

		/* Assume 20 decimal positions each for line and column number,
		 * plus two spaces, plus the line feed, plus the null byte. */
		path_and_place = nmalloc(strlen(item->filename) + 44);
		sprintf(path_and_place, "%s %zd %zd\n",
								item->filename, item->linenumber, item->columnnumber);

		/* Encode newlines in filenames as NULs. */
		length = recode_LF_to_NUL(path_and_place);
		/* Restore the terminating newline. */
		path_and_place[length - 1] = '\n';

		if (fwrite(path_and_place, 1, length, registry) < length)
			jot_error(N_("Error writing %s: %s"), registername, strerror(errno));

		free(path_and_place);
	}

	if (fclose(registry) == EOF)
		jot_error(N_("Error writing %s: %s"), registername, strerror(errno));

	if (stat(registername, &fileinfo) == 0)
		latest_timestamp = fileinfo.st_mtime;
}

/* Reload the positions-register file if it has been modified since last load. */
void reload_positions_if_needed(void)
{
	positionstruct *item, *nextone;
	struct stat fileinfo;

	if (stat(registername, &fileinfo) != 0 || fileinfo.st_mtime == latest_timestamp)
		return;

	for (item = positions_register; item != NULL; item = nextone) {
		nextone = item->next;
		free(item->filename);
		free(item->anchors);
		free(item);
	}

	positions_register = NULL;

	load_positions_register();
}

/* Update the recorded last file positions with the current position in the
 * current buffer.  If no existing entry is found, add a new one at the top. */
void update_positions_register(void)
{
	char *fullpath = get_full_path(openfile->filename);
	positionstruct *previous = NULL;
	positionstruct *item;

	if (fullpath == NULL)
		return;

	reload_positions_if_needed();

	/* Look for a matching filename in the list. */
	for (item = positions_register; item != NULL; item = item->next) {
		if (!strcmp(item->filename, fullpath))
			break;
		previous = item;
	}

	/* If no match was found, make a new node; otherwise, unlink the match. */
	if (item == NULL) {
		item = nmalloc(sizeof(positionstruct));
		item->filename = copy_of(fullpath);
		item->anchors = NULL;
	} else if (previous)
		previous->next = item->next;

	/* Place the found or new node at the beginning, if not already there. */
	if (item != positions_register) {
		item->next = positions_register;
		positions_register = item;
	}

	/* Record the last cursor position and any anchors. */
	item->linenumber = openfile->current->lineno;
	item->columnnumber = xplustabs() + 1;
	free(item->anchors);
	item->anchors = stringify_anchors();

	free(fullpath);

	save_positions_register();
}

/* Check whether the current filename matches an entry in the list of
 * recorded positions.  If yes, restore the relevant cursor position. */
void restore_cursor_position_if_any(void)
{
	char *fullpath = get_full_path(openfile->filename);
	positionstruct *item;

	if (fullpath == NULL)
		return;

	reload_positions_if_needed();

	item = positions_register;
	while (item != NULL && strcmp(item->filename, fullpath) != 0)
		item = item->next;

	free(fullpath);

	if (item && item->anchors)
		restore_anchors(item->anchors);
	if (item)
		goto_line_and_column(item->linenumber, item->columnnumber, TRUE);
}
#endif /* ENABLE_HISTORIES */
