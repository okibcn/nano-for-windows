/* $Id$ */
/**************************************************************************
 *   proto.h                                                              *
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

/* Externs */

#include <sys/stat.h>

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#include "nano.h"

extern int wrap_at;
extern int editwinrows;
extern int current_x, current_y, totlines;
extern int placewewant;
#ifndef NANO_SMALL
extern int mark_beginx;
#endif
extern long totsize;
extern int temp_opt;
extern int wrap_at, flags, tabsize;
extern int search_last_line;
extern int search_offscreen;
extern int currslen;

#ifndef DISABLE_JUSTIFY
extern char *quotestr;
#endif

extern WINDOW *edit, *topwin, *bottomwin;
extern char *filename;
extern struct stat originalfilestat;
extern char *answer;
extern char *hblank;
#ifndef DISABLE_HELP
extern char *help_text;
#endif
extern char *last_search;
extern char *last_replace;
#ifndef DISABLE_OPERATINGDIR
extern char *operating_dir;
extern char *full_operating_dir;
#endif
#ifndef DISABLE_SPELLER
extern char *alt_speller;
#endif

extern int resetstatuspos;
extern struct stat fileinfo;
extern filestruct *current, *fileage, *edittop, *editbot, *filebot; 
extern filestruct *cutbuffer;
#ifndef NANO_SMALL
extern filestruct *mark_beginbuf;
#endif

#ifdef ENABLE_MULTIBUFFER
extern openfilestruct *open_files;
#endif

#ifdef ENABLE_COLOR
extern const colortype *colorstrings;
extern syntaxtype *syntaxes;
extern char *syntaxstr;
#endif

extern shortcut *shortcut_list;
extern shortcut *main_list, *whereis_list;
extern shortcut *replace_list, *goto_list;
extern shortcut *writefile_list, *insertfile_list;
extern shortcut *replace_list_2;
#ifndef NANO_SMALL
extern shortcut *extcmd_list;
#endif
#ifndef DISABLE_HELP
extern shortcut *help_list;
#endif
#ifndef DISABLE_SPELLER
extern shortcut *spell_list;
#endif
#ifndef DISABLE_BROWSER
extern shortcut *browser_list, *gotodir_list;
#endif

#if !defined(DISABLE_BROWSER) || !defined(DISABLE_HELP) || (!defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION))
extern const shortcut *currshortcut;
#endif

#ifdef HAVE_REGEX_H
extern regex_t search_regexp;
extern regmatch_t regmatches[10];  
#ifdef ENABLE_COLOR
extern regex_t syntaxfile_regexp;
extern regmatch_t synfilematches[1];  
#endif /* ENABLE_COLOR */
#endif /* HAVE_REGEX_H */

#ifndef NANO_SMALL
extern toggle *toggles;
extern int jumpok;
#endif

#ifndef NANO_SMALL
extern historyheadtype search_history;
extern historyheadtype replace_history;
#endif

extern int curses_ended;

/* Functions we want available */

/* Public functions in color.c */
#ifdef ENABLE_COLOR
void set_colorpairs(void);
void do_colorinit(void);
void update_color(void);
#endif /* ENABLE_COLOR */

/* Public functions in cut.c */
filestruct *get_cutbottom(void);
void add_to_cutbuffer(filestruct *inptr);
void cut_marked_segment(filestruct *top, size_t top_x, filestruct *bot,
                        size_t bot_x, int destructive);
int do_cut_text(void);
int do_uncut_text(void);

/* Public functions in files.c */
void load_file(int update);
void new_file(void);
filestruct *read_line(char *buf, filestruct *prev, int *line1ins, int len);
int read_file(FILE *f, const char *filename, int quiet);
int open_file(const char *filename, int insert, int quiet);
char *get_next_filename(const char *name);
int do_insertfile(int loading_file);
int do_insertfile_void(void);
#ifdef ENABLE_MULTIBUFFER
openfilestruct *make_new_opennode(openfilestruct *prevnode);
void splice_opennode(openfilestruct *begin, openfilestruct *newnode, openfilestruct *end);
void unlink_opennode(const openfilestruct *fileptr);
void delete_opennode(openfilestruct *fileptr);
void free_openfilestruct(openfilestruct *src);
int add_open_file(int update);
int load_open_file(void);
int open_prevfile(int closing_file);
int open_prevfile_void(void);
int open_nextfile(int closing_file);
int open_nextfile_void(void);
int close_open_file(void);
#endif
#if !defined(DISABLE_SPELLER) || !defined(DISABLE_OPERATINGDIR)
char *get_full_path(const char *origpath);
#endif
#ifndef DISABLE_SPELLER
char *check_writable_directory(const char *path);
char *safe_tempnam(const char *dirname, const char *filename_prefix);
#endif
#ifndef DISABLE_OPERATINGDIR
void init_operating_dir(void);
int check_operating_dir(const char *currpath, int allow_tabcomp);
#endif
int write_file(const char *name, int tmp, int append, int nonamechange);
int do_writeout(const char *path, int exiting, int append);
int do_writeout_void(void);
char *real_dir_from_tilde(const char *buf);
#ifndef DISABLE_TABCOMP
int append_slash_if_dir(char *buf, int *lastwastab, int *place);
char **username_tab_completion(char *buf, int *num_matches);
char **cwd_tab_completion(char *buf, int *num_matches);
char *input_tab(char *buf, int place, int *lastwastab, int *newplace, int *list);
#endif
#ifndef DISABLE_BROWSER
struct stat filestat(const char *path);
int diralphasort(const void *va, const void *vb);
void free_charptrarray(char **array, int len);
const char *tail(const char *foo);
void striponedir(char *foo);
int readable_dir(const char *path);
char **browser_init(const char *path, int *longest, int *numents);
char *do_browser(const char *inpath);
char *do_browse_from(const char *inpath);
#endif

/* Public functions in global.c */
int length_of_list(const shortcut *s);
void sc_init_one(shortcut **shortcutage, int key, const char *desc,
#ifndef DISABLE_HELP
	const char *help,
#endif
	int alt, int misc1, int misc2, int view, int (*func) (void));
#ifndef NANO_SMALL
void toggle_init_one(int val, const char *desc, int flag);
void toggle_init(void);
#ifdef DEBUG
void free_toggles(void);
#endif
#endif
void free_shortcutage(shortcut **shortcutage);
void shortcut_init(int unjustify);
#ifdef DEBUG
void thanks_for_all_the_fish(void);
#endif

/* Public functions in move.c */
int do_home(void);
int do_end(void);
void page_up(void);
int do_page_up(void);
int do_page_down(void);
int do_up(void);
int do_down(void);
int do_left(void);
int do_right(void);

/* Public functions in nano.c */
RETSIGTYPE finish(int sigage);
void die(const char *msg, ...);
void die_save_file(const char *die_filename);
void die_too_small(void);
void print_view_warning(void);
void global_init(int save_cutbuffer);
void window_init(void);
#if !defined(DISABLE_MOUSE) && defined(NCURSES_MOUSE_VERSION)
void mouse_init(void);
#endif
#ifndef DISABLE_HELP
void help_init(void);
#endif
filestruct *make_new_node(filestruct *prevnode);
filestruct *copy_node(const filestruct *src);
void splice_node(filestruct *begin, filestruct *newnode, filestruct *end);
void unlink_node(const filestruct *fileptr);
void delete_node(filestruct *fileptr);
filestruct *copy_filestruct(const filestruct *src);
void free_filestruct(filestruct *src);
void renumber_all(void);
void renumber(filestruct *fileptr);
void print1opt(const char *shortflag, const char *longflag,
		const char *desc);
void usage(void);
void version(void);
void do_early_abort(void);
int no_help(void);
#if defined(DISABLE_JUSTIFY) || defined(DISABLE_SPELLER) || defined(DISABLE_HELP) || defined(NANO_SMALL)
void nano_disabled_msg(void);
#endif
#ifndef NANO_SMALL
RETSIGTYPE cancel_fork(int signal);
int open_pipe(const char *command);
#endif
#ifndef DISABLE_MOUSE
#ifdef NCURSES_MOUSE_VERSION
void do_mouse(void);
#endif
#endif
void do_char(char ch);
int do_backspace(void);
int do_delete(void);
int do_tab(void);
int do_enter(void);
#ifndef NANO_SMALL
int do_next_word(void);
int do_prev_word(void);
#endif
int do_mark(void);
void wrap_reset(void);
#ifndef DISABLE_WRAPPING
int do_wrap(filestruct *inptr);
#endif
#ifndef DISABLE_SPELLER
int do_int_spell_fix(const char *word);
char *do_int_speller(char *tempfile_name);
char *do_alt_speller(char *tempfile_name);
#endif
int do_spell(void);
#if !defined(DISABLE_WRAPPING) && !defined(NANO_SMALL) || !defined(DISABLE_JUSTIFY)
size_t indent_length(const char *line);
#endif
#ifndef DISABLE_JUSTIFY
int justify_format(int changes_allowed, filestruct *line, size_t skip);
#ifdef HAVE_REGEX_H
size_t quote_length(const char *line, const regex_t *qreg);
#else
size_t quote_length(const char *line);
#endif
#ifdef HAVE_REGEX_H
#  define IFREG(a, b) a, b
#else
#  define IFREG(a, b) a
#endif
int quotes_match(const char *a_line, size_t a_quote,
		IFREG(const char *b_line, const regex_t *qreg));
size_t indents_match(const char *a_line, size_t a_indent,
			const char *b_line, size_t b_indent);
filestruct *backup_lines(filestruct *first_line, size_t par_len,
			size_t quote_len);
int breakable(const char *line, int goal);
int break_line(const char *line, int goal, int force);
#endif /* !DISABLE_JUSTIFY */
int do_justify(void);
int do_exit(void);
void signal_init(void);
RETSIGTYPE handle_hupterm(int signal);
RETSIGTYPE do_suspend(int signal);
RETSIGTYPE do_cont(int signal);
#ifndef NANO_SMALL
void handle_sigwinch(int s);
#endif
void print_numlock_warning(void);
#ifndef NANO_SMALL
void do_toggle(const toggle *which);
#endif
int abcd(int input);

/* Public functions in rcfile.c */
#ifdef ENABLE_NANORC
void rcfile_error(const char *msg, ...);
void rcfile_msg(const char *msg, ...);
char *parse_next_word(char *ptr);
char *parse_argument(char *ptr);
#ifdef ENABLE_COLOR
int colortoint(const char *colorname, int *bright);
char *parse_next_regex(char *ptr);
int nregcomp(regex_t *preg, const char *regex, int flags);
void parse_syntax(char *ptr);
void parse_colors(char *ptr);
#endif /* ENABLE_COLOR */
void parse_rcfile(FILE *rcstream);
void do_rcfile(void);
#endif /* ENABLE_NANORC */

/* Public functions in search.c */
#ifdef HAVE_REGEX_H
int regexp_init(const char *regexp);
void regexp_cleanup(void);
#endif
void not_found_msg(const char *str);
void search_abort(void);
void search_init_globals(void);
int search_init(int replacing);
int is_whole_word(int curr_pos, const char *datastr, const char *searchword);
filestruct *findnextstr(int quiet, int bracket_mode,
			const filestruct *begin, int beginx,
			const char *needle, int no_sameline);
int do_search(void);
void replace_abort(void);
#ifdef HAVE_REGEX_H
int replace_regexp(char *string, int create_flag);
#endif
char *replace_line(void);
int do_replace_loop(const char *prevanswer, const filestruct *begin,
			int *beginx, int wholewords, int *i);
int do_replace(void);
int do_gotoline(int line, int save_pos);
int do_gotoline_void(void);
#if defined (ENABLE_MULTIBUFFER) || !defined (DISABLE_SPELLER)
void do_gotopos(int line, int pos_x, int pos_y, int pos_placewewant);
#endif
int do_find_bracket(void);
#ifndef NANO_SMALL
void history_init(void);
historytype *find_node(historytype *h, char *s);
void remove_node(historytype *r);
void insert_node(historytype *h, const char *s);
void update_history(historyheadtype *h, char *s);
char *get_history_older(historyheadtype *h);
char *get_history_newer(historyheadtype *h);
char *get_history_completion(historyheadtype *h, char *s);
void free_history(historyheadtype *h);
#ifdef ENABLE_NANORC
void load_history(void);  
void save_history(void);
#endif
#endif

/* Public functions in utils.c */
#ifdef BROKEN_REGEXEC
int regexec_safe(const regex_t *preg, const char *string, size_t nmatch,
	regmatch_t pmatch[], int eflags);
#endif
int is_cntrl_char(int c);
int num_of_digits(int n);
void align(char **strp);
void null_at(char **data, size_t index);
void unsunder(char *str, size_t true_len);
void sunder(char *str);
#ifndef HAVE_STRCASECMP
int nstricmp(const char *s1, const char *s2);
#endif
#ifndef HAVE_STRNCASECMP
int nstrnicmp(const char *s1, const char *s2, size_t n);
#endif
#ifndef NANO_SMALL
const char *revstrstr(const char *haystack, const char *needle,
			const char *rev_start);
const char *revstristr(const char *haystack, const char *needle,
			const char *rev_start);
#endif
const char *stristr(const char *haystack, const char *needle);
const char *strstrwrapper(const char *haystack, const char *needle,
			const char *rev_start, int line_pos);
void nperror(const char *s);
void *nmalloc(size_t howmuch);
void *nrealloc(void *ptr, size_t howmuch);
char *mallocstrcpy(char *dest, const char *src);
void new_magicline(void);
#ifndef DISABLE_TABCOMP
int check_wildcard_match(const char *text, const char *pattern);
#endif

/* Public functions in winio.c */
int blocking_wgetch(WINDOW *win);
int do_first_line(void);
int do_last_line(void);
int xpt(const filestruct *fileptr, int index);
size_t xplustabs(void);
size_t actual_x(const filestruct *fileptr, size_t xplus);
size_t strnlenpt(const char *buf, size_t size);
size_t strlenpt(const char *buf);
void blank_bottombars(void);
void blank_bottomwin(void);
void blank_edit(void);
void blank_statusbar(void);
void blank_statusbar_refresh(void);
void check_statblank(void);
void nanoget_repaint(const char *buf, const char *inputbuf, int x);
int nanogetstr(int allowtabs, const char *buf, const char *def,
#ifndef NANO_SMALL
		historyheadtype *history_list,
#endif
		const shortcut *s
#ifndef DISABLE_TABCOMP
		, int *list
#endif
		);
void set_modified(void);
void titlebar(const char *path);
void bottombars(const shortcut *s);
void onekey(const char *keystroke, const char *desc, int len);
#ifndef NDEBUG
int check_linenumbers(const filestruct *fileptr);
#endif
int get_page_start(int column);
void reset_cursor(void);
void add_marked_sameline(int begin, int end, filestruct *fileptr, int y,
			 int virt_cur_x, int this_page);
void edit_add(const filestruct *fileptr, int yval, int start
#ifndef NANO_SMALL
		, int virt_mark_beginx,	int virt_cur_x
#endif
		);
void update_line(filestruct *fileptr, int index);
void update_cursor(void);
void center_cursor(void);
void edit_refresh(void);
void edit_refresh_clearok(void);
void edit_update(filestruct *fileptr, topmidbotnone location);
int statusq(int tabs, const shortcut *s, const char *def,
#ifndef NANO_SMALL
		historyheadtype *history_list,
#endif
		const char *msg, ...);
int do_yesno(int all, int leavecursor, const char *msg, ...);
int total_refresh(void);
void display_main_list(void);
void statusbar(const char *msg, ...);
int do_cursorpos(int constant);
int do_cursorpos_void(void);
int line_len(const char *ptr);
int do_help(void);
int keypad_on(WINDOW *win, int newval);
void do_replace_highlight(int highlight_flag, const char *word);
void fix_editbot(void);
#ifdef DEBUG
void dump_buffer(const filestruct *inptr);
void dump_buffer_reverse(void);
#endif
#ifdef NANO_EXTRA
void do_credits(void);
#endif
