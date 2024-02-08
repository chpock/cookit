/*
 * tkDND.h --
 * 
 *    Header files for the drag&drop tk extension.
 *
 * This software is copyrighted by:
 * George Petasis,
 * Software and Knowledge Engineering Laboratory,
 * Institute of Informatics and Telecommunications,
 * National Centre for Scientific Research "Demokritos",
 * Aghia Paraskevi, Athens, Greece.
 * e-mail: petasis@iit.demokritos.gr
 *            and
 * Laurent Riesterer, Rennes, France.
 * e-mail: laurent.riesterer@free.fr
 *
 * The following terms apply to all files associated
 * with the software unless explicitly disclaimed in individual files.
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 */

#ifndef _TKDND
#define _TKDND

#include <string.h>
#include <tk.h>

/*
 * If we aren't in 8.4, don't use 8.4 constness
 */
#ifndef CONST84
#define CONST84
#endif

#if (TCL_MAJOR_VERSION > 8) || ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4))
#   define HAVE_TCL84
#endif
#if (TCL_MAJOR_VERSION > 8) || ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 5))
#   define HAVE_TCL85
#endif

/*
 * If "VERSION" is not defined, place a default value...
 */
#ifndef VERSION
#ifdef PACKAGE_VERSION
#define VERSION PACKAGE_VERSION
#else
#define VERSION "1.0"
#endif
#endif

#ifdef __WIN32__
#   include <windows.h>
#   include <ole2.h>
#   include <tkPlatDecls.h>

#   ifndef Tk_GetHWND
	extern "C" HWND Tk_GetHWND(Window win);
#   endif /* Tk_GetHWND */
#endif /* __WIN32__ */

/*
 * These are passed to Tcl_PkgProvide...
 */
#define TKDND_PACKAGE	"tkdnd"
#define TKDND_VERSION	VERSION

/*
 * Maximum length of the action descriptions list...
 */
#define TKDND_MAX_DESCRIPTIONS_LENGTH 1034
#define TKDND_MAX_DESCRIPTIONS_LENGTH_STR "1024"

/*
 * These are used for selecting the most specific events.
 * (Laurent please add description :-)
 */
#define TKDND_SOURCE               0
#define TKDND_GETDATA              1
#define TKDND_GETCURSOR            2
#define TKDND_TARGET              10
#define TKDND_DRAGENTER           11
#define TKDND_DRAGLEAVE           12
#define TKDND_DRAG                13
#define TKDND_DROP                14
#define TKDND_ASK                 15

/* TODO: remove
#define TKDND_MODS        0x000000FF
#define TKDND_BUTTONS     0x00001F00
*/

/*
 * Debug Facilities...
 */
#ifdef  DND_DEBUG
#include <stdio.h>
#ifdef __WIN32__
/*
 * Under Windows, we keep a log in a file. (Laurent, 09/07/2000)
 */
extern FILE *TkDND_Log;
#define XDND_DEBUG(a)          \
  fprintf(TkDND_Log, "%s, %d: " a,__FILE__,__LINE__);          fflush(TkDND_Log)
#define XDND_DEBUG2(a,b)       \
  fprintf(TkDND_Log, "%s, %d: " a,__FILE__,__LINE__,b);        fflush(TkDND_Log)
#define XDND_DEBUG3(a,b,c)     \
  fprintf(TkDND_Log, "%s, %d: " a,__FILE__,__LINE__,b,c);      fflush(TkDND_Log)
#define XDND_DEBUG4(a,b,c,d)   \
  fprintf(TkDND_Log, "%s, %d: " a,__FILE__,__LINE__,b,c,d);    fflush(TkDND_Log)
#define XDND_DEBUG5(a,b,c,d,e) \
  fprintf(TkDND_Log, "%s, %d: " a,__FILE__,__LINE__,b,c,d,e);  fflush(TkDND_Log)
#define XDND_DEBUG6(a,b,c,d,e,f) \
  fprintf(TkDND_Log, "%s, %d: " a,__FILE__,__LINE__,b,c,d,e,f);fflush(TkDND_Log)
#else /* __WIN32__ */
/*
 * Under Unix, we just write messages to stdout...
 */
#define XDND_DEBUG(a)          \
  printf("%s, %d: " a,__FILE__,__LINE__);           fflush(stdout)
#define XDND_DEBUG2(a,b)       \
  printf("%s, %d: " a,__FILE__,__LINE__,b);         fflush(stdout)
#define XDND_DEBUG3(a,b,c)     \
  printf("%s, %d: " a,__FILE__,__LINE__,b,c);       fflush(stdout)
#define XDND_DEBUG4(a,b,c,d)   \
  printf("%s, %d: " a,__FILE__,__LINE__,b,c,d);     fflush(stdout)
#define XDND_DEBUG5(a,b,c,d,e) \
  printf("%s, %d: " a,__FILE__,__LINE__,b,c,d,e);   fflush(stdout)
#define XDND_DEBUG6(a,b,c,d,e,f) \
  printf("%s, %d: " a,__FILE__,__LINE__,b,c,d,e,f); fflush(stdout)
#endif /* __WIN32__ */
#else  /* DND_DEBUG */
/*
 * Debug is not enabled. Just do nothing :-)
 */
#define XDND_DEBUG(a)
#define XDND_DEBUG2(a,b)
#define XDND_DEBUG3(a,b,c)
#define XDND_DEBUG4(a,b,c,d)
#define XDND_DEBUG5(a,b,c,d,e)
#endif /* DND_DEBUG */

typedef struct _DndType {
  int              priority;          /* For target types, check priorities */
#ifdef    __WIN32__
  CLIPFORMAT       type;              /* Clipboard format (Windows)*/
  CLIPFORMAT       matchedType;       /* Clipboard format (Windows)*/
#else  /* __WIN32__ */
  Atom             type;              /* Clipboard format (Unix)*/
  Atom             matchedType;       /* Clipboard format (Unix)*/
#endif /* __WIN32__ */
  char            *typeStr;           /* Name of type */
  unsigned long    eventType;         /* Type of event */
  unsigned long    eventMask;         /* Modifiers of event */
  char            *script;            /* Script to run */
  struct _DndType *next;              /* Next one in list */
  short            EnterEventSent;    /* Have we send an <DragEnter> event? */
} DndType;

typedef struct _DndInfo {
  Tcl_Interp      *interp;            /* The associated interp */
  Tk_Window        topwin;            /* The main Tk window */
  Tk_Window        tkwin;             /* The associated Tk window */
  DndType          head;              /* Head of the list of supported types */
#ifdef    __WIN32__
  LPDROPTARGET     DropTarget;        /* The OLE IDropTarget object */
#endif /* __WIN32__ */
  DndType         *cbData;
  Tcl_HashEntry   *hashEntry;         /* Hash table entry */
} DndInfo;

typedef struct _DndClass {
  Tk_Window    MainWindow;          /* The main window of our application */
  Tcl_Interp  *interp;              /* A Tcl Interpreter */
  Display     *display;             /* Display Pointer */
} DndClass;

#ifndef LONG
#define LONG long
#endif /* LONG */

#ifdef MAC_TCL
/*
 * Anybody ?
 */
#endif /* MAC_TCL */

#endif /* _TKDND */
