/*
 * tkXDND.cpp --
 * 
 *    This file implements the unix portion of the drag&drop mechanish
 *    for the tk toolkit. The protocol in use under unix is the
 *    XDND protocol.
 *    Also, partial support for Motif drops exists.
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
#include <string.h>
 
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xatom.h>

#include <tcl.h>
#include <tk.h>
#include "tkDND.h"
#include "XDND.h"
#include "Cursors.h"

extern Tcl_HashTable   TkDND_TargetTable;
extern Tcl_HashTable   TkDND_SourceTable;
DndClass              *dnd;
static XErrorHandler   PreviousErrorHandler = NULL;
static unsigned long   FirstProtectRequest;
static Tk_Window       ProtectionOwnerWindow;

extern int   TkDND_DelHandler(DndInfo *infoPtr, char *typeStr,
                    unsigned long eventType, unsigned long eventMask);
extern int   TkDND_DelHandlerByName(Tcl_Interp *interp, Tk_Window topwin,
                    Tcl_HashTable *table, char *windowPath, char *typeStr,
                    unsigned long eventType, unsigned long eventMask);
extern void  TkDND_DestroyEventProc(ClientData clientData, XEvent *eventPtr);
extern int   TkDND_FindMatchingScript(Tcl_HashTable *table,
                    char *windowPath, char *typeStr, Atom typelist[],
                    unsigned long eventType, unsigned long eventMask,
                    int matchExactly, DndType **typePtrPtr,
                    DndInfo **infoPtrPtr);
extern int   TkDND_GetCurrentTypes(Tcl_Interp *interp, Tk_Window topwin,
                                    Tcl_HashTable *table, char *windowPath);
extern void  TkDND_ExpandPercents(DndInfo *infoPtr, DndType *typePtr,
                                    char *before, Tcl_DString *dsPtr,
                                    LONG x, LONG y);
extern int   TkDND_ExecuteBinding(Tcl_Interp *interp, char *script,
                                    int numBytes, Tcl_Obj *data);
extern char *TkDND_GetDataAccordingToType(DndInfo *info, Tcl_Obj *ResultObj,
                                    DndType *typePtr, int *length);  
extern Tcl_Obj 
            *TkDND_CreateDataObjAccordingToType(DndType *typePtr, void *info,
                                    unsigned char *data, int length);
extern int   TkDND_Update(Display *display, int idle);
#ifndef Tk_CreateClientMessageHandler
extern int   Tk_CreateClientMessageHandler(int (*proc) (Tk_Window tkwin,
                                 XEvent *eventPtr));
#endif /* Tk_CreateClientMessageHandler */

/*
 * Forward declarations for procedures defined later in this file:
 */
static int   TkDND_WidgetExists (DndClass *dnd, Window window);
static int   TkDND_WidgetApplyEnter(DndClass *dnd, Window widgets_window,
               Window from, Atom action, int x, int y, Time t, Atom * typelist);
static int   TkDND_WidgetApplyPosition(DndClass *dnd, Window widgets_window,
               Window from, Atom action, Atom *actionList,
               int x, int y, Time t, Atom * typelist, int *want_position,
               Atom *supported_action, Atom *desired_type,
               XRectangle * rectangle);
static int   TkDND_WidgetApplyLeave(DndClass *dnd, Window widgets_window);
static int   TkDND_ParseAction(DndClass *dnd, DndInfo *infoPtr, DndType *typePtr,
               Atom default_action, Atom *supported_action,
               Atom *desired_type); /* Laurent Riesterer 06/07/2000 */
static int   TkDND_WidgetInsertDrop(DndClass *dnd, unsigned char *data,
               int length, int remaining, Window into, Window from, Atom type);
static int   TkDND_WidgetAsk(DndClass *dnd, Window source, Window target,
               Atom *action);
static int   TkDND_WidgetGetData(DndClass *dnd, Window source, 
               unsigned char **data, int *length, Atom type);
static void  TkDND_HandleEvents (DndClass *dnd, XEvent *xevent);
static Atom *TkDND_GetCurrentAtoms(XDND *dnd, Window window);
static int   TkDND_SetCursor(DndClass *dnd, int cursor);
static int   TkDND_XDNDHandler(Tk_Window winPtr, XEvent *eventPtr);
       int   TkDND_AddHandler(Tcl_Interp *interp, Tk_Window topwin,
               Tcl_HashTable *table, char *windowPath, char *typeStr,
               unsigned long eventType, unsigned long eventMask,
               char *script, int isDropTarget, int priority);
       char *TkDND_GetCurrentActionName(void);
       int   TkDND_DndDrag(Tcl_Interp *interp, char *windowPath, int button,
               Tcl_Obj *Actions, char *Descriptions, Tk_Window cursor_window,
               char *cursor_callback);
static void  TkDND_StartProtectedSection(Display *display, Tk_Window window);
static void  TkDND_EndProtectedSection(Display *display);
#ifdef TKDND_ENABLE_SHAPE_EXTENSION
int          Shape_Init(Tcl_Interp *interp);
#endif /* TKDND_ENABLE_SHAPE_EXTENSION */

Cursor noDropCursor = 0, moveCursor = 0, copyCursor = 0, linkCursor = 0,
       askCursor = 0;

/*
 *----------------------------------------------------------------------
 *
 * TkDND_TypeToString --
 *
 *       Returns a string corresponding to the given type atom.
 *
 * Results:
 *       A string in a static memory area holding the string.
 *       Should never be freed or modified.
 *
 * Side effects:
 *       -
 *
 *----------------------------------------------------------------------
 */
char *TkDND_TypeToString(int type) {
  return (char *) Tk_GetAtomName(dnd->MainWindow, (Atom) type);
} /* TkDND_TypeToString */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_StringToType --
 *
 *       Returns a type code corresponding to the given type string.
 *
 * Results:
 *
 * Side effects:
 *       -
 *
 *----------------------------------------------------------------------
 */
Atom TkDND_StringToType(char *typeStr) {
    return Tk_InternAtom(dnd->MainWindow, typeStr);
} /* TkDND_StringToType */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_Init --
 *
 *       Initialize the drag and drop platform specific protocol.
 *       Under unix, this is the XDND protocol.
 *
 * Results:
 *       A structure that holds platform specific information.
 *
 * Side effects:
 *       A set of new cursors is created and a ClientMessage handler
 *       for the XDND protocol is registered...
 *
 *----------------------------------------------------------------------
 */
DndClass *TkDND_Init(Tcl_Interp *interp, Tk_Window topwin) {
    DndClass *dndp;
    XColor black, white;
    Pixmap image_pixmap, mask_pixmap;

    dndp = XDND_Init(Tk_Display(topwin));
    if (dndp == NULL) return NULL;
    dndp->MainWindow = topwin;

    /* Register the callback functions... */
    dndp->WidgetExistsCallback         = &TkDND_WidgetExists;
    dndp->WidgetApplyEnterCallback     = &TkDND_WidgetApplyEnter;
    dndp->WidgetApplyPositionCallback  = &TkDND_WidgetApplyPosition;
    dndp->WidgetApplyLeaveCallback     = &TkDND_WidgetApplyLeave;
    dndp->WidgetInsertDropDataCallback = &TkDND_WidgetInsertDrop;
    dndp->Ask                          = &TkDND_WidgetAsk;
    dndp->GetData                      = &TkDND_WidgetGetData;
    dndp->HandleEvents                 = &TkDND_HandleEvents;
    dndp->GetDragAtoms                 = &TkDND_GetCurrentAtoms;
    dndp->SetCursor                    = &TkDND_SetCursor; 
  
    /* Create DND Cursors... */
    dndp->cursors = (XDNDCursor *) Tcl_Alloc(sizeof(XDNDCursor)*6);
    black.pixel = BlackPixel(dndp->display, DefaultScreen(dndp->display));
    white.pixel = WhitePixel(dndp->display, DefaultScreen(dndp->display));
    XQueryColor(dndp->display, DefaultColormap(dndp->display, 
            DefaultScreen(dndp->display)), &black);
    XQueryColor(dndp->display, DefaultColormap(dndp->display,
            DefaultScreen(dndp->display)), &white);
    /* No Drop Cursor */
    image_pixmap = XCreateBitmapFromData(dndp->display, dndp->RootWindow,
            (char *) noDropCurBits, noDropCursorWidth, noDropCursorHeight);
    mask_pixmap  = XCreateBitmapFromData(dndp->display, dndp->RootWindow,
            (char *) noDropCurMask, noDropCursorWidth, noDropCursorHeight);
    noDropCursor = XCreatePixmapCursor (dndp->display, image_pixmap,
            mask_pixmap, &black, &white, noDropCursorX, noDropCursorY);
    XFreePixmap (dndp->display, image_pixmap);
    XFreePixmap (dndp->display, mask_pixmap);
    dndp->cursors[0].cursor = noDropCursor;
    /* Copy Cursor */
    image_pixmap = XCreateBitmapFromData(dndp->display, dndp->RootWindow,
            (char *) CopyCurBits, CopyCursorWidth, CopyCursorHeight);
    mask_pixmap  = XCreateBitmapFromData(dndp->display, dndp->RootWindow,
            (char *) CopyCurMask, CopyCursorWidth, CopyCursorHeight);
    copyCursor   = XCreatePixmapCursor (dndp->display, image_pixmap,
            mask_pixmap, &black, &white, CopyCursorX, CopyCursorY);
    XFreePixmap (dndp->display, image_pixmap);
    XFreePixmap (dndp->display, mask_pixmap);
    dndp->cursors[1].cursor = copyCursor;
    /* Move Cursor */
    image_pixmap = XCreateBitmapFromData(dndp->display, dndp->RootWindow,
            (char *) MoveCurBits, MoveCursorWidth, MoveCursorHeight);
    mask_pixmap  = XCreateBitmapFromData(dndp->display, dndp->RootWindow,
            (char *) MoveCurMask, MoveCursorWidth, MoveCursorHeight);
    moveCursor   = XCreatePixmapCursor (dndp->display, image_pixmap,
            mask_pixmap, &black, &white, MoveCursorX, MoveCursorY);
    XFreePixmap (dndp->display, image_pixmap);
    XFreePixmap (dndp->display, mask_pixmap);
    dndp->cursors[2].cursor = moveCursor;
    /* Link Cursor */
    image_pixmap = XCreateBitmapFromData(dndp->display, dndp->RootWindow,
            (char *) LinkCurBits, LinkCursorWidth, LinkCursorHeight);
    mask_pixmap  = XCreateBitmapFromData(dndp->display, dndp->RootWindow,
            (char *) LinkCurMask, LinkCursorWidth, LinkCursorHeight);
    linkCursor   = XCreatePixmapCursor (dndp->display, image_pixmap,
            mask_pixmap, &black, &white, LinkCursorX, LinkCursorY);
    XFreePixmap (dndp->display, image_pixmap);
    XFreePixmap (dndp->display, mask_pixmap);
    dndp->cursors[3].cursor = linkCursor;
    /* Ask Cursor */
    image_pixmap = XCreateBitmapFromData(dndp->display, dndp->RootWindow,
            (char *) AskCurBits, AskCursorWidth, AskCursorHeight);
    mask_pixmap  = XCreateBitmapFromData(dndp->display, dndp->RootWindow,
            (char *) AskCurMask, AskCursorWidth, AskCursorHeight);
    askCursor    = XCreatePixmapCursor (dndp->display, image_pixmap,
            mask_pixmap, &black, &white, AskCursorX, AskCursorY);
    XFreePixmap (dndp->display, image_pixmap);
    XFreePixmap (dndp->display, mask_pixmap);
    dndp->cursors[4].cursor = askCursor;
    /* Register Cursors... */

    /* Finally, register the XDND Handler... */
    Tk_CreateClientMessageHandler(&TkDND_XDNDHandler);

#ifdef TKDND_ENABLE_SHAPE_EXTENSION
    /* Load the shape extension... */
    if (Shape_Init(interp) != TCL_OK) {
        /* TODO: Provide the shape command, even if does not work... */
    }
#endif /* TKDND_ENABLE_SHAPE_EXTENSION */
    dnd = dndp;
    return dndp;
} /* TkDND_Init */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_WidgetExists --
 *
 *       Check if the given window belongs to this application or is a foreign
 *       window.
 *
 * Results:
 *       Returns 1 if the specified window belongs to this application.
 *       Else, returns 0.
 *
 * Side effects:
 *       Forces window creation if belonging to this application.
 *
 *----------------------------------------------------------------------
 */
static int TkDND_WidgetExists (DndClass * dnd, Window window) {
    Tk_Window tkwin;

    tkwin = Tk_IdToWindow(dnd->display, window);
    if (tkwin == NULL) {
        XDND_DEBUG2("++++ TkDND_WidgetExists: NOT OUR OWN WIDGET: %ld!\n",
                window);
        return False;
    }
    /*
     * Force the creation of the X-window...
     */
    Tk_MakeWindowExist(tkwin);
#ifdef DND_DEBUG
    {
        char *name;
        name = Tk_PathName(tkwin);
        if (name == NULL) name = "(tk internal/hidden window)";
        XDND_DEBUG3("++++ TkDND_WidgetExists: widget %s: %ld\n", name, window);
    }
#endif /* DND_DEBUG */
    return True;
} /* TkDND_WidgetExists */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_WidgetApplyEnter --
 *
 *       Executes the binding script for the <DragEnter> event, if such a
 *       binding exists for the specified window.
 *
 * Results:
 *       Returns True upon succesful execution.
 *       Else, returns False.
 *
 * Side effects:
 *       Whatever the binding script does :-)
 *
 *----------------------------------------------------------------------
 */
static int TkDND_WidgetApplyEnter(DndClass *dnd, Window widgets_window,
           Window from, Atom action, int x, int y, Time t, Atom *typelist) {
    Tk_Window tkwin;
    Tcl_DString dString;
    DndInfo *infoPtr;
    DndType *typePtr;
    char *pathName;
    int ret;

    XDND_DEBUG("  #### <DragEnter> #### (TkDND_WidgetApplyEnter)\n");
    /* 
     * Unless said otherwise, result is OK
     */
    dnd->CallbackStatus = TCL_OK;
    /* Has the previous window received a DragLeave event? */
    if (dnd->LastEnterDeliveredWindow != None) {
        tkwin = Tk_IdToWindow(dnd->display, dnd->LastEnterDeliveredWindow);
        if (tkwin != NULL) {
            pathName = Tk_PathName(tkwin);
            if (pathName != NULL) {
                XDND_DEBUG3("<DragEnter>: Delivering missed <DragLeave> to "
                        "window: %s %ld\n", pathName, 
                        dnd->LastEnterDeliveredWindow);
                TkDND_WidgetApplyLeave(dnd, dnd->LastEnterDeliveredWindow);
                /* Now, dnd->LastEnterDeliveredWindow should be none...*/
                if (dnd->LastEnterDeliveredWindow != None) {
                    XDND_DEBUG3("<DragEnter>: We missed a <DragLeave> on "
                            "window \"%s\" (%ld)\n", pathName,
                            dnd->LastEnterDeliveredWindow);
                }
                /* 
                 * Check we are still in the target window after processing
                 * event.  By Colin McDonald <mcdonald@boscombe.demon.co.uk>
                 * (14/12/2000)
                 */
                if (dnd->MouseWindow != widgets_window) {
                    XDND_DEBUG2("TkDND_WidgetApplyEnter: Left window %ld"
                            " before executing <DragEnter> script\n",
                            widgets_window);
                    return False;
                }
            }
        }
        dnd->LastEnterDeliveredWindow = None;
    }

    /*
     * Bug reported by Colin McDonald <mcdonald@boscombe.demon.co.uk>: There
     * is a problem with the call to this routine.  It is passed the address
     * of a typelist.  In the calling routines
     * (e.g. MotifDND_HandleClientMessage) this address is passed from
     * dnd->DraggerTypeList.  The first thing TkDND_WidgetApplyEnter does is
     * process any missed leave event for dnd->LastEnterDeliveredWindow.  If
     * the <DragLeave> script causes more events to be received (e.g. it does
     * an "update") then the dnd->DraggerTypeList can change, or most likely
     * is freed and set to a NULL pointer. But the typelist argument in
     * TkDND_WidgetApplyEnter is still pointing to the old memory location,
     * now invalid. This can cause Bad Atom errors when debugging, or a crash
     * in TkDND_FindMatchingScript due to looping through an invalid Atom list
     * with no terminating null.
     */
    typelist = dnd->DraggerTypeList;
    if (typelist == NULL) {
        XDND_DEBUG("<DragEnter>: Called with an empty typelist?\n");
        return False;
    }

    /* Get window path... */
    tkwin = Tk_IdToWindow(dnd->display, widgets_window);
    if (tkwin == NULL) {
        XDND_DEBUG("<DragEnter>: not a TK window!\n");
        return False;
    }
    pathName = Tk_PathName(tkwin);
    if (pathName == NULL) {
        /*
         * This is a tk "hidden" window...
         */
        XDND_DEBUG("<DragEnter>: This is a tk HIDDEN window\n");
        return False;
    }

    XDND_DEBUG2("<DragEnter>: state = %08X\n", dnd->state);

    /* Laurent Riesterer 06/07/2000: support for extended bindings */
    if (TkDND_FindMatchingScript(&TkDND_TargetTable, pathName,
            NULL, typelist, TKDND_DRAGENTER, dnd->state, False,
            &typePtr, &infoPtr) != TCL_OK) {
        goto error;
    }
    dnd->SupportedAction = action;
    if (infoPtr == NULL || typePtr == NULL) {
        XDND_DEBUG3("<DragEnter>: Script not found, info=%p, type=%p\n",
                infoPtr, typePtr);
        if (typePtr == NULL) {
            dnd->DesiredType = typelist[0];
        } else {
            dnd->DesiredType = typePtr->type;
        }
        if (dnd->DesiredType == None && typePtr) {
            /* This is a "string match" type... */
            dnd->DesiredType = typePtr->matchedType;
        }
        return True;
    }
    /* Send the <DragEnter> event... */
    dnd->interp = infoPtr->interp;
    if (dnd->DesiredType == 0 && typePtr) {
        dnd->DesiredType = typePtr->matchedType;
    }
    Tcl_DStringInit(&dString);
    TkDND_ExpandPercents(infoPtr, typePtr, typePtr->script, &dString, x, y);
    XDND_DEBUG2("<DragEnter>: %s\n", Tcl_DStringValue(&dString));
    ret = TkDND_ExecuteBinding(dnd->interp,
            Tcl_DStringValue(&dString), -1, NULL);
    XDND_DEBUG2("<DragEnter>: Return status: %d\n", ret);
    Tcl_DStringFree(&dString);
    if (ret == TCL_ERROR) {
        goto error;
    }
    typePtr->EnterEventSent = True; /* TODO */
    dnd->LastEnterDeliveredWindow = widgets_window;

    TkDND_ParseAction(dnd, infoPtr, typePtr, action,
            &(dnd->SupportedAction), &(dnd->DesiredType));

    return True;

    error:
    dnd->CallbackStatus = TCL_ERROR;
    XUngrabPointer(dnd->display, CurrentTime);
    Tk_BackgroundError(infoPtr->interp);
    while (Tcl_DoOneEvent(TCL_IDLE_EVENTS) != 0) {
        XDND_DEBUG("######## INSIDE LOOP - TkDND_WidgetApplyEnter!\n");
        /* Empty loop body */
    }
    return False;
} /* TkDND_WidgetApplyEnter */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_WidgetApplyPosition --
 *
 *       Executes the binding script for the <Drag> event, if such a
 *       binding exists for the specified window.
 *
 * Results:
 *       Returns True when script returns TCL_OK.
 *       Else (i.e. TCL_ERROR or TCL_BREAK), returns False, meaning that widget
 *       does not want the drop over THIS PARTICULAR coordinates.
 *
 * Side effects:
 *       Whatever the binding script does :-)
 *
 *----------------------------------------------------------------------
 */
static int TkDND_WidgetApplyPosition(DndClass *dnd, Window widgets_window,
           Window from, Atom action, Atom *actionList, int x, int y, Time t,
           Atom *typelist, int *want_position,
           Atom *supported_action, Atom *desired_type,
           XRectangle *rectangle) {
    Tk_Window tkwin;
    Tcl_DString dString;
    DndType *typePtr;
    DndInfo *infoPtr;
    char *pathName;
    int ret;

    dnd->CallbackStatus = TCL_OK;
    if (dnd->SupportedAction == None) dnd->SupportedAction = action;

    /* Get window path... */
    tkwin = Tk_IdToWindow(dnd->display, widgets_window);
    if (tkwin == NULL) {
        XDND_DEBUG("<Drag>: not a TK window!\n");
        return False;
    }
    pathName = Tk_PathName(tkwin);
    if (pathName == NULL) {
        /*
         * This is a tk "hidden" window...
         */
        XDND_DEBUG("<Drag>: This is a tk HIDDEN window\n");
        return False;
    }

    XDND_DEBUG2("<Drag>: state = %08X\n", dnd->state);
    if (TkDND_FindMatchingScript(&TkDND_TargetTable, pathName,
            NULL, typelist, TKDND_DRAG, dnd->state, False,
            &typePtr, &infoPtr) != TCL_OK) {
        goto error;
    }
    if (infoPtr == NULL || typePtr == NULL) {
        XDND_DEBUG3("<Drag>: Script not found, info=%p, type=%p\n",
                infoPtr, typePtr);
        if (typePtr == NULL) return False;
        if (infoPtr == NULL && typePtr == NULL) return False;
        /* 
         * Petasis, 10/07/2000: Bug where there was an X error when dragging
         * among different applications and the drop target did not have a
         * binding script for the <Drag> event...
         */
        if (*supported_action != dnd->DNDActionCopyXAtom &&
                *supported_action != dnd->DNDActionMoveXAtom &&
                *supported_action != dnd->DNDActionLinkXAtom &&
                *supported_action != dnd->DNDActionAskXAtom &&
                *supported_action != dnd->DNDActionPrivateXAtom) {
            *supported_action = dnd->DNDActionCopyXAtom;
        }
        *desired_type       = typePtr->type;
        return True;
    }

    dnd->interp = infoPtr->interp;
    dnd->DesiredType = typePtr->type;
    if (dnd->DesiredType == None && typePtr) {
        /* This is a "string match" type... */
        dnd->DesiredType = typePtr->matchedType;
    }

    Tcl_DStringInit(&dString);
    TkDND_ExpandPercents(infoPtr, typePtr, typePtr->script, &dString, x, y);
#ifdef DND_DEBUG
    XDND_DEBUG2("<Drag>: %s\n", Tcl_DStringValue(&dString));
    XDND_DEBUG3("        Supported type: \"%s\" (0x%08x)\n",
            Tk_GetAtomName(tkwin, dnd->DesiredType), dnd->DesiredType);
#endif
    ret = TkDND_ExecuteBinding(infoPtr->interp, Tcl_DStringValue(&dString),
            -1, NULL);
    Tcl_DStringFree(&dString);
    if (ret == TCL_ERROR) {
        goto error;
    } else if (ret == TCL_OK || ret == TCL_RETURN) {
        *want_position = True;
        return TkDND_ParseAction(dnd, infoPtr, typePtr, action,
                supported_action, desired_type);
    }
    return False;

    error:
    dnd->CallbackStatus = TCL_ERROR;
    XUngrabPointer (dnd->display, CurrentTime);
    Tk_BackgroundError(infoPtr->interp);
    while (Tcl_DoOneEvent(TCL_IDLE_EVENTS) != 0) {
        XDND_DEBUG("######## INSIDE LOOP - TkDND_WidgetApplyPosition!\n");
        /* Empty loop body */
    }
    return False;
} /* TkDND_WidgetApplyPosition */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_ParseAction -- (Laurent Riesterer 06/07/2000)
 *
 *       Analyze the result of a script to find the requested action
 *
 * Results:
 *       True if OK / False else.
 *
 *----------------------------------------------------------------------
 */
static int TkDND_ParseAction(DndClass *dnd, DndInfo *infoPtr, DndType *typePtr,
                             Atom default_action, Atom *supported_action,
                             Atom *desired_type) {
    static char *Actions[] = {
        "none", "default", "copy", "move", "link", "ask", "private",
        (char *) NULL
    };
    enum actions {NONE, DEFAULT, COPY, MOVE, LINK, ASK, PRIVATE};
    Atom *atomPtr;
    Tcl_Obj *objPtr;
    int index, action_ok = False;

    objPtr = Tcl_DuplicateObj(Tcl_GetObjResult(infoPtr->interp));
    Tcl_IncrRefCount(objPtr);

    XDND_DEBUG2("<<ParseAction>>: Wanted Action = '%s'\n",
            Tcl_GetString(objPtr));

    if (Tcl_GetIndexFromObj(infoPtr->interp, objPtr,
            (CONST84 char **) Actions, "action", 0, &index) == TCL_OK) {
        switch ((enum actions) index) {
            case NONE: {
                dnd->CallbackStatus = TCL_BREAK;
                return False;
            }
            case DEFAULT: {
                *supported_action = default_action;
                break;
            }
            case COPY: {
                *supported_action = dnd->DNDActionCopyXAtom;
                break;
            }
            case MOVE: {
                *supported_action = dnd->DNDActionMoveXAtom;
                break;
            }
            case LINK: {
                *supported_action = dnd->DNDActionLinkXAtom;
                break;
            }
            case ASK: {
                *supported_action = dnd->DNDActionAskXAtom;
                break;
            }
            case PRIVATE: {
                *supported_action = dnd->DNDActionPrivateXAtom;
                break;
            }
            default: {
                *supported_action = None;
            }
        }
    } else {
        *supported_action = None;
    }
    Tcl_DecrRefCount(objPtr);
    if (desired_type != NULL) {
        *desired_type = typePtr->type;
        if (typePtr->type == None) *desired_type = typePtr->matchedType;
    }

    /*
     * Is the action returned by the script supported by the drag
     * source?
     */
    if (dnd->DraggerAskActionList != NULL) {
        for (atomPtr = dnd->DraggerAskActionList;
             *atomPtr != None; atomPtr++) {
            if (*supported_action == *atomPtr) {
                action_ok = True;
                break;
            }
        }
        /*
         * We got something not supported by the dragger...
         */
        if (!action_ok) *supported_action = *(dnd->DraggerAskActionList);
    } else {
        /*
         * The dragger does not have an action list. Use the current
         * action...
         */
        *supported_action = default_action;
    }

    if (*supported_action != None)
        return True;

    return False;
}

/*
 *----------------------------------------------------------------------
 *
 * TkDND_WidgetApplyLeave --
 *
 *       Executes the binding script for the <DragLeave> event, if such a
 *       binding exists for the specified window.
 *
 * Results:
 *       None.
 *
 * Side effects:
 *       Whatever the binding script does :-)
 *
 *----------------------------------------------------------------------
 */
static int TkDND_WidgetApplyLeave(DndClass * dnd, Window widgets_window) {
    Tk_Window tkwin;
    Tcl_HashEntry *hPtr;
    Tcl_DString dString;
    DndInfo *infoPtr;
    DndType *curr, *typePtr;
    char *pathName;
    int ret;
  
    /* Get window path... */
    if (widgets_window == None)
        return TCL_OK;

    tkwin = Tk_IdToWindow(dnd->display, widgets_window);
    if (tkwin == NULL)
        return TCL_OK;

    pathName = Tk_PathName(tkwin);
    if (pathName == NULL) {
        XDND_DEBUG2("<DragLeave>: This is a tk HIDDEN window (%ld)\n",
                widgets_window);
        return TCL_OK;
    }
    XDND_DEBUG2("<DragLeave>: Window name: %s\n", pathName);


  /* See if its a drag source */
    hPtr = Tcl_FindHashEntry(&TkDND_TargetTable, pathName);
    if (hPtr == NULL) {
        XDND_DEBUG("<DragLeave>: can not find source\n");
        return TCL_OK;
    }

    infoPtr = (DndInfo *) Tcl_GetHashValue(hPtr);
    dnd->interp = infoPtr->interp;
    dnd->CallbackStatus = TCL_OK;

  /*
   * For all types that we have sent a <DragEnter> event, send a
   * <DragLeave> event...
   */
    for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
        XDND_DEBUG4("<DragLeave>: type='%s', what=%ld, EnterEventSent = %d\n",
                curr->typeStr, curr->eventType, curr->EnterEventSent);
        if (curr->EnterEventSent) { /* This is a <DragEnter> event... */
            XDND_DEBUG2("<DragLeave>:     findind match for type '%s'\n",
                    curr->typeStr);
            if (TkDND_FindMatchingScript(&TkDND_TargetTable, pathName,
                    curr->typeStr, NULL, TKDND_DRAGLEAVE, dnd->state, False,
                    &typePtr, NULL) == TCL_OK) {
                if (typePtr == NULL)
                    return TCL_OK;

                Tcl_DStringInit(&dString);
                TkDND_ExpandPercents(infoPtr, typePtr, typePtr->script,
                        &dString, dnd->x, dnd->y);
                XDND_DEBUG2("<DragLeave>:    %s\n",
                        Tcl_DStringValue(&dString));
                ret = TkDND_ExecuteBinding(infoPtr->interp, 
                        Tcl_DStringValue(&dString), -1, NULL);
                Tcl_DStringFree(&dString);
                if (ret == TCL_ERROR) {
                    dnd->CallbackStatus = TCL_ERROR;
                    XUngrabPointer (dnd->display, CurrentTime);
                    Tk_BackgroundError(infoPtr->interp);
                    while (Tcl_DoOneEvent(TCL_IDLE_EVENTS) != 0) {
                        XDND_DEBUG("######## INSIDE LOOP - TkDND_WidgetApplyLeave!\n");
                        /* Empty loop body */
                    }
                    return TCL_ERROR;
                }
                curr->EnterEventSent = 0;
                dnd->LastEnterDeliveredWindow = None;
            }
        }
    }
    return TCL_OK;
} /* TkDND_WidgetApplyLeave */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_WidgetInsertDrop --
 *
 *       Executes the binding script for the <Drop> event, if such a
 *       binding exists for the specified window.
 *
 * Results:
 *       Always 1.
 *
 * Side effects:
 *       Whatever the binding script does :-)
 *
 *----------------------------------------------------------------------
 */
static int TkDND_WidgetInsertDrop(DndClass *dnd, unsigned char *data,
           int length, int remaining, Window into, Window from, Atom type) {
    Tk_Window tkwin;
    Tcl_Obj *dataObj;
    Tcl_DString dString;
    DndInfo *infoPtr;
    DndType *typePtr;
    Atom typelist[2] = {0, 0};
    char *pathName;
    int ret;
  
    typelist[0] = type;

    /* Get window path... */
    XDND_DEBUG("<Drop> Starting ...\n");
    tkwin = Tk_IdToWindow(dnd->display, into);
    if (tkwin == NULL) return False;
    pathName = Tk_PathName(tkwin);
    if (pathName == NULL) {
        XDND_DEBUG("<Drop>: This is a tk HIDDEN window\n");
        return False;
    }


    XDND_DEBUG2("<Drop>: state = %08X\n", dnd->state);
    if (TkDND_FindMatchingScript(&TkDND_TargetTable, pathName,
            NULL, typelist, TKDND_DROP, dnd->state, False,
            &typePtr, &infoPtr) != TCL_OK) {
        goto error;
    }
    if (infoPtr == NULL || typePtr == NULL) {
        if (infoPtr == NULL) return False;

        XDND_DEBUG("<Drop>: Not script found ...\n");
        return True;
    }

    dnd->interp = infoPtr->interp;
    dnd->CallbackStatus = TCL_OK;


    XDND_DEBUG3("<Drop>: Dropped data: (%d) \"%s\"\n", length, data);
    Tcl_DStringInit(&dString);
    if (typePtr->matchedType == None) {
      typePtr->matchedType = dnd->DesiredType;
    }
    dataObj = TkDND_CreateDataObjAccordingToType(typePtr, NULL, data, length);
    typePtr->matchedType = None;
    if (dataObj == NULL) return False;
    Tcl_IncrRefCount(dataObj);
    TkDND_ExpandPercents(infoPtr, typePtr, typePtr->script, &dString,
            dnd->x, dnd->y);
    XDND_DEBUG3("<Drop>:\n  script=\"%s\"\n  data=  \"%s\"\n",
            Tcl_DStringValue(&dString), Tcl_GetString(dataObj));
    ret = TkDND_ExecuteBinding(dnd->interp, Tcl_DStringValue(&dString), -1,
            dataObj);
    Tcl_DStringFree(&dString);
    Tcl_DecrRefCount(dataObj);
    if (ret == TCL_ERROR) {
        goto error;
    }
    return True;


    error:
    dnd->CallbackStatus = TCL_ERROR;
    XUngrabPointer (dnd->display, CurrentTime);
    Tk_BackgroundError(infoPtr->interp);
    while (Tcl_DoOneEvent(TCL_IDLE_EVENTS) != 0) {
        XDND_DEBUG("######## INSIDE LOOP - TkDND_WidgetInsertDrop!\n");
        /* Empty loop body */
    }
    return False;
} /* TkDND_WidgetInsertDrop */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_WidgetAsk --
 *
 *       This function is responsible for executing the tcl binding that will
 *       handle the ask action. If not a binding is defined, a default
 *       procedure from the dnd tcl library is used...
 *
 * Results:
 *       True on success, False on failure.
 *
 * Side effects:
 *       What ever the tcl scrips do...
 *
 *----------------------------------------------------------------------
 */
static int TkDND_WidgetAsk(DndClass *dnd, Window source, Window target,
                           Atom *action) {
    Tk_Window tkwin;
    Tcl_HashEntry *hPtr;
    Tcl_DString dString;
    DndInfo *infoPtr;
    DndType *curr;
    char *pathName,
        *script = "::dnd::ChooseAskAction %W %X %Y %a %d";
    int ret;
  
    dnd->CallbackStatus = TCL_OK;
    /* Get window path... */
    tkwin = Tk_IdToWindow(dnd->display, target);
    if (tkwin == NULL) {
        XDND_DEBUG("TkDND_WidgetAsk: This is a FOREIGN window\n");
        return False;
    }
    pathName = Tk_PathName(tkwin);
    if (pathName == NULL) {
        XDND_DEBUG("TkDND_WidgetAsk: This is a tk HIDDEN window\n");
        return False;
    }

    /* See if its a drag source */
    hPtr = Tcl_FindHashEntry(&TkDND_TargetTable, pathName);
    if (hPtr == NULL) {
        XDND_DEBUG2("TkDND_WidgetAsk: Window %s is NOT a DROP TARGET...\n",
                pathName);
        return False;
    }

    infoPtr = (DndInfo *) Tcl_GetHashValue(hPtr);
    for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
        if ( (curr->type != None && curr->type == dnd->DesiredType &&
                curr->eventType == TKDND_ASK) ||
                (curr->type == None && curr->matchedType == dnd->DesiredType &&
                        curr->eventType == TKDND_ASK) ) {
            /*
             * Simplified after proposal from Laurent Riesterer,
             * 24/06/2000:
             */
            script = curr->script;
            break;
        }
    }

    /* Execute the script or default procedure if none was found ... */
    Tcl_DStringInit(&dString);
    TkDND_ExpandPercents(infoPtr, curr, script, &dString, dnd->x, dnd->y);
    ret = TkDND_ExecuteBinding(infoPtr->interp, Tcl_DStringValue(&dString),-1,
            NULL);
    Tcl_DStringFree(&dString);
    if (ret == TCL_ERROR) {
        dnd->CallbackStatus = TCL_ERROR;
        Tk_BackgroundError(dnd->interp);
        while (Tcl_DoOneEvent(TCL_IDLE_EVENTS) != 0) {
            XDND_DEBUG("######## INSIDE LOOP - TkDND_WidgetAsk!\n");
            /* Empty loop body */
        }
    } else if (ret == TCL_BREAK) {
        XDND_DEBUG("TkDND_WidgetAsk: Widget refused to post dialog "
                "(returned TCL_BREAK)\n");
        return False;
    }

    TkDND_ParseAction(dnd, infoPtr, NULL, dnd->DNDActionCopyXAtom,
            action, NULL);
    if (*action == dnd->DNDActionAskXAtom
            || dnd->CallbackStatus == TCL_BREAK) {
        /* Returning again Ask is not allowed! */
        *action = None;
    }
    return True;
} /* TkDND_WidgetAsk */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_WidgetGetData --
 *
 *       This function is responsible for executing the tcl binding that will
 *       return the data to be dropped to us...
 *
 * Results:
 *       A standart tcl result.
 *
 * Side effects:
 *       The data is retrieved from widget and is stored inside the dnd
 *       structure...
 *
 *----------------------------------------------------------------------
 */
static int TkDND_WidgetGetData(DndClass *dnd, Window source, 
               unsigned char **data, int *length, Atom type) {
    Tk_Window tkwin;
    Tcl_HashEntry *hPtr;
    Tcl_DString dString;
    DndInfo *infoPtr;
    DndType *curr;
    char *pathName;
    unsigned char *result;
    int ret;
  
    *data = NULL; *length = 0;
    dnd->CallbackStatus = TCL_OK;
    /* Get window path... */
    tkwin = Tk_IdToWindow(dnd->display, source);
    if (tkwin == NULL) {
        XDND_DEBUG("TkDND_WidgetGetData: This is a FOREIGN window\n");
        return False;
    }
    pathName = Tk_PathName(tkwin);
    if (pathName == NULL) {
        XDND_DEBUG("TkDND_WidgetGetData: This is a tk HIDDEN window\n");
        return False;
    }

    /* See if its a drag source */
    hPtr = Tcl_FindHashEntry(&TkDND_SourceTable, pathName);
    if (hPtr == NULL) {
        XDND_DEBUG2("TkDND_WidgetGetData: Window %s is NOT a DRAG SOURCE...\n",
                pathName);
        return False;
    }

    infoPtr = (DndInfo *) Tcl_GetHashValue(hPtr);
    for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
        if (curr->type == type) {
            Tcl_DStringInit(&dString);
            TkDND_ExpandPercents(infoPtr, curr, curr->script,
                    &dString,dnd->x,dnd->y);
            XDND_DEBUG2("<GetData>: %s\n", Tcl_DStringValue(&dString));
            ret = TkDND_ExecuteBinding(infoPtr->interp,
                    Tcl_DStringValue(&dString), -1, NULL);
            Tcl_DStringFree(&dString);
            dnd->CallbackStatus = ret;
            if (ret == TCL_ERROR) {
                XUngrabPointer (dnd->display, CurrentTime);
                Tk_BackgroundError(infoPtr->interp);
            } else if (ret == TCL_BREAK) {
                *data = NULL; *length = 0;
                XDND_DEBUG("TkDND_WidgetGetData: Widget refused to send data "
                        "(returned TCL_BREAK)\n");
                return False;
            }
            /*
             * Petasis, 14/12/2000:
             * What if infoPtr->inyterp == NULL?
             */
            if (infoPtr->interp != NULL) {
                if (curr->matchedType == None) {
                  curr->matchedType = dnd->DesiredType;
                }
                result = TkDND_GetDataAccordingToType(infoPtr, 
                        Tcl_GetObjResult(infoPtr->interp),
                        curr, length);
                curr->matchedType = None;
                *data = (unsigned char *) result;
            }
            return True;
        }
    }
    return False;
} /* TkDND_WidgetGetData */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_HandleEvents --
 *
 *       This function passes the given event to tk for processing.
 *
 * Results:
 *       None
 *
 * Side effects:
 *       -
 *
 *----------------------------------------------------------------------
 */
static void TkDND_HandleEvents (DndClass *dnd, XEvent *xevent) {
    Tk_HandleEvent(xevent);
} /* TkDND_HandleEvents */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_GetCurrentAtoms --
 *
 *       Returns an allocated Atom array filled with the types the specified
 *       window supports as drag sources.
 *
 * Results:
 *       An allocated Atom array. Returned space must be freed...
 *
 * Side effects:
 *       -
 *
 *----------------------------------------------------------------------
 */
static Atom *TkDND_GetCurrentAtoms(XDND *dnd, Window window) {
    Tcl_HashEntry *hPtr;
    DndInfo *infoPtr;
    Tk_Window tkwin;
    DndType *curr;
    Atom *TypeList;
    char *pathName;
    int length = 0;
  
    tkwin = Tk_IdToWindow(dnd->display, window);
    if (tkwin == NULL) return NULL;
  
    pathName = Tk_PathName(tkwin);
    if (pathName == NULL) return NULL;
  
    hPtr = Tcl_FindHashEntry(&TkDND_SourceTable, pathName);
    if (hPtr == NULL) return NULL;
  
    infoPtr = (DndInfo *) Tcl_GetHashValue(hPtr);
    for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
        length++;
    }
    TypeList = (Atom *) Tcl_Alloc(sizeof(Atom)*(length+1));
    length = 0;
    for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
        TypeList[length++] = curr->type;
    }
    TypeList[length] = 0;
    return TypeList;
} /* TkDND_GetCurrentAtoms */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_SetCursor --
 *
 *       Changes the current cursor to the given one.
 *
 * Results:
 *       Returns 0 on error, 1 otherwise.
 *
 * Side effects:
 *       The current cursor is changed.
 *
 *----------------------------------------------------------------------
 */
static int TkDND_SetCursor(DndClass *dnd, int cursor) {
    static int current_cursor = -1, x = 0, y = 0;
    int ret;
    Tcl_DString dString;
    DndInfo info;
    DndType type;
  
    /*
     * Laurent Riesterer 21/06/2000:
     * Proposed the use of "XChangeActivePointerGrab" instead of 
     * "XGrabPointer"...
     */

    /*
     * If cursor is -2, reset cache...
     */
    if (cursor == -2) {
        current_cursor = -1;
        return True;
    }
  
    if (cursor > -1 && current_cursor != cursor) {
        XChangeActivePointerGrab(dnd->display,
                ButtonMotionMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                dnd->cursors[cursor].cursor, CurrentTime);
        current_cursor = cursor;
    }

    /*
     * If a cursor window is specified by the user, move it to follow cursor...
     */
    if (dnd->CursorWindow != NULL && (x != dnd->x || y != dnd->y)) {
        Tk_MoveToplevelWindow(dnd->CursorWindow, dnd->x+10, dnd->y);
        Tk_RestackWindow(dnd->CursorWindow, Above, NULL);
        x = dnd->x; y = dnd->y;

        /*
         * Has also the user registered a Cursor Callback?
         */
        if (dnd->CursorCallback != NULL && dnd->interp != NULL) {
            info.tkwin   = Tk_IdToWindow(dnd->display, dnd->DraggerWindow);
            if (current_cursor > XDND_NODROP_CURSOR) {
                type.typeStr = (char *)
                               Tk_GetAtomName(info.tkwin, dnd->DesiredType);
                type.script  = "";
            } else {
                type.typeStr = "";
                type.script  = NULL;
            }
            Tcl_Preserve((ClientData) dnd->interp);
            Tcl_DStringInit(&dString);
            TkDND_ExpandPercents(&info, &type, dnd->CursorCallback,
                    &dString, x, y);
            ret = TkDND_ExecuteBinding(dnd->interp,
                    Tcl_DStringValue(&dString), -1, NULL);
            Tcl_DStringFree(&dString);
            if (ret == TCL_ERROR) {
                XUngrabPointer(dnd->display, CurrentTime);
                Tk_BackgroundError(dnd->interp);
                Tcl_Release((ClientData) dnd->interp);
                TkDND_Update(dnd->display, 0);
                dnd->CallbackStatus = TCL_ERROR;
                return False;
            }
            Tcl_Release((ClientData) dnd->interp);
        } else if (dnd->interp == NULL) {
            /*
             * I guess the interp should not be null, but let's ignore
             * that for now.  -- hobbs 05/2002
             panic("dnd->interp is null");
             * We may instead want to make sure to ungrab the pointer and
             * change the cursor back.
             TkDND_Update(dnd->display, 0);
             return False;
             */
        }
    }

    /* Simulate "update"... */
    TkDND_Update(dnd->display, 0);
    return True;
} /* TkDND_SetCursor */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_XDNDHandler --
 *
 *       Our XDND ClientMessage handler.
 *
 * Results:
 *       Returns 0 if the ClientMessage is not XDND related, 0 otherwise.
 *
 * Side effects:
 *       ?
 *
 *----------------------------------------------------------------------
 */
static int TkDND_XDNDHandler(Tk_Window winPtr, XEvent *eventPtr) {
    if (eventPtr->type != ClientMessage) return False;
    return XDND_HandleClientMessage(dnd, eventPtr);
} /* TkDND_XDNDHandler */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_AddHandler --
 *
 *        Add a drag&drop handler to a window, freeing up any memory and
 *        objects that need to be cleaned up.
 *
 * Arguments:
 *        interp:                Tcl interpreter
 *        topwin:                The main window
 *        table:                 Hash table we are adding the window/type to
 *        windowPath:            String pathname of window
 *        typeStr:               The data type to add.
 *        script:                Script to run
 *        isDropTarget:          Is this target a drop target?
 *        priority:              Priority of this type
 *
 * Results:
 *        A standard Tcl result
 *
 * Notes:
 *        The table that is passed in must have been initialized.
 *        Inserts a new node in the typelist for the window with the
 *        highest priorities listed first.
 *
 *----------------------------------------------------------------------
 */
int TkDND_AddHandler(Tcl_Interp *interp, Tk_Window topwin,
                     Tcl_HashTable *table, char *windowPath, char *typeStr,
                     unsigned long eventType, unsigned long eventMask,
                     char *script, int isDropTarget, int priority) {
    Tcl_HashEntry *hPtr;
    DndInfo *infoPtr;
    DndType *head, *prev, *curr, *tnew;
    Tk_Window tkwin;
    Window win;
    Atom atom;
#define TkDND_ACTUAL_TYPE_NU 15
    char *ActualType[TkDND_ACTUAL_TYPE_NU];
    int created, len, i, removed = False;

    tkwin = Tk_NameToWindow(interp, windowPath, topwin);
    if (tkwin == NULL) return TCL_ERROR;
    Tk_MakeWindowExist(tkwin);
    win = Tk_WindowId(tkwin);

    /* Now we have priorities, event for Drag sources... */
    /*if (!isDropTarget) priority = 0;*/

    /*
   * Do we already have a binding script for this type of type/event?
   * Remember that we have to search all entries in order to remove all of
   * them that have the same typeStr but different actual types...
   */
    hPtr = Tcl_CreateHashEntry(table, windowPath, &created);
    if (!created) {
        infoPtr = (DndInfo *) Tcl_GetHashValue(hPtr);
        for (curr = (&infoPtr->head)->next; curr != NULL; curr = curr->next) {
            if (strcmp(curr->typeStr, typeStr) == 0
                    && curr->eventType == eventType
                    && curr->eventMask == eventMask) {
                /*
                 * Replace older script...
                 */
                Tcl_Free(curr->script);
                len = strlen(script) + 1;
                curr->script = (char *) Tcl_Alloc(sizeof(char)*len);
                memcpy(curr->script, script, len);
                removed = True;
            }
        }
    }
    if (removed) return TCL_OK;

    /*
     * In order to ensure portability among different protocols, we have to
     * register more than one type for each "platform independed" type
     * requested by the user...
     */
    i = 0;
    if (strcmp(typeStr, "text/plain;charset=UTF-8") == 0) {
        /*
         * Here we handle our platform independed way to transer UTF strings...
         */
        ActualType[i++] = "text/plain;charset=UTF-8"; /* XDND  Protocol */
        ActualType[i++] = "CF_UNICODETEXT";           /* OLE DND Protocol */
        ActualType[i++] = NULL;
    } else if (strcmp(typeStr, "text/plain") == 0) {
        /*
         * Here we handle our platform independed way to pass ASCII strings
         */
        ActualType[i++] = "text/plain";               /* XDND  Protocol */
        ActualType[i++] = "STRING";                   /* Motif Protocol */
        ActualType[i++] = "TEXT";                     /* Motif Protocol */
        ActualType[i++] = "COMPOUND_TEXT";            /* Motif Protocol */
        ActualType[i++] = "CF_TEXT";                  /* OLE DND Protocol */
        ActualType[i++] = "CF_OEMTEXT";               /* OLE DND Protocol */
        ActualType[i++] = NULL;
    } else if (strcmp(typeStr, "text/uri-list") == 0 ||
            strcmp(typeStr, "Files")         == 0   ) {
        /*
         * Here we handle our platform independed way to transer File Names...
         */
        ActualType[i++] = "text/uri-list";            /* XDND  Protocol */
        ActualType[i++] = "text/file";                /* XDND  Protocol */
        ActualType[i++] = "text/url";                 /* XDND  Protocol */
        ActualType[i++] = "url/url";                  /* XDND  Protocol */
        ActualType[i++] = "FILE_NAME";                /* Motif Protocol */
        ActualType[i++] = "SGI_FILE";                 /* Motif Protocol */
        ActualType[i++] = "_NETSCAPE_URL";            /* Motif Protocol */
        ActualType[i++] = "_MOZILLA_URL";             /* Motif Protocol */
        ActualType[i++] = "_SGI_URL";                 /* Motif Protocol */
        ActualType[i++] = "CF_HDROP";                 /* OLE DND  Protocol */
        ActualType[i++] = NULL;
    } else if (strcmp(typeStr, "Text") == 0) {
        /*
         * Here we handle our platform independed way to all supported text
         * formats...
         */
        ActualType[i++] = "text/plain;charset=UTF-8"; /* XDND  Protocol */
        ActualType[i++] = "text/plain";               /* XDND  Protocol */
        ActualType[i++] = "STRING";                   /* Motif Protocol */
        ActualType[i++] = "TEXT";                     /* Motif Protocol */
        ActualType[i++] = "COMPOUND_TEXT";            /* Motif Protocol */
        ActualType[i++] = "CF_UNICODETEXT";           /* OLE DND Protocol */
        ActualType[i++] = "CF_OEMTEXT";               /* OLE DND Protocol */
        ActualType[i++] = "CF_TEXT";                  /* OLE DND Protocol */
        ActualType[i++] = NULL;
    } else if (strcmp(typeStr, "Image") == 0) {
        /*
         * Here we handle our platform independed way to all supported text
         * formats...
         */
        ActualType[i++] = "CF_DIB";                   /* OLE DND Protocol */
        ActualType[i++] = NULL;
    } else {
        /*
         * This is a platform specific type. Register it as is...
         */
        ActualType[i++] = typeStr;
        ActualType[i++] = NULL;
    }

    for (i=0; i<TkDND_ACTUAL_TYPE_NU && ActualType[i]!=NULL; i++) {
        tnew = (DndType *) Tcl_Alloc(sizeof(DndType));
        tnew->priority = priority;
        tnew->matchedType = None;
        len = strlen(typeStr) + 1;
        tnew->typeStr = (char *) Tcl_Alloc(sizeof(char)*len);
        memcpy(tnew->typeStr, typeStr, len);
        tnew->eventType = eventType;
        tnew->eventMask = eventMask;
        len = strlen(script) + 1;
        tnew->script = (char *) Tcl_Alloc(sizeof(char)*len);
        memcpy(tnew->script, script, len);
        tnew->next = NULL;
        tnew->EnterEventSent = 0;
        /* Has the type a wildcard character? */
        if (strchr(ActualType[i], '*') == NULL) {
            atom = Tk_InternAtom(tkwin, ActualType[i]);
        } else {
            atom = None;
        }
        tnew->type = atom;

        if (created) {
            infoPtr = (DndInfo *) Tcl_Alloc(sizeof(DndInfo));
            infoPtr->head.next = NULL;
            infoPtr->interp = interp;
            infoPtr->tkwin = tkwin;
            infoPtr->hashEntry = hPtr;
            /* infoPtr->IDropTarget = isDropTarget; */
            Tk_CreateEventHandler(tkwin, StructureNotifyMask,
                    TkDND_DestroyEventProc, (ClientData) infoPtr);
            Tcl_SetHashValue(hPtr, infoPtr);
            infoPtr->head.next = tnew;
    
            /* Make window DND aware... */
            XDND_Enable(dnd, win);
            created = False;
        } else {
            infoPtr = (DndInfo *) Tcl_GetHashValue(hPtr);
            infoPtr->tkwin = tkwin;
            head = prev = &infoPtr->head;
            /*
             * Laurent Riesterer 09/07/2000:
             * fix start of list to correctly handle priority...
             */
            for (curr = head->next; curr != NULL;
                 prev = curr, curr = curr->next) {
                if (curr->priority > priority)  break;
            }
            XDND_DEBUG5("<<AddHandler>> type='%s','%s'"
                    " / event=%ld / mask=%08x\n",
                    typeStr, ActualType[i], eventType,
                    (unsigned int) eventMask);
            XDND_DEBUG3("<<AddHandler>> curr.prio=%d / tnew.prio=%d\n",
                    (curr ? curr->priority:-1), tnew->priority);
            tnew->next = prev->next;
            prev->next = tnew;
        }
    } /* for (i=0; i<TkDND_ACTUAL_TYPE_NU && ActualType[i]!=NULL; i++) */
    /* Register type with window... */
    /* XDND_AppendType(dnd, win, atom); */
    return TCL_OK;
} /* TkDND_AddHandler */


/*
 *----------------------------------------------------------------------
 * TkDND_GetCurrentTypeName --
 *        Returns a string that describes the current dnd type.
 *
 * Results:
 *        A static string. Should never be freed.
 *
 * Notes:
 *----------------------------------------------------------------------
 */
char *TkDND_GetCurrentTypeName(void) {
    return TkDND_TypeToString(dnd->DesiredType);
} /* TkDND_GetCurrentTypeName */

/*
 *----------------------------------------------------------------------
 * TkDND_GetCurrentTypeCode --
 *        Returns a string that describes the current dnd type code in hex.
 *
 * Results:
 *        A dynamic string. Should be freed.
 *
 * Notes:
 *----------------------------------------------------------------------
 */
char *TkDND_GetCurrentTypeCode(void) {
    char tmp[64], *str;
  
    sprintf(tmp, "0x%08x", dnd->DesiredType);
    str = Tcl_Alloc(sizeof(char) * strlen(tmp));
    strcpy(str, tmp);
    return str;
} /* TkDND_GetCurrentTypeCode */

/*
 *----------------------------------------------------------------------
 * TkDND_GetCurrentActionName --
 *        Returns a string that describes the current dnd action.
 *
 * Results:
 *        A static string. Should never be freed.
 *
 * Notes:
 *----------------------------------------------------------------------
 */
char *TkDND_GetCurrentActionName(void) {
    Atom action = dnd->SupportedAction;

    if (action == dnd->DNDActionCopyXAtom)    return "copy";
    if (action == dnd->DNDActionMoveXAtom)    return "move";
    if (action == dnd->DNDActionLinkXAtom)    return "link";
    if (action == dnd->DNDActionAskXAtom)     return "ask";
    if (action == dnd->DNDActionPrivateXAtom) return "private";
    return "unknown";
} /* TkDND_GetCurrentActionName */

/*
 *----------------------------------------------------------------------
 * TkDND_GetCurrentButton --
 *        Returns a integer that describes the current button used
 *        for the drag operation. (Laurent Riesterer, 24/06/2000)
 *
 * Results:
 *        An integer.
 *
 * Notes:
 *----------------------------------------------------------------------
 */
int TkDND_GetCurrentButton(void) {
    return dnd->button;
} /* TkDND_GetCurrentButton */

/*
 *----------------------------------------------------------------------
 * TkDND_GetCurrentModifiers --
 *        Returns a string that describes the modifiers that are currently
 *        pressed during a drag and drop operation.
 *        (Laurent Riesterer, 24/06/2000)
 * Results:
 *        A dynamic string. Should be freed.
 *
 * Notes:
 *----------------------------------------------------------------------
 */
char *TkDND_GetCurrentModifiers(Tk_Window tkwin) {
    Tcl_DString ds;
    char *str;
    unsigned int AltMsk, MetaMsk;

    AltMsk  = dnd->Alt_ModifierMask;
    MetaMsk = dnd->Meta_ModifierMask;
    Tcl_DStringInit(&ds);
    if (dnd->state & ShiftMask)   Tcl_DStringAppendElement(&ds, "Shift");
    if (dnd->state & ControlMask) Tcl_DStringAppendElement(&ds, "Control");
    if (dnd->state & AltMsk)      Tcl_DStringAppendElement(&ds, "Alt");
    if (dnd->state & MetaMsk)     Tcl_DStringAppendElement(&ds, "Meta");
    if (dnd->state & Mod1Mask && AltMsk != Mod1Mask && MetaMsk != Mod1Mask)
        Tcl_DStringAppendElement(&ds, "Mod1");
    if (dnd->state & Mod2Mask && AltMsk != Mod2Mask && MetaMsk != Mod2Mask)
        Tcl_DStringAppendElement(&ds, "Mod2"); 
    if (dnd->state & Mod3Mask && AltMsk != Mod3Mask && MetaMsk != Mod3Mask)
        Tcl_DStringAppendElement(&ds, "Mod3");
    if (dnd->state & Mod4Mask && AltMsk != Mod4Mask && MetaMsk != Mod4Mask)
        Tcl_DStringAppendElement(&ds, "Mod4");
    if (dnd->state & Mod5Mask && AltMsk != Mod5Mask && MetaMsk != Mod5Mask)
        Tcl_DStringAppendElement(&ds, "Mod5");

    str = Tcl_Alloc(sizeof(char) * (Tcl_DStringLength(&ds)+1));
    memcpy(str, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)+1);
    Tcl_DStringFree(&ds);
    return str;
} /* TkDND_GetCurrentModifiers */

/*
 *----------------------------------------------------------------------
 * TkDND_GetSourceActions --
 *        Returns a string that describes the supported by the target actions.
 *
 * Results:
 *        A dynamic string. Should be freed.
 *
 * Notes:
 *----------------------------------------------------------------------
 */
char *TkDND_GetSourceActions(void) {
    Atom *action = dnd->DraggerAskActionList;
    Tcl_DString ds;
    char *str;

    Tcl_DStringInit(&ds);
    if (action != NULL) {
        while (*action != None) {
            if (*action == dnd->DNDActionCopyXAtom)         str = "copy";
            else if (*action == dnd->DNDActionMoveXAtom)    str = "move";
            else if (*action == dnd->DNDActionLinkXAtom)    str = "link";
            else if (*action == dnd->DNDActionAskXAtom)     str = "ask";
            else if (*action == dnd->DNDActionPrivateXAtom) str = "private";
            else                                            str = "unknown";
            Tcl_DStringAppendElement(&ds, str);
            action++;
        }
    }

    str = Tcl_Alloc(sizeof(char) * (Tcl_DStringLength(&ds)+1));
    memcpy(str, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)+1);
    Tcl_DStringFree(&ds);
    return str;
} /* TkDND_GetSourceActions */

/*
 *----------------------------------------------------------------------
 * TkDND_GetSourceActionDescriptions --
 *        Returns a string that describes the supported by the target action
 *        descriptions.
 *
 * Results:
 *        A dynamic string. Should be freed.
 *
 * Notes:
 *----------------------------------------------------------------------
 */
char *TkDND_GetSourceActionDescriptions(void) {
    char *descriptions = NULL;
    Tcl_DString ds;
    char *str;

    Tcl_DStringInit(&ds);
    if (dnd->DraggerAskDescriptions != NULL) {
        descriptions = dnd->DraggerAskDescriptions;
    }

    if (descriptions != NULL) {
        while (*descriptions != '\0') {
            Tcl_DStringAppendElement(&ds, descriptions);
            descriptions += strlen(descriptions)+1 ;
        }
    }

    str = Tcl_Alloc(sizeof(char) * (Tcl_DStringLength(&ds)+1));
    memcpy(str, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)+1);
    Tcl_DStringFree(&ds);
    return str;
} /* TkDND_GetSourceActionDescriptions */

/*
 *----------------------------------------------------------------------
 * TkDND_GetSourceTypeList --
 *        Returns a string that describes the supported by the drag source
 *        types.
 *
 * Results:
 *        A dynamic string. Should be freed.
 *
 * Notes:
 *        Feature not supported by OLE DND. Simulated.
 *----------------------------------------------------------------------
 */
char *TkDND_GetSourceTypeList(void) {
    Tcl_DString ds;
    Atom *typelist = dnd->DraggerTypeList;
    char *str;
    int i;

    Tcl_DStringInit(&ds);
    for (i = 0; typelist && typelist[i] != None; i++) {
        Tcl_DStringAppendElement(&ds, TkDND_TypeToString(typelist[i]));
    }
    str = Tcl_Alloc(sizeof(char) * (Tcl_DStringLength(&ds)+1));
    memcpy(str, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)+1);
    Tcl_DStringFree(&ds);
    return str;
} /* TkDND_GetSourceTypeList */

/*
 *----------------------------------------------------------------------
 * TkDND_GetSourceTypeCodeList --
 *        Returns a string that describes the supported by the target action
 *        descriptions.
 *
 * Results:
 *        A dynamic string. Should be freed.
 *
 * Notes:
 *        Feature not supported by OLE DND. Simulated.
 *----------------------------------------------------------------------
 */
char *TkDND_GetSourceTypeCodeList(void) {
    Tcl_DString ds;
    Atom *atomPtr = dnd->DraggerTypeList;
    char *str, tmp[64];

    Tcl_DStringInit(&ds);
    for (atomPtr = dnd->DraggerTypeList; *atomPtr != None; atomPtr++) {
        sprintf(tmp, "0x%08x", *atomPtr);
        Tcl_DStringAppendElement(&ds, tmp);
    }
    str = Tcl_Alloc(sizeof(char) * (Tcl_DStringLength(&ds)+1));
    memcpy(str, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)+1);
    Tcl_DStringFree(&ds);
    return str;
} /* TkDND_GetSourceTypeCodeList */

/*
 *----------------------------------------------------------------------
 * TkDND_DndDrag --
 *        Begins a drag and returns when the drop finishes or the
 *        operation is cancelled.
 *
 * Results:
 *        Standard TCL result
 *
 * Notes:
 *        windowPath is assumed to be a valid Tk window pathname at this
 *        point.
 *----------------------------------------------------------------------
 */
int TkDND_DndDrag(Tcl_Interp *interp, char *windowPath, int button,
                  Tcl_Obj *Actions, char *Descriptions,
                  Tk_Window cursor_window, char *cursor_callback) {
    Tcl_HashEntry *hPtr;
    Tcl_Obj **elem;
    Atom *atom, actions[6] = {0, 0, 0, 0, 0, 0};
    DndInfo *infoPtr;
    DndType *curr;
    char *action;
    int type_count = 1, elem_nu, i, result;

    hPtr = Tcl_FindHashEntry(&TkDND_SourceTable, windowPath);
    if (hPtr == NULL) {
        Tcl_AppendResult(interp, "unable to begin drag operation: ",
                "no source types have been bound to ", windowPath,
                (char *) NULL);
        return TCL_ERROR;
    }

    infoPtr = (DndInfo *) Tcl_GetHashValue(hPtr);

    for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
        type_count++;
    }
    atom = (Atom *) Tcl_Alloc(sizeof(Atom)*(type_count+1));
    type_count = 0;
    for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
        atom[type_count++] = curr->type;
        XDND_DEBUG2("TkDND_DndDrag: Adding type: %s\n", curr->typeStr);
    }
    atom[type_count] = 0;

    /*
     * Handle User-requested Actions...
     */
    if (Actions == NULL) {
        /* The User has not specified any actions: defaults to copy... */
        actions[0] = dnd->DNDActionCopyXAtom;
        memset(Descriptions, '\0', TKDND_MAX_DESCRIPTIONS_LENGTH);
        strcpy(Descriptions, "Copy\0\0");
    } else {
        Tcl_ListObjGetElements(interp, Actions, &elem_nu, &elem);
        for (i = 0; i < elem_nu; i++) {
            action = Tcl_GetString(elem[i]);
            if      (strcmp(action,"copy")==0)
                actions[i] = dnd->DNDActionCopyXAtom;
            else if (strcmp(action,"move")==0)
                actions[i] = dnd->DNDActionMoveXAtom;
            else if (strcmp(action,"link")==0)
                actions[i] = dnd->DNDActionLinkXAtom;
            else if (strcmp(action,"ask" )==0)
                actions[i] = dnd->DNDActionAskXAtom;
            else actions[i] = dnd->DNDActionPrivateXAtom;
        }
        actions[5] = 0;
    }

    /*
     * Install an XError Handler, so in case some XDND aware application
     * crashes to not also crash us with a BadWindow or similar error...
     */
    TkDND_StartProtectedSection(Tk_Display(infoPtr->tkwin), infoPtr->tkwin);
    dnd->button = button;
    result = XDND_BeginDrag(dnd, Tk_WindowId(infoPtr->tkwin), actions, atom,
            Descriptions, cursor_window, cursor_callback);
    if(result) {
        /* DND was successful! */
        Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
    } else {
        /* DND was not successful. Make sure all widgets get <DragLeave>... */
        /* TkDND_WidgetApplyLeave(dnd, dnd->dropper_window); */
        Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    }
    XDND_Reset(dnd);
    /*
     * Restore default XError Handler...
     */
    TkDND_EndProtectedSection(Tk_Display(infoPtr->tkwin));
    /*
     * Don't free it, XDND_Reset has already freed it
     * Tcl_Free((char *) atom);
     */
    return TCL_OK;
}

int TkDND_GetDataFromImage(DndInfo *info, char *imageName,
                           char *type, char **result, int *length) {
    /* TODO: implement :-) */
    return 0;
} /* TkDND_GetDataFromImage */

/*
 *----------------------------------------------------------------------
 * TkDND_LocalErrorHandler --
 *        Our local XDND XError Handler.
 *
 * Results:
 *
 * Notes:
 *----------------------------------------------------------------------
 */
static TkDND_LocalErrorHandler(Display *display, XErrorEvent *error) {
    char buffer[512];
    int  return_val = 0;
    if (error->error_code == BadWindow &&
            error->resourceid == Tk_WindowId(ProtectionOwnerWindow) &&
            error->serial     >= FirstProtectRequest) {
        /* This error is for us :-) */
        fprintf(stderr, "tkdnd: XError caugth:\n");
        XGetErrorText(display, error->error_code, buffer, 511);
        fprintf(stderr, "  %s\n", buffer);
        return 0;
    }
    /* Not for us. Try to call the default error handler... */
    if (PreviousErrorHandler != NULL) {
        return_val = (*PreviousErrorHandler)(display, error);
    }
    return return_val;
} /* TkDND_LocalErrorHandler */

/*
 *----------------------------------------------------------------------
 * TkDND_StartProtectedSection --
 *        Registers an XError Handler.
 *
 * Results:
 *        None
 *
 * Notes:
 *----------------------------------------------------------------------
 */
static void TkDND_StartProtectedSection(Display *display, Tk_Window window) {
    PreviousErrorHandler  = XSetErrorHandler(TkDND_LocalErrorHandler);
    FirstProtectRequest   = NextRequest(display);
    ProtectionOwnerWindow = window;
} /* TkDND_StartProtectedSection */

/*
 *----------------------------------------------------------------------
 * TkDND_EndProtectedSection --
 *        Restores the default XError Handler.
 *
 * Results:
 *        None
 *
 * Notes:
 *----------------------------------------------------------------------
 */
static void TkDND_EndProtectedSection(Display *display) {
    XSync(display, False);
    XSetErrorHandler(PreviousErrorHandler);
    PreviousErrorHandler  = NULL;
    ProtectionOwnerWindow = NULL;
} /* TkDND_EndProtectedSection */

/*static Window TkDND_GetXParent(DndClass *dnd, Window w) {
  Window root, parent, *kids;
  unsigned int nkids;

  if (XQueryTree(dnd->display, w, &root, &parent, &kids, &nkids)) {
    if (kids) {
      XFree(kids);
    }
    if (parent!=root) {
      return parent;
    }
  }
  return None;
}*/

#ifdef TKDND_ENABLE_SHAPE_EXTENSION
#include "Shape/shape.c"
#endif /* TKDND_ENABLE_SHAPE_EXTENSION */

/* 
 * End of tkXDND.c
 */
