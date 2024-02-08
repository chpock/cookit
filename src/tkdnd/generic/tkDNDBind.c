/*
 * tkDNDBind.c --
 * 
 *    This file implements a drag&drop mechanish for the tk toolkit.
 *    It contains the top-level command routines as well as some needed
 *    functions for defining and removing bind scripts.
 *
 * This software is copyrighted by:
 * George Petasis, National Centre for Scientific Research "Demokritos",
 * Aghia Paraskevi, Athens, Greece.
 * e-mail: petasis@iit.demokritos.gr
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

#include <ctype.h>
#include <string.h>
#include <tcl.h>
#include <tk.h>
#include "tkDND.h"

#ifndef __WIN32__
#include "XDND.h"
#else
#include "OleDND.h"
#endif
extern DndClass *dnd;

#ifdef __WIN32__
#define TKDND_MODS        (MK_SHIFT | MK_ALT | MK_CONTROL)
#define TKDND_BUTTONS     (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)
#else /* not __WIN32__ */
#define TKDND_MODS        0x000000FF
#define TKDND_BUTTONS     0x00001F00
#endif /* __WIN32__ */


static char *TkDND_GetField _ANSI_ARGS_((char *p, char *copy, int size));
#ifdef __WIN32__
static int   TkDND_FindScript _ANSI_ARGS_((DndInfo *infoPtrPtr, char *typeStr,
                  CLIPFORMAT typelist[], unsigned long eventType,
                  unsigned long eventMask, DndType **typePtrPtr));
extern int   TkDND_FindMatchingScript(Tcl_HashTable *table,
                  char *windowPath, char *typeStr, CLIPFORMAT typelist[],
                  unsigned long eventType, unsigned long eventMask,
                  int matchExactly, DndType **typePtrPtr, DndInfo **infoPtrPtr);
#else /* not __WIN32__ */
static int   TkDND_FindScript _ANSI_ARGS_((DndInfo *infoPtrPtr, char *typeStr,
                  Atom typelist[], unsigned long eventType,
                  unsigned long eventMask, DndType **typePtrPtr));
extern int   TkDND_FindMatchingScript(Tcl_HashTable *table,
                  char *windowPath, char *typeStr, Atom typelist[],
                  unsigned long eventType, unsigned long eventMask,
                  int matchExactly, DndType **typePtrPtr, DndInfo **infoPtrPtr);
#endif /* __WIN32__ */
extern char *TkDND_TypeToString(int type);

/*
 *---------------------------------------------------------------------------
 *
 * TkDND_ParseEventDescription -- (Laurent Riesterer 06/07/2000)
 *
 *        Fill Pattern buffer with information about event from
 *        event string.
 *
 * Results:
 *        Leaves error message in interp and returns 0 if there was an
 *        error due to a badly formed event string.
 *
 * Side effects:
 *        On exit, eventStringPtr points to rest of event string (after the
 *        closing '>', so that this procedure can be called repeatedly to
 *        parse all the events in the entire sequence.
 *
 *---------------------------------------------------------------------------
 */
int
TkDND_ParseEventDescription(
    Tcl_Interp *interp,          /* For error messages. */
    char *eventStringPtr,        /* Hold the event string. */
    unsigned long *eventTypePtr, /* Type of operation */
    unsigned long *eventMaskPtr) /* Filled with event mask of matched event. */
{
  char *p;
#define FIELD_SIZE 48
  char field[FIELD_SIZE];

  p = eventStringPtr;
  if (*p++ != '<') {
    goto error;
  }
  *eventMaskPtr = 0;

  while (1) {
    p = TkDND_GetField(p, field, FIELD_SIZE);

#ifdef __WIN32__
    if      (strcmp(field, "Shift")   == 0) { *eventMaskPtr |= MK_SHIFT; }
    else if (strcmp(field, "Control") == 0) { *eventMaskPtr |= MK_CONTROL; }
    else if (strcmp(field, "Alt")     == 0) { *eventMaskPtr |= MK_ALT; }

    else if (strcmp(field, "Button1") == 0) { *eventMaskPtr |= MK_LBUTTON; }
    else if (strcmp(field, "Button2") == 0) { *eventMaskPtr |= MK_MBUTTON; }
    else if (strcmp(field, "Button3") == 0) { *eventMaskPtr |= MK_RBUTTON; }

#else /* not __WIN32__ */
    if      (strcmp(field, "Shift")   == 0) { *eventMaskPtr |= ShiftMask; }
    else if (strcmp(field, "Control") == 0) { *eventMaskPtr |= ControlMask; }
    else if (strcmp(field, "Alt")     == 0) {
      *eventMaskPtr |= dnd->Alt_ModifierMask; }
    else if (strcmp(field, "Meta")    == 0) {
      *eventMaskPtr |= dnd->Meta_ModifierMask; }

    else if (strcmp(field, "Button1") == 0) { *eventMaskPtr |= Button1Mask; }
    else if (strcmp(field, "Button2") == 0) { *eventMaskPtr |= Button2Mask; }
    else if (strcmp(field, "Button3") == 0) { *eventMaskPtr |= Button3Mask; }
    else if (strcmp(field, "Button4") == 0) { *eventMaskPtr |= Button4Mask; }
    else if (strcmp(field, "Button5") == 0) { *eventMaskPtr |= Button5Mask; }

    else if (strcmp(field, "Mod1")    == 0) { *eventMaskPtr |= Mod1Mask; }
    else if (strcmp(field, "Mod2")    == 0) { *eventMaskPtr |= Mod2Mask; }
    else if (strcmp(field, "Mod3")    == 0) { *eventMaskPtr |= Mod3Mask; }
    else if (strcmp(field, "Mod4")    == 0) { *eventMaskPtr |= Mod4Mask; }
    else if (strcmp(field, "Mod5")    == 0) { *eventMaskPtr |= Mod5Mask; }
#endif

    else break;
  }

  if      (strcmp(field, "DragEnter") == 0) { *eventTypePtr = TKDND_DRAGENTER; }
  else if (strcmp(field, "DragLeave") == 0) { *eventTypePtr = TKDND_DRAGLEAVE; }
  else if (strcmp(field, "Drag")      == 0) { *eventTypePtr = TKDND_DRAG; }
  else if (strcmp(field, "Drop")      == 0) { *eventTypePtr = TKDND_DROP; }
  else if (strcmp(field, "Ask")       == 0) { *eventTypePtr = TKDND_ASK; }
  else {
    *eventTypePtr = 0;
    goto error;
  }

  if (p[0] != '>' || p[1] != '\0') {
    goto error;
  }

  return TCL_OK;

error:
  Tcl_SetResult(interp, "invalid event type \"", TCL_STATIC);
  Tcl_AppendResult(interp, eventStringPtr, "\"", (char *) NULL);
  return TCL_ERROR;
} /* TkDND_ParseEventDescription */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_GetField -- (Laurent Riesterer 06/07/2000)
 *
 *        Used to parse pattern descriptions.  Copies up to
 *        size characters from p to copy, stopping at end of
 *        string, space, "-", ">", or whenever size is
 *        exceeded.
 *
 * Results:
 *        The return value is a pointer to the character just
 *        after the last one copied (usually "-" or space or
 *        ">", but could be anything if size was exceeded).
 *        Also places NULL-terminated string (up to size
 *        character, including NULL), at copy.
 *
 * Side effects:
 *        None.
 *
 *----------------------------------------------------------------------
 */

static char *
TkDND_GetField(
    char *p,           /* Pointer to part of pattern. */
    char *copy,        /* Place to copy field. */
    int size)          /* Maximum number of characters to copy. */
{
#define UCHAR(c) ((unsigned char) (c))

  while ((*p != '\0') && !isspace(UCHAR(*p)) && (*p != '>') &&
         (*p != '-')  && (size > 1)) {
    *copy = *p;
    p++;
    copy++;
    size--;
  }
  *copy = '\0';

  while ((*p == '-') || isspace(UCHAR(*p))) {
    p++;
  }
  return p;
} /* TkDND_GetField */

/*
 *----------------------------------------------------------------------
 * TkDND_FindMatchingScript -- (Laurent Riesterer 06/07/2000)
 *
 *        Find the closest matching script for the type handler
 *        of the given window.
 *
 * Results:
 *        A standard TCL result.
 *
 *----------------------------------------------------------------------
 */
int
TkDND_FindMatchingScript(
    Tcl_HashTable *table,        /* Hash table containing the bindings 
                                    if NULL, then *infoPtrPtr MUST contain
                                    the infoPtr */
    char *windowPath,            /* Name of window (hash key) */
    char *typeStr,               /* textual representation of type
                                    (NULL if not used) */
#ifdef __WIN32__
    CLIPFORMAT typelist[],         /* Search for this type */
#else
    Atom typelist[],             /* Array for possibles matching types */
#endif
    unsigned long eventType,     /* Type of event */
    unsigned long eventMask,     /* Mask of event */
    int matchExactly,            /* Not zero if need an exact match */
    DndType **typePtrPtr,        /* if not NULL, store DndType */
    DndInfo **infoPtrPtr)        /* if not NULL, store DndInfo */
{
  Tcl_HashEntry *hPtr;
  DndInfo *infoPtr;
  int found;

  if (typePtrPtr != NULL) *typePtrPtr = NULL;
  /* remove state we don't use */
  eventMask = eventMask & (TKDND_MODS | TKDND_BUTTONS);

  if (table != NULL) {
    if (infoPtrPtr != NULL) *infoPtrPtr = NULL;
    hPtr = Tcl_FindHashEntry(table, windowPath);
    if (hPtr == NULL) {
      XDND_DEBUG2("<FindScript> Cannot locate wanted widget '%s'\n",
                  windowPath);
      if (infoPtrPtr != NULL) *infoPtrPtr = NULL;
      return TCL_OK;
    }

    infoPtr = (DndInfo *) Tcl_GetHashValue(hPtr);
    if (infoPtrPtr != NULL) {
      XDND_DEBUG2("<FindScript> setting infoPtr = %p\n", infoPtr);
      *infoPtrPtr = infoPtr;
    }
  } else {
    if (infoPtrPtr != NULL) {
      infoPtr = *infoPtrPtr;
    } else {
      return TCL_OK;
    }
  }

  /*
   * First try to find and exact match
   */
  found = TkDND_FindScript(infoPtr, typeStr, typelist, eventType, eventMask,
                          typePtrPtr);
  if (found) {
    XDND_DEBUG("<FindScript> -------------\n");
    return TCL_OK;
  } else if (matchExactly) {
    if (infoPtr != NULL) {
      Tcl_SetResult(infoPtr->interp, "script not found", TCL_STATIC);
    }
    return TCL_ERROR;
  }

  /*
   * Remove modifiers
   */
  if ((eventMask & ~TKDND_MODS) != eventMask) {
    XDND_DEBUG("<FindScript> ---- no mods -------------\n");
    found = TkDND_FindScript(infoPtr, typeStr, typelist, eventType,
                            eventMask & ~TKDND_MODS, typePtrPtr);
    if (found) {
      return TCL_OK;
    }
#ifdef DND_DEBUG
  } else {
    XDND_DEBUG("<FindScript> ---- skipping no mods -------------\n");
#endif /* DND_DEBUG */
  }

  /*
   * Remove buttons
   */
  if (((eventMask & ~TKDND_BUTTONS) != eventMask) && 
      ((eventMask & ~TKDND_BUTTONS) != (eventMask & ~TKDND_MODS))) {
    XDND_DEBUG("<FindScript> ---- no buttons -------------\n");
    found = TkDND_FindScript(infoPtr, typeStr, typelist, eventType,
                            eventMask & ~TKDND_BUTTONS, typePtrPtr);
    if (found) {
      return TCL_OK;
    }
#ifdef DND_DEBUG
  } else {
    XDND_DEBUG("<FindScript> ---- skippinp no buttons -------------\n");
#endif /* DND_DEBUG */
  }

  /*
   * Remove modifiers & buttons
   */
  if ((eventMask != 0 && ((eventMask & ~TKDND_BUTTONS) != 0) &&
      (eventMask & ~TKDND_MODS) != 0)) {
    XDND_DEBUG("<FindScript> ---- no mods, no buttons -------------\n");
    found = TkDND_FindScript(infoPtr, typeStr, typelist, eventType,
                            0, typePtrPtr);
    if (found) {
      return TCL_OK;
    }
  } else {
#ifdef DND_DEBUG
    XDND_DEBUG("<FindScript> ---- skipping no mods, no buttons ------------\n");
#endif /* DND_DEBUG */
  }

  /*
   * Last chance: try to find if a Drop event is associated with the type
   * We indicate its only to set the desired type by resetting the infoPtr
   */
  if ((eventType == TKDND_DRAGENTER || eventType == TKDND_DRAG) &&
      !matchExactly) {
    XDND_DEBUG("<FindScript> ---- recurse for DRAG_ENTER -------------\n");
    TkDND_FindMatchingScript(table, windowPath, typeStr, typelist,
                             TKDND_DROP, eventMask, False,
                             typePtrPtr, infoPtrPtr);
    *infoPtrPtr = NULL;
  }
  XDND_DEBUG("<FindScript> -------------\n");
/* if (!found && infoPtrPtr != NULL) {
     *infoPtrPtr = NULL;
   }*/
  return TCL_OK;
} /* TkDND_FindMatchingScript */

/*
 *----------------------------------------------------------------------
 * TkDND_FindScript -- (Laurent Riesterer 06/07/2000)
 *
 *       TODO: Laurent please add description... 
 *
 * Results:
 *        A standard TCL result.
 *
 *----------------------------------------------------------------------
 */
static int
TkDND_FindScript(
    DndInfo *infoPtr,
    char *typeStr,                /* textual representation of type
                                     (NULL if not used) */
#ifdef __WIN32__
    CLIPFORMAT typelist[],        /* Search for this type */
#else
    Atom       typelist[],        /* Array for possibles matching types */
#endif
    unsigned long eventType,      /* Type of event */
    unsigned long eventMask,      /* Mask of event */
    DndType **typePtrPtr)         /* if not NULL, store DndType */
{
  DndType *curr;
  int found = 0, i;
  char *AtomName, *formatName;

  AtomName = NULL; /* Avoid warnings under windows... */
  i = 0;           /* Avoid warnings under windows... */

#ifdef __WIN32__
  XDND_DEBUG4("<FindScript> start search: type=%s / event=%d / mask=%08X\n",
                 typeStr, eventType, eventMask); 
#else /* __WIN32__ */
  XDND_DEBUG4("<FindScript> start search: type='%s' / type=%ld, mask=%08X\n"
              "   list=", typeStr, eventType, (unsigned int) eventMask);
#ifdef DND_DEBUG
  for (i = 0; typelist && typelist[i]; i++) {
    AtomName = TkDND_TypeToString(typelist[i]);
    XDND_DEBUG3("   Type[%d] = \"%s\"\n", i, AtomName);
  }
#endif /* DND_DEBUG */
#endif /* __WIN32__ */
  for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
    curr->matchedType = 0;
    /* Test types */
    if (typeStr != NULL) {
      XDND_DEBUG4("<FindScript>     for '%s', %ld / %08x\n", curr->typeStr,
                  curr->eventType, (unsigned int) curr->eventMask);
      if (curr->eventType == eventType && curr->eventMask == eventMask &&
          Tcl_StringCaseMatch(typeStr, curr->typeStr, 1)) {
        found = 1;
        break;
      }
    } else {

#ifndef __WIN32__
#ifdef DND_DEBUG
      AtomName = "<string match>";
      if (curr->type) AtomName = XGetAtomName(dnd->display, curr->type);
      XDND_DEBUG5("<FindScript>     prio=%3d / type='%-20s' / event=%2ld / "
                  "mask=%08x\n", curr->priority, AtomName,
                  curr->eventType, (unsigned int) curr->eventMask);
      if (curr->type) XFree(AtomName);
#endif /* DND_DEBUG */
      for (i = 0; typelist && typelist[i] != None; i++) {
        if (curr->eventType == eventType && curr->eventMask == eventMask) {
          if (curr->type == typelist[i]) {
            found = 1;
            break;
          } else if (curr->type == None) {
            /*
             * If the type is None is a signal that string matching is involved
             */
            AtomName = (char *) Tk_GetAtomName(infoPtr->tkwin, typelist[i]);
            if (Tcl_StringCaseMatch(AtomName, curr->typeStr, 1)) {
              found = 1;
              curr->matchedType = typelist[i];
              /*
               * Since now we have a match, perhaps we can select a type that
               * we know how to get the data?
               */
              for (i = 0; typelist && typelist[i]; i++) {
                AtomName = (char *) Tk_GetAtomName(infoPtr->tkwin, typelist[i]);
                if (!strcmp(AtomName,"text/plain;charset=UTF-8") ||
                    !strcmp(AtomName,"text/plain")               ||
                    !strcmp(AtomName,"STRING")                   ||
                    !strcmp(AtomName,"TEXT")                     ||
                    !strcmp(AtomName,"COMPOUND_TEXT")            ||
                    !strcmp(AtomName,"CF_UNICODETEXT")           ||
                    !strcmp(AtomName,"CF_TEXT")                  ||
                    !strcmp(AtomName,"CF_OEMTEXT")               ||
                    !strcmp(AtomName,"text/uri-list")            ||
                    !strcmp(AtomName,"text/file")                ||
                    !strcmp(AtomName,"text/url")                 ||
                    !strcmp(AtomName,"url/url")                  ||
                    !strcmp(AtomName,"FILE_NAME")                ||
                    !strcmp(AtomName,"SGI_FILE")                 ||
                    !strcmp(AtomName,"_NETSCAPE_URL")            ||
                    !strcmp(AtomName,"_MOZILLA_URL")             ||
                    !strcmp(AtomName,"_SGI_URL")                 ||
                    !strcmp(AtomName,"CF_HDROP")                 ||
                    !strcmp(AtomName,"CF_DIB")) {
                  curr->matchedType = typelist[i]; break;
                }
              } /* for (i = 0; typelist && typelist[i]; i++) */
              break;
            }
          }
        }
      }
      if (found) break;
#else /* __WIN32__ */
      for (i = 0; typelist && typelist[i]; i++) {
        if (curr->eventType == eventType && curr->eventMask == eventMask) {
          if (curr->type == typelist[i]) {
            found = 1;
            break;
          } else if (curr->type == 0) {
            /*
             * If the type is 0 is a signal that string matching is involved...
             */
            formatName = TkDND_TypeToString(typelist[i]);
            if (Tcl_StringCaseMatch(formatName, curr->typeStr, 1)) {
              found = 1;
              curr->matchedType   = typelist[i];
              dnd->DesiredTypeStr = formatName;
              /*
               * Since now we have a match, perhaps we can select a type that
               * we know how to get the data?
               */
              for (i = 0; typelist && typelist[i]; i++) {
                switch (typelist[i]) { /* Sorted according to preference */
                  case CF_UNICODETEXT: {
                    curr->matchedType   =typelist[i];
                    dnd->DesiredTypeStr =TkDND_TypeToString(typelist[i]);break;}
                  case CF_TEXT:
                  case CF_OEMTEXT:     {
                    /* Are we sure CF_UNICODETEXT is not supported? */
                    int j;
                    for (j=0; typelist[j]; j++) {
                      if (typelist[j] == CF_UNICODETEXT) {i = j; break;}
                    }
                    if (typelist[i] == CF_OEMTEXT) {
                      /* CF_UNICODETEXT is not supported. Perhaps CF_TEXT is? */
                      for (j=0; typelist[j]; j++) {
                        if (typelist[j] == CF_TEXT) {i = j; break;}
                      }
                    }
                    curr->matchedType   =typelist[i];
                    dnd->DesiredTypeStr =TkDND_TypeToString(typelist[i]);break;}
                  case CF_HDROP:       {
                    curr->matchedType   =typelist[i];
                    dnd->DesiredTypeStr =TkDND_TypeToString(typelist[i]);break;}
                  case CF_DIB:         {
                    curr->matchedType   =typelist[i];
                    dnd->DesiredTypeStr =TkDND_TypeToString(typelist[i]);break;}
                  default:             {
                    if (typelist[i] == dnd->UniformResourceLocator ||
                        typelist[i] == dnd->FileName ||
                        typelist[i] == dnd->HTML_Format||
                        typelist[i] == dnd->RichTextFormat) {
                      curr->matchedType   =typelist[i];
                      dnd->DesiredTypeStr =TkDND_TypeToString(typelist[i]);
                      break;
                    }
                    continue;
                  }
                }
                break;
              }
              break;
            }
          }
        }
      }
      if (found) break;
#endif /* __WIN32__ */
    }
  }
  XDND_DEBUG2("<FindScript> findmatching = %d\n", found);

  *typePtrPtr = curr;
  return found;
} /* TkDND_FindScript */

/*
 *----------------------------------------------------------------------
 * GetCurrentScript -- (Laurent Riesterer 06/07/2000)
 *
 *        Set interp result with the current script for the type handler
 *        of the given window.
 *
 * Results:
 *        A standard TCL result.
 *
 *----------------------------------------------------------------------
 */
int TkDND_GetCurrentScript(Tcl_Interp *interp, Tk_Window topwin,
                           Tcl_HashTable *table, char *windowPath,
                           char *typeStr,
                           unsigned long eventType, unsigned long eventMask) {
  Tk_Window tkwin;
  DndType *typePtr;

  tkwin = Tk_NameToWindow(interp, windowPath, topwin);
  if (tkwin == NULL) {
    return TCL_ERROR;
  }

  if (TkDND_FindMatchingScript(table, windowPath, typeStr, NULL,
                        eventType, eventMask, True, &typePtr, NULL) != TCL_OK) {
    return TCL_ERROR;
  }

  Tcl_SetResult(interp, typePtr->script, TCL_VOLATILE);
  return TCL_OK;
} /* TkDND_GetCurrentScript */

/*
 * End of File.
 */
