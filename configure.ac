# Configuration for GNU nano - a small and user-friendly text editor
#
# Copyright (C) 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
#
# GNU Nano is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# GNU Nano is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Nano; if not, write to the Free Software
# Foundation, Inc.
# 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# $Id$

AC_INIT([GNU Nano], [1.2.5-cvs], [nano-devel@gnu.org], [nano])
AC_CONFIG_SRCDIR([nano.c])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([config.h:config.h.in])

AC_PREREQ(2.52)

dnl Checks for programs.
AC_PROG_CC
AC_ISC_POSIX
AC_SYS_LARGEFILE

dnl Internationalization macros.
AM_GNU_GETTEXT_VERSION(0.11.5)
AM_GNU_GETTEXT([external], [need-ngettext])

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h getopt.h libintl.h limits.h regex.h termio.h termios.h unistd.h)
AC_CHECK_HEADER(regex.h,
    AC_MSG_CHECKING([for broken regexec])
    AC_TRY_RUN([
#include <sys/types.h>
#include <regex.h>
int main () { regex_t reg; size_t n = 1; regmatch_t r; regcomp(&reg, "\\<", 0); regexec(&reg, "", n, &r, 0); regfree(&reg); return 0; }],
	AC_MSG_RESULT(no),
	AC_MSG_RESULT(yes); AC_DEFINE(BROKEN_REGEXEC, 1, [Define this if your regexec() function segfaults when passed an empty string under certain conditions.])
    )
)

dnl options
AC_ARG_ENABLE(debug, 
[  --enable-debug          Enable debugging (disabled by default)],
 [if test x$enableval = xyes; then
    AC_DEFINE(DEBUG, 1, [Define this to enable nano debug messages and assert warnings.])
    debug_support=yes
 fi])

if test "$debug_support" != "yes"; then
    AC_DEFINE(NDEBUG, 1, [Shut up the assert warnings :-)])
fi

AC_ARG_ENABLE(extra,
[  --enable-extra          Enable extra (optional) functions, including easter eggs],
[if test x$enableval = xyes; then
    AC_DEFINE(NANO_EXTRA, 1, [Define this to enable the extra stuff.]) extra_support=yes
    AC_DEFINE(ENABLE_MULTIBUFFER, 1, [Define this to enable multiple file buffers.]) multibuffer_support=yes
 fi])

AC_ARG_ENABLE(tiny,
[  --enable-tiny           Disable features for the sake of size],
[if test x$enableval = xyes; then
    AC_DEFINE(NANO_SMALL, 1, [Define this to make the nano executable as small as possible.]) tiny_support=yes
    AC_DEFINE(DISABLE_BROWSER, 1, [Define this to disable the built-in (crappy) file browser.]) 
    AC_DEFINE(DISABLE_HELP, 1, [Define this to disable the ^G help menu.]) 
    AC_DEFINE(DISABLE_JUSTIFY, 1, [Define this to disable the justify routine.]) 
    AC_DEFINE(DISABLE_MOUSE, 1, [Define this to disable the mouse functions.]) 
    AC_DEFINE(DISABLE_OPERATINGDIR, 1, [Define this to disable setting of the operating directory (chroot of sorts).]) 
    AC_DEFINE(DISABLE_SPELLER, 1, [Define this to disable the use(full|less) spelling functions.]) 
    AC_DEFINE(DISABLE_TABCOMP, 1, [Define to disable the tab completion code Chris worked so hard on!]) 
 fi])

AC_ARG_ENABLE(browser,
[  --disable-browser       Disable mini file browser],
 [if test x$enableval != xyes; then
    AC_DEFINE(DISABLE_BROWSER, 1, [Define this to disable the built-in (crappy) file browser.])
 fi])

AC_ARG_ENABLE(help,
[  --disable-help          Disable help function (^G)],
 [if test x$enableval != xyes; then
    AC_DEFINE(DISABLE_HELP, 1, [Define this to disable the ^G help menu.])
 fi])

AC_ARG_ENABLE(justify,
[  --disable-justify       Disable justify/unjustify function],
 [if test x$enableval != xyes; then
    AC_DEFINE(DISABLE_JUSTIFY, 1, [Define this to disable the justify routine.]) 
 fi])

AC_ARG_ENABLE(mouse,
[  --disable-mouse         Disable mouse support (and -m flag)],
 [if test x$enableval != xyes; then
    AC_DEFINE(DISABLE_MOUSE, 1, [Define this to disable the mouse functions.])
 fi])

AC_ARG_ENABLE(operatingdir,
[  --disable-operatingdir  Disable setting of operating directory (chroot of sorts)],
 [if test x$enableval != xyes; then
    AC_DEFINE(DISABLE_OPERATINGDIR, 1, [Define this to disable setting of the operating directory (chroot of sorts).])
 fi])

AC_ARG_ENABLE(speller,
[  --disable-speller       Disable spell checker function],
 [if test x$enableval != xyes; then
    AC_DEFINE(DISABLE_SPELLER, 1, [Define this to disable the use(full|less) spelling functions.])
 fi])

AC_ARG_ENABLE(tabcomp,
[  --disable-tabcomp       Disable tab completion code for a smaller binary],
 [if test x$enableval != xyes; then
    AC_DEFINE(DISABLE_TABCOMP, 1, [Define to disable the tab completion code Chris worked so hard on!]) 
 fi])

AC_ARG_ENABLE(wrapping,
[  --disable-wrapping      Disable all wrapping of text (and -w flag)],
 [if test x$enableval != xyes; then
    AC_DEFINE(DISABLE_WRAPPING, 1, [Define this to disable any and all text wrapping.])
 fi])

AC_ARG_ENABLE(wrapping-as-root,
[  --disable-wrapping-as-root  Disable wrapping of text as root by default],
 [if test x$enableval != xyes; then
    AC_DEFINE(DISABLE_ROOTWRAP, 1, [Define this to disable wrapping as root by default.])
 fi])

AC_ARG_ENABLE(color,
[  --enable-color          Enable color and syntax highlighting],
[if test x$enableval = xyes; then
    AC_DEFINE(ENABLE_NANORC, 1, [Define this to use the .nanorc file.]) nanorc_support=yes
    AC_DEFINE(ENABLE_COLOR, 1, [Define this to have syntax highlighting, requires ENABLE_NANORC too!]) color_support=yes
    AC_MSG_WARN([

***********************************************************************
*** WARNING: Color support is far from perfect, but functional.     ***
*** Be careful with syntax in your .nanorc or nano may malfunction. ***
***********************************************************************
])
fi])

AC_ARG_ENABLE(multibuffer,
[  --enable-multibuffer    Enable multiple file buffers],
[if test x$enableval = xyes; then
    AC_DEFINE(ENABLE_MULTIBUFFER, 1, [Define this to enable multiple file buffers.]) multibuffer_support=yes
 fi])

AC_ARG_ENABLE(nanorc,
[  --enable-nanorc         Enable use of .nanorc file],
[if test x$enableval = xyes; then
    AC_DEFINE(ENABLE_NANORC, 1, [Define this to use the .nanorc file.]) nanorc_support=yes
 fi])

AC_ARG_ENABLE(all,
[  --enable-all            Enable ALL extra nano functionality (kitchen sink option)],
[if test x$enableval = xyes; then
    AC_DEFINE(ENABLE_NANORC, 1, [Define this to use the .nanorc file.]) nanorc_support=yes
    AC_DEFINE(ENABLE_COLOR, 1, [Define this to have syntax highlighting, requires ENABLE_NANORC too!]) color_support=yes
    AC_DEFINE(ENABLE_MULTIBUFFER, 1, [Define this to enable multiple file buffers.]) multibuffer_support=yes
    AC_DEFINE(NANO_EXTRA, 1, [Define this to enable the extra stuff.]) extra_support=yes
    AC_MSG_WARN([

***********************************************************************
*** WARNING: Color support is far from perfect, but functional.     ***
*** Be careful with syntax in your .nanorc or nano may malfunction. ***
***********************************************************************
])
fi])

AC_MSG_CHECKING([whether to use slang])
CURSES_LIB_NAME=""
AC_ARG_WITH(slang,
[  --with-slang[=DIR]      Use the slang library instead of curses],
[    case "$with_slang" in
    no)
	AC_MSG_RESULT(no)
	;;
    *)
	AC_MSG_RESULT(yes)

	if test "$with_slang" != "yes"; then
	    # Add additional search path
	    LDFLAGS="-L$with_slang/lib $LDFLAGS"
	    CPPFLAGS="-I$with_slang/include $CPPFLAGS"
	fi

	AC_CHECK_HEADER(slcurses.h,
	    AC_MSG_CHECKING([for SLtt_initialize in -lslang])
	    _libs=$LIBS
	    LIBS="$LIBS -lslang"
	    AC_TRY_RUN([
#include <stdio.h>
#include <slcurses.h>
int main () { SLtt_initialize (NULL); return 0; }],
		[AC_MSG_RESULT(yes)
		AC_DEFINE(USE_SLANG, 1, [Define to use the slang wrappers for curses instead of native curses.])
		slang_support=yes
		if test "$with_slang" != "yes"; then
		    CURSES_LIB="-L${with_slang}/lib -lslang"
		else
		    CURSES_LIB="-lslang"
		fi
		CURSES_LIB_NAME=slang], [
		AC_MSG_RESULT(no)
		# We might need the term library
		for termlib in ncurses curses termcap terminfo termlib; do
		    AC_CHECK_LIB([${termlib}], tputs, [tcap="-l$termlib"])
		    test -n "$tcap" && break
		done

		AC_MSG_CHECKING([for SLtt_initialize in -lslang $tcap])
		LIBS="$LIBS $tcap"
		AC_TRY_RUN([
#include <stdio.h>
#include <slcurses.h>
int main () { SLtt_initialize (NULL); return 0; }],
		    [AC_MSG_RESULT(yes)
		    AC_DEFINE(USE_SLANG, 1, [Define to use the slang wrappers for curses instead of native curses.])
		    slang_support=yes
		    if test "$with_slang" != "yes"; then
			CURSES_LIB="-L${with_slang}/lib -lslang $tcap"
		    else
			CURSES_LIB="-lslang $tcap"
		    fi
		    CURSES_LIB_NAME=slang], [
		    AC_MSG_RESULT(no)
		    # We might need the math library
		    AC_MSG_CHECKING([for SLtt_initialize in -lslang $tcap -lm])
		    LIBS="$LIBS -lm"
		    AC_TRY_RUN([
#include <stdio.h>
#include <slcurses.h>
int main () { SLtt_initialize (NULL); return 0; }],
			[AC_MSG_RESULT(yes)
			AC_DEFINE(USE_SLANG, 1, [Define to use the slang wrappers for curses instead of native curses.])
			slang_support=yes
			if test "$with_slang" != "yes"; then
			    CURSES_LIB="-L${with_slang}/lib -lslang $tcap -lm"
			else
			    CURSES_LIB="-lslang $tcap -lm"
			fi
			CURSES_LIB_NAME=slang],
			[AC_MSG_RESULT(no)],
AC_MSG_WARN([*** Can not use slang when cross-compiling]))],
AC_MSG_WARN([*** Can not use slang when cross-compiling]))], 
AC_MSG_WARN([*** Can not use slang when cross-compiling])),
	    AC_MSG_ERROR([
*** The header file slcurses.h was not found. If you wish to use
*** slang support this header file is required. Please either
*** install a version of slang that includes the slcurses.h file or
*** do not call the configure script with --with-slang
]))

	test "${_libs+set}" = "set" && LIBS=$_libs

	if test "$with_slang" != "yes"; then
	    LDFLAGS=${_ldflags}
	fi
	;;
    esac], [AC_MSG_RESULT(no)])

dnl Checks for functions
AC_CHECK_FUNCS(snprintf vsnprintf strcasecmp strncasecmp)
if test "x$ac_cv_func_snprintf" = "xno" -o "xac_cv_func_vsnprintf" = "xno"
then
	AM_PATH_GLIB(1.2.4,,
	    [AC_MSG_ERROR([
*** snprintf() and/or vsnprintf() not found.  GLIB not found either.
*** You need both snprintf() and vsnprintf().  Alternatively you can
*** install the GLIB library which can be found at ftp://ftp.gtk.org/.])],
	    glib)
	glib_cflags=`$GLIB_CONFIG --cflags glib`
	glib_libs=`$GLIB_CONFIG --libs glib`
fi

dnl Checks for typedefs, structures, and compiler characteristics.

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(getopt_long)

dnl Checks for libraries.


if eval "test x$CURSES_LIB_NAME = x"
then
    AC_CHECK_HEADERS(curses.h ncurses.h)
    AC_CHECK_LIB(ncurses, tgetent, [CURSES_LIB="-lncurses" CURSES_LIB_NAME=ncurses])
fi

if eval "test x$CURSES_LIB_NAME = x"
then
    AC_CHECK_LIB(curses, tgetent, [CURSES_LIB="-lcurses" CURSES_LIB_NAME=curses])
fi

# Fallback for PDCurses and less useful curses libs...
if eval "test x$CURSES_LIB_NAME = x"
then
    AC_CHECK_LIB(curses, initscr, [CURSES_LIB="-lcurses" CURSES_LIB_NAME=curses])
fi

if eval "test x$CURSES_LIB_NAME = x"
then
    AC_CHECK_LIB(termcap, tgetent, [CURSES_LIB="-ltermcap" CURSES_LIB_NAME=termcap])
fi

if eval "test x$CURSES_LIB_NAME = x"
then
    AC_MSG_WARN([
*** No termcap lib available, consider getting the official ncurses
*** distribution from ftp://ftp.gnu.org/pub/gnu/ncurses if you get
*** errors compiling nano.])
else
    AC_MSG_RESULT([Using $CURSES_LIB_NAME as the termcap library])
fi


AC_CHECK_LIB([$CURSES_LIB_NAME], use_default_colors, AC_DEFINE(HAVE_USE_DEFAULT_COLORS, 1, [Define this if your curses library has the use_default_colors command.]))
if test x$slang_support != xyes; then
    # Taken from aumix (can't tell from the variable name?)
    AC_CACHE_CHECK([for private member _use_keypad in WINDOW],
    aumix_cv_struct_window_usekeypad,
    [AC_TRY_COMPILE([#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif], [WINDOW w; w._use_keypad;],
    aumix_cv_struct_window_usekeypad=yes, aumix_cv_struct_window_usekeypad=no)])
    
    if test $aumix_cv_struct_window_usekeypad = yes; then
	AC_DEFINE(HAVE_USEKEYPAD, 1, [Define this if your curses lib has the _use_keypad flag.])
    fi
fi

dnl Parse any configure options

LIBS="$LIBS $CURSES_LIB"

AC_SUBST(CURSES_LIB)

if test "x$glib_cflags" != "x"
then
	CFLAGS="$CFLAGS $glib_cflags"
fi
if test "x$glib_libs" != "x"
then
	LDFLAGS="$LDFLAGS $glib_libs"
fi

AC_CONFIG_FILES([Makefile m4/Makefile po/Makefile.in nano.spec])
AC_OUTPUT
