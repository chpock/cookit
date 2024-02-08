/*
 * tkWinPort.h --
 *
 *	This header file handles porting issues that occur because of
 *	differences between Windows and Unix. It should be the only
 *	file that contains #ifdefs to handle different flavors of OS.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tkWinPort.h,v 1.16 2010/04/20 08:17:33 nijtmans Exp $
 */

#ifndef _WINPORT
#define _WINPORT

/*
 *---------------------------------------------------------------------------
 * The following sets of #includes and #ifdefs are required to get Tcl to
 * compile under the windows compilers.
 *---------------------------------------------------------------------------
 */

#include <wchar.h>
#include <io.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <limits.h>

/*
 * Need to block out this include for building extensions with MetroWerks
 * compiler for Win32.
 */

#ifndef __MWERKS__
#include <sys/stat.h>
#endif

#include <time.h>

#ifdef _MSC_VER
#   ifndef hypot
#	define hypot _hypot
#   endif
#endif /* _MSC_VER */

/*
 *  Pull in the typedef of TCHAR for windows.
 */
#if !defined(_TCHAR_DEFINED)
#   include <tchar.h>
#   ifndef _TCHAR_DEFINED
	/* Borland seems to forget to set this. */
	typedef _TCHAR TCHAR;
#	define _TCHAR_DEFINED
#   endif
#endif

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#ifdef __CYGWIN__
#   ifndef _vsnprintf
#	define _vsnprintf vsnprintf
#   endif
#   ifndef _wcsicmp
#	define _wcsicmp wcscasecmp
#   endif
#else
#   ifndef strncasecmp
#	define strncasecmp strnicmp
#   endif
#   ifndef strcasecmp
#	define strcasecmp stricmp
#   endif
#endif

#define NBBY 8

#ifndef OPEN_MAX
#define OPEN_MAX 32
#endif

/*
 * The following define causes Tk to use its internal keysym hash table
 */

#define REDO_KEYSYM_LOOKUP

/*
 * The following macro checks to see whether there is buffered
 * input data available for a stdio FILE.
 */

#ifdef _MSC_VER
#    define TK_READ_DATA_PENDING(f) ((f)->_cnt > 0)
#else /* _MSC_VER */
#    define TK_READ_DATA_PENDING(f) ((f)->level > 0)
#endif /* _MSC_VER */

/*
 * The following stubs implement various calls that don't do anything
 * under Windows.
 */

#define TkpCmapStressed(tkwin,colormap) (0)
#define XFlush(display)
#define XGrabServer(display)
#define XUngrabServer(display)
#define TkpSync(display)

/*
 * The following functions are implemented as macros under Windows.
 */

#define XFree(data) {if ((data) != NULL) ckfree((char *) (data));}
#define XNoOp(display) {display->request++;}
#define XSynchronize(display, bool) {display->request++;}
#define XSync(display, bool) {display->request++;}
#define XVisualIDFromVisual(visual) (visual->visualid)

/*
 * The following Tk functions are implemented as macros under Windows.
 */

#define TkpGetPixel(p) (((((p)->red >> 8) & 0xff) \
	| ((p)->green & 0xff00) | (((p)->blue << 8) & 0xff0000)) | 0x20000000)

/*
 * These calls implement native bitmaps which are not currently 
 * supported under Windows.  The macros eliminate the calls.
 */

#define TkpDefineNativeBitmaps()
#define TkpCreateNativeBitmap(display, source) None
#define TkpGetNativeAppBitmap(display, name, w, h) None

#endif /* _WINPORT */
