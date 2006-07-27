/* $Id$ */
/**************************************************************************
 *   nano.c                                                               *
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <limits.h>
#include <assert.h>
#include "proto.h"
#include "nano.h"

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifndef DISABLE_WRAPJUSTIFY
static int fill = 0;	/* Fill - where to wrap lines, basically */
#endif

static struct termios oldterm;	/* The user's original term settings */
static struct sigaction act;	/* For all our fun signal handlers */

static sigjmp_buf jmpbuf;	/* Used to return to mainloop after SIGWINCH */

#ifndef NANO_SMALL
static int resizepending = 0;	/* Got a resize request while reading data */
#endif

/* What we do when we're all set to exit */
RETSIGTYPE finish(int sigage)
{
    keypad(edit, TRUE);
    keypad(bottomwin, TRUE);

    if (!ISSET(NO_HELP)) {
	mvwaddstr(bottomwin, 1, 0, hblank);
	mvwaddstr(bottomwin, 2, 0, hblank);
    } else
	mvwaddstr(bottomwin, 0, 0, hblank);

    wrefresh(bottomwin);
    endwin();

    /* Restore the old term settings */
    tcsetattr(0, TCSANOW, &oldterm);

#if !defined(NANO_SMALL) && defined(ENABLE_NANORC)
    if (!ISSET(NO_RCFILE) && ISSET(HISTORYLOG))
	save_history();
#endif

#ifdef DEBUG
    thanks_for_all_the_fish();
#endif

    exit(sigage);
}

/* Die (gracefully?) */
void die(const char *msg, ...)
{
    va_list ap;

    endwin();
    curses_ended = TRUE;

    /* Restore the old term settings */
    tcsetattr(0, TCSANOW, &oldterm);

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);

    /* save the currently loaded file if it's been modified */
    if (ISSET(MODIFIED))
	die_save_file(filename);

#ifdef ENABLE_MULTIBUFFER
    /* then save all of the other modified loaded files, if any */
    if (open_files != NULL) {
	openfilestruct *tmp;

	tmp = open_files;

	while (open_files->prev != NULL)
	    open_files = open_files->prev;

	while (open_files->next != NULL) {

	    /* if we already saved the file above (i. e. if it was the
	       currently loaded file), don't save it again */
	    if (tmp != open_files) {
		/* make sure open_files->fileage and fileage, and
		   open_files->filebot and filebot, are in sync; they
		   might not be if lines have been cut from the top or
		   bottom of the file */
		fileage = open_files->fileage;
		filebot = open_files->filebot;
		/* save the file if it's been modified */
		if (open_files->file_flags & MODIFIED)
		    die_save_file(open_files->filename);
	    }
	    open_files = open_files->next;
	}
    }
#endif

    exit(1); /* We have a problem: exit w/ errorlevel(1) */
}

void die_save_file(const char *die_filename)
{
    char *ret;
    int i = -1;

    /* If we can't save, we have REAL bad problems, but we might as well
       TRY. */
    if (die_filename[0] == '\0')
	ret = get_next_filename("nano.save");
    else {
	char *buf = charalloc(strlen(die_filename) + 6);

	strcpy(buf, die_filename);
	strcat(buf, ".save");
	ret = get_next_filename(buf);
	free(buf);
    }
    if (ret[0] != '\0')
	i = write_file(ret, 1, 0, 0);

    if (i != -1)
	fprintf(stderr, _("\nBuffer written to %s\n"), ret);
    else
	fprintf(stderr, _("\nNo %s written (too many backup files?)\n"), ret);

    free(ret);
}

/* Die with an error message that the screen was too small if, well, the
 * screen is too small. */
void die_too_small(void)
{
    die(_("Window size is too small for nano...\n"));
}

void print_view_warning(void)
{
    statusbar(_("Key illegal in VIEW mode"));
}

/* Initialize global variables - no better way for now.  If
 * save_cutbuffer is nonzero, don't set cutbuffer to NULL. */
void global_init(int save_cutbuffer)
{
    current_x = 0;
    current_y = 0;

    editwinrows = LINES - 5 + no_help();
    if (editwinrows < MIN_EDITOR_ROWS || COLS < MIN_EDITOR_COLS)
	die_too_small();

    fileage = NULL;
    if (!save_cutbuffer)
	cutbuffer = NULL;
    current = NULL;
    edittop = NULL;
    editbot = NULL;
    totlines = 0;
    totsize = 0;
    placewewant = 0;

#ifndef DISABLE_WRAPJUSTIFY
    fill = wrap_at;
    if (fill <= 0)
	fill += COLS;
    if (fill < 0)
	fill = 0;
#endif

    hblank = charalloc(COLS + 1);
    memset(hblank, ' ', COLS);
    hblank[COLS] = '\0';
}

void window_init(void)
{
    editwinrows = LINES - 5 + no_help();
    if (editwinrows < MIN_EDITOR_ROWS)
	die_too_small();

    if (edit != NULL)
	delwin(edit);
    if (topwin != NULL)
	delwin(topwin);
    if (bottomwin != NULL)
	delwin(bottomwin);

    /* Set up the main text window */
    edit = newwin(editwinrows, COLS, 2, 0);

    /* And the other windows */
    topwin = newwin(2, COLS, 0, 0);
    bottomwin = newwin(3 - no_help(), COLS, LINES - 3 + no_help(), 0);

    /* Oops, I guess we need this again.  Moved here so the keypad still
       works after a Meta-X, for example, if we were using it before. */
    if (!ISSET(ALT_KEYPAD)
#if !defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION)
	|| ISSET(USE_MOUSE)
#endif
	) {
	keypad(edit, TRUE);
	keypad(bottomwin, TRUE);
    }
}

#if !defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION)
void mouse_init(void)
{
    if (ISSET(USE_MOUSE)) {
	keypad_on(edit, 1);
	keypad_on(bottomwin, 1);

	mousemask(BUTTON1_RELEASED, NULL);
	mouseinterval(50);
    } else
	mousemask(0, NULL);
}
#endif

#ifndef DISABLE_HELP
/* This function allocates help_text, and stores the help string in it. 
 * help_text should be NULL initially. */
void help_init(void)
{
    size_t allocsize = 1;	/* space needed for help_text */
    char *ptr = NULL;
#ifndef NANO_SMALL
    const toggle *t;
#endif
    const shortcut *s;

    /* First set up the initial help text for the current function */
    if (currshortcut == whereis_list || currshortcut == replace_list
	     || currshortcut == replace_list_2)
	ptr = _("Search Command Help Text\n\n "
		"Enter the words or characters you would like to search "
		"for, then hit enter.  If there is a match for the text you "
		"entered, the screen will be updated to the location of the "
		"nearest match for the search string.\n\n "
		"The previous search string will be shown in brackets after "
		"the Search: prompt.  Hitting Enter without entering any text "
		"will perform the previous search.\n\n The following function "
		"keys are available in Search mode:\n\n");
    else if (currshortcut == goto_list)
	ptr = _("Go To Line Help Text\n\n "
		"Enter the line number that you wish to go to and hit "
		"Enter.  If there are fewer lines of text than the "
		"number you entered, you will be brought to the last line "
		"of the file.\n\n The following function keys are "
		"available in Go To Line mode:\n\n");
    else if (currshortcut == insertfile_list)
	ptr = _("Insert File Help Text\n\n "
		"Type in the name of a file to be inserted into the current "
		"file buffer at the current cursor location.\n\n "
		"If you have compiled nano with multiple file buffer "
		"support, and enable multiple buffers with the -F "
		"or --multibuffer command line flags, the Meta-F toggle, or "
		"a nanorc file, inserting a file will cause it to be "
		"loaded into a separate buffer (use Meta-< and > to switch "
		"between file buffers).\n\n If you need another blank "
		"buffer, do not enter any filename, or type in a "
		"nonexistent filename at the prompt and press "
		"Enter.\n\n The following function keys are "
		"available in Insert File mode:\n\n");
    else if (currshortcut == writefile_list)
	ptr = _("Write File Help Text\n\n "
		"Type the name that you wish to save the current file "
		"as and hit Enter to save the file.\n\n If you have "
		"selected text with Ctrl-^, you will be prompted to "
		"save only the selected portion to a separate file.  To "
		"reduce the chance of overwriting the current file with "
		"just a portion of it, the current filename is not the "
		"default in this mode.\n\n The following function keys "
		"are available in Write File mode:\n\n");
#ifndef DISABLE_BROWSER
    else if (currshortcut == browser_list)
	ptr = _("File Browser Help Text\n\n "
		"The file browser is used to visually browse the "
		"directory structure to select a file for reading "
		"or writing.  You may use the arrow keys or Page Up/"
		"Down to browse through the files, and S or Enter to "
		"choose the selected file or enter the selected "
		"directory.  To move up one level, select the directory "
		"called \"..\" at the top of the file list.\n\n The "
		"following function keys are available in the file "
		"browser:\n\n");
    else if (currshortcut == gotodir_list)
	ptr = _("Browser Go To Directory Help Text\n\n "
		"Enter the name of the directory you would like to "
		"browse to.\n\n If tab completion has not been disabled, "
		"you can use the TAB key to (attempt to) automatically "
		"complete the directory name.\n\n The following function "
		"keys are available in Browser Go To Directory mode:\n\n");
#endif
#ifndef DISABLE_SPELLER
    else if (currshortcut == spell_list)
	ptr = _("Spell Check Help Text\n\n "
		"The spell checker checks the spelling of all text "
		"in the current file.  When an unknown word is "
		"encountered, it is highlighted and a replacement can "
		"be edited.  It will then prompt to replace every "
		"instance of the given misspelled word in the "
		"current file.\n\n The following other functions are "
		"available in Spell Check mode:\n\n");
#endif
#ifndef NANO_SMALL
    else if (currshortcut == extcmd_list)
	ptr = _("External Command Help Text\n\n "
		"This menu allows you to insert the output of a command "
		"run by the shell into the current buffer (or a new "
		"buffer in multibuffer mode).\n\n The following keys are "
		"available in this mode:\n\n");
#endif
    else /* Default to the main help list */
	ptr = _(" nano help text\n\n "
	  "The nano editor is designed to emulate the functionality and "
	  "ease-of-use of the UW Pico text editor.  There are four main "
	  "sections of the editor: The top line shows the program "
	  "version, the current filename being edited, and whether "
	  "or not the file has been modified.  Next is the main editor "
	  "window showing the file being edited.  The status line is "
	  "the third line from the bottom and shows important messages. "
	  "The bottom two lines show the most commonly used shortcuts "
	  "in the editor.\n\n "
	  "The notation for shortcuts is as follows: Control-key "
	  "sequences are notated with a caret (^) symbol and are entered "
	  "with the Control (Ctrl) key.  Escape-key sequences are notated "
	  "with the Meta (M) symbol and can be entered using either the "
	  "Esc, Alt or Meta key depending on your keyboard setup.  The "
	  "following keystrokes are available in the main editor window.  "
	  "Alternative keys are shown in parentheses:\n\n");

    allocsize += strlen(ptr);

    /* The space needed for the shortcut lists, at most COLS characters,
     * plus '\n'. */
    allocsize += (COLS + 1) * length_of_list(currshortcut);

#ifndef NANO_SMALL
    /* If we're on the main list, we also count the toggle help text. 
     * Each line has "M-%c\t\t\t", which fills 24 columns, plus at most
     * COLS - 24 characters, plus '\n'.*/
    if (currshortcut == main_list) {
	size_t endislen = strlen(_("enable/disable"));

	for (t = toggles; t != NULL; t = t->next)
	    allocsize += 8 + strlen(t->desc) + endislen;
    }
#endif /* !NANO_SMALL */

    /* help_text has been freed and set to NULL unless the user resized
     * while in the help screen. */
    free(help_text);

    /* Allocate space for the help text */
    help_text = charalloc(allocsize);

    /* Now add the text we want */
    strcpy(help_text, ptr);
    ptr = help_text + strlen(help_text);

    /* Now add our shortcut info */
    for (s = currshortcut; s != NULL; s = s->next) {
	/* true if the character in s->altval is shown in first column */
	int meta_shortcut = 0;

	if (s->val > 0 && s->val < 32)
	    ptr += sprintf(ptr, "^%c", s->val + 64);
#ifndef NANO_SMALL
	else if (s->val == NANO_CONTROL_SPACE)
	    ptr += sprintf(ptr, "^%.6s", _("Space"));
	else if (s->altval == NANO_ALT_SPACE) {
	    meta_shortcut = 1;
	    ptr += sprintf(ptr, "M-%.5s", _("Space"));
	} else if (s->val == KEY_UP)
	    ptr += sprintf(ptr, "%.2s", _("Up"));
#endif
	else if (s->altval > 0) {
	    meta_shortcut = 1;
	    ptr += sprintf(ptr, "M-%c", s->altval -
			(('A' <= s->altval && s->altval <= 'Z') ||
			'a' <= s->altval ? 32 : 0));
	}
	/* Hack */
	else if (s->val >= 'a') {
	    meta_shortcut = 1;
	    ptr += sprintf(ptr, "M-%c", s->val - 32);
	}

	*(ptr++) = '\t';

	if (s->misc1 > KEY_F0 && s->misc1 <= KEY_F(64))
	    ptr += sprintf(ptr, "(F%d)", s->misc1 - KEY_F0);

	*(ptr++) = '\t';

	if (!meta_shortcut && s->altval > 0)
	    ptr += sprintf(ptr, "(M-%c)", s->altval -
		(('A' <= s->altval && s->altval <= 'Z') || 'a' <= s->altval
			? 32 : 0));

	*(ptr++) = '\t';

	assert(s->help != NULL);
	ptr += sprintf(ptr, "%.*s\n", COLS > 24 ? COLS - 24 : 0, s->help);
    }

#ifndef NANO_SMALL
    /* And the toggles... */
    if (currshortcut == main_list)
	for (t = toggles; t != NULL; t = t->next) {
	    assert(t->desc != NULL);
	    ptr += sprintf(ptr, "M-%c\t\t\t%s %s\n", t->val - 32, t->desc,
				_("enable/disable"));
	}
#endif /* !NANO_SMALL */

    /* If all went well, we didn't overwrite the allocated space for
       help_text. */
    assert(strlen(help_text) < allocsize);
}
#endif

/* Create a new filestruct node.  Note that we specifically do not set
 * prevnode->next equal to the new line. */
filestruct *make_new_node(filestruct *prevnode)
{
    filestruct *newnode = (filestruct *)nmalloc(sizeof(filestruct));

    newnode->data = NULL;
    newnode->prev = prevnode;
    newnode->next = NULL;
    newnode->lineno = prevnode != NULL ? prevnode->lineno + 1 : 1;

    return newnode;
}

/* Make a copy of a node to a pointer (space will be malloc()ed). */
filestruct *copy_node(const filestruct *src)
{
    filestruct *dst = (filestruct *)nmalloc(sizeof(filestruct));

    assert(src != NULL);

    dst->data = charalloc(strlen(src->data) + 1);
    dst->next = src->next;
    dst->prev = src->prev;
    strcpy(dst->data, src->data);
    dst->lineno = src->lineno;

    return dst;
}

/* Splice a node into an existing filestruct. */
void splice_node(filestruct *begin, filestruct *newnode, filestruct *end)
{
    if (newnode != NULL) {
	newnode->next = end;
	newnode->prev = begin;
    }
    if (begin != NULL)
	begin->next = newnode;
    if (end != NULL)
	end->prev = newnode;
}

/* Unlink a node from the rest of the filestruct. */
void unlink_node(const filestruct *fileptr)
{
    assert(fileptr != NULL);

    if (fileptr->prev != NULL)
	fileptr->prev->next = fileptr->next;

    if (fileptr->next != NULL)
	fileptr->next->prev = fileptr->prev;
}

/* Delete a node from the filestruct. */
void delete_node(filestruct *fileptr)
{
    if (fileptr != NULL) {
	if (fileptr->data != NULL)
	    free(fileptr->data);
	free(fileptr);
    }
}

/* Okay, now let's duplicate a whole struct! */
filestruct *copy_filestruct(const filestruct *src)
{
    filestruct *head;	/* copy of src, top of the copied list */
    filestruct *prev;	/* temp that traverses the list */

    assert(src != NULL);

    prev = copy_node(src);
    prev->prev = NULL;
    head = prev;
    src = src->next;
    while (src != NULL) {
	prev->next = copy_node(src);
	prev->next->prev = prev;
	prev = prev->next;

	src = src->next;
    }

    prev->next = NULL;
    return head;
}

/* Frees a filestruct. */
void free_filestruct(filestruct *src)
{
    if (src != NULL) {
	while (src->next != NULL) {
	    src = src->next;
	    delete_node(src->prev);
#ifdef DEBUG
	    fprintf(stderr, "%s: free'd a node, YAY!\n", "delete_node()");
#endif
	}
	delete_node(src);
#ifdef DEBUG
	fprintf(stderr, "%s: free'd last node.\n", "delete_node()");
#endif
    }
}

void renumber_all(void)
{
    filestruct *temp;
    int i = 1;

    assert(fileage == NULL || fileage != fileage->next);
    for (temp = fileage; temp != NULL; temp = temp->next)
	temp->lineno = i++;
}

void renumber(filestruct *fileptr)
{
    if (fileptr == NULL || fileptr->prev == NULL || fileptr == fileage)
	renumber_all();
    else {
	int lineno = fileptr->prev->lineno;

	assert(fileptr != fileptr->next);
	for (; fileptr != NULL; fileptr = fileptr->next)
	    fileptr->lineno = ++lineno;
    }
}

/* Print one usage string to the screen, removes lots of duplicate 
 * strings to translate and takes out the parts that shouldn't be 
 * translatable (the flag names). */
void print1opt(const char *shortflag, const char *longflag,
		const char *desc)
{
    printf(" %s\t", shortflag);
    if (strlen(shortflag) < 8)
	printf("\t");

#ifdef HAVE_GETOPT_LONG
    printf("%s\t", longflag);
    if (strlen(longflag) < 8)
	printf("\t\t");
    else if (strlen(longflag) < 16)
	printf("\t");
#endif

    printf("%s\n", desc);
}

void usage(void)
{
#ifdef HAVE_GETOPT_LONG
    printf(_("Usage: nano [+LINE] [GNU long option] [option] [file]\n\n"));
    printf(_("Option\t\tLong option\t\tMeaning\n"));
#else
    printf(_("Usage: nano [+LINE] [option] [file]\n\n"));
    printf(_("Option\t\tMeaning\n"));
#endif /* HAVE_GETOPT_LONG */

    print1opt("-h, -?", "--help", _("Show this message"));
    print1opt(_("+LINE"), "", _("Start at line number LINE"));
#ifndef NANO_SMALL
    print1opt("-B", "--backup", _("Backup existing files on save"));
    print1opt("-D", "--dos", _("Write file in DOS format"));
#endif
#ifdef ENABLE_MULTIBUFFER
    print1opt("-F", "--multibuffer", _("Enable multiple file buffers"));
#endif
#ifdef ENABLE_NANORC
#ifndef NANO_SMALL
    print1opt("-H", "--historylog", _("Log & read search/replace string history"));
#endif
    print1opt("-I", "--ignorercfiles", _("Don't look at nanorc files"));
#endif
    print1opt("-K", "--keypad", _("Use alternate keypad routines"));
#ifndef NANO_SMALL
    print1opt("-M", "--mac", _("Write file in Mac format"));
    print1opt("-N", "--noconvert", _("Don't convert files from DOS/Mac format"));
#endif
#ifndef DISABLE_JUSTIFY
    print1opt(_("-Q [str]"), _("--quotestr=[str]"), _("Quoting string, default \"> \""));
#endif
#ifdef HAVE_REGEX_H
    print1opt("-R", "--regexp", _("Do regular expression searches"));
#endif
#ifndef NANO_SMALL
    print1opt("-S", "--smooth", _("Smooth scrolling"));
#endif
    print1opt(_("-T [num]"), _("--tabsize=[num]"), _("Set width of a tab to num"));
    print1opt("-V", "--version", _("Print version information and exit"));
#ifdef ENABLE_COLOR
    print1opt(_("-Y [str]"), _("--syntax [str]"), _("Syntax definition to use"));
#endif
    print1opt("-c", "--const", _("Constantly show cursor position"));
#ifndef NANO_SMALL
    print1opt("-i", "--autoindent", _("Automatically indent new lines"));
    print1opt("-k", "--cut", _("Let ^K cut from cursor to end of line"));
#endif
    print1opt("-l", "--nofollow", _("Don't follow symbolic links, overwrite"));
#if !defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION)
    print1opt("-m", "--mouse", _("Enable mouse"));
#endif
#ifndef DISABLE_OPERATINGDIR
    print1opt(_("-o [dir]"), _("--operatingdir=[dir]"), _("Set operating directory"));
#endif
    print1opt("-p", "--preserve", _("Preserve XON (^Q) and XOFF (^S) keys"));
#ifndef DISABLE_WRAPJUSTIFY
    print1opt(_("-r [#cols]"), _("--fill=[#cols]"), _("Set fill cols to (wrap lines at) #cols"));
#endif
#ifndef DISABLE_SPELLER
    print1opt(_("-s [prog]"), _("--speller=[prog]"), _("Enable alternate speller"));
#endif
    print1opt("-t", "--tempfile", _("Auto save on exit, don't prompt"));
    print1opt("-v", "--view", _("View (read only) mode"));
#ifndef DISABLE_WRAPPING
    print1opt("-w", "--nowrap", _("Don't wrap long lines"));
#endif
    print1opt("-x", "--nohelp", _("Don't show help window"));
    print1opt("-z", "--suspend", _("Enable suspend"));

    /* this is a special case */
    printf(" %s\t\t\t%s\n","-a, -b, -e, -f, -g, -j", _("(ignored, for Pico compatibility)"));

    exit(0);
}

void version(void)
{
    printf(_(" GNU nano version %s (compiled %s, %s)\n"),
	   VERSION, __TIME__, __DATE__);
    printf(_
	   (" Email: nano@nano-editor.org	Web: http://www.nano-editor.org/"));
    printf(_("\n Compiled options:"));

#ifndef ENABLE_NLS
    printf(" --disable-nls");
#endif
#ifdef DEBUG
    printf(" --enable-debug");
#endif
#ifdef NANO_EXTRA
    printf(" --enable-extra");
#endif
#ifdef NANO_SMALL
    printf(" --enable-tiny");
#else
#ifdef DISABLE_BROWSER
    printf(" --disable-browser");
#endif
#ifdef DISABLE_HELP
    printf(" --disable-help");
#endif
#ifdef DISABLE_JUSTIFY
    printf(" --disable-justify");
#endif
#if defined(DISABLE_MOUSE) || !defined(NCURSES_MOUSE_VERSION)
    printf(" --disable-mouse");
#endif
#ifdef DISABLE_OPERATINGDIR
    printf(" --disable-operatingdir");
#endif
#ifdef DISABLE_SPELLER
    printf(" --disable-speller");
#endif
#ifdef DISABLE_TABCOMP
    printf(" --disable-tabcomp");
#endif
#endif /* NANO_SMALL */
#ifdef DISABLE_WRAPPING
    printf(" --disable-wrapping");
#endif
#ifdef DISABLE_ROOTWRAP
    printf(" --disable-wrapping-as-root");
#endif
#ifdef ENABLE_COLOR
    printf(" --enable-color");
#endif
#ifdef ENABLE_MULTIBUFFER
    printf(" --enable-multibuffer");
#endif
#ifdef ENABLE_NANORC
    printf(" --enable-nanorc");
#endif
#ifdef USE_SLANG
    printf(" --with-slang");
#endif
    printf("\n");
}

/* Stuff we do when we abort from programs and want to clean up the
 * screen.  This doesn't do much right now. */
void do_early_abort(void)
{
    blank_statusbar_refresh();
}

int no_help(void)
{
    return ISSET(NO_HELP) ? 2 : 0;
}

#if defined(DISABLE_JUSTIFY) || defined(DISABLE_SPELLER) || defined(DISABLE_HELP) || defined(NANO_SMALL)
void nano_disabled_msg(void)
{
    statusbar(_("Sorry, support for this function has been disabled"));
}
#endif

#ifndef NANO_SMALL
static int pid;		/* This is the PID of the newly forked process 
			 * below.  It must be global since the signal
			 * handler needs it. */
RETSIGTYPE cancel_fork(int signal)
{
    if (kill(pid, SIGKILL) == -1)
	nperror("kill");
}

int open_pipe(const char *command)
{
    int fd[2];
    FILE *f;
    struct sigaction oldaction, newaction;
			/* original and temporary handlers for SIGINT */
#ifdef _POSIX_VDISABLE
    struct termios term, newterm;
#endif /* _POSIX_VDISABLE */
    int cancel_sigs = 0;
    /* cancel_sigs == 1 means that sigaction() failed without changing
     * the signal handlers.  cancel_sigs == 2 means the signal handler
     * was changed, but the tcsetattr didn't succeed.
     *
     * I use this variable since it is important to put things back when
     * we finish, even if we get errors. */

    /* Make our pipes. */

    if (pipe(fd) == -1) {
	statusbar(_("Could not pipe"));
	return 1;
    }

    /* Fork a child. */

    if ((pid = fork()) == 0) {
	close(fd[0]);
	dup2(fd[1], fileno(stdout));
	dup2(fd[1], fileno(stderr));
	/* If execl() returns at all, there was an error. */
      
	execl("/bin/sh", "sh", "-c", command, 0);
	exit(0);
    }

    /* Else continue as parent. */

    close(fd[1]);

    if (pid == -1) {
	close(fd[0]);
	statusbar(_("Could not fork"));
	return 1;
    }

    /* Before we start reading the forked command's output, we set
     * things up so that ^C will cancel the new process. */
    if (sigaction(SIGINT, NULL, &newaction) == -1) {
	cancel_sigs = 1;
	nperror("sigaction");
    } else {
	newaction.sa_handler = cancel_fork;
	if (sigaction(SIGINT, &newaction, &oldaction) == -1) {
	    cancel_sigs = 1;
	    nperror("sigaction");
	}
    }
    /* Note that now oldaction is the previous SIGINT signal handler,
     * to be restored later. */

    /* See if the platform supports disabling individual control
     * characters. */
#ifdef _POSIX_VDISABLE
    if (cancel_sigs == 0 && tcgetattr(0, &term) == -1) {
	cancel_sigs = 2;
	nperror("tcgetattr");
    }
    if (cancel_sigs == 0) {
	newterm = term;
	/* Grab oldterm's VINTR key :-) */
	newterm.c_cc[VINTR] = oldterm.c_cc[VINTR];
	if (tcsetattr(0, TCSANOW, &newterm) == -1) {
	    cancel_sigs = 2;
	    nperror("tcsetattr");
	}
    }
#endif   /* _POSIX_VDISABLE */

    f = fdopen(fd[0], "rb");
    if (f == NULL)
      nperror("fdopen");
    
    read_file(f, "stdin", 0);
    /* if multibuffer mode is on, we could be here in view mode; if so,
       don't set the modification flag */
    if (!ISSET(VIEW_MODE))
	set_modified();

    if (wait(NULL) == -1)
	nperror("wait");

#ifdef _POSIX_VDISABLE
    if (cancel_sigs == 0 && tcsetattr(0, TCSANOW, &term) == -1)
	nperror("tcsetattr");
#endif   /* _POSIX_VDISABLE */

    if (cancel_sigs != 1 && sigaction(SIGINT, &oldaction, NULL) == -1)
	nperror("sigaction");

    return 0;
}
#endif /* NANO_SMALL */

#if !defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION)
void do_mouse(void)
{
    MEVENT mevent;
    int currslen;
    const shortcut *s = currshortcut;

    if (getmouse(&mevent) == ERR)
	return;

    /* If mouse not in edit or bottom window, return */
    if (wenclose(edit, mevent.y, mevent.x) && currshortcut == main_list) {
	int sameline;
	    /* Did they click on the line with the cursor?  If they
	       clicked on the cursor, we set the mark. */
	size_t xcur;
	    /* The character they clicked on. */

	/* Subtract out size of topwin.  Perhaps we need a constant
	 * somewhere? */
	mevent.y -= 2;

	sameline = mevent.y == current_y;

	/* Move to where the click occurred. */
	for(; current_y < mevent.y && current->next != NULL; current_y++)
	    current = current->next;
	for(; current_y > mevent.y && current->prev != NULL; current_y--)
	    current = current->prev;

	xcur = actual_x(current, get_page_start(xplustabs()) + mevent.x);

	/* Selecting where the cursor is toggles the mark.  As does
	   selecting beyond the line length with the cursor at the end of
	   the line. */
	if (sameline && xcur == current_x) {
	    if (ISSET(VIEW_MODE)) {
		print_view_warning();
		return;
	    }
	    do_mark();
	}

	current_x = xcur;
	placewewant = xplustabs();
	edit_refresh();
    } else if (wenclose(bottomwin, mevent.y, mevent.x) && !ISSET(NO_HELP)) {
	int i, k;

	if (currshortcut == main_list)
	    currslen = MAIN_VISIBLE;
	else
	    currslen = length_of_list(currshortcut);

	if (currslen < 2)
	    k = COLS / 6;
	else 
	    k = COLS / ((currslen + (currslen %2)) / 2);

	/* Determine what shortcut list was clicked */
	mevent.y -= (editwinrows + 3);

	if (mevent.y < 0) /* They clicked on the statusbar */
	    return;

	/* Don't select stuff beyond list length */
	if (mevent.x / k >= currslen)	
	    return;

	for (i = 0; i < (mevent.x / k) * 2 + mevent.y; i++)
	    s = s->next;

	/* And ungetch that value */
	ungetch(s->val);

	/* And if it's an alt-key sequence, we should probably send alt
	   too ;-) */
	if (s->val >= 'a' && s->val <= 'z')
	   ungetch(27);
    }
}
#endif

/* The user typed a printable character; add it to the edit buffer. */
void do_char(char ch)
{
    size_t current_len = strlen(current->data);
#if !defined(DISABLE_WRAPPING) || defined(ENABLE_COLOR)
    int refresh = 0;
	/* Do we have to run edit_refresh(), or can we get away with
	 * update_line()? */
#endif

    /* magic-line: when a character is inserted on the current magic line,
     * it means we need a new one! */
    if (filebot == current && current->data[0] == '\0') {
	new_magicline();
	fix_editbot();
    }

    /* more dangerousness fun =) */
    current->data = charealloc(current->data, current_len + 2);
    assert(current_x <= current_len);
    memmove(&current->data[current_x + 1],
	    &current->data[current_x],
	    current_len - current_x + 1);
    current->data[current_x] = ch;
    totsize++;
    set_modified();

#ifndef NANO_SMALL
    /* note that current_x has not yet been incremented */
    if (current == mark_beginbuf && current_x < mark_beginx)
	mark_beginx++;
#endif

    do_right();

#ifndef DISABLE_WRAPPING
    if (!ISSET(NO_WRAP) && ch != '\t')
	refresh = do_wrap(current);
#endif

#ifdef ENABLE_COLOR
    refresh = 1;
#endif

#if !defined(DISABLE_WRAPPING) || defined(ENABLE_COLOR)
    if (refresh)
	edit_refresh();
#endif
}

int do_backspace(void)
{
    int refresh = 0;
    if (current_x > 0) {
	assert(current_x <= strlen(current->data));
	/* Let's get dangerous */
	memmove(&current->data[current_x - 1], &current->data[current_x],
		strlen(current->data) - current_x + 1);
#ifdef DEBUG
	fprintf(stderr, "current->data now = \"%s\"\n", current->data);
#endif
	align(&current->data);
#ifndef NANO_SMALL
	if (current_x <= mark_beginx && mark_beginbuf == current)
	    mark_beginx--;
#endif
	do_left();
#ifdef ENABLE_COLOR
	refresh = 1;
#endif
    } else {
	filestruct *previous;
	const filestruct *tmp;

	if (current == fileage)
	    return 0;		/* Can't delete past top of file */

	previous = current->prev;
	current_x = strlen(previous->data);
	placewewant = strlenpt(previous->data);
#ifndef NANO_SMALL
	if (current == mark_beginbuf) {
	    mark_beginx += current_x;
	    mark_beginbuf = previous;
	}
#endif
	previous->data = charealloc(previous->data,
				  current_x + strlen(current->data) + 1);
	strcpy(previous->data + current_x, current->data);

	unlink_node(current);
	delete_node(current);
	tmp = current;
	current = (previous->next ? previous->next : previous);
	renumber(current);
	    /* We had to renumber before doing update_line. */
	if (tmp == edittop)
	    page_up();

	/* Ooops, sanity check */
	if (tmp == filebot) {
	    filebot = current;
	    editbot = current;

	    /* Recreate the magic line if we're deleting it AND if the
	       line we're on now is NOT blank.  if it is blank we
	       can just use IT for the magic line.   This is how Pico
	       appears to do it, in any case. */
	    if (current->data[0] != '\0') {
		new_magicline();
		fix_editbot();
	    }
	}

	current = previous;
	if (current_y > 0)
	    current_y--;
	totlines--;
#ifdef DEBUG
	fprintf(stderr, "After, data = \"%s\"\n", current->data);
#endif
	refresh = 1;
    }

    totsize--;
    set_modified();
    if (refresh)
	edit_refresh();
    return 1;
}

int do_delete(void)
{
    int refresh = 0;

    /* blbf -> blank line before filebot (see below) */
    int blbf = 0;

    if (current->next == filebot && current->data[0] == '\0')
	blbf = 1;

    placewewant = xplustabs();

    if (current_x != strlen(current->data)) {
	/* Let's get dangerous */
	memmove(&current->data[current_x], &current->data[current_x + 1],
		strlen(current->data) - current_x);

	align(&current->data);
#ifdef ENABLE_COLOR
	refresh = 1;
#endif
    } else if (current->next != NULL && (current->next != filebot || blbf)) {
	/* We can delete the line before filebot only if it is blank: it
	   becomes the new magic line then. */

	filestruct *foo;

	current->data = charealloc(current->data,
				 strlen(current->data) +
				 strlen(current->next->data) + 1);
	strcat(current->data, current->next->data);

	foo = current->next;
	if (filebot == foo) {
	    filebot = current;
	    editbot = current;
	}

	unlink_node(foo);
	delete_node(foo);
	renumber(current);
	totlines--;
	refresh = 1;
    } else
	return 0;

    totsize--;
    set_modified();
    update_line(current, current_x);
    if (refresh)
	edit_refresh();
    return 1;
}

int do_tab(void)
{
    do_char('\t');
    return 1;
}

/* Someone hits return *gasp!* */
int do_enter(void)
{
    filestruct *newnode;
    char *tmp;

    newnode = make_new_node(current);
    assert(current != NULL && current->data != NULL);
    tmp = &current->data[current_x];

#ifndef NANO_SMALL
    /* Do auto-indenting, like the neolithic Turbo Pascal editor. */
    if (ISSET(AUTOINDENT)) {
	int extra = 0;
	const char *spc = current->data;

	while (*spc == ' ' || *spc == '\t') {
	    extra++;
	    spc++;
	}
	/* If current_x < extra, then we are breaking the line in the
	 * indentation.  Autoindenting should add only current_x
	 * characters of indentation. */
	if (current_x < extra)
	    extra = current_x;
	else
	    current_x = extra;
	totsize += extra;

	newnode->data = charalloc(strlen(tmp) + extra + 1);
	strncpy(newnode->data, current->data, extra);
	strcpy(&newnode->data[extra], tmp);
    } else 
#endif
    {
	current_x = 0;
	newnode->data = charalloc(strlen(tmp) + 1);
	strcpy(newnode->data, tmp);
    }
    *tmp = '\0';

    if (current->next == NULL) {
	filebot = newnode;
	editbot = newnode;
    }
    splice_node(current, newnode, current->next);

    totsize++;
    renumber(current);
    current = newnode;
    align(&current->data);

    /* The logic here is as follows:
     *    -> If we are at the bottom of the buffer, we want to recenter
     *       (read: rebuild) the screen and forcibly move the cursor.
     *    -> otherwise, we want simply to redraw the screen and update
     *       where we think the cursor is.
     */
    if (current_y == editwinrows - 1) {
#ifndef NANO_SMALL
	if (ISSET(SMOOTHSCROLL))
	    edit_update(current, NONE);
	else
#endif
	    edit_update(current, CENTER);
	reset_cursor();
    } else {
	current_y++;
	edit_refresh();
	update_cursor();
    }

    totlines++;
    set_modified();

    placewewant = xplustabs();
    return 1;
}

#ifndef NANO_SMALL
int do_next_word(void)
{
    filestruct *old = current;

    assert(current != NULL && current->data != NULL);

    /* Skip letters in this word first. */
    while (current->data[current_x] != '\0' &&
	    isalnum((int)current->data[current_x]))
	current_x++;

    for (; current != NULL; current = current->next) {
	while (current->data[current_x] != '\0' &&
		!isalnum((int)current->data[current_x]))
	    current_x++;

	if (current->data[current_x] != '\0')
	    break;

	current_x = 0;
    }
    if (current == NULL)
	current = filebot;

    placewewant = xplustabs();

    if (current->lineno >= editbot->lineno) {
	/* If we're on the last line, don't center the screen. */
	if (current->lineno == filebot->lineno)
	    edit_refresh();
	else
	    edit_update(current, CENTER);
    }
    else {
	/* If we've jumped lines, refresh the old line.  We can't just
	   use current->prev here, because we may have skipped over some
	   blank lines, in which case the previous line is the wrong
	   one. */
	if (current != old) {
	    update_line(old, 0);
	    /* If the mark was set, then the lines between old and
	       current have to be updated too. */
	    if (ISSET(MARK_ISSET)) {
		while (old->next != current) {
		    old = old->next;
		    update_line(old, 0);
		}
	    }
	}
	update_line(current, current_x);
    }
    return 0;
}

/* The same thing for backwards. */
int do_prev_word(void)
{
    filestruct *old = current;

    assert(current != NULL && current->data != NULL);

    /* Skip letters in this word first. */
    while (current_x >= 0 && isalnum((int)current->data[current_x]))
	current_x--;

    for (; current != NULL; current = current->prev) {
	while (current_x >= 0 && !isalnum((int)current->data[current_x]))
	    current_x--;

	if (current_x >= 0)
	    break;

	if (current->prev != NULL)
	    current_x = strlen(current->prev->data);
    }

    if (current != NULL) {
	while (current_x > 0 && isalnum((int)current->data[current_x - 1]))
	    current_x--;
    } else {
	current = fileage;
	current_x = 0;
    }

    placewewant = xplustabs();

    if (current->lineno <= edittop->lineno) {
	/* If we're on the first line, don't center the screen. */
	if (current->lineno == fileage->lineno)
	    edit_refresh();
	else
	    edit_update(current, CENTER);
    }
    else {
	/* If we've jumped lines, refresh the old line.  We can't just
	   use current->prev here, because we may have skipped over some
	   blank lines, in which case the previous line is the wrong
	   one. */
	if (current != old) {
	    update_line(old, 0);
	    /* If the mark was set, then the lines between old and
	       current have to be updated too. */
	    if (ISSET(MARK_ISSET)) {
		while (old->prev != current) {
		    old = old->prev;
		    update_line(old, 0);
		}
	    }
	}
	update_line(current, current_x);
    }
    return 0;
}
#endif /* !NANO_SMALL */

int do_mark(void)
{
#ifdef NANO_SMALL
    nano_disabled_msg();
#else
    if (!ISSET(MARK_ISSET)) {
	statusbar(_("Mark Set"));
	SET(MARK_ISSET);
	mark_beginbuf = current;
	mark_beginx = current_x;
    } else {
	statusbar(_("Mark UNset"));
	UNSET(MARK_ISSET);
	edit_refresh();
    }
#endif
    return 1;
}

void wrap_reset(void)
{
    UNSET(SAMELINEWRAP);
}

#ifndef DISABLE_WRAPPING
/* We wrap the given line.  Precondition: we assume the cursor has been 
 * moved forward since the last typed character.  Return value:
 * whether we wrapped. */
int do_wrap(filestruct *inptr)
{
    size_t len = strlen(inptr->data);	/* length of the line we wrap */
    int i = 0;			/* generic loop variable */
    int wrap_loc = -1;		/* index of inptr->data where we wrap */
    int word_back = -1;
#ifndef NANO_SMALL
    const char *indentation = NULL;
	/* indentation to prepend to the new line */
    int indent_len = 0;		/* strlen(indentation) */
#endif
    const char *after_break;	/* text after the wrap point */
    int after_break_len;	/* strlen(after_break) */
    int wrapping = 0;		/* do we prepend to the next line? */
    const char *wrap_line = NULL;
	/* the next line, minus indentation */
    int wrap_line_len = 0;	/* strlen(wrap_line) */
    char *newline = NULL;	/* the line we create */
    int new_line_len = 0;	/* eventual length of newline */

/* There are three steps.  First, we decide where to wrap.  Then, we
 * create the new wrap line.  Finally, we clean up. */

/* Step 1, finding where to wrap.  We are going to add a new-line
 * after a white-space character.  In this step, we set wrap_loc as the
 * location of this replacement.
 *
 * Where should we break the line?  We need the last "legal wrap point"
 * such that the last word before it ended at or before fill.  If there
 * is no such point, we settle for the first legal wrap point.
 *
 * A "legal wrap point" is a white-space character that is not followed by
 * white-space.
 *
 * If there is no legal wrap point or we found the last character of the
 * line, we should return without wrapping.
 *
 * Note that the initial indentation does not count as a legal wrap
 * point if we are going to auto-indent!
 *
 * Note that the code below could be optimised, by not calling strnlenpt()
 * so often. */

#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT))
	i = indent_length(inptr->data);
#endif
    wrap_line = inptr->data + i;
    for(; i < len; i++, wrap_line++) {
	/* record where the last word ended */
	if (*wrap_line != ' ' && *wrap_line != '\t')
	    word_back = i;
	/* if we have found a "legal wrap point" and the current word
	 * extends too far, then we stop */
	if (wrap_loc != -1 && strnlenpt(inptr->data, word_back + 1) > fill)
	    break;
	/* we record the latest "legal wrap point" */
	if (word_back != i && wrap_line[1] != ' ' && wrap_line[1] != '\t')
	    wrap_loc = i;
    }
    if (wrap_loc < 0 || i == len)
	return 0;

/* Step 2, making the new wrap line.  It will consist of indentation +
 * after_break + " " + wrap_line (although indentation and wrap_line are
 * conditional on flags and #defines). */

    /* after_break is the text that will be moved to the next line. */
    after_break = inptr->data + wrap_loc + 1;
    after_break_len = len - wrap_loc - 1;
    assert(after_break_len == strlen(after_break));

    /* new_line_len will later be increased by the lengths of indentation
     * and wrap_line. */
    new_line_len = after_break_len;

    /* We prepend the wrapped text to the next line, if the flag is set,
     * and there is a next line, and prepending would not make the line
     * too long. */
    if (ISSET(SAMELINEWRAP) && inptr->next) {
	wrap_line = inptr->next->data;
	wrap_line_len = strlen(wrap_line);

	/* +1 for the space between after_break and wrap_line */
	if ((new_line_len + 1 + wrap_line_len) <= fill) {
	    wrapping = 1;
	    new_line_len += (1 + wrap_line_len);
	}
    }

#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT)) {
	/* Indentation comes from the next line if wrapping, else from
	 * this line. */
	indentation = (wrapping ? wrap_line : inptr->data);
	indent_len = indent_length(indentation);
	if (wrapping)
	    /* The wrap_line text should not duplicate indentation.
	     * Note in this case we need not increase new_line_len. */
	    wrap_line += indent_len;
	else
	    new_line_len += indent_len;
    }
#endif

    /* Now we allocate the new line and copy into it. */
    newline = charalloc(new_line_len + 1);  /* +1 for \0 */
    *newline = '\0';

#ifndef NANO_SMALL
    if (ISSET(AUTOINDENT)) {
	strncpy(newline, indentation, indent_len);
	newline[indent_len] = '\0';
    }
#endif
    strcat(newline, after_break);
    /* We end the old line after wrap_loc.  Note this does not eat the
     * space. */
    null_at(&inptr->data, wrap_loc + 1);
    totsize++;
    if (wrapping) {
	/* In this case, totsize increases by 1 since we add a space
	 * between after_break and wrap_line.  If the line already ends
	 * in a tab or a space, we don't add a space and decrement
	 * totsize to account for that. */
	if (!isspace((int) newline[strlen(newline) - 1]))
	    strcat(newline, " ");
	else
	    totsize--;
	strcat(newline, wrap_line);
	free(inptr->next->data);
	inptr->next->data = newline;
    } else {
	filestruct *temp = (filestruct *)nmalloc(sizeof(filestruct));

	/* In this case, the file size changes by +1 for the new line, and
	 * +indent_len for the new indentation. */
#ifndef NANO_SMALL
	totsize += indent_len;
#endif
	totlines++;
	temp->data = newline;
	temp->prev = inptr;
	temp->next = inptr->next;
	temp->prev->next = temp;
	/* If temp->next is NULL, then temp is the last line of the
	 * file, so we must set filebot. */
	if (temp->next != NULL)
	    temp->next->prev = temp;
	else
	    filebot = temp;
    }

/* Step 3, clean up.  Here we reposition the cursor and mark, and do some
 * other sundry things. */

    /* later wraps of this line will be prepended to the next line. */
    SET(SAMELINEWRAP);

    /* Each line knows its line number.  We recalculate these if we
     * inserted a new line. */
    if (!wrapping)
	renumber(inptr);

    /* If the cursor was after the break point, we must move it. */
    if (current_x > wrap_loc) {
	current = current->next;
	current_x -=
#ifndef NANO_SMALL
		-indent_len +
#endif
		wrap_loc + 1;
	wrap_reset();
	placewewant = xplustabs();
    }

#ifndef NANO_SMALL
    /* If the mark was on this line after the wrap point, we move it down.
     * If it was on the next line and we wrapped, we must move it
     * right. */
    if (mark_beginbuf == inptr && mark_beginx > wrap_loc) {
	mark_beginbuf = inptr->next;
	mark_beginx -= wrap_loc - indent_len + 1;
    } else if (wrapping && mark_beginbuf == inptr->next)
	mark_beginx += after_break_len;
#endif /* !NANO_SMALL */

    /* Place the cursor. */
    reset_cursor();

    return 1;
}
#endif /* !DISABLE_WRAPPING */

#ifndef DISABLE_SPELLER
/* word is misspelled in the file.  Let the user replace it.  We return
   False if the user cancels. */
int do_int_spell_fix(const char *word)
{
    char *save_search;
    char *save_replace;
    filestruct *current_save = current;
    int current_x_save = current_x;
    filestruct *edittop_save = edittop;
	/* Save where we are. */
    int i = 0;
	/* The return value. */
#ifndef NANO_SMALL
    int case_sens_set = ISSET(CASE_SENSITIVE);
    int reverse_search_set = ISSET(REVERSE_SEARCH);
    int mark_set = ISSET(MARK_ISSET);
#endif
#ifdef HAVE_REGEX_H
    int regexp_set = ISSET(USE_REGEXP);
#endif

#ifndef NANO_SMALL
    /* Make sure Spell Check is case sensitive */
    SET(CASE_SENSITIVE);
    /* Make sure Spell Check goes forward only */
    UNSET(REVERSE_SEARCH);
    /* Make sure the marking highlight is off during Spell Check */
    UNSET(MARK_ISSET);
#endif
#ifdef HAVE_REGEX_H
    /* Make sure Spell Check doesn't use regular expressions */
    UNSET(USE_REGEXP);
#endif

    /* save the current search/replace strings */
    search_init_globals();
    save_search = last_search;
    save_replace = last_replace;

    /* set search/replace strings to mis-spelt word */
    last_search = mallocstrcpy(NULL, word);
    last_replace = mallocstrcpy(NULL, word);

    /* start from the top of file */
    current = fileage;
    current_x = -1;

    search_last_line = FALSE;

    /* We find the first whole-word occurrence of word. */
    while (findnextstr(TRUE, TRUE, fileage, -1, word, 0))
	if (is_whole_word(current_x, current->data, word)) {
	    edit_refresh();

	    do_replace_highlight(TRUE, word);

	    /* allow replace word to be corrected */
	    i = statusq(0, spell_list, word,
#ifndef NANO_SMALL
			NULL,
#endif
			 _("Edit a replacement"));

	    do_replace_highlight(FALSE, word);

	    if (i != -1 && strcmp(word, answer)) {
		int j = 0;

		search_last_line = FALSE;
		current_x--;
		do_replace_loop(word, current_save, &current_x_save, TRUE, &j);
	    }

	    break;
	}

    /* restore the search/replace strings */
    free(last_search);    last_search=save_search;
    free(last_replace);   last_replace=save_replace;

    /* restore where we were */
    current = current_save;
    current_x = current_x_save;
    edittop = edittop_save;

#ifndef NANO_SMALL
    /* restore case sensitivity setting */
    if (!case_sens_set)
	UNSET(CASE_SENSITIVE);

    /* restore Search/Replace direction */
    if (reverse_search_set)
	SET(REVERSE_SEARCH);

    /* restore marking highlight */
    if (mark_set)
	SET(MARK_ISSET);
#endif
#ifdef HAVE_REGEX_H
    /* restore regular expression usage setting */
    if (regexp_set)
	SET(USE_REGEXP);
#endif

    return i != -1;
}

/* Integrated spell checking using 'spell' program.  Return value: NULL
 * for normal termination, otherwise the error string. */
char *do_int_speller(char *tempfile_name)
{
    char *read_buff, *read_buff_ptr, *read_buff_word;
    size_t pipe_buff_size, read_buff_size, read_buff_read, bytesread;
    int spell_fd[2], sort_fd[2], uniq_fd[2], tempfile_fd = -1;
    pid_t pid_spell, pid_sort, pid_uniq;
    int spell_status, sort_status, uniq_status;

    /* Create all three pipes up front */

    if (pipe(spell_fd) == -1 || pipe(sort_fd) == -1 || pipe(uniq_fd) == -1)
	return _("Could not create pipe");

    statusbar(_("Creating misspelled word list, please wait..."));
    /* A new process to run spell in */

    if ((pid_spell = fork()) == 0) {

	/* Child continues, (i.e. future spell process) */

	close(spell_fd[0]);

	/* replace the standard in with the tempfile */
	if ((tempfile_fd = open(tempfile_name, O_RDONLY)) == -1)
	    goto close_pipes_and_exit;

	if (dup2(tempfile_fd, STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(tempfile_fd);

	/* send spell's standard out to the pipe */
	if (dup2(spell_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(spell_fd[1]);

	/* Start spell program, we are using the PATH here!?!? */
	execlp("spell", "spell", NULL);

	/* Should not be reached, if spell is found!!! */
	exit(1);
    }

    /* Parent continues here */
    close(spell_fd[1]);

    /* A new process to run sort in */
    if ((pid_sort = fork()) == 0) {

	/* Child continues, (i.e. future spell process) */
	/* replace the standard in with output of the old pipe */
	if (dup2(spell_fd[0], STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(spell_fd[0]);

	/* send sort's standard out to the new pipe */
	if (dup2(sort_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(sort_fd[1]);

	/* Start sort program.  Use -f to remove mixed case without having
	   to have ANOTHER pipe for tr.  If this isn't portable, let me know. */
	execlp("sort", "sort", "-f", NULL);

	/* Should not be reached, if sort is found */
	exit(1);
    }

    close(spell_fd[0]);
    close(sort_fd[1]);

    /* A new process to run uniq in */
    if ((pid_uniq = fork()) == 0) {

	/* Child continues, (i.e. future uniq process) */
	/* replace the standard in with output of the old pipe */
	if (dup2(sort_fd[0], STDIN_FILENO) != STDIN_FILENO)
	    goto close_pipes_and_exit;

	close(sort_fd[0]);

	/* send uniq's standard out to the new pipe */
	if (dup2(uniq_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
	    goto close_pipes_and_exit;

	close(uniq_fd[1]);

	/* Start uniq program, we are using PATH */
	execlp("uniq", "uniq", NULL);

	/* Should not be reached, if uniq is found */
	exit(1);
    }

    close(sort_fd[0]);
    close(uniq_fd[1]);

    /* Child process was not forked successfully */
    if (pid_spell < 0 || pid_sort < 0 || pid_uniq < 0) {
	close(uniq_fd[0]);
	return _("Could not fork");
    }

    /* Get system pipe buffer size */
    if ((pipe_buff_size = fpathconf(uniq_fd[0], _PC_PIPE_BUF)) < 1) {
	close(uniq_fd[0]);
	return _("Could not get size of pipe buffer");
    }

    /* Read-in the returned spelling errors */
    read_buff_read = 0;
    read_buff_size = pipe_buff_size + 1;
    read_buff = read_buff_ptr = charalloc(read_buff_size);

    while ((bytesread = read(uniq_fd[0], read_buff_ptr, pipe_buff_size)) > 0) {
	read_buff_read += bytesread;
	read_buff_size += pipe_buff_size;
	read_buff = read_buff_ptr = charealloc(read_buff, read_buff_size);
	read_buff_ptr += read_buff_read;

    }

    *read_buff_ptr = (char)NULL;
    close(uniq_fd[0]);

    /* Process the spelling errors */
    read_buff_word = read_buff_ptr = read_buff;

    while (*read_buff_ptr != '\0') {

	if ((*read_buff_ptr == '\n') || (*read_buff_ptr == '\r')) {
	    *read_buff_ptr = (char)NULL;
	    if (read_buff_word != read_buff_ptr) {
		if (!do_int_spell_fix(read_buff_word)) {
		    read_buff_word = read_buff_ptr;
		    break;
		}
	    }
	    read_buff_word = read_buff_ptr + 1;
	}
	read_buff_ptr++;
    }

    /* special case where last word doesn't end with \n or \r */
    if (read_buff_word != read_buff_ptr)
	do_int_spell_fix(read_buff_word);

    free(read_buff);
    replace_abort();
    edit_refresh();

    /* Process end of spell process */

    waitpid(pid_spell, &spell_status, 0);
    waitpid(pid_sort, &sort_status, 0);
    waitpid(pid_uniq, &uniq_status, 0);

    if (WIFEXITED(spell_status) == 0 || WEXITSTATUS(spell_status))
	return _("Error invoking \"spell\"");

    if (WIFEXITED(sort_status)  == 0 || WEXITSTATUS(sort_status))
	return _("Error invoking \"sort -f\"");

    if (WIFEXITED(uniq_status) == 0 || WEXITSTATUS(uniq_status))
	return _("Error invoking \"uniq\"");

    /* Otherwise... */
    return NULL;

close_pipes_and_exit:

    /* Don't leak any handles */
    close(tempfile_fd);
    close(spell_fd[0]);
    close(spell_fd[1]);
    close(sort_fd[0]);
    close(sort_fd[1]);
    close(uniq_fd[0]);
    close(uniq_fd[1]);
    exit(1);
}

/* External spell checking.  Return value: NULL for normal termination,
 * otherwise the error string. */
char *do_alt_speller(char *tempfile_name)
{
    int alt_spell_status, lineno_cur = current->lineno;
    int x_cur = current_x, y_cur = current_y, pww_cur = placewewant;
    pid_t pid_spell;
    char *ptr;
    static int arglen = 3;
    static char **spellargs = (char **)NULL;
#ifndef NANO_SMALL
    int mark_set = ISSET(MARK_ISSET);
    int mbb_lineno_cur = 0;
	/* We're going to close the current file, and open the output of
	   the alternate spell command.  The line that mark_beginbuf
	   points to will be freed, so we save the line number and restore
	   afterwards. */

    if (mark_set) {
	mbb_lineno_cur = mark_beginbuf->lineno;
	UNSET(MARK_ISSET);
    }
#endif

    endwin();

    /* Set up an argument list to pass the execvp function */
    if (spellargs == NULL) {
	spellargs = (char **)nmalloc(arglen * sizeof(char *));

	spellargs[0] = strtok(alt_speller, " ");
	while ((ptr = strtok(NULL, " ")) != NULL) {
	    arglen++;
	    spellargs = (char **)nrealloc(spellargs, arglen * sizeof(char *));
	    spellargs[arglen - 3] = ptr;
	}
	spellargs[arglen - 1] = NULL;
    }
    spellargs[arglen - 2] = tempfile_name;

    /* Start a new process for the alternate speller */
    if ((pid_spell = fork()) == 0) {
	/* Start alternate spell program; we are using the PATH here!?!? */
	execvp(spellargs[0], spellargs);

	/* Should not be reached, if alternate speller is found!!! */
	exit(1);
    }

    /* Could not fork?? */
    if (pid_spell < 0)
	return _("Could not fork");

    /* Wait for alternate speller to complete */

    wait(&alt_spell_status);
    if (!WIFEXITED(alt_spell_status) || WEXITSTATUS(alt_spell_status) != 0) {
	char *altspell_error = NULL;
	char *invoke_error = _("Could not invoke \"%s\"");
	int msglen = strlen(invoke_error) + strlen(alt_speller) + 2;

	altspell_error = charalloc(msglen);
	snprintf(altspell_error, msglen, invoke_error, alt_speller);
	return altspell_error;
    }

    refresh();
    free_filestruct(fileage);
    global_init(1);
    open_file(tempfile_name, 0, 1);

#ifndef NANO_SMALL
    if (mark_set) {
	do_gotopos(mbb_lineno_cur, mark_beginx, y_cur, 0);
	mark_beginbuf = current;
	mark_beginx = current_x;
	    /* In case the line got shorter, assign mark_beginx. */
	SET(MARK_ISSET);
    }
#endif

    /* go back to the old position, mark the file as modified, and make
       sure that the titlebar is refreshed */
    do_gotopos(lineno_cur, x_cur, y_cur, pww_cur);
    set_modified();
    clearok(topwin, FALSE);
    titlebar(NULL);

    return NULL;
}
#endif

int do_spell(void)
{
#ifdef DISABLE_SPELLER
    nano_disabled_msg();
    return TRUE;
#else
    char *temp, *spell_msg;

    if ((temp = safe_tempnam(0, "nano.")) == NULL) {
	statusbar(_("Could not create a temporary filename: %s"),
		  strerror(errno));
	return 0;
    }

    if (write_file(temp, 1, 0, 0) == -1) {
	statusbar(_("Spell checking failed: unable to write temp file!"));
	free(temp);
	return 0;
    }

#ifdef ENABLE_MULTIBUFFER
    /* update the current open_files entry before spell-checking, in case
       any problems occur */
    add_open_file(1);
#endif

    if (alt_speller != NULL)
	spell_msg = do_alt_speller(temp);
    else
	spell_msg = do_int_speller(temp);
    remove(temp);
    free(temp);

    if (spell_msg != NULL) {
	statusbar(_("Spell checking failed: %s"), spell_msg);
	return 0;
    }

    statusbar(_("Finished checking spelling"));
    return 1;
#endif
}

#if !defined(DISABLE_WRAPPING) && !defined(NANO_SMALL) || !defined(DISABLE_JUSTIFY)
/* The "indentation" of a line is the white-space between the quote part
 * and the non-white-space of the line. */
size_t indent_length(const char *line)
{
    size_t len = 0;

    assert(line != NULL);
    while (*line == ' ' || *line == '\t') {
	line++;
	len++;
    }
    return len;
}
#endif /* !DISABLE_WRAPPING && !NANO_SMALL || !DISABLE_JUSTIFY */

#ifndef DISABLE_JUSTIFY
/* justify_format() replaces Tab by Space and multiple spaces by 1 (except
 * it maintains 2 after a . ! or ?).  Note the terminating \0
 * counts as a space.
 *
 * If !changes_allowed and justify_format() needs to make a change, it
 * returns 1, otherwise returns 0.
 *
 * If changes_allowed, justify_format() might make line->data
 * shorter, and change the actual pointer with null_at().
 *
 * justify_format() will not look at the first skip characters of line.
 * skip should be at most strlen(line->data).  The skip+1st character must
 * not be whitespace. */
int justify_format(int changes_allowed, filestruct *line, size_t skip)
{
    const char *punct = ".?!";
    const char *brackets = "'\")}]>";
    char *back, *front;

    /* These four asserts are assumptions about the input data. */
    assert(line != NULL);
    assert(line->data != NULL);
    assert(skip < strlen(line->data));
    assert(line->data[skip] != ' ' && line->data[skip] != '\t');

    back = line->data + skip;
    front = back;
    for (front = back; ; front++) {
	int remove_space = 0;
	    /* Do we want to remove this space? */

	if (*front == '\t') {
	    if (!changes_allowed)
		return 1;
	    *front = ' ';
	}
	/* these tests are safe since line->data + skip is not a space */
	if ((*front == '\0' || *front == ' ') && *(front - 1) == ' ') {
	    const char *bob = front - 2;

	    remove_space = 1;
	    for (bob = back - 2; bob >= line->data + skip; bob--) {
		if (strchr(punct, *bob) != NULL) {
		    remove_space = 0;
		    break;
		}
		if (strchr(brackets, *bob) == NULL)
		    break;
	    }
	}

	if (remove_space) {
	    /* Now *front is a space we want to remove.  We do that by
	     * simply failing to assign it to *back. */
	    if (!changes_allowed)
		return 1;
#ifndef NANO_SMALL
	    if (mark_beginbuf == line && back - line->data < mark_beginx)
		mark_beginx--;
#endif
	    if (*front == '\0')
		*(back - 1) = '\0';
	} else {
	    *back = *front;
	    back++;
	}
	if (*front == '\0')
	    break;
    }

    back--;
    assert(*back == '\0' && *front == '\0');

    /* This assert merely documents a fact about the loop above. */
    assert(changes_allowed != 0 || back == front);

    /* Now back is the new end of line->data. */
    if (back != front) {
	totsize -= front - back;
	null_at(&line->data, back - line->data);
#ifndef NANO_SMALL
	if (mark_beginbuf == line && back - line->data < mark_beginx)
	    mark_beginx = back - line->data;
#endif
    }
    return 0;
}

/* The "quote part" of a line is the largest initial substring matching
 * the quote string.  This function returns the length of the quote part
 * of the given line.
 *
 * Note that if !HAVE_REGEX_H then we match concatenated copies of
 * quotestr. */
#ifdef HAVE_REGEX_H
size_t quote_length(const char *line, const regex_t *qreg)
{
    regmatch_t matches;
    int rc = regexec(qreg, line, 1, &matches, 0);

    if (rc == REG_NOMATCH || matches.rm_so == (regoff_t) -1)
	return 0;
    /* matches.rm_so should be 0, since the quote string should start with
     * the caret ^. */
    return matches.rm_eo;
}
#else	/* !HAVE_REGEX_H */
size_t quote_length(const char *line)
{
    size_t qdepth = 0;
    size_t qlen = strlen(quotestr);

    /* Compute quote depth level */
    while (strncmp(line + qdepth, quotestr, quotelen) == 0)
	qdepth += qlen;
    return qdepth;
}
#endif	/* !HAVE_REGEX_H */

/* a_line and b_line are lines of text.  The quotation part of a_line is
 * the first a_quote characters.  Check that the quotation part of
 * b_line is the same. */
int quotes_match(const char *a_line, size_t a_quote,
		IFREG(const char *b_line, const regex_t *qreg))
{
    /* Here is the assumption about a_quote: */
    assert(a_quote == quote_length(IFREG(a_line, qreg)));
    return a_quote == quote_length(IFREG(b_line, qreg)) &&
	!strncmp(a_line, b_line, a_quote);
}

/* We assume a_line and b_line have no quote part.  Then, we return whether
 * b_line could follow a_line in a paragraph. */
size_t indents_match(const char *a_line, size_t a_indent,
			const char *b_line, size_t b_indent)
{
    assert(a_indent == indent_length(a_line));
    assert(b_indent == indent_length(b_line));

    return b_indent <= a_indent && !strncmp(a_line, b_line, b_indent);
}

/* Put the next par_len lines, starting with first_line, in the cut
 * buffer.  We assume there are enough lines after first_line.  We leave
 * copies of the lines in place, too.  We return the new copy of
 * first_line. */
filestruct *backup_lines(filestruct *first_line, size_t par_len,
			size_t quote_len)
{
    /* We put the original lines, not copies, into the cut buffer, just
     * out of a misguided sense of consistency, so if you un-cut, you
     * get the actual same paragraph back, not a copy. */
    filestruct *alice = first_line;

    set_modified();
    cutbuffer = NULL;
    for(; par_len > 0; par_len--) {
	filestruct *bob = copy_node(alice);

	if (alice == first_line)
	    first_line = bob;
	if (alice == current)
	    current = bob;
	if (alice == edittop)
	    edittop = bob;
#ifndef NANO_SMALL
	if (alice == mark_beginbuf)
	    mark_beginbuf = bob;
#endif
	justify_format(1, bob,
			quote_len + indent_length(bob->data + quote_len));

	assert(alice != NULL && bob != NULL);
	add_to_cutbuffer(alice);
	splice_node(bob->prev, bob, bob->next);
	alice = bob->next;
    }
    return first_line;
}

/* Is it possible to break line at or before goal? */
int breakable(const char *line, int goal)
{
    for(; *line != '\0' && goal >= 0; line++) {
	if (*line == ' ' || *line == '\t')
	    return TRUE;

	if (is_cntrl_char(*line) != 0)
	    goal -= 2;
	else
	    goal -= 1;
    }
    /* If goal is not negative, the whole line (one word) was short
     * enough. */
    return goal >= 0;
}

/* We are trying to break a chunk off line.  We find the last space such
 * that the display length to there is at most goal + 1.  If there is
 * no such space, and force is not 0, then we find the first space.
 * Anyway, we then take the last space in that group of spaces.  The
 * terminating '\0' counts as a space. */
int break_line(const char *line, int goal, int force)
{
    /* Note that we use int instead of size_t, since goal is at most COLS,
     * the screen width, which will always be reasonably small. */
    int space_loc = -1;
	/* Current tentative return value.  Index of the last space we
	 * found with short enough display width.  */
    int cur_loc = 0;
	/* Current index in line */

    assert(line != NULL);
    for(; *line != '\0' && goal >= 0; line++, cur_loc++) {
	if (*line == ' ')
	    space_loc = cur_loc;
	assert(*line != '\t');

	if (is_cntrl_char(*line))
	    goal -= 2;
	else
	    goal--;
    }
    if (goal >= 0)
	/* In fact, the whole line displays shorter than goal. */
	return cur_loc;
    if (space_loc == -1) {
	/* No space found short enough. */
	if (force)
	    for(; *line != '\0'; line++, cur_loc++)
		if (*line == ' ' && *(line + 1) != ' ' && *(line + 1) != '\0')
		    return cur_loc;
	return -1;
    }
    /* Perhaps the character after space_loc is a space.  But because
     * of justify_format(), there can be only two adjacent. */
    if (*(line - cur_loc + space_loc + 1) == ' ' ||
	*(line - cur_loc + space_loc + 1) == '\0')
	space_loc++;
    return space_loc;
}
#endif /* !DISABLE_JUSTIFY */

/* This function justifies the current paragraph. */
int do_justify(void)
{
#ifdef DISABLE_JUSTIFY
    nano_disabled_msg();
    return 1;
#else

/* To explain the justifying algorithm, I first need to define some
 * phrases about paragraphs and quotation:
 *   A line of text consists of a "quote part", followed by an
 *   "indentation part", followed by text.  The functions quote_length()
 *   and indent_length() calculate these parts.
 *
 *   A line is "part of a paragraph" if it has a part not in the quote
 *   part or the indentation.
 *
 *   A line is "the beginning of a paragraph" if it is part of a paragraph
 *   and
 *	1) it is the top line of the file, or
 *	2) the line above it is not part of a paragraph, or
 *	3) the line above it does not have precisely the same quote
 *	   part, or
 *	4) the indentation of this line is not an initial substring of the
 *	   indentation of the previous line, or
 *	5) this line has no quote part and some indentation, and
 *	   AUTOINDENT is not set.
 *   The reason for number 5) is that if AUTOINDENT is not set, then an
 *   indented line is expected to start a paragraph, like in books.  Thus,
 *   nano can justify an indented paragraph only if AUTOINDENT is turned
 *   on.
 *
 *   A contiguous set of lines is a "paragraph" if each line is part of
 *   a paragraph and only the first line is the beginning of a paragraph.
 */

    size_t quote_len;
	/* Length of the initial quotation of the paragraph we justify. */
    size_t par_len;
	/* Number of lines in that paragraph. */
    filestruct *first_mod_line = NULL;
	/* Will be the first line of the resulting justified paragraph
	 * that differs from the original.  For restoring after uncut. */
    filestruct *last_par_line = current;
	/* Will be the last line of the result, also for uncut. */
    filestruct *cutbuffer_save = cutbuffer;
	/* When the paragraph gets modified, all lines from the changed
	 * one down are stored in the cut buffer.  We back up the original
	 * to restore it later. */

    /* We save these global variables to be restored if the user
     * unjustifies.  Note we don't need to save totlines. */
    int current_x_save = current_x;
    int current_y_save = current_y;
    filestruct *current_save = current;
    int flags_save = flags;
    long totsize_save = totsize;
    filestruct *edittop_save = edittop;
    filestruct *editbot_save = editbot;
#ifndef NANO_SMALL
    filestruct *mark_beginbuf_save = mark_beginbuf;
    int mark_beginx_save = mark_beginx;
#endif

    size_t indent_len;	/* generic indentation length */
    filestruct *line;	/* generic line of text */
    size_t i;		/* generic loop variable */

#ifdef HAVE_REGEX_H
    regex_t qreg;	/* qreg is the compiled quotation regexp. 
			 * We no longer care about quotestr. */
    int rc = regcomp(&qreg, quotestr, REG_EXTENDED);

    if (rc) {
	size_t size = regerror(rc, &qreg, NULL, 0);
	char *strerror = charalloc(size);

	regerror(rc, &qreg, strerror, size);
	statusbar(_("Bad quote string %s: %s"), quotestr, strerror);
	free(strerror);
	return -1;
    }
#endif

    /* Here is an assumption that is always true anyway. */
    assert(current != NULL);

/* Here we find the first line of the paragraph to justify.  If the
 * current line is in a paragraph, then we move back to the first line. 
 * Otherwise we move down to the first line that is in a paragraph. */
    quote_len = quote_length(IFREG(current->data, &qreg));
    indent_len = indent_length(current->data + quote_len);

    current_x = 0;
    if (current->data[quote_len + indent_len] != '\0') {
	/* This line is part of a paragraph.  So we must search back to
	 * the first line of this paragraph.  First we check items 1) and
	 * 3) above. */
	while (current->prev != NULL && quotes_match(current->data,
			quote_len, IFREG(current->prev->data, &qreg))) {
	    size_t temp_id_len =
			indent_length(current->prev->data + quote_len);
		/* The indentation length of the previous line. */

	    /* Is this line the beginning of a paragraph, according to
	       items 2), 5), or 4) above?  If so, stop. */
	    if (current->prev->data[quote_len + temp_id_len] == '\0' ||
		    (quote_len == 0 && indent_len > 0
#ifndef NANO_SMALL
			&& !ISSET(AUTOINDENT)
#endif
			) ||
		    !indents_match(current->prev->data + quote_len,
			temp_id_len, current->data + quote_len, indent_len))
		break;
	    indent_len = temp_id_len;
	    current = current->prev;
	    current_y--;
	}
    } else {
	/* This line is not part of a paragraph.  Move down until we get
	 * to a non "blank" line. */
	do {
	    /* There is no next paragraph, so nothing to justify. */
	    if (current->next == NULL) {
		placewewant = 0;
		edit_refresh();
#ifdef HAVE_REGEX_H
		regfree(&qreg);
#endif
		return 0;
	    }
	    current = current->next;
	    current_y++;
	    quote_len = quote_length(IFREG(current->data, &qreg));
	    indent_len = indent_length(current->data + quote_len);
	} while (current->data[quote_len + indent_len] == '\0');
    }
/* Now current is the first line of the paragraph, and quote_len
 * is the quotation length of that line. */

/* Next step, compute par_len, the number of lines in this paragraph. */
    line = current;
    par_len = 1;
    indent_len = indent_length(line->data + quote_len);

    while (line->next != NULL && quotes_match(current->data, quote_len,
				IFREG(line->next->data, &qreg))) {
	size_t temp_id_len = indent_length(line->next->data + quote_len);

	if (!indents_match(line->data + quote_len, indent_len,
			line->next->data + quote_len, temp_id_len) ||
		line->next->data[quote_len + temp_id_len] == '\0' ||
		(quote_len == 0 && temp_id_len > 0
#ifndef NANO_SMALL
			&& !ISSET(AUTOINDENT)
#endif
		))
	    break;
	indent_len = temp_id_len;
	line = line->next;
	par_len++;
    }
#ifdef HAVE_REGEX_H
    /* We no longer need to check quotation. */
    regfree(&qreg);
#endif
/* Now par_len is the number of lines in this paragraph.  Should never
 * call quotes_match() or quote_length() again. */

/* Next step, we loop through the lines of this paragraph, justifying
 * each one individually. */
    SET(JUSTIFY_MODE);
    for(; par_len > 0; current_y++, par_len--) {
	size_t line_len;
	size_t display_len;
	    /* The width of current in screen columns. */
	int break_pos;
	    /* Where we will break the line. */

	indent_len = indent_length(current->data + quote_len) +
			quote_len; 
	/* justify_format() removes excess spaces from the line, and
	 * changes tabs to spaces.  The first argument, 0, means don't
	 * change the line, just say whether there are changes to be
	 * made.  If there are, we do backup_lines(), which copies the
	 * original paragraph to the cutbuffer for unjustification, and
	 * then calls justify_format() on the remaining lines. */
	if (first_mod_line == NULL && justify_format(0, current, indent_len))
	    first_mod_line = backup_lines(current, par_len, quote_len);

	line_len = strlen(current->data);
	display_len = strlenpt(current->data);

	if (display_len > fill) {
	    /* The line is too long.  Try to wrap it to the next. */
	    break_pos = break_line(current->data + indent_len,
			    fill - strnlenpt(current->data, indent_len),
			    1);
	    if (break_pos == -1 || break_pos + indent_len == line_len)
		/* We can't break the line, or don't need to, so just go
		 * on to the next. */
		goto continue_loc;
	    break_pos += indent_len;
	    assert(break_pos < line_len);
	    /* If we haven't backed up the paragraph, do it now. */
	    if (first_mod_line == NULL)
		first_mod_line = backup_lines(current, par_len, quote_len);
	    if (par_len == 1) {
		/* There is no next line in this paragraph.  We make a new
		 * line and copy text after break_pos into it. */
		splice_node(current, make_new_node(current),
				current->next);
		/* In a non-quoted paragraph, we copy the indent only if
		   AUTOINDENT is turned on. */
		if (quote_len == 0)
#ifndef NANO_SMALL
		    if (!ISSET(AUTOINDENT))
#endif
			indent_len = 0;
		current->next->data = charalloc(indent_len + line_len -
						break_pos);
		strncpy(current->next->data, current->data,
			indent_len);
		strcpy(current->next->data + indent_len,
			current->data + break_pos + 1);
		assert(strlen(current->next->data) ==
			indent_len + line_len - break_pos - 1);
		totlines++;
		totsize += indent_len;
		par_len++;
	    } else {
		size_t next_line_len = strlen(current->next->data);

		indent_len = quote_len +
			indent_length(current->next->data + quote_len);
		current->next->data = charealloc(current->next->data,
			next_line_len + line_len - break_pos + 1);

		memmove(current->next->data + indent_len + line_len - break_pos,
			current->next->data + indent_len,
			next_line_len - indent_len + 1);
		strcpy(current->next->data + indent_len,
			current->data + break_pos + 1);
		current->next->data[indent_len + line_len - break_pos - 1]
			= ' ';
#ifndef NANO_SMALL
		if (mark_beginbuf == current->next) {
		    if (mark_beginx < indent_len)
			mark_beginx = indent_len;
		    mark_beginx += line_len - break_pos;
		}
#endif
	    }
#ifndef NANO_SMALL
	    if (mark_beginbuf == current && mark_beginx > break_pos) {
		mark_beginbuf = current->next;
		mark_beginx -= break_pos + 1 - indent_len;
	    }
#endif
	    null_at(&current->data, break_pos);
	    current = current->next;
	} else if (display_len < fill && par_len > 1) {
	    size_t next_line_len;

	    indent_len = quote_len +
			indent_length(current->next->data + quote_len);
	    /* If we can't pull a word from the next line up to this one,
	     * just go on. */
	    if (!breakable(current->next->data + indent_len,
		    fill - display_len - 1))
		goto continue_loc;

	    /* If we haven't backed up the paragraph, do it now. */
	    if (first_mod_line == NULL)
		first_mod_line = backup_lines(current, par_len, quote_len);

	    break_pos = break_line(current->next->data + indent_len,
				fill - display_len - 1, FALSE);
	    assert(break_pos != -1);

	    current->data = charealloc(current->data,
					line_len + break_pos + 2);
	    current->data[line_len] = ' ';
	    strncpy(current->data + line_len + 1,
			current->next->data + indent_len, break_pos);
	    current->data[line_len + break_pos + 1] = '\0';
#ifndef NANO_SMALL
	    if (mark_beginbuf == current->next) {
		if (mark_beginx < indent_len + break_pos) {
 		    mark_beginbuf = current;
		    if (mark_beginx <= indent_len)
			mark_beginx = line_len + 1;
		    else
			mark_beginx = line_len + 1 + mark_beginx - indent_len;
		} else
		    mark_beginx -= break_pos + 1;
	    }
#endif
	    next_line_len = strlen(current->next->data);
	    if (indent_len + break_pos == next_line_len) {
		line = current->next;

		/* Don't destroy edittop! */
		if (line == edittop)
		    edittop = current;

		unlink_node(line);
		delete_node(line);
		totlines--;
		totsize -= indent_len;
		current_y--;
	    } else {
		memmove(current->next->data + indent_len,
			current->next->data + indent_len + break_pos + 1,
			next_line_len - break_pos - indent_len);
		null_at(&current->next->data,
			next_line_len - break_pos);
		current = current->next;
	    }
	} else
  continue_loc:
	    current = current->next;
    }
    UNSET(JUSTIFY_MODE);

/* We are now done justifying the paragraph.  There are cleanup things to
 * do, and we check for unjustify. */

    /* totlines, totsize, and current_y have been maintained above.  We
     * now set last_par_line to the new end of the paragraph, update
     * fileage, set current_x.  Also, edit_refresh() needs the line
     * numbers to be right, so we renumber(). */
    last_par_line = current->prev;
    if (first_mod_line != NULL) {
	if (first_mod_line->prev == NULL)
	    fileage = first_mod_line;
	renumber(first_mod_line);
    }

    if (current_y > editwinrows - 1)
	edit_update(current, CENTER);
    else
	edit_refresh();

    statusbar(_("Can now UnJustify!"));
    /* Change the shortcut list to display the unjustify code */
    shortcut_init(1);
    display_main_list();
    reset_cursor();

    /* Now get a keystroke and see if it's unjustify; if not, unget the
     * keystroke and return. */

#if !defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION)
    /* If it was a mouse click, parse it with do_mouse() and it might
     * become the unjustify key.  Else give it back to the input stream. */
    if ((i = blocking_wgetch(edit)) == KEY_MOUSE)
	do_mouse();
    else
	ungetch(i);
#endif

    if ((i = blocking_wgetch(edit)) != NANO_UNJUSTIFY_KEY) {
	ungetch(i);
	/* Did we back up anything at all? */
	if (cutbuffer != cutbuffer_save)
	    free_filestruct(cutbuffer);
	placewewant = 0;
    } else {
	/* Else restore the justify we just did (ungrateful user!) */
	current = current_save;
	current_x = current_x_save;
	current_y = current_y_save;
	edittop = edittop_save;
	editbot = editbot_save;
	if (first_mod_line != NULL) {
	    filestruct *cutbottom = get_cutbottom();

	    /* Splice the cutbuffer back into the file. */
	    cutbottom->next = last_par_line->next;
	    cutbottom->next->prev = cutbottom;
		/* The line numbers after the end of the paragraph have
		 * been changed, so we change them back. */
	    renumber(cutbottom->next);
	    if (first_mod_line->prev != NULL) {
		cutbuffer->prev = first_mod_line->prev;
		cutbuffer->prev->next = cutbuffer;
	    } else
		fileage = cutbuffer;
	    cutbuffer = NULL;

	    last_par_line->next = NULL;
	    free_filestruct(first_mod_line);

	    /* Restore global variables from before justify */
	    totsize = totsize_save;
	    totlines = filebot->lineno;
#ifndef NANO_SMALL
	    mark_beginbuf = mark_beginbuf_save;
	    mark_beginx = mark_beginx_save;
#endif
	    flags = flags_save;
	    if (!ISSET(MODIFIED)) {
		titlebar(NULL);
		wrefresh(topwin);
	    }
	}
	edit_refresh();
    }
    cutbuffer = cutbuffer_save;
    blank_statusbar_refresh();
    /* display shortcut list with UnCut */
    shortcut_init(0);
    display_main_list();

    return 0;
#endif
}

int do_exit(void)
{
    int i;

    if (!ISSET(MODIFIED)) {

#ifdef ENABLE_MULTIBUFFER
	if (!close_open_file()) {
	    display_main_list();
	    return 1;
	}
	else
#endif
	    finish(0);
    }

    if (ISSET(TEMP_OPT)) {
	i = 1;
    } else {
	i = do_yesno(0, 0,
		     _
		     ("Save modified buffer (ANSWERING \"No\" WILL DESTROY CHANGES) ? "));
    }

#ifdef DEBUG
    dump_buffer(fileage);
#endif

    if (i == 1) {
	if (do_writeout(filename, 1, 0) > 0) {

#ifdef ENABLE_MULTIBUFFER
	    if (!close_open_file()) {
		display_main_list();
		return 1;
	    }
	    else
#endif
		finish(0);
	}
    } else if (i == 0) {

#ifdef ENABLE_MULTIBUFFER
	if (!close_open_file()) {
	    display_main_list();
	    return 1;
	}
	else
#endif
	    finish(0);
    } else
	statusbar(_("Cancelled"));

    display_main_list();
    return 1;
}

void signal_init(void)
{
#ifdef _POSIX_VDISABLE
    struct termios term;
#endif

    /* Trap SIGINT and SIGQUIT cuz we want them to do useful things. */
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SIG_IGN;
    sigaction(SIGINT, &act, NULL);

    /* Trap SIGHUP and SIGTERM cuz we want to write the file out. */
    act.sa_handler = handle_hupterm;
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

#ifndef NANO_SMALL
    act.sa_handler = handle_sigwinch;
    sigaction(SIGWINCH, &act, NULL);
#endif

#ifdef _POSIX_VDISABLE
    tcgetattr(0, &term);

    if (!ISSET(PRESERVE)) {
	/* Ignore ^S and ^Q, much to Chris' chagrin */
	term.c_cc[VSTOP] = _POSIX_VDISABLE;
	term.c_cc[VSTART] = _POSIX_VDISABLE;
    }
#ifdef VDSUSP
    term.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif /* VDSUSP */

#endif /* _POSIX_VDISABLE */

    if (!ISSET(SUSPEND)) {
	/* Insane! */
#ifdef _POSIX_VDISABLE
	term.c_cc[VSUSP] = _POSIX_VDISABLE;
#else
	act.sa_handler = SIG_IGN;
	sigaction(SIGTSTP, &act, NULL);
#endif
    } else {
	/* If we don't do this, it seems other stuff interrupts the
	   suspend handler!  Try using nano with mutt without this
	   line. */
	sigfillset(&act.sa_mask);

	act.sa_handler = do_suspend;
	sigaction(SIGTSTP, &act, NULL);

	act.sa_handler = do_cont;
	sigaction(SIGCONT, &act, NULL);
    }

#ifdef _POSIX_VDISABLE
    tcsetattr(0, TCSANOW, &term);
#endif
}

/* Handler for SIGHUP and SIGTERM */
RETSIGTYPE handle_hupterm(int signal)
{
    die(_("Received SIGHUP or SIGTERM\n"));
}

/* What do we do when we catch the suspend signal */
RETSIGTYPE do_suspend(int signal)
{
    endwin();
    printf("\n\n\n\n\n%s\n", _("Use \"fg\" to return to nano"));
    fflush(stdout);

    /* Restore the terminal settings for the disabled keys */
    tcsetattr(0, TCSANOW, &oldterm);

    /* Trap SIGHUP and SIGTERM so we can properly deal with them while
       suspended */
    act.sa_handler = handle_hupterm;
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    /* We used to re-enable the default SIG_DFL and raise SIGTSTP, but 
       then we could be (and were) interrupted in the middle of the call.
       So we do it the mutt way instead */
    kill(0, SIGSTOP);
}

/* Restore the suspend handler when we come back into the prog */
RETSIGTYPE do_cont(int signal)
{
    /* Now we just update the screen instead of having to reenable the
       SIGTSTP handler. */
    doupdate();

    /* The Hurd seems to need this, otherwise a ^Y after a ^Z will
	start suspending again. */
    signal_init();

#ifndef NANO_SMALL
    /* Perhaps the user resized the window while we slept. */
    handle_sigwinch(0);
#endif
}

#ifndef NANO_SMALL
void handle_sigwinch(int s)
{
    const char *tty = ttyname(0);
    int fd;
    int result = 0;
    struct winsize win;

    if (!jumpok) {
	resizepending = 1;
	return;
    }

    if (tty == NULL)
	return;
    fd = open(tty, O_RDWR);
    if (fd == -1)
	return;
    result = ioctl(fd, TIOCGWINSZ, &win);
    close(fd);
    if (result == -1)
	return;

    /* Could check whether the COLS or LINES changed, and return
     * otherwise.  EXCEPT, that COLS and LINES are ncurses global
     * variables, and in some cases ncurses has already updated them. 
     * But not in all cases, argh. */
    COLS = win.ws_col;
    LINES = win.ws_row;
    editwinrows = LINES - 5 + no_help();
    if (editwinrows < MIN_EDITOR_ROWS || COLS < MIN_EDITOR_COLS)
	die_too_small();

#ifndef DISABLE_WRAPJUSTIFY
    fill = wrap_at;
    if (fill <= 0)
	fill += COLS;
    if (fill < 0)
	fill = 0;
#endif

    hblank = charealloc(hblank, COLS + 1);
    memset(hblank, ' ', COLS);
    hblank[COLS] = '\0';

#ifdef USE_SLANG
    /* Slang curses emulation brain damage: If we just do what curses
     * does here, it'll only work properly if the resize made the
     * window smaller.  Do what mutt does: Leave and immediately reenter
     * Slang screen management mode. */
    SLsmg_reset_smg();
    SLsmg_init_smg();
#else
    /* Do the equivalent of what Minimum Profit does: Leave and
     * immediately reenter curses mode. */
    endwin();
    doupdate();
#endif
 
    /* Do the equivalent of what both mutt and Minimum Profit do:
     * Reinitialize all the windows based on the new screen
     * dimensions. */
    window_init();

    fix_editbot();

    if (current_y > editwinrows - 1)
	edit_update(editbot, CENTER);

    /* Redraw the contents of the windows that need it. */
    blank_statusbar();
    display_main_list();
    total_refresh();

    /* Turn cursor back on for sure */
    curs_set(1);

    /* Jump back to main loop */
    siglongjmp(jmpbuf, 1);
}
#endif /* !NANO_SMALL */

/* If the NumLock key has made the keypad go awry, print an error
   message; hopefully we can address it later. */
void print_numlock_warning(void)
{
    static int didmsg = 0;
    if (!didmsg) {
	statusbar(_
		  ("NumLock glitch detected.  Keypad will malfunction with NumLock off"));
	didmsg = 1;
    }
}

#ifndef NANO_SMALL
void do_toggle(const toggle *which)
{
    int enabled;

    /* Even easier! */
    TOGGLE(which->flag);

    switch (which->val) {
    case TOGGLE_SUSPEND_KEY:
	signal_init();
	break;
#if !defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION)
    case TOGGLE_MOUSE_KEY:
	mouse_init();
	break;
#endif
    case TOGGLE_NOHELP_KEY:
	wclear(bottomwin);
	wrefresh(bottomwin);
	window_init();
	fix_editbot();
	edit_refresh();
	display_main_list();
	break;
    case TOGGLE_DOS_KEY:
	UNSET(MAC_FILE);
	break;
    case TOGGLE_MAC_KEY:
	UNSET(DOS_FILE);
	break;
#ifdef ENABLE_COLOR
    case TOGGLE_SYNTAX_KEY:
	edit_refresh();
	break;
#endif
    }

    /* We are assuming here that shortcut_init() above didn't free and
     * reallocate the toggles. */
    enabled = ISSET(which->flag);
    if (which->val == TOGGLE_NOHELP_KEY || which->val == TOGGLE_WRAP_KEY)
	enabled = !enabled;
    statusbar("%s %s", which->desc,
		enabled ? _("enabled") : _("disabled"));
}
#endif /* !NANO_SMALL */

/* This function returns the correct keystroke, given the A,B,C or D
   input key.  This is a common sequence of many terms which send
   Esc-O-[A-D] or Esc-[-[A-D]. */
int abcd(int input)
{
    switch (input) {
    case 'A':
    case 'a':
	return KEY_UP;
    case 'B':
    case 'b':
	return KEY_DOWN;
    case 'C':
    case 'c':
	return KEY_RIGHT;
    case 'D':
    case 'd':
	return KEY_LEFT;
    default:
	return 0;
    }
}

int main(int argc, char *argv[])
{
    int optchr;
    int startline = 0;		/* Line to try and start at */
    int modify_control_seq = 0;
    int fill_flag_used = 0;	/* Was the fill option used? */
    const shortcut *s;
    int keyhandled = 0;	/* Have we handled the keystroke yet? */
    int kbinput = -1;		/* Input from keyboard */

#ifndef NANO_SMALL
    const toggle *t;
#endif
#ifdef _POSIX_VDISABLE
    struct termios term;
#endif
#ifdef HAVE_GETOPT_LONG
    int option_index = 0;
    const struct option long_options[] = {
	{"help", 0, 0, 'h'},
#ifdef ENABLE_MULTIBUFFER
	{"multibuffer", 0, 0, 'F'},
#endif
#ifdef ENABLE_NANORC
#ifndef NANO_SMALL
	{"historylog", 0, 0, 'H'},
#endif
	{"ignorercfiles", 0, 0, 'I'},
#endif
	{"keypad", 0, 0, 'K'},
#ifndef DISABLE_JUSTIFY
	{"quotestr", 1, 0, 'Q'},
#endif
#ifdef HAVE_REGEX_H
	{"regexp", 0, 0, 'R'},
#endif
	{"tabsize", 1, 0, 'T'},
	{"version", 0, 0, 'V'},
#ifdef ENABLE_COLOR
	{"syntax", 1, 0, 'Y'},
#endif
	{"const", 0, 0, 'c'},
	{"nofollow", 0, 0, 'l'},
#if !defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION)
	{"mouse", 0, 0, 'm'},
#endif
#ifndef DISABLE_OPERATINGDIR
	{"operatingdir", 1, 0, 'o'},
#endif
	{"preserve", 0, 0, 'p'},
#ifndef DISABLE_WRAPJUSTIFY
	{"fill", 1, 0, 'r'},
#endif
#ifndef DISABLE_SPELLER
	{"speller", 1, 0, 's'},
#endif
	{"tempfile", 0, 0, 't'},
	{"view", 0, 0, 'v'},
#ifndef DISABLE_WRAPPING
	{"nowrap", 0, 0, 'w'},
#endif
	{"nohelp", 0, 0, 'x'},
	{"suspend", 0, 0, 'z'},
#ifndef NANO_SMALL
	{"backup", 0, 0, 'B'},
	{"dos", 0, 0, 'D'},
	{"mac", 0, 0, 'M'},
	{"noconvert", 0, 0, 'N'},
	{"smooth", 0, 0, 'S'},
	{"autoindent", 0, 0, 'i'},
	{"cut", 0, 0, 'k'},
#endif
	{0, 0, 0, 0}
    };
#endif

    setlocale(LC_ALL, "");
#ifdef ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

#if !defined(ENABLE_NANORC) && defined(DISABLE_ROOTWRAP) && !defined(DISABLE_WRAPPING)
    /* if we don't have rcfile support, we're root, and
       --disable-wrapping-as-root is used, turn wrapping off */
    if (geteuid() == NANO_ROOT_UID)
	SET(NO_WRAP);
#endif

#ifdef HAVE_GETOPT_LONG
    while ((optchr = getopt_long(argc, argv, "h?BDFHIKMNQ:RST:VY:abcefgijklmo:pr:s:tvwxz",
				 long_options, &option_index)) != -1) {
#else
    while ((optchr =
	    getopt(argc, argv, "h?BDFHIKMNQ:RST:VY:abcefgijklmo:pr:s:tvwxz")) != -1) {
#endif

	switch (optchr) {

	case 'a':
	case 'b':
	case 'e':
	case 'f':
	case 'g':
	case 'j':
	    /* Pico compatibility flags */
	    break;
#ifndef NANO_SMALL
	case 'B':
	    SET(BACKUP_FILE);
	    break;
	case 'D':
	    SET(DOS_FILE);
	    break;
#endif
#ifdef ENABLE_MULTIBUFFER
	case 'F':
	    SET(MULTIBUFFER);
	    break;
#endif
#ifdef ENABLE_NANORC
#ifndef NANO_SMALL
	case 'H':
	    SET(HISTORYLOG);
	    break;
#endif
	case 'I':
	    SET(NO_RCFILE);
	    break;
#endif
	case 'K':
	    SET(ALT_KEYPAD);
	    break;
#ifndef NANO_SMALL
	case 'M':
	    SET(MAC_FILE);
	    break;
	case 'N':
	    SET(NO_CONVERT);
	    break;
#endif
#ifndef DISABLE_JUSTIFY
	case 'Q':
	    quotestr = mallocstrcpy(quotestr, optarg);
	    break;
#endif
#ifdef HAVE_REGEX_H
	case 'R':
	    SET(USE_REGEXP);
	    break;
#endif
#ifndef NANO_SMALL
	case 'S':
	    SET(SMOOTHSCROLL);
	    break;
#endif
	case 'T':
	    {
		int i;
		char *first_error;

		/* Using strtol() instead of atoi() lets us accept 0
		 * while checking other errors. */
		i = (int)strtol(optarg, &first_error, 10);
		if (errno == ERANGE || *optarg == '\0' || *first_error != '\0')
		    usage();
		else
		    tabsize = i;
		if (tabsize <= 0) {
		    fprintf(stderr, _("Tab size is too small for nano...\n"));
		    exit(1);
		}
	    }
	    break;
	case 'V':
	    version();
	    exit(0);
#ifdef ENABLE_COLOR
	case 'Y':
	    syntaxstr = mallocstrcpy(syntaxstr, optarg);
	    break;
#endif
	case 'c':
	    SET(CONSTUPDATE);
	    break;
#ifndef NANO_SMALL
	case 'i':
	    SET(AUTOINDENT);
	    break;
	case 'k':
	    SET(CUT_TO_END);
	    break;
#endif
	case 'l':
	    SET(NOFOLLOW_SYMLINKS);
	    break;
#if !defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION)
	case 'm':
	    SET(USE_MOUSE);
	    break;
#endif
#ifndef DISABLE_OPERATINGDIR
	case 'o':
	    operating_dir = mallocstrcpy(operating_dir, optarg);
	    break;
#endif
	case 'p':
	    SET(PRESERVE);
#ifdef HAVE_GETOPT_LONG
#endif
	    break;
#ifndef DISABLE_WRAPJUSTIFY
	case 'r':
	    {
		int i;
		char *first_error;

		/* Using strtol() instead of atoi() lets us accept 0
		 * while checking other errors. */
		i = (int)strtol(optarg, &first_error, 10);
		if (errno == ERANGE || *optarg == '\0' || *first_error != '\0')
		    usage();
		else
		    wrap_at = i;
	    }
	    fill_flag_used = 1;
	    break;
#endif
#ifndef DISABLE_SPELLER
	case 's':
	    alt_speller = mallocstrcpy(alt_speller, optarg);
	    break;
#endif
	case 't':
	    SET(TEMP_OPT);
	    break;
	case 'v':
	    SET(VIEW_MODE);
	    break;
#ifndef DISABLE_WRAPPING
	case 'w':
	    SET(NO_WRAP);
	    break;
#endif
	case 'x':
	    SET(NO_HELP);
	    break;
	case 'z':
	    SET(SUSPEND);
	    break;
	default:
	    usage();
	}
    }

/* We've read through the command line options.  Now back up the flags
   and values that are set, and read the rcfile(s).  If the values
   haven't changed afterward, restore the backed-up values. */
#ifdef ENABLE_NANORC
    if (!ISSET(NO_RCFILE)) {
#ifndef DISABLE_OPERATINGDIR
	char *operating_dir_cpy = operating_dir;
#endif
#ifndef DISABLE_WRAPPING
	int wrap_at_cpy = wrap_at;
#endif
#ifndef DISABLE_JUSTIFY
	char *quotestr_cpy = quotestr;
#endif
#ifndef DISABLE_SPELLER
	char *alt_speller_cpy = alt_speller;
#endif
	int tabsize_cpy = tabsize;
	long flags_cpy = flags;

#ifndef DISABLE_OPERATINGDIR
	operating_dir = NULL;
#endif
#ifndef DISABLE_JUSTIFY
	quotestr = NULL;
#endif
#ifndef DISABLE_SPELLER
	alt_speller = NULL;
#endif

	do_rcfile();

#ifndef DISABLE_OPERATINGDIR
	if (operating_dir_cpy != NULL) {
	    free(operating_dir);
	    operating_dir = operating_dir_cpy;
	}
#endif
#ifndef DISABLE_WRAPPING
	if (fill_flag_used)
	    wrap_at = wrap_at_cpy;
#endif
#ifndef DISABLE_JUSTIFY
	if (quotestr_cpy != NULL) {
	    free(quotestr);
	    quotestr = quotestr_cpy;
	}
#endif
#ifndef DISABLE_SPELLER
	if (alt_speller_cpy != NULL) {
	    free(alt_speller);
	    alt_speller = alt_speller_cpy;
	}
#endif
	if (tabsize_cpy > 0)
	    tabsize = tabsize_cpy;
	flags |= flags_cpy;
    }
#if defined(DISABLE_ROOTWRAP) && !defined(DISABLE_WRAPPING)
    else if (geteuid() == NANO_ROOT_UID)
	SET(NO_WRAP);
#endif
#endif /* ENABLE_NANORC */

#ifndef NANO_SMALL
    history_init();
#ifdef ENABLE_NANORC
    if (!ISSET(NO_RCFILE) && ISSET(HISTORYLOG))
	load_history();
#endif
#endif

#ifndef DISABLE_OPERATINGDIR
    /* Set up the operating directory.  This entails chdir()ing there,
       so that file reads and writes will be based there. */
    init_operating_dir();
#endif

#ifndef DISABLE_JUSTIFY
    if (quotestr == NULL)
#ifdef HAVE_REGEX_H
	quotestr = mallocstrcpy(NULL, "^([ \t]*[|>:}#])+");
#else
	quotestr = mallocstrcpy(NULL, "> ");
#endif
#endif /* !DISABLE_JUSTIFY */
    if (tabsize == -1)
	tabsize = 8;

    /* Clear the filename we'll be using */
    filename = charalloc(1);
    filename[0] = '\0';

    /* If there's a +LINE flag, it is the first non-option argument. */
    if (0 < optind && optind < argc && argv[optind][0] == '+') {
	startline = atoi(&argv[optind][1]);
	optind++;
    }
    if (0 < optind && optind < argc)
	filename = mallocstrcpy(filename, argv[optind]);

    /* See if there's a non-option in argv (first non-option is the
       filename, if +LINE is not given) */
    if (argc > 1 && argc > optind) {
	/* Look for the +line flag... */
	if (argv[optind][0] == '+') {
	    startline = atoi(&argv[optind][1]);
	    optind++;
	    if (argc > optind)
		filename = mallocstrcpy(filename, argv[optind]);
	} else
	    filename = mallocstrcpy(filename, argv[optind]);
    }

    /* First back up the old settings so they can be restored, duh */
    tcgetattr(0, &oldterm);

#ifdef _POSIX_VDISABLE
    term = oldterm;
    term.c_cc[VINTR] = _POSIX_VDISABLE;
    term.c_cc[VQUIT] = _POSIX_VDISABLE;
    term.c_lflag &= ~IEXTEN;
    tcsetattr(0, TCSANOW, &term);
#endif

    /* now ncurses init stuff... */
    initscr();
    savetty();
    nonl();
    cbreak();
    noecho();

    /* Set up some global variables */
    global_init(0);
    shortcut_init(0);
    signal_init();

#ifdef DEBUG
    fprintf(stderr, "Main: set up windows\n");
#endif

    window_init();
#if !defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION)
    mouse_init();
#endif

    if (!ISSET(ALT_KEYPAD)) {
	keypad(edit, TRUE);
	keypad(bottomwin, TRUE);
    }

#ifdef DEBUG
    fprintf(stderr, "Main: bottom win\n");
#endif
    /* Set up bottom of window */
    display_main_list();

#ifdef DEBUG
    fprintf(stderr, "Main: open file\n");
#endif

    open_file(filename, 0, 0);
#ifdef ENABLE_MULTIBUFFER
    /* If we're using multibuffers and more than one file is specified
       on the command line, load them all and switch to the first one
       afterward */
    if (ISSET(MULTIBUFFER) && optind + 1 < argc) {
	for (optind++; optind < argc; optind++) {
	    add_open_file(1);
	    new_file();
	    filename = mallocstrcpy(filename, argv[optind]);
	    open_file(filename, 0, 0);
	    load_file(0);
	}
	open_nextfile_void();
    }
#endif

    titlebar(NULL);

    if (startline > 0)
	do_gotoline(startline, 0);

    /* Return here after a sigwinch */
    sigsetjmp(jmpbuf, 1);

#ifndef NANO_SMALL
    jumpok = 1;
    if (resizepending) {
	resizepending = 0;
	handle_sigwinch(0);
    }
#endif

    /* SHUT UP GCC! */
    startline = 0;
    fill_flag_used = 0;
    keyhandled = 0;

    /* This variable should be initialized after the sigsetjmp(), so we
       can't do Esc-Esc then quickly resize and muck things up. */
    modify_control_seq = 0;

    edit_refresh();
    reset_cursor();

    while (1) {
	keyhandled = 0;

	if (ISSET(CONSTUPDATE))
	    do_cursorpos(1);

#if !defined(DISABLE_BROWSER) || !defined(DISABLE_HELP) || (!defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION))
	currshortcut = main_list;
#endif

#ifndef _POSIX_VDISABLE
	/* We're going to have to do it the old way, i.e. on cygwin */
	raw();
#endif

	kbinput = blocking_wgetch(edit);
#ifdef DEBUG
	fprintf(stderr, "AHA!  %c (%d)\n", kbinput, kbinput);
#endif
	if (kbinput == 27) {	/* Grab Alt-key stuff first */
	    kbinput = blocking_wgetch(edit);
	    switch (kbinput) {
		/* Alt-O, suddenly very important ;) */
	    case 'O':
		kbinput = blocking_wgetch(edit);
		/* Shift or Ctrl + Arrows are Alt-O-[2,5,6]-[A,B,C,D] on some terms */
		if (kbinput == '2' || kbinput == '5' || kbinput == '6')
		    kbinput = blocking_wgetch(edit);
		if ((kbinput <= 'D' && kbinput >= 'A') ||
			(kbinput <= 'd' && kbinput >= 'a'))
		    kbinput = abcd(kbinput);
		/* Alt-O-H and Alt-O-F are Home and End on some terms */
		else if (kbinput == 'F')
		    kbinput = KEY_END;
		else if (kbinput == 'H')
		    kbinput = KEY_HOME;
		/* Alt-O-P through Alt-O-S are F1 through F4 on some terms */
		else if (kbinput <= 'S' && kbinput >= 'P')
		    kbinput = KEY_F(kbinput - 79);
		/* Alt-O-j through Alt-O-y are produced by the NumLock glitch */
		else if (kbinput <= 'y' && kbinput >= 'j')
		    print_numlock_warning();
#ifdef DEBUG
		else {
		    fprintf(stderr, "I got Alt-O-%c! (%d)\n",
			    kbinput, kbinput);
		    break;
		}
#endif
		break;
	    case 27:
		/* If we get Alt-Alt, the next keystroke should be the same as a
		   control sequence */
		modify_control_seq = 1;
		keyhandled = 1;
		break;
	    case '[':
		kbinput = blocking_wgetch(edit);
		switch (kbinput) {
		case '1':	/* Alt-[-1-[0-5,7-9] = F1-F8 in X at least */
		    kbinput = blocking_wgetch(edit);
		    if (kbinput >= '1' && kbinput <= '5') {
			kbinput = KEY_F(kbinput - 48);
			blocking_wgetch(edit);
		    } else if (kbinput >= '7' && kbinput <= '9') {
			kbinput = KEY_F(kbinput - 49);
			blocking_wgetch(edit);
		    } else if (kbinput == '~')
			kbinput = KEY_HOME;
#ifdef DEBUG
		    else {
			fprintf(stderr, "I got Alt-[-1-%c! (%d)\n",
				kbinput, kbinput);
			break;
		    }
#endif
		    break;
		case '2':	/* Alt-[-2-[0,1,3,4] = F9-F12 in many terms */
		    kbinput = blocking_wgetch(edit);
		    switch (kbinput) {
		    case '0':
			kbinput = KEY_F(9);
			blocking_wgetch(edit);
			break;
		    case '1':
			kbinput = KEY_F(10);
			blocking_wgetch(edit);
			break;
		    case '3':
			kbinput = KEY_F(11);
			blocking_wgetch(edit);
			break;
		    case '4':
			kbinput = KEY_F(12);
			blocking_wgetch(edit);
			break;
		    case '~':
			kbinput = NANO_INSERTFILE_KEY;
			break;
#ifdef DEBUG
		    default:
			fprintf(stderr, "I got Alt-[-2-%c! (%d)\n",
				kbinput, kbinput);
			break;
#endif
		    }
		    break;
		case '3':	/* Alt-[-3 = Delete? */
		    kbinput = NANO_DELETE_KEY;
		    blocking_wgetch(edit);
		    break;
		case '4':	/* Alt-[-4 = End? */
		    kbinput = NANO_END_KEY;
		    blocking_wgetch(edit);
		    break;
		case '5':	/* Alt-[-5 = Page Up */
		    kbinput = KEY_PPAGE;
		    blocking_wgetch(edit);
		    break;
		case 'V':	/* Alt-[-V = Page Up in Hurd Console */
		case 'I':	/* Alt-[-I = Page Up - FreeBSD Console */
		    kbinput = KEY_PPAGE;
		    break;
		case '6':	/* Alt-[-6 = Page Down */
		    kbinput = KEY_NPAGE;
		    blocking_wgetch(edit);
		    break;
		case 'U':	/* Alt-[-U = Page Down in Hurd Console */
		case 'G':	/* Alt-[-G = Page Down - FreeBSD Console */
		    kbinput = KEY_NPAGE;
		    break;
		case '7':
		    kbinput = KEY_HOME;
		    blocking_wgetch(edit);
		    break;
		case '8':
		    kbinput = KEY_END;
		    blocking_wgetch(edit);
		    break;
		case '9':	/* Alt-[-9 = Delete in Hurd Console */
		    kbinput = KEY_DC;
		    break;
		case '@':	/* Alt-[-@ = Insert in Hurd Console */
		case 'L':	/* Alt-[-L = Insert - FreeBSD Console */
		    kbinput = NANO_INSERTFILE_KEY;
		    break;
		case '[':	/* Alt-[-[-[A-E], F1-F5 in Linux console */
		    kbinput = blocking_wgetch(edit);
		    if (kbinput >= 'A' && kbinput <= 'E')
			kbinput = KEY_F(kbinput - 64);
		    break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		    kbinput = abcd(kbinput);
		    break;
		case 'H':
		    kbinput = KEY_HOME;
		    break;
		case 'F':
		case 'Y':		/* End Key in Hurd Console */
		    kbinput = KEY_END;
		    break;
		default:
#ifdef DEBUG
		    fprintf(stderr, "I got Alt-[-%c! (%d)\n",
			    kbinput, kbinput);
#endif
		    break;
		}
		break;
#ifdef ENABLE_MULTIBUFFER
	    case NANO_OPENPREV_KEY:
	    case NANO_OPENPREV_ALTKEY:
		open_prevfile_void();
		keyhandled = 1;
		break;
	    case NANO_OPENNEXT_KEY:
	    case NANO_OPENNEXT_ALTKEY:
		open_nextfile_void();
		keyhandled = 1;
		break;
#endif
	    default:
		/* Check for the altkey defs.... */
		for (s = main_list; s != NULL; s = s->next)
		    if (kbinput == s->altval || (kbinput >= 'A' &&
			    kbinput <= 'Z' && kbinput == s->altval - 32)) {
			if (ISSET(VIEW_MODE) && !s->viewok)
			    print_view_warning();
			else {
			    if (s->func != do_cut_text)
				UNSET(KEEP_CUTBUFFER);
			    s->func();
			}
			keyhandled = 1;
			break;
		    }
#ifndef NANO_SMALL
		if (!keyhandled)
		    /* And for toggle switches */
		    for (t = toggles; t != NULL; t = t->next)
			if (kbinput == t->val || (t->val >= 'a' &&
				t->val <= 'z' && kbinput == t->val - 32)) {
			    UNSET(KEEP_CUTBUFFER);
			    do_toggle(t);
			    keyhandled = 1;
			    break;
		        }
#endif
#ifdef DEBUG
		fprintf(stderr, "I got Alt-%c! (%d)\n", kbinput,
			kbinput);
#endif
		break;
	    }
	}
	/* Hack, make insert key do something useful, like insert file */
	else if (kbinput == KEY_IC)
	    kbinput = NANO_INSERTFILE_KEY;

	/* If modify_control_seq is set, we received an Alt-Alt
	   sequence before this, so we make this key a control sequence
	   by subtracting 32, 64, or 96, depending on its value. */
	if (!keyhandled && modify_control_seq) {
	    if (kbinput == ' ')
		kbinput -= 32;
	    else if (kbinput >= 'A' && kbinput < 'a')
		kbinput -= 64;
	    else if (kbinput >= 'a' && kbinput <= 'z')
		kbinput -= 96;

	    modify_control_seq = 0;
	}

	/* Look through the main shortcut list to see if we've hit a
	   shortcut key */

	if (!keyhandled)
#if !defined(DISABLE_BROWSER) || !defined (DISABLE_HELP) || (!defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION))
	    for (s = currshortcut; s != NULL && !keyhandled; s = s->next) {
#else
	    for (s = main_list; s != NULL && !keyhandled; s = s->next) {
#endif
		if (kbinput == s->val ||
		    (s->misc1 && kbinput == s->misc1) ||
		    (s->misc2 && kbinput == s->misc2)) {
		    if (ISSET(VIEW_MODE) && !s->viewok)
			print_view_warning();
		    else {
			if (s->func != do_cut_text)
			    UNSET(KEEP_CUTBUFFER);
			s->func();
		    }
		    keyhandled = 1;
		    /* Rarely, the value of s can change after
		       s->func(), leading to problems; get around this
		       by breaking out explicitly once we successfully
		       handle a shortcut */
		    break;
		}
	    }

	if (!keyhandled)
	    UNSET(KEEP_CUTBUFFER);

#ifdef _POSIX_VDISABLE
	/* Don't even think about changing this string */
	if (kbinput == 19)
	    statusbar(_("XOFF ignored, mumble mumble."));
	if (kbinput == 17)
	    statusbar(_("XON ignored, mumble mumble."));
#endif
	/* If we're in raw mode or using Alt-Alt-x, we have to catch
	   Control-S and Control-Q */
	if (kbinput == 17 || kbinput == 19)
	    keyhandled = 1;

	/* Catch ^Z by hand when triggered also
	   KEY_SUSPEND == ^Z in Linux console when keypad() is used and
	   nano is built with ncurses? */
#ifdef KEY_SUSPEND
	if (kbinput == 26 || kbinput == KEY_SUSPEND) {
#else
	if (kbinput == 26) {
#endif
	    if (ISSET(SUSPEND))
		do_suspend(0);
	    keyhandled = 1;
	}

	/* Last gasp, stuff that's not in the main lists */
	if (!keyhandled)
	    switch (kbinput) {
#if !defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION)
	    case KEY_MOUSE:
		do_mouse();
		break;
#endif

	    case -1:		/* Stuff that we don't want to do squat */
	    case 0:		/* Erg */
	    case 29:		/* Ctrl-] */
	    case 410:		/* Must ignore this, it's sent when we resize */
#ifdef PDCURSES
	    case 541:		/* ???? */
	    case 542:		/* Control and alt in Windows *shrug* */
	    case 543:		/* Right ctrl key */
	    case 544:
	    case 545:		/* Right alt key */
#endif

		break;
	    default:
#ifdef DEBUG
		fprintf(stderr, "I got %c (%d)!\n", kbinput, kbinput);
#endif
		/* We no longer stop unhandled sequences so that people with
		   odd character sets can type... */

		if (ISSET(VIEW_MODE))
		    print_view_warning();
		else
		    do_char(kbinput);
	    }

	reset_cursor();
	wrefresh(edit);
    }
    assert(0);
}
