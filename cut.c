/* $Id$ */
/**************************************************************************
 *   cut.c                                                                *
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
#include <stdio.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

static int marked_cut;		/* Is the cutbuffer from a mark? */

#ifndef NANO_SMALL
static int concatenate_cut;	/* Should we add this cut string to the
				   end of the last one? */
#endif

static filestruct *cutbottom = NULL;
				/* Pointer to end of cutbuffer */

filestruct *get_cutbottom(void)
{
    return cutbottom;
}

void add_to_cutbuffer(filestruct *inptr)
{
#ifdef DEBUG
    fprintf(stderr, "add_to_cutbuffer() called with inptr->data = %s\n",
	    inptr->data);
#endif

    if (cutbuffer == NULL) {
	cutbuffer = inptr;
	inptr->prev = NULL;
#ifndef NANO_SMALL
    } else if (concatenate_cut && !ISSET(JUSTIFY_MODE)) {
	/* Just tack the text in inptr onto the text in cutbottom,
	   unless we're backing up lines while justifying text. */
	cutbottom->data = charealloc(cutbottom->data,
		strlen(cutbottom->data) + strlen(inptr->data) + 1);
	strcat(cutbottom->data, inptr->data);
	return;
#endif
    } else {
	cutbottom->next = inptr;
	inptr->prev = cutbottom;
    }

    inptr->next = NULL;
    cutbottom = inptr;
}

#ifndef NANO_SMALL
/* Cut a marked segment instead of a whole line.
 *
 * The first cut character is top->data[top_x].  Unless top == bot, the
 * last cut line has length bot_x.  That is, if bot_x > 0 then we cut to
 * bot->data[bot_x - 1].
 *
 * destructive is whether to actually modify the file structure, if not
 * then just copy the buffer into cutbuffer and don't pull it from the
 * file.
 *
 * If destructive, then we maintain totsize, totlines, filebot, the
 * magic line, and line numbers.  Also, we set current and current_x so
 * the cursor will be on the first character after what was cut.  We do
 * not do any screen updates. */
void cut_marked_segment(filestruct *top, size_t top_x, filestruct *bot,
			size_t bot_x, int destructive)
{
    filestruct *tmp, *next;
    size_t newsize;

    if (top == bot && top_x == bot_x)
	return;
    assert(top != NULL && bot != NULL);

    /* Make top be no later than bot. */
    if (top->lineno > bot->lineno) {
	filestruct *swap = top;
	int swap2 = top_x;

	top = bot;
	bot = swap;

	top_x = bot_x;
	bot_x = swap2;
    } else if (top == bot && top_x > bot_x) {
	/* And bot_x can't be an earlier character than top_x. */
	int swap = top_x;

	top_x = bot_x;
	bot_x = swap;
    }

    /* Make the first cut line manually. */
    tmp = copy_node(top);
    newsize = (top == bot ? bot_x - top_x : strlen(top->data + top_x));
    memmove(tmp->data, top->data + top_x, newsize);
    null_at(&tmp->data, newsize);
    add_to_cutbuffer(tmp);

    /* And make the remainder line manually too. */
    if (destructive) {
	current_x = top_x;
	totsize -= newsize;
	totlines -= bot->lineno - top->lineno;

	newsize = top_x + strlen(bot->data + bot_x) + 1;
	if (top == bot) {
	    /* In this case, the remainder line is shorter, so we must
	       move text from the end forward first. */
	    memmove(top->data + top_x, bot->data + bot_x,
			newsize - top_x);
	    top->data = charealloc(top->data, newsize);
	} else {
	    totsize -= bot_x + 1;

	    /* Here, the remainder line might get longer, so we
	       realloc() it first. */
	    top->data = charealloc(top->data, newsize);
	    memmove(top->data + top_x, bot->data + bot_x,
			newsize - top_x);
	}
    }

    if (top == bot) {
#ifdef DEBUG
	dump_buffer(cutbuffer);
#endif
	return;
    }

    tmp = top->next;
    while (tmp != bot) {
	next = tmp->next;
	if (!destructive)
	    tmp = copy_node(tmp);
	else
	    totsize -= strlen(tmp->data) + 1;
	add_to_cutbuffer(tmp);
	tmp = next;
    }

    /* Make the last cut line manually. */
    tmp = copy_node(bot);
    null_at(&tmp->data, bot_x);
    add_to_cutbuffer(tmp);
#ifdef DEBUG
    dump_buffer(cutbuffer);
#endif

    if (destructive) {
	top->next = bot->next;
	if (top->next != NULL)
	    top->next->prev = top;
	delete_node(bot);
	renumber(top);
	current = top;
	if (bot == filebot) {
	    filebot = top;
	    assert(bot_x == 0);
	    if (top_x > 0)
		new_magicline();
	}
    }
#ifdef DEBUG
    dump_buffer(cutbuffer);
#endif
}
#endif

int do_cut_text(void)
{
    filestruct *fileptr;
#ifndef NANO_SMALL
    int dontupdate = 0;
#endif

    assert(current != NULL && current->data != NULL);

    check_statblank();

    if (!ISSET(KEEP_CUTBUFFER)) {
	free_filestruct(cutbuffer);
	cutbuffer = NULL;
	marked_cut = 0;
#ifndef NANO_SMALL
	concatenate_cut = 0;
#endif
#ifdef DEBUG
	fprintf(stderr, "Blew away cutbuffer =)\n");
#endif
    }

    /* You can't cut the magic line except with the mark.  But
       trying does clear the cutbuffer if KEEP_CUTBUFFER is not set. */
    if (current == filebot
#ifndef NANO_SMALL
			&& !ISSET(MARK_ISSET)
#endif
						)
	return 0;

    SET(KEEP_CUTBUFFER);

#ifndef NANO_SMALL
    if (ISSET(CUT_TO_END) && !ISSET(MARK_ISSET)) {
	assert(current_x >= 0 && current_x <= strlen(current->data));

	if (current->data[current_x] == '\0') {
	    /* If the line is empty and we didn't just cut a non-blank
	       line, create a dummy line and add it to the cutbuffer */
	    if (marked_cut != 1 && current->next != filebot) {
		filestruct *junk = make_new_node(current);

	        junk->data = charalloc(1);
		junk->data[0] = '\0';
		add_to_cutbuffer(junk);
#ifdef DEBUG
		dump_buffer(cutbuffer);
#endif
	    }

	    do_delete();
	    marked_cut = 2;
	    return 1;
	} else {
	    SET(MARK_ISSET);

	    mark_beginx = strlen(current->data);
	    mark_beginbuf = current;
	    dontupdate = 1;
	}
    }

    if (ISSET(MARK_ISSET)) {
	/* Don't do_update() and move the screen position if the marked
	   area lies entirely within the screen buffer */
	dontupdate |= current->lineno >= edittop->lineno &&
			current->lineno <= editbot->lineno &&
			mark_beginbuf->lineno >= edittop->lineno &&
			mark_beginbuf->lineno <= editbot->lineno;
	cut_marked_segment(current, current_x, mark_beginbuf,
				mark_beginx, 1);

	placewewant = xplustabs();
	UNSET(MARK_ISSET);

	/* If we just did a marked cut of part of a line, we should add
	   the first line of any cut done immediately afterward to the
	   end of this cut, as Pico does. */
	if (current == mark_beginbuf && current_x < strlen(current->data))
	    concatenate_cut = 1;
	marked_cut = 1;
	if (dontupdate)
	    edit_refresh();
	else
	    edit_update(current, CENTER);
	set_modified();

	return 1;
    }
#endif /* !NANO_SMALL */

    totlines--;
    totsize -= strlen(current->data) + 1;
    fileptr = current;
    current = current->next;
    current->prev = fileptr->prev;
    add_to_cutbuffer(fileptr);
#ifdef DEBUG
    dump_buffer(cutbuffer);
#endif

    if (fileptr == fileage)
	fileage = current;
    else
	current->prev->next = current;

    if (fileptr == edittop)
	edittop = current;

    renumber(current);
    current_x = 0;
    edit_refresh();
    set_modified();
    marked_cut = 0;
#ifndef NANO_SMALL
    concatenate_cut = 0;
#endif
    placewewant = 0;
    return 1;
}

int do_uncut_text(void)
{
    filestruct *tmp = current, *fileptr = current;
    filestruct *newbuf = NULL, *newend = NULL;
    char *tmpstr, *tmpstr2;
    filestruct *hold = current;
    int i;

    wrap_reset();
    check_statblank();
    if (cutbuffer == NULL || fileptr == NULL)
	return 0;		/* AIEEEEEEEEEEEE */

    /* If we're uncutting a previously non-marked block, uncut to end if
       we're not at the beginning of the line.  If we are at the
       beginning of the line, set placewewant to 0.  Pico does both of
       these. */
    if (marked_cut == 0) {
	if (current_x != 0)
	    marked_cut = 2;
	else
	    placewewant = 0;
    }

    /* If we're going to uncut on the magicline, always make a new
       magicline in advance. */
    if (current->next == NULL)
	new_magicline();

    if (marked_cut == 0 || cutbuffer->next != NULL)
    {
	newbuf = copy_filestruct(cutbuffer);
	for (newend = newbuf; newend->next != NULL && newend != NULL;
		newend = newend->next)
	    totlines++;
    }

    /* Hook newbuf into fileptr */
    if (marked_cut != 0) {
	int recenter_me = 0;
	    /* Should we eventually use edit_update(CENTER)? */

	/* If there's only one line in the cutbuffer */
	if (cutbuffer->next == NULL) {
	    size_t buf_len = strlen(cutbuffer->data);
	    size_t cur_len = strlen(current->data);

	    current->data = charealloc(current->data, cur_len + buf_len + 1);
	    memmove(current->data + current_x + buf_len,
			current->data + current_x, cur_len - current_x + 1);
	    strncpy(current->data + current_x, cutbuffer->data, buf_len);
		/* Use strncpy() to not copy the terminal '\0'. */

	    current_x += buf_len;
	    totsize += buf_len;

	    placewewant = xplustabs();
	    update_cursor();
	} else {		/* yuck -- no kidding! */
	    tmp = current->next;
	    /* New beginning */
	    tmpstr = charalloc(current_x + strlen(newbuf->data) + 1);
	    strncpy(tmpstr, current->data, current_x);
	    strcpy(&tmpstr[current_x], newbuf->data);
	    totsize += strlen(newbuf->data) + strlen(newend->data) + 1;

	    /* New end */
	    tmpstr2 = charalloc(strlen(newend->data) +
			      strlen(&current->data[current_x]) + 1);
	    strcpy(tmpstr2, newend->data);
	    strcat(tmpstr2, &current->data[current_x]);

	    free(current->data);
	    current->data = tmpstr;
	    current->next = newbuf->next;
	    newbuf->next->prev = current;
	    delete_node(newbuf);

	    current_x = strlen(newend->data);
	    placewewant = xplustabs();
	    free(newend->data);
	    newend->data = tmpstr2;

	    newend->next = tmp;

	    /* If tmp isn't null, we're in the middle: update the
	       prev pointer.  If it IS null, we're at the end; update
	       the filebot pointer */

	    if (tmp != NULL)
		tmp->prev = newend;
	    else {
		/* Fix the editbot pointer too */
		if (editbot == filebot)
		    editbot = newend;
		filebot = newend;
		new_magicline();
	    }

	    /* Now why don't we update the totsize also */
	    for (tmp = current->next; tmp != newend; tmp = tmp->next)
		totsize += strlen(tmp->data) + 1;

	    current = newend;
	    if (editbot->lineno < newend->lineno)
		recenter_me = 1;
	}

	/* If marked cut == 2, that means that we're doing a cut to end
	   and we don't want anything else on the line, so we have to
	   screw up all the work we just did and separate the line.
	   There must be a better way to do this, but not at 1AM on a
	   work night. */

	if (marked_cut == 2) {
	    tmp = make_new_node(current);
	    tmp->data = mallocstrcpy(NULL, current->data + current_x);
	    splice_node(current, tmp, current->next);
	    null_at(&current->data, current_x);
	    current = current->next;
	    current_x = 0;
	    placewewant = 0;

	    /* Extra line added, update stuff */
	    totlines++;
	    totsize++;
	}
	/* Renumber from BEFORE where we pasted ;) */
	renumber(hold);

#ifdef DEBUG
	dump_buffer(fileage);
	dump_buffer(cutbuffer);
#endif
	set_modified();
	if (recenter_me)
	    edit_update(current, CENTER);
	else
	    edit_refresh();
	return 0;
    }

    if (fileptr != fileage) {
	tmp = fileptr->prev;
	tmp->next = newbuf;
	newbuf->prev = tmp;
    } else
	fileage = newbuf;
    totlines++;		/* Unmarked uncuts don't split lines */

    /* This is so uncutting at the top of the buffer will work => */
    if (current_y == 0)
	edittop = newbuf;

    /* Connect the end of the buffer to the filestruct */
    newend->next = fileptr;
    fileptr->prev = newend;

    /* Recalculate size *sigh* */
    for (tmp = newbuf; tmp != fileptr; tmp = tmp->next)
	totsize += strlen(tmp->data) + 1;

    i = editbot->lineno;
    renumber(newbuf);
    /* Center the screen if we've moved beyond the line numbers of both
       the old and new editbots */
    if (i < newend->lineno && editbot->lineno < newend->lineno)
	edit_update(fileptr, CENTER);
    else
	edit_refresh();

#ifdef DEBUG
    dump_buffer_reverse();
#endif

    set_modified();
    return 1;
}
