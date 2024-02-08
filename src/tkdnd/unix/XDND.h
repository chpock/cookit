/*
 * XDND.h -- Definitions of the Tk XDND Drag'n'Drop Protocol Implementation
 * 
 *    This file implements the unix portion of the drag&drop mechanish
 *    for the tk toolkit. The protocol in use under unix is the
 *    XDND protocol.
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

#ifndef _X_DND_H
#define _X_DND_H
#include <tcl.h>
#include <tk.h>
#include "tkDND.h"

#ifdef  TKDND_ENABLE_MOTIF_DRAGS
#ifndef TKDND_ENABLE_MOTIF_DROPS
#define TKDND_ENABLE_MOTIF_DROPS
#endif /* TKDND_ENABLE_MOTIF_DROPS */
#endif /* TKDND_ENABLE_MOTIF_DRAGS */

/*
 * We also support Motif drops :-)
 */
#ifdef TKDND_ENABLE_MOTIF_DROPS
#include "Dnd.h"
#endif /* TKDND_ENABLE_MOTIF_DROPS */

#define XDND_VERSION 3
#define XDND_MINVERSION 3
#define XDND_ENTERTYPECOUNT 3
#define XDND_BOOL short

#define XDND_NODROP_CURSOR     0
#define XDND_COPY_CURSOR       1
#define XDND_MOVE_CURSOR       2
#define XDND_LINK_CURSOR       3
#define XDND_ASK_CURSOR        4
#define XDND_PRIVATE_CURSOR    5

/*
 * Debug Facilities...
 */
#ifndef XDND_DEBUG
#ifdef  DND_DEBUG
#include <stdio.h>
#define XDND_DEBUG(a)          \
  printf("%s, %d: " a,__FILE__,__LINE__);         fflush(stdout)
#define XDND_DEBUG2(a,b)       \
  printf("%s, %d: " a,__FILE__,__LINE__,b);       fflush(stdout)
#define XDND_DEBUG3(a,b,c)     \
  printf("%s, %d: " a,__FILE__,__LINE__,b,c);     fflush(stdout)
#define XDND_DEBUG4(a,b,c,d)   \
  printf("%s, %d: " a,__FILE__,__LINE__,b,c,d);   fflush(stdout)
#define XDND_DEBUG5(a,b,c,d,e) \
  printf("%s, %d: " a,__FILE__,__LINE__,b,c,d,e); fflush(stdout)
#else  /* DND_DEBUG */
#define XDND_DEBUG(a)
#define XDND_DEBUG2(a,b)
#define XDND_DEBUG3(a,b,c)
#define XDND_DEBUG4(a,b,c,d)
#define XDND_DEBUG5(a,b,c,d,e)
#endif /* DND_DEBUG */
#endif /* XDND_DEBUG */

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef LONG_MAX
#define LONG_MAX 0x8000000L
#endif

#define Min(x,y) (x<y?x:y)
#define XDND_Sqrt(x) ((x)*(x))
#ifndef False
#define False 0
#endif /* False */
#ifndef True
#define True 1
#endif /* True */

typedef struct _XDND_Cursor {
  int width, height;
  int x, y;
  unsigned char *image_data, *mask_data;
  char *_action;
  Pixmap image_pixmap, mask_pixmap;
  Cursor cursor;
  Atom action;
} XDNDCursor;

typedef struct _XDND_Struct {
  Tk_Window    MainWindow;          /* The main window of our application */
  Tcl_Interp  *interp;              /* A Tcl Interpreter */
  Display     *display;             /* Display Pointer */
  Window       RootWindow;          /* Display's root window */
  Atom         XDNDVersion;         /* Protocol version being used */
  int          x;                   /* Current position of the mouse */
  int          y;                   /* Current position of the mouse */
  int          button;              /* Current button used for drag operation*/
  unsigned int state;               /* Current state to convert to modifiers */
  int          CallbackStatus;      /* The return value of last tcl callback */
  XDND_BOOL    ResetValues;         /* This is used by TkDND_HandleDNDLeave */

    /* Slots used during drag... */
  XDND_BOOL   InternalDrag;         /* True if the drag is in the same app */
  XDND_BOOL   ReceivedStatusFlag;   /* True if received any XdndStatus 
                                        from current target */
  char       *data;                 /* Pointer to hold the dnd data */
  int         index;                /* Length of dnd data */
  
    /* Drag source window info */
  Window      DraggerWindow;        /* Window of the drag source */
  Atom       *DraggerTypeList;      /* Type list supported by the drag source */
  Atom       *DraggerAskActionList; /* Actions supported by the drag source */
  char       *DraggerAskDescriptions; /* Descriptions of supported actions */
  Tk_Window   CursorWindow;         /* A window to replace cursor */
  char       *CursorCallback;       /* A Callback to update cursor window */
  XDND_BOOL   WaitForStatusFlag;    /* True if waiting for XdndStatus message */

    /* Current drop target info */ 
  Window      Toplevel;             /* The toplevel that the mouse is in */
  Window      MouseWindow;          /* Window that mouse is in; not owned */
  XDND_BOOL   MouseWindowIsAware;   /* True if itsMouseWindow has XdndAware */
  Window      MsgWindow;            /* Window that receives messages 
                                       (not MouseWindow if proxy) */
  Atom        DesiredType;          /* The drop desired type */
#ifdef XDND_USE_TK_GET_SELECTION
  char       *DesiredName;          /* The string of the desired type */
#endif /* XDND_USE_TK_GET_SELECTION */
  Atom        SupportedAction;      /* The supported by the target action */
  
  XDND_BOOL   WillAcceptDropFlag;   /* True if target will accept drop */
  Time        LastEventTime;        /* Time of the last processed event */
  
  XDND_BOOL   IsDraggingFlag;       /* True until XDND_Finish is called */
  XDND_BOOL   UseMouseRectFlag;     /* True if use MouseRectR */
  XRectangle  MouseRectR;           /* Don't send another XdndPosition while 
                                       inside here */
  XDNDCursor *cursors;              /* The available cursors... */

  /*
   * Motif slots...
   */
#ifdef TKDND_ENABLE_MOTIF_DROPS
  DndData     Motif_DND_Data;       /* Internal Motif data... */
  int         Motif_DND;            /* True if protocol in use is Motif */
  Atom        Motif_DND_SuccessAtom;
  Atom        Motif_DND_FailureAtom;
#endif /* TKDND_ENABLE_MOTIF_DROPS */
#ifdef TKDND_ENABLE_MOTIF_DRAGS
  Window      Motif_LastToplevel;   /* The last toplevel we send enter */
  int         Motif_ToplevelAware;  /* True if dnd->Toplevel supports Motif */
  Atom        Motif_DND_Selection;  /* The selection that will be used */
  Atom        Motif_DND_WM_STATE;   /* WM_STATE property */
#endif /* TKDND_ENABLE_MOTIF_DRAGS */

  /*
   * Keep info about the last window that we send a <DragEnter> event.
   * If the LastEnterDeliveredWindow is not None, then we have to simulate
   * a DragLeave to that window. We have to do that, as qt for example
   * sometimes forgets (!) to send Leave events to windows that overlap...
   */
  Window      LastEnterDeliveredWindow;
  unsigned int
              Alt_ModifierMask;    /* The modifier the Alt keys are bind to */
  unsigned int
              Meta_ModifierMask;   /* The modifier the Meta keys are bind to */
  
  /* Here we keep some frequently used Atoms... */
  Atom        DNDSelectionName;

  Atom        DNDProxyXAtom;
  Atom        DNDAwareXAtom;
  Atom        DNDTypeListXAtom;
  
  Atom        DNDEnterXAtom;
  Atom        DNDHereXAtom;
  Atom        DNDStatusXAtom;
  Atom        DNDLeaveXAtom;
  Atom        DNDDropXAtom;
  Atom        DNDFinishedXAtom;

  Atom        DNDActionCopyXAtom;
  Atom        DNDActionMoveXAtom;
  Atom        DNDActionLinkXAtom;
  Atom        DNDActionAskXAtom;
  Atom        DNDActionPrivateXAtom;

  Atom        DNDActionListXAtom;
  Atom        DNDActionDescriptionXAtom;

  Atom        DNDDirectSave0XAtom;

  Atom        DNDMimePlainTextXAtom;
  Atom        DNDStringAtom;
  Atom        DNDNonProtocolAtom;

  /*
   * Allow user to register event callbacks...
   */

  /* This function will be called for various windows (application or
   * foreign). It must return True if given window is a valid one and belongs
   * to this application. Else, False must be returned.
   * If this method is not set then the code assumes that no widgets have
   * support for recieving drops. In this case none of the widget methods
   * need be set.
   */
  int (*WidgetExistsCallback) (struct _XDND_Struct *dnd, Window window);
  /* This function will be called when the mouse has entered a widget to
   * announce "XdndEnter". This function must return True if the widget will
   * accept the drop and False otherwise. If True is returned, the apply
   * position callback (the next pointer :-)) will be immediately called...
   */
  int (*WidgetApplyEnterCallback) (struct _XDND_Struct *dnd,
        Window target, Window source, Atom action, int x, int y,
        Time t, Atom *typelist);
  /* This function will be called when the mouse is moving inside a drag
   * target. It is responsible for changing the drop target widget appearence.
   * It must also sets the correct data to pointers. As per the protocol, if
   * the widget cannot perform the action specified by `action' then it should
   * return either XdndActionPrivate or XdndActionCopy into supported_action
   * (leaving  supported_action unchanged is equivalent to XdndActionCopy).
   * Returns True if widget is ready to accept the drop...
   */
  int (*WidgetApplyPositionCallback) (struct _XDND_Struct *dnd,
        Window target, Window source, Atom action, Atom *actionList,
        int x, int y, Time t, Atom *typelist, int *wantPosition,
        Atom *supported_action, Atom *desired_type, XRectangle *rectangle);
  /* This function will be called when the mouse leaves a drop target widget,
   * or when the drop is canceled. It must update the widget border to its
   * default appearance...
   */
  int (*WidgetApplyLeaveCallback) (struct _XDND_Struct *dnd, Window target);
  /* This function must insert the data into the drop target */
  int (*WidgetInsertDropDataCallback) (struct _XDND_Struct *dnd,
       unsigned char *data, int length, int remaining,
       Window into, Window from, Atom type);
  /* This function will be used in order to get the user-prefered action, if
   * the action is XdndAsk... */
  int (*Ask) (struct _XDND_Struct *dnd, Window source, Window target,
              Atom *action);
  /* This is our callback to get the data that is to be dropped from the drag
   * source... */
  int (*GetData) (struct _XDND_Struct *dnd, Window source, 
                  unsigned char **data, int *length, Atom type);
  /* This function will be called, in order to handle events that are not
   * related to the XDND protocol... */
  void (*HandleEvents) (struct _XDND_Struct *dnd, XEvent *xevent);
  /*
   * This function must return the current types that a window supports as a
   * drag source...
   */
  Atom *(*GetDragAtoms) (struct _XDND_Struct *dnd, Window window);
  /*
   * Set the cursor callback...
   * The requested cursors will be one of the extern Cursor variables:
   *   noDropCursor, moveCursor, copyCursor, linkCursor, askCursor
   */
  int (*SetCursor) (struct _XDND_Struct *dnd, int cursor);
} XDND;
#define DndClass XDND
extern Cursor noDropCursor, moveCursor, copyCursor, linkCursor, askCursor;

/*
 * Function Prototypes...
 */

void      XDND_Reset(XDND *dndp);
XDND     *XDND_Init(Display *display);
void      XDND_Enable(XDND *dnd, Window window);
XDND_BOOL XDND_IsDndAware(XDND *dnd, Window window, Window* proxy, Atom *vers);
int       XDND_AtomListLength(Atom *list);
int       XDND_DescriptionListLength(char *list);
Atom     *XDND_GetTypeList(XDND *dnd, Window window);
void      XDND_AnnounceTypeList(XDND *dnd, Window window, Atom *list);
void      XDND_AppendType(XDND *dnd, Window window, Atom type);
void      XDND_AnnounceAskActions(XDND *dnd, Window window, Atom *Actions,
                                  char *Descriptions);
Atom     *XDND_GetAskActions(XDND *dnd, Window window);
char     *XDND_GetAskActionDescriptions(XDND *dnd, Window window);
XDND_BOOL XDND_DraggerCanProvideText(XDND *dnd);
XDND_BOOL XDND_FindTarget(XDND *dnd, int x, int y,
                          Window *toplevel, Window *msgWindow,
                          Window *target, XDND_BOOL *aware, Atom *version);
Window    XDND_FindToplevel(XDND *dnd, Window window);

XDND_BOOL XDND_BeginDrag(XDND *dnd, Window source, Atom *actions, Atom *types,
                         char *Descriptions, Tk_Window cursor_window,
                         char *cursor_callback);

void      XDND_SendDNDEnter(XDND *dnd, Window window, Window msgWindow,
                            XDND_BOOL isAware, Atom vers);
XDND_BOOL XDND_SendDNDPosition(XDND *dnd, Atom action);
XDND_BOOL XDND_SendDNDStatus(XDND *dnd, Atom action);
XDND_BOOL XDND_SendDNDLeave(XDND *dnd);
XDND_BOOL XDND_SendDNDDrop(XDND *dnd);
XDND_BOOL XDND_SendDNDSelection(XDND *dnd, XSelectionRequestEvent *request);

int XDND_HandleClientMessage(XDND *dnd, XEvent *xevent);
int XDND_HandleDNDEnter(XDND *dnd, XClientMessageEvent clientMessage);
int XDND_HandleDNDHere(XDND *dnd, XClientMessageEvent clientMessage);
int XDND_HandleDNDLeave(XDND *dnd, XClientMessageEvent clientMessage);
int XDND_HandleDNDDrop(XDND *dnd, XClientMessageEvent clientMessage);
int XDND_GetSelProc(ClientData clientData, Tcl_Interp *interp, char *portion);

int XDND_HandleDNDStatus(XDND *dnd, XClientMessageEvent clientMessage);

#ifdef TKDND_ENABLE_MOTIF_DROPS
int MotifDND_HandleClientMessage(XDND *dnd, XEvent xevent);
#endif /* TKDND_ENABLE_MOTIF_DROPS */
#ifdef  __cplusplus
}
#endif

#endif /* _X_DND_H */
