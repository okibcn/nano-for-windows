/* $Id$ */
/**************************************************************************
 *   search.c                                                             *
 *                                                                        *
 *   Copyright (C) 1999-2004 Chris Allegretta                             *
 *   Copyright (C) 2005-2006 David Lawrence Ramsey                        *
 *   This program is free software; you can redistribute it and/or modify *
 *   it under the terms of the GNU General Public License as published by *
 *   the Free Software Foundation; either version 2, or (at your option)  *
 *   any later version.                                                   *
 *                                                                        *
 *   This program is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 *   GNU General Public License for more details.                         *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program; if not, write to the Free Software          *
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            *
 *                                                                        *
 **************************************************************************/

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

/* Regular expression helper functions */

#ifdef HAVE_REGEX_H
int regexp_init(const char *regexp)
{
    /* Hmm, perhaps we should check for whether regcomp returns successfully */
    if (regcomp(&search_regexp, regexp, (ISSET(CASE_SENSITIVE) ? 0 : REG_ICASE) 
		| REG_EXTENDED) != 0)
	return 0;

    SET(REGEXP_COMPILED);
    return 1;
}

void regexp_cleanup(void)
{
    UNSET(REGEXP_COMPILED);
    regfree(&search_regexp);
}
#endif

void not_found_msg(const char *str)
{
    if (strlen(str) <= COLS / 2)
	statusbar(_("\"%s\" not found"), str);
    else {
	char *foo = mallocstrcpy(NULL, str);

	foo[COLS / 2] = '\0';
	statusbar(_("\"%s...\" not found"), foo);
	free(foo);
    }
}

void search_abort(void)
{
    display_main_list();
    wrefresh(bottomwin);
    if (ISSET(MARK_ISSET))
	edit_refresh_clearok();

#ifdef HAVE_REGEX_H
    if (ISSET(REGEXP_COMPILED))
	regexp_cleanup();
#endif
}

void search_init_globals(void)
{
    if (last_search == NULL) {
	last_search = charalloc(1);
	last_search[0] = '\0';
    }
    if (last_replace == NULL) {
	last_replace = charalloc(1);
	last_replace[0] = '\0';
    }
}

/* Set up the system variables for a search or replace.  Return -1 on
 * abort, 0 on success, and 1 on rerun calling program.  Return -2 to
 * run opposite program (search -> replace, replace -> search).
 *
 * replacing = 1 if we call from do_replace(), 0 if called from
 * do_search(). */
int search_init(int replacing)
{
    int i = 0;
    char *buf;
    static char *backupstring = NULL;
#ifdef HAVE_REGEX_H
    const char *regex_error = _("Invalid regex \"%s\"");
#endif /* HAVE_REGEX_H */

    search_init_globals();

    if (backupstring == NULL)
	backupstring = mallocstrcpy(backupstring, "");

#ifndef NANO_SMALL
    search_history.current = (historytype *)&search_history.next;
#endif

    if (last_search[0] != '\0') {
	buf = charalloc(COLS / 3 + 7);
	/* We use COLS / 3 here because we need to see more on the line */
	sprintf(buf, " [%.*s%s]", COLS / 3, last_search,
		strlen(last_search) > COLS / 3 ? "..." : "");
    } else {
	buf = charalloc(1);
	buf[0] = '\0';
    }

    /* This is now one simple call.  It just does a lot */
    i = statusq(0, replacing ? replace_list : whereis_list, backupstring,
#ifndef NANO_SMALL
	&search_history,
#endif
	"%s%s%s%s%s%s",
	_("Search"),

	/* This string is just a modifier for the search prompt,
	   no grammar is implied */
	ISSET(CASE_SENSITIVE) ? _(" [Case Sensitive]") : "",

	/* This string is just a modifier for the search prompt,
	   no grammar is implied */
	ISSET(USE_REGEXP) ? _(" [Regexp]") : "",

	/* This string is just a modifier for the search prompt,
	   no grammar is implied */
	ISSET(REVERSE_SEARCH) ? _(" [Backwards]") : "",

	replacing ? _(" (to replace)") : "",
	buf);

    /* Release buf now that we don't need it anymore */
    free(buf);

    /* Cancel any search, or just return with no previous search */
    if (i == -1 || (i < 0 && last_search[0] == '\0')) {
	statusbar(_("Search Cancelled"));
	reset_cursor();
	free(backupstring);
	backupstring = NULL;
#ifndef NANO_SMALL
	search_history.current = search_history.next;
#endif
	return -1;
    } else {
	switch (i) {
	case -2:	/* Same string */
#ifdef HAVE_REGEX_H
	    if (ISSET(USE_REGEXP))
		/* If answer is "", use last_search! */
		if (regexp_init(last_search) == 0) {
		    statusbar(regex_error, last_search);
		    reset_cursor();
		    free(backupstring);
		    backupstring = NULL;
		    return -3;
		}
#endif
	    break;
	case 0:		/* They entered something new */
#ifdef HAVE_REGEX_H
	    if (ISSET(USE_REGEXP))
		if (regexp_init(answer) == 0) {
		    statusbar(regex_error, answer);
		    reset_cursor();
		    free(backupstring);
		    backupstring = NULL;
#ifndef NANO_SMALL
		    search_history.current = search_history.next;
#endif
		    return -3;
		}
#endif
	    free(backupstring);
	    backupstring = NULL;
	    last_replace[0] = '\0';
	    break;
#ifndef NANO_SMALL
	case TOGGLE_CASE_KEY:
	    TOGGLE(CASE_SENSITIVE);
	    backupstring = mallocstrcpy(backupstring, answer);
	    return 1;
	case TOGGLE_BACKWARDS_KEY:
	    TOGGLE(REVERSE_SEARCH);
	    backupstring = mallocstrcpy(backupstring, answer);
	    return 1;
#ifdef HAVE_REGEX_H
	case TOGGLE_REGEXP_KEY:
	    TOGGLE(USE_REGEXP);
	    backupstring = mallocstrcpy(backupstring, answer);
	    return 1;
#endif
#endif /* !NANO_SMALL */
	case NANO_OTHERSEARCH_KEY:
	    backupstring = mallocstrcpy(backupstring, answer);
	    return -2;		/* Call the opposite search function */
	case NANO_FROMSEARCHTOGOTO_KEY:
	    free(backupstring);
	    backupstring = NULL;
#ifndef NANO_SMALL
	    search_history.current = search_history.next;
#endif
	    i = (int)strtol(answer, &buf, 10);	/* Just testing answer here */
	    if (!(errno == ERANGE || *answer == '\0' || *buf != '\0'))
		do_gotoline(-1, 0);
	    else
		do_gotoline_void();
	    return -3;
	default:
	    do_early_abort();
	    free(backupstring);
	    backupstring = NULL;
	    return -3;
	}
    }
    return 0;
}

int is_whole_word(int curr_pos, const char *datastr, const char *searchword)
{
    size_t sln = curr_pos + strlen(searchword);

    /* start of line or previous character not a letter and end of line
       or next character not a letter */
    return (curr_pos < 1 || !isalpha((int)datastr[curr_pos - 1])) &&
	(sln == strlen(datastr) || !isalpha((int)datastr[sln]));
}

filestruct *findnextstr(int quiet, int bracket_mode,
			const filestruct *begin, int beginx,
			const char *needle, int no_sameline)
{
    filestruct *fileptr = current;
    const char *searchstr, *rev_start = NULL, *found = NULL;
    int current_x_find = 0;

    search_offscreen = 0;

    if (!ISSET(REVERSE_SEARCH)) {		/* forward search */
	/* Argh, current_x is set to -1 by nano.c:do_int_spell_fix(), and
	 * strlen returns size_t, which is unsigned. */
	assert(current_x < 0 || current_x <= strlen(fileptr->data));
	current_x_find = current_x;
	if (current_x_find < 0 || fileptr->data[current_x_find] != '\0')
	    current_x_find++;

	searchstr = &fileptr->data[current_x_find];

	/* Look for needle in searchstr.  Keep going until we find it
	 * and, if no_sameline is set, until it isn't on the current
	 * line.  If we don't find it, we'll end up at
	 * current[current_x] regardless of whether no_sameline is
	 * set. */
	while ((found = strstrwrapper(searchstr, needle, rev_start, current_x_find)) == NULL || (no_sameline && fileptr == current)) {

	    /* finished processing file, get out */
	    if (search_last_line) {
		if (!quiet)
		    not_found_msg(needle);
		update_line(fileptr, current_x);
	        return NULL;
	    }

	    update_line(fileptr, 0);

	    /* reset current_x_find between lines */
	    current_x_find = 0;

	    fileptr = fileptr->next;

	    if (fileptr == editbot)
		search_offscreen = 1;

	    /* EOF reached?, wrap around once */
	    if (fileptr == NULL) {
		/* don't wrap if looking for bracket match */
		if (bracket_mode)
		    return NULL;
		fileptr = fileage;
		search_offscreen = 1;
		if (!quiet)
		    statusbar(_("Search Wrapped"));
	    }

	    /* Original start line reached */
	    if (fileptr == begin)
		search_last_line = 1;

	    searchstr = fileptr->data;
	}

	/* We found an instance */
	current_x_find = found - fileptr->data;
	/* Ensure we haven't wrapped around again! */
	if (search_last_line && current_x_find > beginx) {
	    if (!quiet)
		not_found_msg(needle);
	    return NULL;
	}
    }
#ifndef NANO_SMALL
    else {	/* reverse search */
	current_x_find = current_x - 1;
	/* Make sure we haven't passed the beginning of the string */
	rev_start = &fileptr->data[current_x_find];
	searchstr = fileptr->data;

	/* Look for needle in searchstr.  Keep going until we find it
	 * and, if no_sameline is set, until it isn't on the current
	 * line.  If we don't find it, we'll end up at
	 * current[current_x] regardless of whether no_sameline is
	 * set. */
	while ((found = strstrwrapper(searchstr, needle, rev_start, current_x_find)) == NULL || (no_sameline && fileptr == current)) {

	    /* finished processing file, get out */
	    if (search_last_line) {
		if (!quiet)
		    not_found_msg(needle);
		return NULL;
	    }

	    update_line(fileptr, 0);

	    /* reset current_x_find between lines */
	    current_x_find = 0;

	    fileptr = fileptr->prev;

	    if (fileptr == edittop->prev)
		search_offscreen = 1;

	    /* SOF reached?, wrap around once */
/* ? */	    if (fileptr == NULL) {
		if (bracket_mode)
		   return NULL;
		fileptr = filebot;
		search_offscreen = 1;
		if (!quiet)
		    statusbar(_("Search Wrapped"));
	    }
	    /* Original start line reached */
	    if (fileptr == begin)
		search_last_line = 1;

	    searchstr = fileptr->data;
	    rev_start = fileptr->data + strlen(fileptr->data);
	}

	/* We found an instance */
	current_x_find = found - fileptr->data;
	/* Ensure we haven't wrapped around again! */
	if ((search_last_line) && (current_x_find < beginx)) {
	    if (!quiet)
		not_found_msg(needle);
	    return NULL;
	}
    }
#endif /* !NANO_SMALL */

    /* Set globals now that we are sure we found something */
    current = fileptr;
    current_x = current_x_find;

    if (!bracket_mode) {
	update_line(current, current_x);
	placewewant = xplustabs();
	reset_cursor();
    }
    return fileptr;
}

/* Search for a string. */
int do_search(void)
{
    int i;
    filestruct *fileptr = current, *didfind;
    int fileptr_x = current_x;

    wrap_reset();
    i = search_init(0);
    switch (i) {
    case -1:
	current = fileptr;
	search_abort();
	return 0;
    case -3:
	search_abort();
	return 0;
    case -2:
	do_replace();
	return 0;
    case 1:
	do_search();
	search_abort();
	return 1;
    }

     /* If answer is now "", copy last_search into answer... */
    if (answer[0] == '\0')
	answer = mallocstrcpy(answer, last_search);
    else
	last_search = mallocstrcpy(last_search, answer);

#ifndef NANO_SMALL
    /* add this search string to the search history list */
    if (answer[0] != '\0')
	update_history(&search_history, answer);
#endif	/* !NANO_SMALL */

    search_last_line = 0;
    didfind = findnextstr(FALSE, FALSE, current, current_x, answer, 0);

    if (fileptr == current && fileptr_x == current_x && didfind != NULL)
	statusbar(_("This is the only occurrence"));
    else if (current->lineno <= edittop->lineno
	|| current->lineno >= editbot->lineno)
        edit_update(current, CENTER);

    search_abort();

    return 1;
}

void replace_abort(void)
{
    /* Identical to search_abort, so we'll call it here.  If it
       does something different later, we can change it back.  For now
       it's just a waste to duplicate code */
    search_abort();
    placewewant = xplustabs();
}

#ifdef HAVE_REGEX_H
int replace_regexp(char *string, int create_flag)
{
    /* Split personality here - if create_flag is NULL, just calculate
     * the size of the replacement line (necessary because of
     * subexpressions like \1 \2 \3 in the replaced text). */

    char *c;
    int new_size = strlen(current->data) + 1;
    int search_match_count = regmatches[0].rm_eo - regmatches[0].rm_so;

    new_size -= search_match_count;

    /* Iterate through the replacement text to handle subexpression
     * replacement using \1, \2, \3, etc. */

    c = last_replace;
    while (*c != '\0') {
	if (*c != '\\') {
	    if (create_flag)
		*string++ = *c;
	    c++;
	    new_size++;
	} else {
	    int num = (int) *(c + 1) - (int) '0';
	    if (num >= 1 && num <= 9) {

		int i = regmatches[num].rm_eo - regmatches[num].rm_so;

		if (num > search_regexp.re_nsub) {
		    /* Ugh, they specified a subexpression that doesn't
		     * exist. */
		    return -1;
		}

		/* Skip over the replacement expression */
		c += 2;

		/* But add the length of the subexpression to new_size */
		new_size += i;

		/* And if create_flag is set, append the result of the
		 * subexpression match to the new line */
		if (create_flag) {
		    strncpy(string, current->data + current_x +
			    regmatches[num].rm_so, i);
		    string += i;
		}

	    } else {
		if (create_flag)
		    *string++ = *c;
		c++;
		new_size++;
	    }
	}
    }

    if (create_flag)
	*string = '\0';

    return new_size;
}
#endif

char *replace_line(void)
{
    char *copy, *tmp;
    int new_line_size;
    int search_match_count;

    /* Calculate size of new line */
#ifdef HAVE_REGEX_H
    if (ISSET(USE_REGEXP)) {
	search_match_count = regmatches[0].rm_eo - regmatches[0].rm_so;
	new_line_size = replace_regexp(NULL, 0);
	/* If they specified an invalid subexpression in the replace
	 * text, return NULL, indicating an error */
	if (new_line_size < 0)
	    return NULL;
    } else {
#else
    {
#endif
	search_match_count = strlen(last_search);
	new_line_size = strlen(current->data) - strlen(last_search) +
	    strlen(last_replace) + 1;
    }

    /* Create buffer */
    copy = charalloc(new_line_size);

    /* Head of Original Line */
    strncpy(copy, current->data, current_x);
    copy[current_x] = '\0';

    /* Replacement Text */
    if (!ISSET(USE_REGEXP))
	strcat(copy, last_replace);
#ifdef HAVE_REGEX_H
    else
	replace_regexp(copy + current_x, 1);
#endif

    /* The tail of the original line */

    /* This may expose other bugs, because it no longer goes through
     * each character in the string and tests for string goodness.  But
     * because we can assume the invariant that current->data is less
     * than current_x + strlen(last_search) long, this should be safe. 
     * Or it will expose bugs ;-) */
    tmp = current->data + current_x + search_match_count;
    strcat(copy, tmp);

    return copy;
}

/* Step through each replace word and prompt user before replacing
 * word.  Return -1 if the string to replace isn't found at all.
 * Otherwise, return the number of replacements made. */
int do_replace_loop(const char *prevanswer, const filestruct *begin,
			int *beginx, int wholewords, int *i)
{
    int replaceall = 0, numreplaced = -1;
#ifdef HAVE_REGEX_H
    /* The starting-line match and bol/eol regex flags. */
    int beginline = 0, bol_eol = 0;
#endif
    filestruct *fileptr = NULL;

    switch (*i) {
	case -1:	/* Aborted enter. */
	    if (last_replace[0] != '\0')
		answer = mallocstrcpy(answer, last_replace);
	    statusbar(_("Replace Cancelled"));
	    replace_abort();
	    return 0;
	case 0:		/* They actually entered something. */
	    break;
	default:
	if (*i != -2) {	/* First page, last page, for example, could
			 * get here. */
	    do_early_abort();
	    replace_abort();
	    return 0;
        }
    }

    last_replace = mallocstrcpy(last_replace, answer);
    while (1) {
	size_t match_len;

	/* Sweet optimization by Rocco here. */
	fileptr = findnextstr(fileptr || replaceall || search_last_line,
		FALSE, begin, *beginx, prevanswer,
#ifdef HAVE_REGEX_H
		/* We should find a bol and/or eol regex only once per
		 * line.  If the bol_eol flag is set, it means that the
		 * last search found one on the beginning line, so we
		 * should skip over the beginning line when doing this
		 * search. */
		bol_eol
#else
		0
#endif
		);

#ifdef HAVE_REGEX_H
	/* If the bol_eol flag is set, we've found a match on the
	 * beginning line already, and we're still on the beginning line
	 * after the search, it means that we've wrapped around, so
	 * we're done. */
	if (bol_eol && beginline && fileptr == begin)
	    fileptr = NULL;
	/* Otherwise, set the beginline flag if we've found a match on
	 * the beginning line, reset the bol_eol flag, and continue. */
	else {
	    if (fileptr == begin)
		beginline = 1;
	    bol_eol = 0;
	}
#endif

	if (current->lineno <= edittop->lineno
	    || current->lineno >= editbot->lineno)
	    edit_update(current, CENTER);

	/* No more matches.  Done! */
	if (fileptr == NULL)
	    break;

	/* Make sure only whole words are found. */
	if (wholewords && !is_whole_word(current_x, fileptr->data, prevanswer))
	    continue;

	/* If we're here, we've found the search string. */
	if (numreplaced == -1)
	    numreplaced = 0;

#ifdef HAVE_REGEX_H
	if (ISSET(USE_REGEXP))
	    match_len = regmatches[0].rm_eo - regmatches[0].rm_so;
    	else
#endif
	    match_len = strlen(prevanswer);

	if (!replaceall) {
	    curs_set(0);
	    do_replace_highlight(TRUE, prevanswer);

	    *i = do_yesno(1, 1, _("Replace this instance?"));

	    do_replace_highlight(FALSE, prevanswer);
	    curs_set(1);

	    if (*i == -1)	/* We canceled the replace. */
		break;
	}

#ifdef HAVE_REGEX_H
	/* Set the bol_eol flag if we're doing a bol and/or eol regex
	 * replace ("^", "$", or "^$"). */
	if (ISSET(USE_REGEXP) && regexec(&search_regexp, prevanswer, 0, NULL, REG_NOTBOL | REG_NOTEOL) == REG_NOMATCH)
	    bol_eol = 1;
#endif

	if (*i > 0 || replaceall) {	/* Yes, replace it!!!! */
	    char *copy;
	    int length_change;

	    if (*i == 2)
		replaceall = 1;

	    copy = replace_line();
	    if (copy == NULL) {
		statusbar(_("Replace failed: unknown subexpression!"));
		replace_abort();
		return 0;
	    }

	    length_change = strlen(copy) - strlen(current->data);

#ifndef NANO_SMALL
	    if (current == mark_beginbuf && mark_beginx > current_x) {
		if (mark_beginx < current_x + match_len)
		    mark_beginx = current_x;
		else
		    mark_beginx += length_change;
	    }
#endif

	    assert(0 <= match_len + length_change);
	    if (current == begin && current_x <= *beginx) {
		if (*beginx < current_x + match_len)
		    *beginx = current_x + match_len;
		*beginx += length_change;
	    }

	    /* Set the cursor at the last character of the replacement
	     * text, so searching will resume after the replacement
	     * text.  Note that current_x might be set to -1 here. */
#ifndef NANO_SMALL
	    if (!ISSET(REVERSE_SEARCH))
#endif
		current_x += match_len + length_change - 1;

	    /* Cleanup. */
	    totsize += length_change;
	    free(current->data);
	    current->data = copy;

	    edit_refresh();
	    set_modified();
	    numreplaced++;
	}
    }

    /* If text has been added to the magicline, make a new magicline. */
    if (filebot->data[0] != '\0')
	new_magicline();

    return numreplaced;
}

/* Replace a string. */
int do_replace(void)
{
    int i, numreplaced, beginx;
    filestruct *begin;
    char *prevanswer = NULL;

    if (ISSET(VIEW_MODE)) {
	print_view_warning();
	replace_abort();
	return 0;
    }

    i = search_init(1);
    switch (i) {
    case -1:
	statusbar(_("Replace Cancelled"));
	replace_abort();
	return 0;
    case 1:
	do_replace();
	return 1;
    case -2:
	do_search();
	return 0;
    case -3:
	replace_abort();
	return 0;
    }

#ifndef NANO_SMALL
    if (answer[0] != '\0')
	update_history(&search_history, answer);
#endif	/* !NANO_SMALL */

    /* If answer is now == "", copy last_search into answer 
	(and prevanswer)...  */
    if (answer[0] == '\0')
	answer = mallocstrcpy(answer, last_search);
    else
	last_search = mallocstrcpy(last_search, answer);

    prevanswer = mallocstrcpy(prevanswer, last_search);

#ifndef NANO_SMALL
    replace_history.current = (historytype *)&replace_history.next;
    last_replace = mallocstrcpy(last_replace, "");
#endif

    i = statusq(0, replace_list_2, last_replace,
#ifndef NANO_SMALL
		&replace_history,
#endif
		_("Replace with"));

#ifndef NANO_SMALL
    if (i == 0 && answer[0] != '\0')
	update_history(&replace_history, answer);
#endif	/* !NANO_SMALL */

    begin = current;
    beginx = current_x;
    search_last_line = 0;

    numreplaced = do_replace_loop(prevanswer, begin, &beginx, FALSE, &i);

    /* restore where we were */
    current = begin;
    current_x = beginx;
    renumber_all();
    edit_update(current, CENTER);

    if (numreplaced >= 0)
	statusbar(P_("Replaced %d occurrence", "Replaced %d occurrences",
		numreplaced), numreplaced);
    else
	not_found_msg(prevanswer);

    free(prevanswer);
    replace_abort();
    return 1;
}

int do_gotoline(int line, int save_pos)
{
    static char *linestr = NULL;

    linestr = mallocstrcpy(linestr, answer);

    if (line <= 0) {		/* Ask for it */
	int st = statusq(FALSE, goto_list, line != 0 ? linestr : "",
#ifndef NANO_SMALL
			NULL,
#endif
			_("Enter line number"));

	/* Cancel, or Enter with blank string. */
	if (st == -1 || st == -2)
	    statusbar(_("Aborted"));
	if (st != 0) {
	    display_main_list();
	    return 0;
	}

	line = atoi(answer);

	/* Bounds check */
	if (line <= 0) {
	    statusbar(_("Come on, be reasonable"));
	    display_main_list();
	    return 0;
	}
    }

    for (current = fileage; current->next != NULL && line > 1; line--)
	current = current->next;

    current_x = 0;

    /* if save_pos is nonzero, don't change the cursor position when
       updating the edit window */
    if (save_pos)
    	edit_update(current, NONE);
    else
	edit_update(current, CENTER);
    placewewant = 0;
    display_main_list();
    return 1;
}

int do_gotoline_void(void)
{
    return do_gotoline(0, 0);
}

#if defined(ENABLE_MULTIBUFFER) || !defined(DISABLE_SPELLER)
void do_gotopos(int line, int pos_x, int pos_y, int pos_placewewant)
{
    /* since do_gotoline() resets the x-coordinate but not the
       y-coordinate, set the coordinates up this way */
    current_y = pos_y;
    do_gotoline(line, 1);

    /* make sure that the x-coordinate is sane here */
    if (pos_x > strlen(current->data))
	pos_x = strlen(current->data);

    /* set the rest of the coordinates up */
    current_x = pos_x;
    placewewant = pos_placewewant;
    update_line(current, pos_x);
}
#endif

#if !defined(NANO_SMALL) && defined(HAVE_REGEX_H)
int do_find_bracket(void)
{
    char ch_under_cursor, wanted_ch;
    const char *pos, *brackets = "([{<>}])";
    char regexp_pat[] = "[  ]";
    int offset, have_search_offscreen = 0, flagsave, current_x_save, count = 1;
    filestruct *current_save;

    ch_under_cursor = current->data[current_x];

/*    if ((!(pos = strchr(brackets, ch_under_cursor))) || (!((offset = pos - brackets) < 8))) { */

    if (((pos = strchr(brackets, ch_under_cursor)) == NULL) || (((offset = pos - brackets) < 8) == 0)) {
	statusbar(_("Not a bracket"));
	return 1;
    }

    blank_statusbar_refresh();

    wanted_ch = *(brackets + ((strlen(brackets) - (offset + 1))));

    current_x_save = current_x;
    current_save = current;
    flagsave = flags;
    SET(USE_REGEXP);

/* apparent near redundancy with regexp_pat[] here is needed, [][] works, [[]] doesn't */

    if (offset < (strlen(brackets) / 2)) {			/* on a left bracket */
	regexp_pat[1] = wanted_ch;
	regexp_pat[2] = ch_under_cursor;
	UNSET(REVERSE_SEARCH);
    } else {							/* on a right bracket */
	regexp_pat[1] = ch_under_cursor;
	regexp_pat[2] = wanted_ch;
	SET(REVERSE_SEARCH);
    }

    regexp_init(regexp_pat);

    while (1) {
	search_last_line = 0;
	if (findnextstr(1, 1, current, current_x, regexp_pat, 0) != NULL) {
	    have_search_offscreen |= search_offscreen;

	    /* found identical bracket */
	    if (current->data[current_x] == ch_under_cursor)
		count++;
	    else {

		/* found complementary bracket */
		if (!(--count)) {
		    if (have_search_offscreen)
			edit_update(current, CENTER);
		    else
			update_line(current, current_x);
		    placewewant = xplustabs();
		    reset_cursor();
		    break;
		}
	    }
	} else {

	    /* didn't find either left or right bracket */
	    statusbar(_("No matching bracket"));
	    current_x = current_x_save;
	    current = current_save;
	    update_line(current, current_x);
	    break;
	}
    }

    if (ISSET(REGEXP_COMPILED))
	regexp_cleanup();
    flags = flagsave;
    return 0;
}
#endif

#ifndef NANO_SMALL
/*
 * search and replace history list support functions
 */

/* initialize search and replace history lists */
void history_init(void)
{
    search_history.next = (historytype *)&search_history.prev;
    search_history.prev = NULL;
    search_history.tail = (historytype *)&search_history.next;
    search_history.current = search_history.next;
    search_history.count = 0;
    search_history.len = 0;

    replace_history.next = (historytype *)&replace_history.prev;
    replace_history.prev = NULL;
    replace_history.tail = (historytype *)&replace_history.next;
    replace_history.current = replace_history.next;
    replace_history.count = 0;
    replace_history.len = 0;
}

/* find first node containing string *s in history list *h */
historytype *find_node(historytype *h, char *s)
{
    for (; h->next != NULL; h = h->next)
	if (strcmp(s, h->data) == 0)
	    return h;
    return NULL;
}

/* remove node *r */
void remove_node(historytype *r)
{
    r->prev->next = r->next;
    r->next->prev = r->prev;
    free(r->data);
    free(r);
}

/* add a node after node *h */
void insert_node(historytype *h, const char *s)
{
    historytype *a;

    a = (historytype *)nmalloc(sizeof(historytype));
    a->next = h->next;
    a->prev = h->next->prev;
    h->next->prev = a;
    h->next = a;
    a->data = mallocstrcpy(NULL, s);
}

/* update history list */
void update_history(historyheadtype *h, char *s)
{
    historytype *p;

    if ((p = find_node(h->next, s)) != NULL) {
	if (p == h->next)		/* catch delete and re-insert of
					   same string in 1st node */
	    goto up_hs;
	remove_node(p);			/* delete identical older string */
	h->count--;
    }
    if (h->count == MAX_SEARCH_HISTORY) {	/* list 'full', delete oldest */
	remove_node(h->tail);
	h->count--;
    }
    insert_node((historytype *)h, s);
    h->count++;
    SET(HISTORY_CHANGED);
  up_hs:
    h->current = h->next;
}

/* return a pointer to either the next older history or NULL if no more */
char *get_history_older(historyheadtype *h)
{
    if (h->current->next != NULL) {	/* any older entries? */
	h->current = h->current->next;	/* yes */
	return h->current->data;	/* return it */
    }
    return NULL;			/* end of list */
}

char *get_history_newer(historyheadtype *h)
{
    if (h->current->prev != NULL) {
	h->current = h->current->prev;
	if (h->current->prev != NULL)
	    return h->current->data;
    }
    return NULL;
}

/* get a completion */
char *get_history_completion(historyheadtype *h, char *s)
{
    historytype *p;

    for (p = h->current->next; p->next != NULL; p = p->next) {
	if (strncmp(s, p->data, h->len) == 0 && strlen(p->data) != h->len) {
	    h->current = p;
	    return p->data;
	}
    }
    h->current = (historytype*)h;
    null_at(&s, h->len);
    return s;
}

/* free a history list */
void free_history(historyheadtype *h)
{
    historytype *p, *n;

    for (p = h->next; (n = p->next); p = n)
	remove_node(p);
}

/* end of history support functions */
#endif /* !NANO_SMALL */
