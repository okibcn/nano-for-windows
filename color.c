/* $Id$ */
/**************************************************************************
 *   color.c                                                              *
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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "proto.h"
#include "nano.h"

#ifdef ENABLE_COLOR

/* For each syntax list entry, we go through the list of colors and
 * assign color pairs. */
void set_colorpairs(void)
{
    const syntaxtype *this_syntax = syntaxes;

    for(; this_syntax != NULL; this_syntax = this_syntax->next) {
	colortype *this_color = this_syntax->color;
	int color_pair = 1;

	for(; this_color != NULL; this_color = this_color->next) {
	    const colortype *beforenow = this_syntax->color;

	    for(; beforenow != NULL && beforenow != this_color && 
			(beforenow->fg != this_color->fg ||
			 beforenow->bg != this_color->bg ||
			 beforenow->bright != this_color->bright);
		    beforenow = beforenow->next)
		;

	    if (beforenow != NULL && beforenow != this_color)
		this_color->pairnum = beforenow->pairnum;
	    else {
		this_color->pairnum = color_pair;
		color_pair++;
	    }
	}
    }
}

void do_colorinit(void)
{
    if (has_colors()) {
	const colortype *tmpcolor = NULL;
#ifdef HAVE_USE_DEFAULT_COLORS
	int defok;
#endif

	start_color();
	/* Add in colors, if available */

#ifdef HAVE_USE_DEFAULT_COLORS
	defok = use_default_colors() != ERR;
#endif

	for (tmpcolor = colorstrings; tmpcolor != NULL;
		tmpcolor = tmpcolor->next) {
	    short background = tmpcolor->bg;

	    if (background == -1)
#ifdef HAVE_USE_DEFAULT_COLORS
		if (!defok)
#endif
		    background = COLOR_BLACK;

	    init_pair(tmpcolor->pairnum, tmpcolor->fg, background);

#ifdef DEBUG
	    fprintf(stderr, "Running %s with fg = %d and bg = %d\n",
		"init_pair()", tmpcolor->fg, tmpcolor->bg);
#endif
	}
    }
}

/* Update the color information based on the current filename. */
void update_color(void)
{
    const syntaxtype *tmpsyntax;

    colorstrings = NULL;
    for (tmpsyntax = syntaxes; tmpsyntax != NULL; tmpsyntax = tmpsyntax->next) {
	const exttype *e;

	for (e = tmpsyntax->extensions; e != NULL; e = e->next) {
	    /* Set colorstrings if we matched the extension regex */
	    if (regexec(&e->val, filename, 0, NULL, 0) == 0)
		colorstrings = tmpsyntax->color;

	    if (colorstrings != NULL)
		break;
	}
    }

    /* if we haven't found a match, use the override string */
    if (colorstrings == NULL && syntaxstr != NULL) {
	for (tmpsyntax = syntaxes; tmpsyntax != NULL; 
	     tmpsyntax = tmpsyntax->next) {
	    if (!strcasecmp(tmpsyntax->desc, syntaxstr))
		colorstrings = tmpsyntax->color;
	}
    }
    do_colorinit();
}

#endif /* ENABLE_COLOR */
