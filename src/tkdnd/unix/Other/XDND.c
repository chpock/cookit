/*
 * XDND.c -- Tk XDND Drag'n'Drop Protocol Implementation
 * 
 *    This file implements the unix portion of the drag&drop mechanism
 *    for the tk toolkit. The protocol in use under unix is the
 *    XDND protocol.
 *
 * This software is copyrighted by:
 * George Petasis, National Centre for Scientific Research "Demokritos",
 * Aghia Paraskevi, Athens, Greece.
 * e-mail: petasis@iit.demokritos.gr
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h> 
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <tcl.h>
#include <tk.h>
#include <XDND.h>
#include <time.h>

/*
 * Define the default cursors...
 */

/*
static char dnd_copy_cursor_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0x00, 0x02, 0x00, 0x08, 0x01,
   0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0xe8, 0x0f,
   0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0x08, 0x01,
   0x02, 0x00, 0x08, 0x00, 0x02, 0x04, 0x08, 0x00, 0x02, 0x0c, 0x08, 0x00,
   0x02, 0x1c, 0x08, 0x00, 0x02, 0x3c, 0x08, 0x00, 0x02, 0x7c, 0x08, 0x00,
   0x02, 0xfc, 0x08, 0x00, 0x02, 0xfc, 0x09, 0x00, 0x02, 0xfc, 0x0b, 0x00,
   0x02, 0x7c, 0x08, 0x00, 0xfe, 0x6d, 0x0f, 0x00, 0x00, 0xc4, 0x00, 0x00,
   0x00, 0xc0, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80, 0x01, 0x00,
   0x00, 0x00, 0x00, 0x00};

static char dnd_copy_mask_bits[] = {
   0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f,
   0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
   0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
   0x07, 0x06, 0xfc, 0x1f, 0x07, 0x0e, 0xfc, 0x1f, 0x07, 0x1e, 0x1c, 0x00,
   0x07, 0x3e, 0x1c, 0x00, 0x07, 0x7e, 0x1c, 0x00, 0x07, 0xfe, 0x1c, 0x00,
   0x07, 0xfe, 0x1d, 0x00, 0x07, 0xfe, 0x1f, 0x00, 0x07, 0xfe, 0x1f, 0x00,
   0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0x1e, 0x00, 0xff, 0xef, 0x1f, 0x00,
   0x00, 0xe6, 0x01, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x00, 0xc0, 0x03, 0x00,
   0x00, 0x80, 0x01, 0x00};

static char dnd_move_cursor_bits[] = {
   0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0x02, 0x00, 0x08, 0x02, 0x00, 0x08,
   0x02, 0x00, 0x08, 0x02, 0x00, 0x08, 0x02, 0x00, 0x08, 0x02, 0x00, 0x08,
   0x02, 0x00, 0x08, 0x02, 0x00, 0x08, 0x02, 0x04, 0x08, 0x02, 0x0c, 0x08,
   0x02, 0x1c, 0x08, 0x02, 0x3c, 0x08, 0x02, 0x7c, 0x08, 0x02, 0xfc, 0x08,
   0x02, 0xfc, 0x09, 0x02, 0xfc, 0x0b, 0x02, 0x7c, 0x08, 0xfe, 0x6d, 0x0f,
   0x00, 0xc4, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01,
   0x00, 0x00, 0x00};

static char dnd_move_mask_bits[] = {
   0xff, 0xff, 0x1f, 0xff, 0xff, 0x1f, 0xff, 0xff, 0x1f, 0x07, 0x00, 0x1c,
   0x07, 0x00, 0x1c, 0x07, 0x00, 0x1c, 0x07, 0x00, 0x1c, 0x07, 0x00, 0x1c,
   0x07, 0x00, 0x1c, 0x07, 0x06, 0x1c, 0x07, 0x0e, 0x1c, 0x07, 0x1e, 0x1c,
   0x07, 0x3e, 0x1c, 0x07, 0x7e, 0x1c, 0x07, 0xfe, 0x1c, 0x07, 0xfe, 0x1d,
   0x07, 0xfe, 0x1f, 0x07, 0xfe, 0x1f, 0xff, 0xff, 0x1f, 0xff, 0xff, 0x1e,
   0xff, 0xef, 0x1f, 0x00, 0xe6, 0x01, 0x00, 0xc0, 0x03, 0x00, 0xc0, 0x03,
   0x00, 0x80, 0x01};

static char dnd_link_cursor_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0x00, 0x02, 0x00, 0x08, 0x01,
   0x02, 0x00, 0x88, 0x00, 0x02, 0x00, 0x48, 0x00, 0x02, 0x00, 0xe8, 0x0f,
   0x02, 0x00, 0x48, 0x00, 0x02, 0x00, 0x88, 0x00, 0x02, 0x00, 0x08, 0x01,
   0x02, 0x00, 0x08, 0x00, 0x02, 0x04, 0x08, 0x00, 0x02, 0x0c, 0x08, 0x00,
   0x02, 0x1c, 0x08, 0x00, 0x02, 0x3c, 0x08, 0x00, 0x02, 0x7c, 0x08, 0x00,
   0x02, 0xfc, 0x08, 0x00, 0x02, 0xfc, 0x09, 0x00, 0x02, 0xfc, 0x0b, 0x00,
   0x02, 0x7c, 0x08, 0x00, 0xfe, 0x6d, 0x0f, 0x00, 0x00, 0xc4, 0x00, 0x00,
   0x00, 0xc0, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80, 0x01, 0x00,
   0x00, 0x00, 0x00, 0x00};

static char dnd_link_mask_bits[] = {
   0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f,
   0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
   0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
   0x07, 0x06, 0xfc, 0x1f, 0x07, 0x0e, 0xfc, 0x1f, 0x07, 0x1e, 0x1c, 0x00,
   0x07, 0x3e, 0x1c, 0x00, 0x07, 0x7e, 0x1c, 0x00, 0x07, 0xfe, 0x1c, 0x00,
   0x07, 0xfe, 0x1d, 0x00, 0x07, 0xfe, 0x1f, 0x00, 0x07, 0xfe, 0x1f, 0x00,
   0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0x1e, 0x00, 0xff, 0xef, 0x1f, 0x00,
   0x00, 0xe6, 0x01, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x00, 0xc0, 0x03, 0x00,
   0x00, 0x80, 0x01, 0x00};

static char dnd_ask_cursor_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x0f, 0x00, 0x02, 0x00, 0x88, 0x03,
   0x02, 0x00, 0x48, 0x04, 0x02, 0x00, 0x08, 0x04, 0x02, 0x00, 0x08, 0x02,
   0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0x08, 0x01, 0x02, 0x00, 0x08, 0x00,
   0x02, 0x00, 0x08, 0x01, 0x02, 0x04, 0x08, 0x00, 0x02, 0x0c, 0x08, 0x00,
   0x02, 0x1c, 0x08, 0x00, 0x02, 0x3c, 0x08, 0x00, 0x02, 0x7c, 0x08, 0x00,
   0x02, 0xfc, 0x08, 0x00, 0x02, 0xfc, 0x09, 0x00, 0x02, 0xfc, 0x0b, 0x00,
   0x02, 0x7c, 0x08, 0x00, 0xfe, 0x6d, 0x0f, 0x00, 0x00, 0xc4, 0x00, 0x00,
   0x00, 0xc0, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80, 0x01, 0x00,
   0x00, 0x00, 0x00, 0x00};

static char dnd_ask_mask_bits[] = {
   0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff, 0xff, 0x1f,
   0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
   0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f, 0x07, 0x00, 0xfc, 0x1f,
   0x07, 0x06, 0xfc, 0x1f, 0x07, 0x0e, 0xfc, 0x1f, 0x07, 0x1e, 0x1c, 0x00,
   0x07, 0x3e, 0x1c, 0x00, 0x07, 0x7e, 0x1c, 0x00, 0x07, 0xfe, 0x1c, 0x00,
   0x07, 0xfe, 0x1d, 0x00, 0x07, 0xfe, 0x1f, 0x00, 0x07, 0xfe, 0x1f, 0x00,
   0xff, 0xff, 0x1f, 0x00, 0xff, 0xff, 0x1e, 0x00, 0xff, 0xef, 0x1f, 0x00,
   0x00, 0xe6, 0x01, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x00, 0xc0, 0x03, 0x00,
   0x00, 0x80, 0x01, 0x00};

 
static XDNDCursor dnd_cursors[] =
{
    {29, 25, 10, 10, dnd_copy_cursor_bits, dnd_copy_mask_bits,
                     "XdndNoDrop", 0, 0, 0, 0},
    {29, 25, 10, 10, dnd_copy_cursor_bits, dnd_copy_mask_bits,
                     "XdndActionCopy", 0, 0, 0, 0},
    {21, 25, 10, 10, dnd_move_cursor_bits, dnd_move_mask_bits,
                     "XdndActionMove", 0, 0, 0, 0},
    {29, 25, 10, 10, dnd_link_cursor_bits, dnd_link_mask_bits,
                     "XdndActionLink", 0, 0, 0, 0},
    {29, 25, 10, 10, dnd_ask_cursor_bits, dnd_ask_mask_bits,
                     "XdndActionAsk", 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};*/

/*
 * XDND_Reset --
 *  Resets a dnd structure...
 */
void XDND_Reset(XDND *dndp) {
  dndp->interp                      = NULL;
  dndp->x                           = 0;
  dndp->y                           = 0;
  dndp->CallbackStatus              = 0;
  
  dndp->InternalDrag                = False;
  dndp->ReceivedStatusFlag          = False;
  if (dndp->data != NULL)             Tcl_Free(dndp->data);
  dndp->data                        = NULL;
  dndp->index                       = 0;
  
  dndp->DraggerWindow               = None;
  if (dndp->DraggerTypeList != NULL)  Tcl_Free((char *)dndp->DraggerTypeList);
  dndp->DraggerTypeList             = NULL;
  dndp->DraggerAskActionList        = NULL;
  dndp->WaitForStatusFlag           = False;
  
  dndp->Toplevel                    = None;
  dndp->MouseWindow                 = None;
  dndp->MouseWindowIsAware          = False;
  dndp->MsgWindow                   = None;
  dndp->DesiredType                 = None;
#ifdef XDND_USE_TK_GET_SELECTION
  dndp->DesiredName                 = NULL;
#endif /* XDND_USE_TK_GET_SELECTION */
  dndp->SupportedAction             = None;

  dndp->WillAcceptDropFlag          = False;
  dndp->LastEventTime               = CurrentTime;
  
  dndp->IsDraggingFlag              = False;
  dndp->LastEnterDeliveredWindow    = None;
#ifdef TKDND_ENABLE_MOTIF_DROPS
  dndp->Motif_DND                   = False;
#endif /* TKDND_ENABLE_MOTIF_DROPS */
} /* XDND_Reset */

/*
 * XDND_Init --
 *  Allocates & initialises a dnd structure...
 */
XDND *XDND_Init(Display *display) {
  XDND *dndp;
/*XDNDCursor *cursor;
  XColor black, white;*/

  dndp = (XDND *) Tcl_Alloc(sizeof(XDND));
  if (dndp == NULL) return NULL;

  /* Initialise dndp structure */
  dndp->data                        = NULL;
  dndp->DraggerTypeList             = NULL;
  dndp->DraggerAskDescriptions      = NULL;
  XDND_Reset(dndp);

  dndp->display                     = display;
  dndp->RootWindow                  = DefaultRootWindow(display);
  dndp->XDNDVersion                 = XDND_VERSION;
  
  /*dndp->cursors                     = dnd_cursors;*/

  /*
   * Initialise Cursors...
   */
  /*black.pixel = BlackPixel(display, DefaultScreen(display));
  white.pixel = WhitePixel(display, DefaultScreen(display));

  XQueryColor(display, DefaultColormap(display, DefaultScreen(display)),&black);
  XQueryColor(display, DefaultColormap(display, DefaultScreen(display)),&white);

  for (cursor = &dndp->cursors[0]; cursor->width; cursor++) {
    cursor->image_pixmap = XCreateBitmapFromData(display, dndp->RootWindow,
      (char *) cursor->image_data, cursor->width, cursor->height);
    cursor->mask_pixmap = XCreateBitmapFromData(display, dndp->RootWindow,
      (char *) cursor->mask_data, cursor->width, cursor->height);
    cursor->cursor = XCreatePixmapCursor (display, cursor->image_pixmap,
      cursor->mask_pixmap, &black, &white, cursor->x, cursor->y);
    XFreePixmap (display, cursor->image_pixmap);
    XFreePixmap (display, cursor->mask_pixmap);
    cursor->action = XInternAtom (dndp->display, cursor->_action, False);
    cursor->image_pixmap = cursor->mask_pixmap = 0;
  }*/

  /*
   * Create required X atoms...
   */
  dndp->DNDSelectionName      = XInternAtom(display, "XdndSelection", False);
  dndp->DNDProxyXAtom         = XInternAtom(display, "XdndProxy", False);
  dndp->DNDAwareXAtom         = XInternAtom(display, "XdndAware", False);
  dndp->DNDTypeListXAtom      = XInternAtom(display, "XdndTypeList", False);
  dndp->DNDEnterXAtom         = XInternAtom(display, "XdndEnter", False);
  dndp->DNDHereXAtom          = XInternAtom(display, "XdndPosition", False);
  dndp->DNDStatusXAtom        = XInternAtom(display, "XdndStatus", False);
  dndp->DNDLeaveXAtom         = XInternAtom(display, "XdndLeave", False);
  dndp->DNDDropXAtom          = XInternAtom(display, "XdndDrop", False);
  dndp->DNDFinishedXAtom      = XInternAtom(display, "XdndFinished", False);
  dndp->DNDActionCopyXAtom    = XInternAtom(display, "XdndActionCopy", False);
  dndp->DNDActionMoveXAtom    = XInternAtom(display, "XdndActionMove", False);
  dndp->DNDActionLinkXAtom    = XInternAtom(display, "XdndActionLink", False);
  dndp->DNDActionAskXAtom     = XInternAtom(display, "XdndActionAsk", False);
  dndp->DNDActionPrivateXAtom = XInternAtom(display, "XdndActionPrivate",False);
  dndp->DNDActionListXAtom    = XInternAtom(display, "XdndActionList", False);
  dndp->DNDActionDescriptionXAtom =
                          XInternAtom(display,"XdndActionDescription", False);
  dndp->DNDDirectSave0XAtom   = XInternAtom(display, "XdndDirectSave0", False);
  dndp->DNDMimePlainTextXAtom = XInternAtom(display, "text/plain", False);
  dndp->DNDStringAtom         = XInternAtom(display, "STRING", False);
  dndp->DNDNonProtocolAtom    = XInternAtom(display, "TkDndBinarySelectionAtom",
                                            False);

  /*
   * Motif Needed XAtoms...
   */
#ifdef TKDND_ENABLE_MOTIF_DROPS
  dndp->Motif_DND             = False;
  dndp->Motif_DND_SuccessAtom = XInternAtom(display, "XmTRANSFER_SUCCESS",
                                            False);
  dndp->Motif_DND_FailureAtom = XInternAtom(display, "XmTRANSFER_FAILURE",
                                            False);
#endif /* TKDND_ENABLE_MOTIF_DROPS */

  dndp->WidgetExistsCallback         = NULL;
  dndp->WidgetApplyEnterCallback     = NULL;
  dndp->WidgetApplyPositionCallback  = NULL;
  dndp->WidgetApplyLeaveCallback     = NULL;
  dndp->WidgetInsertDropDataCallback = NULL;
  dndp->Ask                          = NULL;
  dndp->GetData                      = NULL;
  dndp->HandleEvents                 = NULL;
  dndp->GetDragAtoms                 = NULL;
  dndp->SetCursor                    = NULL;
  return dndp;
} /* XDND_Init */

/*
 * EnableDND --
 *  Creates the XdndAware property on the specified window and all its parents
 */
void XDND_Enable(XDND *dnd, Window window) {
  Tk_Window tkwin;
  Window root_return, parent, *children_return = 0;
  unsigned int nchildren_return;
  int status;
  Atom version = XDND_VERSION;

  /*
   * Find the real toplevel that holds the wanted window. The property must be
   * set on each top-level X window that contains widgets that can accept
   * drops. (new in version 3)
   * The property should not be set on subwindows... 
   */
  status = XQueryTree(dnd->display, window, &root_return, &parent,
                      &children_return, &nchildren_return);
  if (children_return) XFree(children_return);
  if (status) {
    if (dnd->WidgetExistsCallback) {
      if (!(*dnd->WidgetExistsCallback) (dnd, parent)) {
        /*
         * Force Tk to create the window...
         */
        tkwin = Tk_IdToWindow(dnd->display, window);
        if (tkwin != NULL) Tk_MakeWindowExist(tkwin);
        XChangeProperty(dnd->display, window, dnd->DNDAwareXAtom, XA_ATOM, 32,
               PropModeReplace, (unsigned char *) &version, 1);
        XDND_DEBUG2("XDND_Enable: Enabling window %ld\n", window);

#ifdef TKDND_ENABLE_MOTIF_DROPS
        DndWriteReceiverProperty(dnd->display, window, DND_DRAG_DYNAMIC);
#endif /* TKDND_ENABLE_MOTIF_DROPS */
      } else {
        XDND_Enable(dnd, parent);
      }
    }
  }
} /* XDND_Enable */

/*
 * XDND_IsDndAware --
 *  Returns True if the given X window supports the XDND protocol.
 *    proxy is the window to which the client messages should be sent.
 *    vers is the version to use.
 */
XDND_BOOL XDND_IsDndAware(XDND *dnd, Window window, Window* proxy, Atom *vers) {
  XDND_BOOL result = False;
  Atom actualType, *data;
  int actualFormat;
  unsigned long itemCount, remainingBytes;
  unsigned char* rawData = NULL;
  *proxy = window;
  *vers  = 0;

  /* check for XdndProxy... */
  XGetWindowProperty(dnd->display, window, dnd->DNDProxyXAtom,
                     0, LONG_MAX, False, XA_WINDOW, &actualType, &actualFormat,
                     &itemCount, &remainingBytes, &rawData);
  if (actualType == XA_WINDOW && actualFormat == 32 && itemCount > 0) {
    *proxy = (Window) rawData;
    XFree(rawData);
    rawData = NULL;

    /* check XdndProxy on proxy window -- must point to itself */
    XGetWindowProperty(dnd->display, *proxy, dnd->DNDProxyXAtom,
                      0, LONG_MAX, False, XA_WINDOW, &actualType, &actualFormat,
                      &itemCount, &remainingBytes, &rawData);
    if (actualType != XA_WINDOW || actualFormat != 32 || itemCount == 0 ||
       (Window) rawData != *proxy) {
      *proxy = window;
    }
  }
  XFree(rawData);
  rawData = NULL;

  /* check XdndAware... */
  XGetWindowProperty(dnd->display, *proxy, dnd->DNDAwareXAtom,
                     0, LONG_MAX, False, XA_ATOM, &actualType, &actualFormat,
                     &itemCount, &remainingBytes, &rawData);
  if (actualType == XA_ATOM && actualFormat == 32 && itemCount > 0) {
    data = (Atom*) rawData;
    if (data[0] >= XDND_MINVERSION) {
      result = True;
      if (XDND_MINVERSION < data[0]) *vers = XDND_MINVERSION;
      else *vers = data[0];
      XDND_DEBUG3("XDND_IsDndAware: Window %ld is XDND aware. Version: %ld\n",
                  window, *vers);
    } else {
      *proxy = None;
    }
  }
  XFree(rawData);
  return result;
} /* XDND_IsDndAware */

/*
 * XDND_AtomListLength --
 *  This function returns the length of a given Atom list
 */
int XDND_AtomListLength(Atom *list) {
  int n;
  if (list == NULL) return 0;
  for (n = 0; list[n]; n++);
  return n;
} /* XDND_AtomListLength */

/*
 * XDND_DescriptionListLength --
 *  This function returns the length of a given Description list...
 */
int XDND_DescriptionListLength(char *list) {
  int n = 0;

  if (list == NULL) return 0;
  while (1) {
    if (*list == '\0' && *(list+1) == '\0') return n+1;
    /* Just to make sure... */
    if (n > 1000000) return 0;
    list++;
    n++;
  }
} /* XDND_DescriptionListLength */

/*
 * XDND_GetTypeList --
 *  Returns a type list of all the currently defined types of the given
 *  window. 
 *  Note that the caller must free the pointer returned...
 */
Atom *XDND_GetTypeList(XDND *dnd, Window window) {
  Atom actualType, *data, *TypeList=NULL;
  int actualFormat, i;
  unsigned long itemCount, remainingBytes;
  unsigned char *rawData = NULL;
  
  /*
   * Check if the "XdndTypeList" property is present...
   */
  XGetWindowProperty(dnd->display, window, dnd->DNDTypeListXAtom, 
                     0, LONG_MAX, False, XA_ATOM, &actualType, &actualFormat,
                     &itemCount, &remainingBytes, &rawData);
  if (actualType == XA_ATOM && actualFormat == 32 && itemCount > 0) {
    TypeList = (Atom *) Tcl_Alloc(sizeof(Atom)*(itemCount+1));
    if (TypeList == NULL) return NULL;
    data = (Atom *) rawData;
    for (i=0; i<itemCount; i++) {
      TypeList[i] = data[i];
    }
    TypeList[itemCount] = 0;
    XFree(rawData);
  } else {
    /*
     * The "XdndTypeList" was non-existent or empty...
     */
    if (rawData != NULL) XFree(rawData);
  }
  return TypeList;
} /* XDND_GetTypeList */

/*
 * XDND_AnnounceTypeList --
 *  Creates the XdndTypeList property on the specified window if there
 *  are more than XDND_ENTERTYPECOUNT data types. Also, finds the toplevel
 *  window and sets the first three types on property XdndAware.
 */
void XDND_AnnounceTypeList(XDND *dnd, Window window, Atom *list) {
  Window toplevel;
  Atom version = XDND_VERSION;
  int typeCount = XDND_AtomListLength(list);
#ifdef DND_DEBUG
  int i;
#endif /* DND_DEBUG */
  
  /*
   * Make sure that the toplevel is XDND aware. Also, try to append the
   * supported types...
   */
  toplevel = XDND_FindToplevel(dnd, window);
  if (toplevel != None) {
    XDND_DEBUG2("XDND_AnnounceTypeList: Refresing XdndAware on toplevel %ld\n",
                toplevel);
    XChangeProperty(dnd->display, toplevel, dnd->DNDAwareXAtom, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *) &version, 1);
    XChangeProperty(dnd->display, toplevel, dnd->DNDAwareXAtom, XA_ATOM, 32,
                    PropModeAppend,  (unsigned char *) list, typeCount);
  }

  if (typeCount > XDND_ENTERTYPECOUNT) {
    /* The list has more than XDND_ENTERTYPECOUNT types. The property 
     * "XdndTypeList" must be created...
     */
    if (toplevel != None) {
      XChangeProperty(dnd->display, toplevel, dnd->DNDTypeListXAtom, XA_ATOM,32,
                      PropModeReplace, (unsigned char*) list, typeCount);
    }
    XChangeProperty(dnd->display, window, dnd->DNDTypeListXAtom, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*) list, typeCount);
    XDND_DEBUG3("XDND_AnnounceTypeList: TypeList (%d) on window %ld:\n",
                typeCount, window);
  } else {
    XDND_DEBUG4("XDND_AnnounceTypeList: > %d types (%d) on window %ld:\n",
                 XDND_ENTERTYPECOUNT, typeCount, window);
  }
#ifdef DND_DEBUG
  for (i=0; i<typeCount; i++) {
    XDND_DEBUG3("\t==> Atom[%d]: %s\n", i, XGetAtomName(dnd->display, list[i]));
  }
#endif /* DND_DEBUG */
} /* XDND_AnnounceTypeList */

/*
 * XDND_AppendType --
 *  This function will append a type in the current acceptable typelist of the
 *  specified window...
 */
void XDND_AppendType(XDND *dnd, Window window, Atom type) {
  Atom types[2];

  types[0] = type;
  types[1] = 0;
  XChangeProperty(dnd->display, window, dnd->DNDTypeListXAtom, XA_ATOM, 32,
                    PropModeAppend, (unsigned char*) types, 1);
} /* XDND_AppendType */

/*
 * XDND_AnnounceAskActions --
 *  Creates the XdndActionList and XdndActionDescription properties on
 *  DraggerWindow.
 */
void XDND_AnnounceAskActions(XDND *dnd, Window window, Atom *Actions,
                             char *Descriptions) {
  Window toplevel;
  int actionCount, desCount;
#ifdef DND_DEBUG
  int i;
#endif /* DND_DEBUG */
  
  /*
   * Announce actions to the toplevel window also...
   */
  actionCount = XDND_AtomListLength(Actions);
  desCount    = XDND_DescriptionListLength(Descriptions);
  toplevel    = XDND_FindToplevel(dnd, window);
  if (toplevel != None) {
    XDND_DEBUG3("XDND_AnnounceAskActions: Announcing on toplevel %ld %d "
                "actions:\n", toplevel, actionCount);
    XChangeProperty(dnd->display, toplevel, dnd->DNDActionListXAtom, XA_ATOM,
       32, PropModeReplace, (unsigned char *) Actions, actionCount);
    XChangeProperty(dnd->display, toplevel, dnd->DNDActionDescriptionXAtom,
       XA_STRING, 8, PropModeReplace, (unsigned char *) Descriptions,
       desCount);
  }
  XChangeProperty(dnd->display, window, dnd->DNDActionListXAtom, XA_ATOM,
       32, PropModeReplace, (unsigned char *) Actions, actionCount);
  XChangeProperty(dnd->display, window, dnd->DNDActionDescriptionXAtom,
       XA_STRING, 8, PropModeReplace, (unsigned char *) Descriptions,
       desCount);
  
#ifdef DND_DEBUG
  for (i = 0; i < actionCount; i++) {
    XDND_DEBUG3("\t==> Atom[%d]: %s\n", i, 
      XGetAtomName(dnd->display, Actions[i]));
  }
#endif /* DND_DEBUG */
} /* XDND_AnnounceAskActions */

/*
 * XDND_GetAskActions --
 *  Returns the action list of all the currently defined actions of the given
 *  window. 
 *  Note that the caller must free the pointer returned...
 */
Atom *XDND_GetAskActions(XDND *dnd, Window window) {
  Atom actualType, *data, *ActionList=NULL;
  int actualFormat, i;
  unsigned long itemCount, remainingBytes;
  unsigned char *rawData = NULL;
  
  /*
   * Check if the "XdndActionList" property is present...
   */
  XGetWindowProperty(dnd->display, window, dnd->DNDActionListXAtom, 
                     0, LONG_MAX, False, XA_ATOM, &actualType, &actualFormat,
                     &itemCount, &remainingBytes, &rawData);
  if (actualType == XA_ATOM && actualFormat == 32 && itemCount > 0) {
    ActionList = (Atom *) Tcl_Alloc(sizeof(Atom)*(itemCount+1));
    if (ActionList == NULL) return NULL;
    data = (Atom *) rawData;
    for (i=0; i<itemCount; i++) {
      ActionList[i] = data[i];
    }
    ActionList[itemCount] = 0;
    XFree(rawData);
  } else {
    /*
     * The "XdndActionList" was non-existent or empty...
     */
    if (rawData != NULL) XFree(rawData);
  }
  return ActionList;
} /* XDND_GetAskActions */

/*
 * XDND_GetAskActionDescriptions --
 *  Returns the action descriptions of all the currently defined actions of
 *  the given window. 
 *  Note that the caller must free the pointer returned...
 */
char *XDND_GetAskActionDescriptions(XDND *dnd, Window window) {
  Atom actualType;
  char *data;
  int actualFormat;
  unsigned long itemCount, remainingBytes;
  unsigned char *rawData = NULL;
  
  /*
   * Check if the "XdndDescriptionList" property is present...
   */
  XGetWindowProperty(dnd->display, window, dnd->DNDActionDescriptionXAtom, 
                     0, LONG_MAX, False, XA_STRING, &actualType, &actualFormat,
                     &itemCount, &remainingBytes, &rawData);
  if (actualType == XA_STRING && actualFormat == 8 && itemCount > 0) {
    data = (char *) rawData;
    if (dnd->DraggerAskDescriptions != NULL) {
      memset(dnd->DraggerAskDescriptions, '\0', TKDND_MAX_DESCRIPTIONS_LENGTH);
      if (itemCount > TKDND_MAX_DESCRIPTIONS_LENGTH - 1) {
        itemCount = TKDND_MAX_DESCRIPTIONS_LENGTH -1;
        data[itemCount]   = '\0';
        data[itemCount+1] = '\0';
      }
      memcpy(dnd->DraggerAskDescriptions, data, itemCount+1);
    }
    XFree(rawData);
  } else {
    /*
     * The "XdndDescriptionList" was non-existent or empty...
     */
    if (rawData != NULL) XFree(rawData);
  }
  return dnd->DraggerAskDescriptions;
} /* XDND_GetAskActionDescriptions */

/*
 * XDND_DraggerCanProvideText --
 *  Returns True if text/plain is in itsDraggerTypeList.
 */
XDND_BOOL XDND_DraggerCanProvideText(XDND *dnd) {
  int i;
  
  for (i = 1; i <= XDND_AtomListLength(dnd->DraggerTypeList); i++) {
    if (dnd->DraggerTypeList[i] == dnd->DNDMimePlainTextXAtom) {
      return True;
    }
  }
  return False;
} /* XDND_DraggerCanProvideText */

/*
 * XDND_SendDNDEnter
 *  Sends XdndEnter message :-)
 */
void XDND_SendDNDEnter(XDND *dnd, Window window, Window msgWindow,
                       XDND_BOOL isAware, Atom vers) {
  XEvent xEvent;
  int typeCount, msgTypeCount, i;
  
  dnd->XDNDVersion        = vers;
  dnd->MouseWindow        = window;
  dnd->MouseWindowIsAware = isAware;
  dnd->MsgWindow          = msgWindow;

  dnd->WillAcceptDropFlag = False;
  dnd->WaitForStatusFlag  = False;
  dnd->UseMouseRectFlag   = False;
  dnd->MouseRectR.x       = dnd->MouseRectR.y      = 0;
  dnd->MouseRectR.width   = dnd->MouseRectR.height = 0;
  
  if (dnd->MouseWindowIsAware) {
    XDND_DEBUG2("XDND_SendDNDEnter: Sending XdndEnter to window %ld\n",
                dnd->MsgWindow);

    memset(&xEvent, 0, sizeof (xEvent));
    typeCount                   = XDND_AtomListLength(dnd->DraggerTypeList);
    
    xEvent.xclient.type         = ClientMessage;
    xEvent.xclient.display      = dnd->display;
    xEvent.xclient.window       = dnd->MouseWindow;
    xEvent.xclient.message_type = dnd->DNDEnterXAtom;
    xEvent.xclient.format       = 32;

    xEvent.xclient.data.l[0]    = dnd->DraggerWindow; /* source window */
    xEvent.xclient.data.l[1]    = 0; /* version in high byte, bit 0 => more
                                        data types */
    xEvent.xclient.data.l[1]   |= (dnd->XDNDVersion << 24);
    if (typeCount > XDND_ENTERTYPECOUNT) {
      xEvent.xclient.data.l[1] |= 1;
    }
    xEvent.xclient.data.l[2] = None;
    xEvent.xclient.data.l[3] = None;
    xEvent.xclient.data.l[4] = None;

    msgTypeCount = Min(typeCount, 3);
    for (i = 0; i < msgTypeCount; i++) {
      xEvent.xclient.data.l[2 + i] = dnd->DraggerTypeList[i];
    }
    XSendEvent(dnd->display, dnd->MsgWindow, 0, 0, &xEvent);
  }
} /* XDND_SendDNDEnter */

/*
 * XDND_SendDNDPosition --
 *  This is sent to let the target to inform target about mouse position... 
 */
XDND_BOOL XDND_SendDNDPosition(XDND *dnd, Atom action) {
  XEvent xEvent;
  
  if (dnd->MsgWindow == None || dnd->MouseWindow == None) {
    return False;
  }
  
  xEvent.xclient.type         = ClientMessage;
  xEvent.xclient.display      = dnd->display;
  xEvent.xclient.window       = dnd->MouseWindow;
  xEvent.xclient.message_type = dnd->DNDHereXAtom;
  xEvent.xclient.format       = 32;

  xEvent.xclient.data.l[0]  = dnd->DraggerWindow; /* source window (sender) */
  xEvent.xclient.data.l[1]  = 0;
  xEvent.xclient.data.l[2]  = (dnd->x << 16) | dnd->y; /* mouse coordinates */
  xEvent.xclient.data.l[3]  = CurrentTime;
  xEvent.xclient.data.l[4]  = action;

  XDND_DEBUG2("XDND_SendDNDPosition: Sending XdndPosition to window %ld\n",
              dnd->MsgWindow);
  XSendEvent(dnd->display, dnd->MsgWindow, 0, 0, &xEvent);
  return True;
} /* XDND_SendDNDPosition */

/*
 * XDND_SendDNDStatus --
 *  This is sent to let the source know that we are ready for another
 *  DNDHere message...
 */
XDND_BOOL XDND_SendDNDStatus(XDND *dnd, Atom action) {
  XEvent xEvent;
  int width = 1, height = 1;
  
  if (dnd->DraggerWindow == None) {
    return False;
  }
  
  memset (&xEvent, 0, sizeof(XEvent));
  
  xEvent.xclient.type         = ClientMessage;
  xEvent.xclient.display      = dnd->display;
  xEvent.xclient.window       = dnd->DraggerWindow;
  xEvent.xclient.message_type = dnd->DNDStatusXAtom;
  xEvent.xclient.format       = 32;

  xEvent.xclient.data.l[0]  = dnd->MouseWindow;
  xEvent.xclient.data.l[1]  = 0;
  
  /*
   * Set a rectangle of 1x1 pixel at mouse position...
   */
  xEvent.xclient.data.l[2]  = ((dnd->x) << 16) | ((dnd->y) & 0xFFFFUL);
  xEvent.xclient.data.l[3]  = ((width)  << 16) | ((height) & 0xFFFFUL);

  XDND_DEBUG2("XDND_SendDNDStatus: WillAcceptDropFlag=%d\n",
              dnd->WillAcceptDropFlag);
  
  if (dnd->WillAcceptDropFlag) {
    xEvent.xclient.data.l[1] |= 1;
    xEvent.xclient.data.l[4]  = action;
  } else {
    xEvent.xclient.data.l[4]  = None;
  }
  XSendEvent(dnd->display, dnd->DraggerWindow, 0, 0, &xEvent);
  return True;
} /* XDND_SendDNDStatus */

/*
 * XDND_HandleDNDStatus --
 */
int XDND_HandleDNDStatus(XDND *dnd, XClientMessageEvent clientMessage) {
  Window target;
  Atom   TargetSupportedAction;
  int    TargetWillAccept, TargetWantsPosition;
  long   x, y, width, height;

  /*
   * Extract XdndStatus information from ClientMessage... */
  target                = clientMessage.data.l[0];
  //if (target != dnd->MouseWindow) return False;
  TargetWillAccept      = clientMessage.data.l[1] & 0x1L;
  TargetWantsPosition   = clientMessage.data.l[1] & 0x2UL;
  x                     = clientMessage.data.l[2] >> 16;
  y                     = clientMessage.data.l[2] & 0xFFFFL;
  width                 = clientMessage.data.l[3] >> 16;
  height                = clientMessage.data.l[3] & 0xFFFFL;
  TargetSupportedAction = (Atom) clientMessage.data.l[4];

  if (TargetSupportedAction == None || !TargetWillAccept) {
    /*
     * We got None as a supported action. This means that the drop will not be
     * accepted...
     */
 
  //  TargetWillAccept      = False;
  //  TargetWantsPosition   = False;
  //  TargetSupportedAction = None;
    TargetSupportedAction = dnd->DNDActionCopyXAtom;
    
  /*
   * TODO: I have decided to ignore the None value, because there are a few
   * malbehaved applications, like cooledit that they send None but they mean
   * drop :-)
    TargetSupportedAction = dnd->DNDActionCopyXAtom;
   */
  }
  
  /*
   * Fill relevant slots in dnd structure...
   */
  /* dnd->MouseWindow     = target; */
  dnd->WillAcceptDropFlag = TargetWillAccept;
  dnd->SupportedAction    = TargetSupportedAction;
  dnd->WaitForStatusFlag  = False; /* XdndStatus message received ! */

  /*
   * Change the cursor according to the supported by the target action...
   */
  if (TargetWillAccept) {
    if (TargetSupportedAction == dnd->DNDActionCopyXAtom) {
      if (dnd->SetCursor != NULL) (*dnd->SetCursor) (dnd, XDND_COPY_CURSOR);
    } else if (TargetSupportedAction == dnd->DNDActionMoveXAtom) {
      if (dnd->SetCursor != NULL) (*dnd->SetCursor) (dnd, XDND_MOVE_CURSOR);
    } else if (TargetSupportedAction == dnd->DNDActionLinkXAtom) {
      if (dnd->SetCursor != NULL) (*dnd->SetCursor) (dnd, XDND_LINK_CURSOR);
    } else if (TargetSupportedAction == dnd->DNDActionAskXAtom) {
      if (dnd->SetCursor != NULL) (*dnd->SetCursor) (dnd, XDND_ASK_CURSOR);
    } else if (TargetSupportedAction == dnd->DNDActionPrivateXAtom) {
      if (dnd->SetCursor != NULL) (*dnd->SetCursor) (dnd, XDND_PRIVATE_CURSOR);
    } else {
      XDND_DEBUG2("XDND_HandleDNDStatus: Error: Unkown action %ld\n",
                  TargetSupportedAction);
      if (dnd->SetCursor != NULL) (*dnd->SetCursor) (dnd, XDND_NODROP_CURSOR);
      dnd->WillAcceptDropFlag = False;
      dnd->SupportedAction    = None;
      return False;
    }
  } else {
    if (dnd->SetCursor != NULL) (*dnd->SetCursor) (dnd, XDND_NODROP_CURSOR);
  }
  

  XDND_DEBUG2("XDND_HandleDNDStatus: ClientMessage from window %ld\n", target);
  XDND_DEBUG4("    Accepts Drop=%d, Wants Position=%d, Action=%s,\n",
     TargetWillAccept, TargetWantsPosition, 
     ((TargetSupportedAction == None)?"None":XGetAtomName(dnd->display, TargetSupportedAction)));
  XDND_DEBUG5("    x=%ld, y=%ld, width=%ld, height=%ld\n", x, y, width, height);
  return True;
} /* XDND_HandleDNDStatus */

/*
 * XDND_SendDNDLeave --
 *  This is sent to let the target know that the mouse has left the window... 
 */
XDND_BOOL XDND_SendDNDLeave(XDND *dnd) {
  XEvent xEvent;
  
  if (dnd->MsgWindow == None) {
    return False;
  }
  
  xEvent.xclient.type         = ClientMessage;
  xEvent.xclient.display      = dnd->display;
  xEvent.xclient.window       = dnd->MouseWindow;
  xEvent.xclient.message_type = dnd->DNDLeaveXAtom;
  xEvent.xclient.format       = 32;

  xEvent.xclient.data.l[0]  = dnd->DraggerWindow;
  xEvent.xclient.data.l[1]  = 0;
  xEvent.xclient.data.l[2]  = 0; /* empty rectangle */
  xEvent.xclient.data.l[3]  = 0;

  XDND_DEBUG3("XDND_SendDNDLeave: Sending XdndLeave to window %ld "
              "for window %ld\n", dnd->MsgWindow, dnd->MouseWindow);
  XSendEvent(dnd->display, dnd->MsgWindow, 0, 0, &xEvent);
  return True;
} /* XDND_SendDNDLeave */

/*
 * XDND_SendDNDDrop --
 *  Notify the drop target that a drop is requested by the drag source... 
 */
XDND_BOOL XDND_SendDNDDrop(XDND *dnd) {
  XEvent xEvent;
  
  if (dnd->MsgWindow == None) {
    return False;
  }
  
  xEvent.xclient.type         = ClientMessage;
  xEvent.xclient.display      = dnd->display;
  xEvent.xclient.window       = dnd->MouseWindow;
  xEvent.xclient.message_type = dnd->DNDDropXAtom;
  xEvent.xclient.format       = 32;

  xEvent.xclient.data.l[0]  = dnd->DraggerWindow;
  xEvent.xclient.data.l[1]  = 0;
  xEvent.xclient.data.l[2]  = dnd->LastEventTime;
  xEvent.xclient.data.l[3]  = 0;

  XDND_DEBUG2("XDND_SendDNDDrop: Sending XdndDrop to window %ld\n",
              dnd->MsgWindow);
  XSendEvent(dnd->display, dnd->MsgWindow, 0, 0, &xEvent);
  return True;
} /* XDND_SendDNDDrop */

/*
 * XDND_SendDNDSelection --
 *  Notify the drop target that a drop is requested by the drag source... 
 */
XDND_BOOL XDND_SendDNDSelection(XDND *dnd, XSelectionRequestEvent *request) {
  XEvent xEvent;
  
  if (request->requestor == None) {
    return False;
  }
  
  XDND_DEBUG3("XDND_SendDNDSelection: Sending \"%s\" (%d)\n",
              dnd->data, dnd->index);
  XChangeProperty(dnd->display, request->requestor, request->property,
                  request->target, 8, PropModeReplace, dnd->data, dnd->index);
  xEvent.xselection.type      = SelectionNotify;
  xEvent.xselection.property  = request->property;
  xEvent.xselection.display   = request->display;
  xEvent.xselection.requestor = request->requestor;
  xEvent.xselection.selection = request->selection;
  xEvent.xselection.target    = request->target;
  xEvent.xselection.time      = request->time;

  XDND_DEBUG2("XDND_SendDNDSelection: Sending Selection to window %ld\n",
              request->requestor);
  XSendEvent(dnd->display, request->requestor, 0, 0, &xEvent);
  return True;
} /* XDND_SendDNDSelection */

/*
 * XDND_FindTarget --
 *  Searches for XdndAware window under the cursor.
 *  We have to search the tree all the way from the root window to the leaf
 *  because we have to pass through whatever layers the window manager has
 *  inserted. Also, while traversing the tree, if we found a window that is
 *  XDND aware (and according to the protocol this property must exist only on
 *  toplevel windows), the toplevel window is returned...
 *  If any of the variables toplevel, msgWindow, aware and version is NULL,
 *  then there will be no check for the XdndAware property...
 */
XDND_BOOL XDND_FindTarget(XDND *dnd, int x, int y, 
                          Window *toplevel, Window *msgWindow,
                          Window *target, XDND_BOOL *aware, Atom *version) {
  int xd, yd;
  Window child, new_child;
  if (toplevel != NULL && msgWindow != NULL &&
      aware    != NULL && version   != NULL) {
    *toplevel = *msgWindow = *target = None;
    *aware    = False;
    *version  = None;
  } else {
    /* Just to make sure... */
    toplevel = msgWindow = NULL;
    aware    = NULL;
    version  = NULL;
  }

  if (dnd->RootWindow == None || dnd->DraggerWindow == None) return False;
  if (dnd->Toplevel != None && !dnd->IsDraggingFlag) {
    child = dnd->Toplevel;
  } else {
    child = dnd->RootWindow;
  }
  
  for (;;) {
    new_child = None;
    if (!XTranslateCoordinates(dnd->display, dnd->RootWindow, child, x, y,
        &xd, &yd, &new_child)) { 
      break;
    }
    if (new_child == None) break;
    child = new_child;
    /*
     * Check for XdndAware property...
     */
    if (aware != NULL && !(*aware)) {
      if (XDND_IsDndAware(dnd, child, msgWindow, version)) {
        *toplevel = child;
        *aware = True;
      }
    }
  }
  *target = child;
  return True;
} /* XDND_FindTarget */

/*
 * XDND_FindToplevel --
 * Find the real toplevel that holds the given window...
 * Warning: This works only for tk windows. Do not attempt to use it in order
 * to find toplevels belonging to other applications. The results will be
 * wrong!
 */
Window XDND_FindToplevel(XDND *dnd, Window window) {
  Window root_return, parent, *children_return = 0;
  unsigned int nchildren_return;
  int status;
#ifdef DND_DEBUG
  Tk_Window tkwin;
  char *name;
#endif /* DND_DEBUG */
  
  if (window == None) return None;
  status = XQueryTree(dnd->display, window, &root_return, &parent,
                      &children_return, &nchildren_return);
  if (children_return) XFree(children_return);
  if (status) {
    if (dnd->WidgetExistsCallback) {
      if (!(*dnd->WidgetExistsCallback) (dnd, parent)) {
#ifdef DND_DEBUG
        tkwin = Tk_IdToWindow(dnd->display, window);
        name = Tk_PathName(tkwin);
        if (name == NULL) name = "(tk internal/hidden window)";
        XDND_DEBUG3("++++ XDND_FindToplevel: widget %s: %ld\n", name, window);
#endif /* DND_DEBUG */
        return window;
      } else {
        return XDND_FindToplevel(dnd, parent);
      }
    }
  }
  return None;
} /* XDND_FindToplevel */

/*
 * D R A G   O P E R A T I O N S
 */

/*
 * XDND_BeginDrag --
 *  This function begins a drag action...
 */
XDND_BOOL XDND_BeginDrag(XDND *dnd, Window source, Atom *actions, Atom *types,
                         char *Descriptions) {
  XEvent xevent, fake_xevent;
  XRectangle rectangle;
  Window toplevel, msgWindow, mouseWindow;
  Atom actualAction;
  int accept, win_x_temp, win_y_temp;
  int status = True, want_position;
  unsigned int mask_return;
  float x_mouse, y_mouse;
  
  /*
   * First make sure that we want a drag: wait until the cursor moves more
   * than five pixels...
   */
  do {
    XNextEvent (dnd->display, &xevent);
    if (xevent.type == ButtonRelease) {
      /*XSendEvent (dnd->display, xevent.xany.window, 0, 
                  ButtonReleaseMask, &xevent);*/
      if (dnd->HandleEvents != NULL) (*dnd->HandleEvents) (dnd, &xevent);
      return False;
    } else {
      if (dnd->HandleEvents != NULL) (*dnd->HandleEvents) (dnd, &xevent);
    }
  } while (xevent.type != MotionNotify);
  x_mouse = (float) xevent.xmotion.x_root;
  y_mouse = (float) xevent.xmotion.y_root;
  for (;;) {
    XNextEvent (dnd->display, &xevent);
    if (xevent.type == MotionNotify) {
      if (XDND_Sqrt((x_mouse - xevent.xmotion.x_root) * 
                    (x_mouse - xevent.xmotion.x_root) +
                    (y_mouse - xevent.xmotion.y_root) *
                    (y_mouse - xevent.xmotion.y_root)) > 4.0) break;
    }
    if (xevent.type == ButtonRelease) {
      /* XSendEvent(dnd->display, xevent.xany.window, 0,
                 ButtonReleaseMask, &xevent);*/
      Tk_HandleEvent(&xevent);
      return False;
    }
  }
  XDND_DEBUG("\n\n\n\nXDND_BeginDrag: moved 5 pixels - going to drag\n");

  XDND_Reset(dnd);
  dnd->x = xevent.xmotion.x_root;
  dnd->y = xevent.xmotion.y_root;

  dnd->IsDraggingFlag         = True;
  dnd->InternalDrag           = False;
  dnd->DraggerWindow          = source;
  dnd->DraggerTypeList        = types;
  dnd->DraggerAskActionList   = actions;
  dnd->DraggerAskDescriptions = Descriptions;
  dnd->Toplevel               = XDND_FindToplevel(dnd, source);
  dnd->MouseWindow            = None;
  dnd->DesiredType            = types[0];
  dnd->SupportedAction        = actions[0];
  dnd->MouseWindowIsAware     = False;
  dnd->MsgWindow              = None;
  if (dnd->data != NULL)        Tcl_Free(dnd->data);
  dnd->data                   = NULL;
  dnd->index                  = 0;

  /*
   * This function will create the Window property "XdndTypeList" if we have
   * more than XDND_ENTERTYPECOUNT types...
   */
  XDND_AnnounceTypeList(dnd, source, types);
  XDND_AnnounceAskActions(dnd, source, actions, Descriptions);

  /*
   * Get the ownership of selection, grab the cursor and set the
   * noDrop cursor...
   */
  XSetSelectionOwner(dnd->display, dnd->DNDSelectionName, dnd->DraggerWindow,
                     CurrentTime);
  if (XGrabPointer(dnd->display, dnd->RootWindow, False,
     ButtonMotionMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
     GrabModeAsync, GrabModeAsync, None, dnd->cursors[0].cursor, CurrentTime) 
     != GrabSuccess) {
        XDND_DEBUG("XDND_BeginDrag: Unable to grab pointer\n");
  }
  
  /* 
   * Enter our private Drag Event Loop. We have to by-pass tk's event loop, as
   * intergrating this stuff with tk event loop would require a substantially
   * number of changes and the tkDND would never be a trully loadable
   * extension...
   */
  while (xevent.xany.type != ButtonRelease) {
    XAllowEvents(dnd->display, SyncPointer, CurrentTime);
    XNextEvent(dnd->display, &xevent);
    switch (xevent.type) {
      case Expose: {/* Handle Expose Events... */
        if (dnd->HandleEvents != NULL) (*dnd->HandleEvents) (dnd, &xevent);
        break;
      } /* Expose */
      /* this event is not actually reported, so we find out by ourselves
       * from motion events */
      case EnterNotify: break;
      /* this event is not actually reported, so we find out by ourselves
       * from motion events */
      case LeaveNotify: break;
      /* done, but must send a leave event */
      case ButtonRelease: {

        /*
         * D R O P   S E C T I O N: We have a button reliese. It is either a
         * drop, or time to exit or private event loop and give control to
         * Tk...
         */

        if (dnd->WillAcceptDropFlag) {
          if (dnd->InternalDrag) {
            if (dnd->data != NULL) Tcl_Free(dnd->data);
            dnd->data = NULL; dnd->index = 0;

            /*
             * Do we have to display an Ask dialog?
             */
            if (dnd->SupportedAction == dnd->DNDActionAskXAtom) {
              if (dnd->Ask != NULL) {
                status = (*dnd->Ask) (dnd, dnd->DraggerWindow, dnd->MouseWindow,
                                      &actualAction);
                if (!status || actualAction == None) {
                  /*
                   * Either an error has occured or the user has canceled the
                   * drop. So, call the <DragLeave> callback...
                   */
                  XDND_DEBUG2("XDND_BeginDrag: Internal Window, executing Leave"
                               "directly for window %ld\n", dnd->MouseWindow);
                  if (dnd->WidgetApplyLeaveCallback != NULL) {
                    status =
                       (*dnd->WidgetApplyLeaveCallback) (dnd, dnd->MouseWindow);
                  }
                  goto exit;
                }
                dnd->SupportedAction = actualAction;
              } else {
               /* TODO: What to do here? How to decide on the correct action? */
              }
            }
            
            /*
             * Retieve the data from widget...
             */
            if (dnd->GetData != NULL) {
              status = (*dnd->GetData) (dnd, dnd->DraggerWindow,
                        (unsigned char **) &dnd->data, &dnd->index,
                        dnd->DesiredType);
            }
            /*
             * Call the insert widget data callback...
             */
            if (dnd->WidgetInsertDropDataCallback != NULL) {
              (*dnd->WidgetInsertDropDataCallback) (dnd, 
                     (unsigned char *) dnd->data, dnd->index, 0,
                     dnd->MouseWindow, dnd->DraggerWindow, dnd->DesiredType);
            }
            if (dnd->data != NULL) Tcl_Free(dnd->data);
            dnd->data = NULL;
          } else {
            /*
             * Keep a copy of the button release event. We need it in order to
             * give it back to tk when we are done!
             */
            memcpy(&fake_xevent, &xevent, sizeof(xevent));
            
            /*
             * Send an XdndDrop message to the target!
             */
            dnd->LastEventTime = xevent.xbutton.time;
            status = XSetSelectionOwner(dnd->display, dnd->DNDSelectionName,
                                        dnd->DraggerWindow, CurrentTime);
            XDND_DEBUG3("XDND_BeginDrag: XSetSelectionOwner %s (status=%d).\n",
                        (status ? "succeded" : "FAILED"), status);
            XDND_SendDNDDrop(dnd);

            /*
             * Now we must start another Event loop, to wait for XdndSelection
             * requests from the drop target, as it will need to access the
             * data...
             */
            for (;;) {
              XAllowEvents(dnd->display, SyncPointer, CurrentTime);
              XNextEvent(dnd->display, &xevent);
              switch (xevent.type) {
                case Expose: {/* Handle Expose Events... */
                  if (dnd->HandleEvents != NULL)
                          (*dnd->HandleEvents) (dnd, &xevent);
                  break;
                } /* Expose */
                case MotionNotify: {
                  /* If more than a few seconds have passed, the other app has
                   * probably crashed :-) */
                  if (xevent.xmotion.time > dnd->LastEventTime + 10000) {
                    /* allow a 10 second timeout as default */
                    XDND_DEBUG("XDND_BeginDrag: Timeout - exiting event loop");
                    memcpy(&xevent, &fake_xevent, sizeof(xevent));
                    goto exit;
                  }
                  break;
                }
                case ClientMessage: {
                  XDND_DEBUG("XDND_BeginDrag: Loop 2-ClientMessage recieved\n");
                  if (xevent.xclient.message_type == dnd->DNDFinishedXAtom) {
                    XDND_DEBUG("XDND_BeginDrag: XdndFinished received...\n");
                    memcpy(&xevent, &fake_xevent, sizeof(xevent));
                    goto exit;
                  }
                  XDND_HandleClientMessage(dnd, &xevent);
                  break;
                } /* ClientMessage */
                case SelectionRequest: {
                  if (xevent.xselectionrequest.selection != 
                      dnd->DNDSelectionName) break;
                  XDND_DEBUG("XDND_BeginDrag: SelectionRequest: getting widget "
                             "data...\n");
                  /*
                   * Retieve the data from widget...
                   */
                  if (dnd->data != NULL) Tcl_Free(dnd->data);
                  dnd->data = NULL; dnd->index = 0;
                  dnd->DesiredType = xevent.xselectionrequest.target;
                  if (dnd->GetData != NULL) {
                    status = (*dnd->GetData) (dnd, dnd->DraggerWindow,
                             (unsigned char **) &dnd->data, &dnd->index,
                             dnd->DesiredType);
                  } 
                  XDND_DEBUG("XDND_BeginDrag: Sending Selection...\n");
                  XDND_SendDNDSelection(dnd, &xevent.xselectionrequest);
                  if (dnd->data != NULL) {
                    Tcl_Free(dnd->data);
                    dnd->data = NULL; dnd->index = 0;
                  }
                  memcpy(&xevent, &fake_xevent, sizeof(xevent));
                  goto exit;
                } /* SelectionRequest */
              } /* switch (xevent.type) */
            } /* for (;;) */
          }
        } else {
          /* No drop, so send XdndLeave... */
          if (dnd->MouseWindowIsAware) {
            if (!dnd->InternalDrag) {
              XDND_SendDNDLeave(dnd);
            } else if (dnd->WidgetApplyLeaveCallback != NULL) {
                (*dnd->WidgetApplyLeaveCallback) (dnd, dnd->MouseWindow);
            }
          }
          XDND_DEBUG("XDND_BeginDrag: ButtonRelease: exiting tkDND private "
                     "event loop without dropping...\n");
        }
        break;
      }
      /*
       * The cursor has moved...
       */
      case MotionNotify: {
        memcpy (&fake_xevent, &xevent, sizeof(xevent));
        XDND_DEBUG("XDND_BeginDrag: MotionNotify recieved:\n");
        /*
         * Get mouse coordinates relative to the root window.
         */
        if (!XQueryPointer(dnd->display, dnd->DraggerWindow, &(dnd->RootWindow),
                           &mouseWindow, &(dnd->x), &(dnd->y),
                           &win_x_temp, &win_y_temp, &mask_return)) break;
        /*
         * Find the topmost window the mouse is in. The XDND_FindTarget
         * function will search the whole window tree from the root window
         * until the leaf window the mouse is in. If in the way a window that
         * has the XdndAware property is found, the proper slots are filled...
         */
        XDND_FindTarget(dnd, dnd->x, dnd->y,
                        &toplevel, &msgWindow,
                        &mouseWindow, &(dnd->MouseWindowIsAware),
                        &(dnd->XDNDVersion));
        if (mouseWindow == None) {
          XDND_DEBUG("XDND_BeginDrag: Mouse cursor in the root window?\n");
          break;
        }

        /*
         * Have we entered a new toplevel window?
         * A toplevel window has the XdndAware atom in its properties.
         * According to the XDND protocol, this is a toplevel window, as
         * only toplevel windows are allowed to carry this property...
         */
        if (dnd->Toplevel != toplevel) {
          XDND_DEBUG4("XDND_BeginDrag: Entering Toplevel: %ld, Old Toplevel"
                      ": %ld XdndAware=%d...\n", toplevel, dnd->Toplevel,
                      dnd->MouseWindowIsAware);
          dnd->Toplevel = toplevel;
        }

        /*
         * Have we entered a new widget?
         */
        if (dnd->MouseWindow != mouseWindow) {
          XDND_DEBUG3("XDND_BeginDrag: Entering Window: %ld, Old Window: %ld\n",
                      mouseWindow, dnd->MouseWindow);
          if (dnd->MouseWindow == None) {
            XDND_DEBUG("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
          }
          /*
           * Send a XdndLeave message, to the old window, if any...
           */
          if (dnd->MouseWindow != None) {
            /*
             * Was the last window one of our own windows? If this is the
             * case, call the callbacks directly. There is no need to go
             * through the XServer...
             */
            if (dnd->InternalDrag) {
              XDND_DEBUG2("XDND_BeginDrag: Internal Window, executing Leave "
                          "directly for window %ld\n", dnd->MouseWindow);
              if (dnd->WidgetApplyLeaveCallback != NULL) {
                status =
                      (*dnd->WidgetApplyLeaveCallback) (dnd, dnd->MouseWindow);
                if (status == TCL_ERROR) {
                  XDND_DEBUG2("XDND_BeginDrag: Last error:\n     %s\n",
                              Tcl_GetString(Tcl_GetObjResult(dnd->interp)));
                  goto exit;
                }
              }
            } else {
              /*
               * The last window belongs to a foreign application. Send the
               * proper messages through the XServer...
               */
              XDND_DEBUG2("XDND_BeginDrag: External Window, sending "
                          "XdndLeave to window %ld\n", dnd->MouseWindow);
              XDND_SendDNDLeave(dnd);
            } /* if (dnd->InternalDrag) */
            dnd->WillAcceptDropFlag = False;
            dnd->SupportedAction = actions[0];
          } /* if (dnd->MouseWindow != None) */

          dnd->MouseWindow = mouseWindow;
          dnd->MsgWindow = msgWindow;
          /*
           * We are now officially in the new window. Is the new window XDND
           * aware?
           */
          if (dnd->MouseWindowIsAware) {
            dnd->InternalDrag = False; /* We don't know that yet... */
            /* We are at a new window, so reset preffered action :-) */
            dnd->SupportedAction = actions[0];

            /*
             * We need to send an XdndEnter message.
             * Is it an internal window? 
             */
            if (dnd->WidgetExistsCallback != NULL) {
              if ((*dnd->WidgetExistsCallback) (dnd, mouseWindow)) {
                /*
                 * This is an internal window: Do not send a XdndEnter
                 * message through the XServer, but call the callback
                 * directly...
                 */
                XDND_DEBUG2("XDND_BeginDrag: Internal Window, sending Enter "
                            "directly for window %ld\n", mouseWindow);
                dnd->InternalDrag = True;
                if (dnd->WidgetApplyEnterCallback != NULL) {
                  accept = (*dnd->WidgetApplyEnterCallback) (dnd, mouseWindow,
                            source, actions[0], dnd->x, dnd->y,
                            CurrentTime, dnd->DraggerTypeList);
                  if (dnd->CallbackStatus != TCL_OK) goto exit;
#ifndef XDND_DEBUG
                  if (!accept) XDND_DEBUG("XDND_BeginDrag: Enter denied!\n");
#endif /* XDND_DEBUG */
                } /* if (dnd->WidgetApplyEnterCallback != NULL) */
              } /* if ((*dnd->WidgetExistsCallback) (dnd, mouseWindow)) */
            } /* if (dnd->WidgetExistsCallback != NULL) */
            
            /*
             * If the new window does not belong to this application, send the
             * XdndEnter message...
             */
            if (!dnd->InternalDrag) {
              XDND_DEBUG2("XDND_BeginDrag: Foreign Window, sending XdndEnter "
                          "ClientMessage to window %ld\n", dnd->MsgWindow);
              XDND_SendDNDEnter(dnd, mouseWindow, dnd->MsgWindow, True,
                                dnd->XDNDVersion);
            }
          } else {
            /*
             * The new window is not XDND aware...
             */
            XDND_DEBUG2("XDND_BeginDrag: Window %ld is not XDND aware...\n",
                        dnd->Toplevel);
            if (dnd->SetCursor != NULL)
                                    (*dnd->SetCursor) (dnd, XDND_NODROP_CURSOR);
            dnd->MouseWindowIsAware = False;
            dnd->MsgWindow = None;
          } /* if (dnd->MouseWindowIsAware) */
        } /* if (dnd->MouseWindow != mouseWindow) */
          
        /*
         * Whether we are in a new window or not, report the mouse position
         * to the window, if it is XDND aware...
         */
        if (dnd->MouseWindowIsAware) {
          if (dnd->InternalDrag) {
            /*
             * Simulate the XdndPosition/XdndStatus dialog without going
             * through the X-Server, as it is an internal drop...
             */
            dnd->WaitForStatusFlag = False;
            if (dnd->WidgetApplyPositionCallback != NULL) {
              status = (*dnd->WidgetApplyPositionCallback) (dnd, 
                         dnd->MouseWindow, dnd->DraggerWindow,
                         actions[0], actions, dnd->x, dnd->y,
                         CurrentTime, dnd->DraggerTypeList, &want_position,
                         &(dnd->SupportedAction), &(dnd->DesiredType),
                         &rectangle);
              if (dnd->CallbackStatus == TCL_ERROR) goto exit;
              if (status) {
                /*
                 * The target will accept the drop...
                 */
                dnd->WillAcceptDropFlag = True;
                /*
                 * Change the cursor according to the supported by the
                 * target action...
                 */
                 if (dnd->SetCursor != NULL) {
                   if (dnd->SupportedAction == dnd->DNDActionCopyXAtom) {
                     (*dnd->SetCursor) (dnd, XDND_COPY_CURSOR);
                   } else
                   if (dnd->SupportedAction == dnd->DNDActionMoveXAtom) {
                     (*dnd->SetCursor) (dnd, XDND_MOVE_CURSOR);
                   } else
                   if (dnd->SupportedAction == dnd->DNDActionLinkXAtom) {
                     (*dnd->SetCursor) (dnd, XDND_LINK_CURSOR);
                   } else
                   if (dnd->SupportedAction == dnd->DNDActionAskXAtom) {
                     (*dnd->SetCursor) (dnd, XDND_ASK_CURSOR);
                   } else
                   if (dnd->SupportedAction == dnd->DNDActionPrivateXAtom) {
                     (*dnd->SetCursor) (dnd, XDND_PRIVATE_CURSOR);
                   } else {
                     (*dnd->SetCursor) (dnd, XDND_NODROP_CURSOR);
                   }
                 } /* if (dnd->SetCursor != NULL) */
              } else {
                /*
                 * The widget refused drop...
                 */
                if (dnd->SetCursor != NULL) 
                   (*dnd->SetCursor) (dnd, XDND_NODROP_CURSOR);
              } /* if (status) */
            } /* if (dnd->WidgetApplyPositionCallback != NULL) */
          } else {
            /*
             * This is a foreign window. Send XdndPosition message...
             */
            if (!dnd->WaitForStatusFlag) {
              /*
               * If we are not waiting for XdndStatus message (in a slow
               * network...) send an XdndPosition message:
               */
              XDND_SendDNDPosition(dnd, dnd->SupportedAction);
              dnd->WaitForStatusFlag = True;
            }
          } /* if (dnd->InternalDrag) */
        } /* if (dnd->MouseWindowIsAware) */
        break;
      } /* MotionNotify */
      case ClientMessage: {
        XDND_DEBUG("XDND_BeginDrag: ClientMessage recieved\n");
        XDND_HandleClientMessage(dnd, &xevent);
        break;
      } /* ClientMessage */
      case SelectionRequest: {
        XDND_DEBUG("XDND_BeginDrag: SelectionRequest: getting widget data\n");
        if (xevent.xselectionrequest.selection != dnd->DNDSelectionName) break;
        /*
         * Retieve the data from widget...
         */
        if (dnd->data != NULL) Tcl_Free(dnd->data);
        dnd->data = NULL; dnd->index = 0;
        dnd->DesiredType = xevent.xselectionrequest.target;
        if (dnd->GetData != NULL) {
          status = (*dnd->GetData) (dnd, dnd->DraggerWindow,
                   (unsigned char **) &dnd->data, &dnd->index,
                    dnd->DesiredType);
        } 
        XDND_DEBUG("XDND_BeginDrag: Sending Selection...\n");
        XDND_SendDNDSelection(dnd, &xevent.xselectionrequest);
        if (dnd->data != NULL) {
          Tcl_Free(dnd->data);
          dnd->data = NULL; dnd->index = 0;
        }
        break;
      } /* SelectionRequest */
    } /* switch (xevent.type) */
  } /* while (xevent.xany.type != ButtonRelease) */
exit:
  XUngrabPointer(dnd->display, CurrentTime);
  /*
   * Tk had processed a ButtonPress event when we get called and stealed
   * the events from tk. So, pass this button release back to tk, so as
   * not to activate menus, etc...
   */
  if (dnd->HandleEvents != NULL) (*dnd->HandleEvents) (dnd, &xevent);
  XDND_Reset(dnd);
  XDND_DEBUG("XDND_BeginDrag: Returning to Tk's event loop...\n");
  return status;
} /* XDND_BeginDrag */

/*
 * D R O P   O P E R A T I O N S
 */

/*
 * XDND_HandleClientMessage --
 *  Handle Client Messages...
 *  Usually these events come for a target window, while a drop cursor is over
 *  a target window...
 */
int XDND_HandleClientMessage(XDND *dnd, XEvent *xevent) {
  XClientMessageEvent clientMessage;
  
  clientMessage = xevent->xclient;
  /*
   * target:  receive and process window-level enter and leave
   */
  if (clientMessage.message_type == dnd->DNDEnterXAtom) {
    return XDND_HandleDNDEnter(dnd, clientMessage);
  } else if (clientMessage.message_type == dnd->DNDHereXAtom) {
    return XDND_HandleDNDHere(dnd, clientMessage);
  } else if (clientMessage.message_type == dnd->DNDLeaveXAtom) {
    return XDND_HandleDNDLeave(dnd, clientMessage);
  } else if (clientMessage.message_type == dnd->DNDDropXAtom) {
    return XDND_HandleDNDDrop(dnd, clientMessage);
  } else if (clientMessage.message_type == dnd->DNDStatusXAtom) {
    /*
     * source:  clear our flag when we receive status message
     */
    return XDND_HandleDNDStatus(dnd, clientMessage);
  } else if (clientMessage.message_type == dnd->DNDFinishedXAtom) {
    XDND_DEBUG("XDND_HandleClientMessage: Received XdndFinished\n");
    return True;
  } else {
#ifdef TKDND_ENABLE_MOTIF_DROPS
    /*
     * Is it a Motif Message?
     */
    if (MotifDND_HandleClientMessage(dnd, *xevent)) {
      return True;
    }
#endif /* TKDND_ENABLE_MOTIF_DROPS */
    /* 
     * Not for us :-)
     */
    return False;
  }
  return False;
} /* XDND_HandleClientMessage */

/*
 * XDND_HandleDNDEnter --
 *  This function is called when the mouse enters a toplevel that is XDND
 *  aware (note: not a widget!). Its purpose is to anounce the source window
 *  and its supported types...
 */
int XDND_HandleDNDEnter(XDND *dnd, XClientMessageEvent clientMessage) {
  Atom *typelist;
  int vers;
#ifdef DND_DEBUG
  Atom *atom;
  char *AtomName;
#endif /* DND_DEBUG */
  
  if (dnd->IsDraggingFlag) {
    return False;
  }
  vers = (clientMessage.data.l[1] >> 24);
  if (vers < XDND_MINVERSION) return False;
  dnd->XDNDVersion = vers;
  XDND_DEBUG2("XDND_HandleDNDEnter: Using protocol version %ld\n",
              dnd->XDNDVersion);

  dnd->IsDraggingFlag       = False;
  dnd->DraggerWindow        = clientMessage.data.l[0];
  dnd->Toplevel             = clientMessage.window;
  dnd->MouseWindow          = None;
  dnd->WillAcceptDropFlag   = False;
  dnd->DesiredType          = None;
  if (dnd->DraggerTypeList != NULL) {
    Tcl_Free((char *)dnd->DraggerTypeList);
    dnd->DraggerTypeList    = NULL;
  }

  /*XSelectInput(dnd->display, dnd->DraggerWindow, StructureNotifyMask);*/
  
  /*
   * Does the drag source supports less than 3 types?
   */
  if ((clientMessage.data.l[1] & 0x1UL) == 0) {
    typelist = (Atom *) Tcl_Alloc(sizeof(Atom)*4);
    if (typelist == NULL) return False;
    typelist[0] = clientMessage.data.l[2];
    typelist[1] = clientMessage.data.l[3];
    typelist[2] = clientMessage.data.l[4];
    typelist[3] = None;
#ifdef DND_DEBUG
    XDND_DEBUG("XDND_HandleDNDEnter: Drag source supports at most 3 types:\n");
    for (atom = typelist; *atom != 0; atom++) {
      AtomName = XGetAtomName(dnd->display, *atom);
      XDND_DEBUG2("       %s\n", AtomName);
      XFree(AtomName);
    }
#endif /* DND_DEBUG */
  } else {
    typelist = XDND_GetTypeList(dnd, dnd->DraggerWindow);
    if (typelist == NULL) {
      XDND_DEBUG("XDND_HandleDNDEnter: There are no supported types by the "
                 "drag source!\n");
      return False;
    }
#ifdef DND_DEBUG
    XDND_DEBUG("XDND_HandleDNDEnter: Drag source supports more than 3 types:\n");
    for (atom = typelist; *atom != 0; atom++) {
      AtomName = XGetAtomName(dnd->display, *atom);
      XDND_DEBUG2("       %s\n", AtomName);
      XFree(AtomName);
    }
#endif /* DND_DEBUG */
  }
  dnd->DraggerTypeList = typelist;

  /*
   * Get the supported by the drag source actions...
   */
  if (dnd->DraggerAskActionList != NULL) {
    Tcl_Free((char *) dnd->DraggerAskActionList);
  }
  dnd->DraggerAskActionList = XDND_GetAskActions(dnd, dnd->DraggerWindow);
  /* The following command will place the actions in the variable
   * dnd->DraggerAskDescriptions... */
  XDND_GetAskActionDescriptions(dnd, dnd->DraggerWindow);
  return True;
} /* XDND_HandleDNDEnter */

/*
 * XDND_HandleDNDHere --
 *  This function will be constantly called while the mouse is moving inside
 *  the toplevel. It is responsible to get the widgets below the mouse and
 *  send the appropriate messages to the widgets (like Enter/Leave...)
 */
int XDND_HandleDNDHere(XDND *dnd, XClientMessageEvent clientMessage) {
  XRectangle rectangle;
  Window target;
  Atom action, supportedAction = None, desired_type;
  Time time;
  int wantPosition, will_accept = True;
  XDND_DEBUG("XDND_HandleDNDHere:\n");
  
  if (dnd->DraggerWindow != clientMessage.data.l[0]) return False;
  time                =  clientMessage.data.l[3];
  action              =  clientMessage.data.l[4];
  dnd->x              = (clientMessage.data.l[2] >> 16);
  dnd->y              = (clientMessage.data.l[2] & 0xFFFFUL);
  dnd->IsDraggingFlag = False;
  dnd->Toplevel       = clientMessage.window;
  if (!XDND_FindTarget(dnd, dnd->x, dnd->y, NULL, NULL, &target,
                       NULL, NULL)) return False;
  /* Is the target widget the same as the previous call? If not, send a Leave
   * event to the old window and an enter event to the new one... */
  if (target != dnd->MouseWindow) {
    if (dnd->MouseWindow != None && dnd->WidgetApplyLeaveCallback != NULL) {
      (*dnd->WidgetApplyLeaveCallback) (dnd, dnd->MouseWindow);
    }
    dnd->MouseWindow = target;
    if (dnd->WidgetApplyEnterCallback != NULL) {
      will_accept = (*dnd->WidgetApplyEnterCallback) (dnd, target,
      dnd->DraggerWindow, action, dnd->x, dnd->y, time, dnd->DraggerTypeList);
      /* TODO: The callback may raise an error. What can we do? The drag
       * source has a global cursor drag... */
    }
  }
  dnd->MouseWindowIsAware = True;
  if (will_accept) {
    dnd->WillAcceptDropFlag = (*dnd->WidgetApplyPositionCallback) (dnd, target, 
        dnd->DraggerWindow, action, dnd->DraggerAskActionList,
        dnd->x, dnd->y, time, dnd->DraggerTypeList,
        &wantPosition, &supportedAction, &desired_type, &rectangle);
    /* TODO: The callback may raise an error. What can we do? The drag
     * source has a global cursor drag... */
    dnd->DesiredType = desired_type;
    if (dnd->WillAcceptDropFlag) {
      XDND_DEBUG2("XDND_HandleDNDHere: Supported Action: %s\n",
                  XGetAtomName(dnd->display, supportedAction));
    } 
  } else {
    dnd->MouseWindow        = None;
    dnd->MouseWindowIsAware = True;
    dnd->WillAcceptDropFlag = False;
    dnd->DesiredType        = None;
    XDND_DEBUG("XDND_HandleDNDHere: Window refuses drop!\n");
  }
  //TODO:
  dnd->SupportedAction = supportedAction;
  XDND_SendDNDStatus(dnd, supportedAction);
  return True;
} /* XDND_HandleDNDHere */

/*
 * XDND_HandleDNDLeave --
 */
int XDND_HandleDNDLeave(XDND *dnd, XClientMessageEvent clientMessage) {
  XDND_DEBUG("XDND_HandleDNDLeave\n");
  
  /*XSelectInput(dnd->display, dnd->DraggerWindow, NoEventMask);*/
  if (dnd->DraggerWindow != clientMessage.data.l[0]) return False;
  if (dnd->WidgetApplyLeaveCallback != NULL) {
    (*dnd->WidgetApplyLeaveCallback) (dnd, dnd->MouseWindow);
  }
  dnd->IsDraggingFlag       = False;
  dnd->DraggerWindow        = None;
  if (dnd->DraggerTypeList != NULL) {
    Tcl_Free((char *)dnd->DraggerTypeList);
    dnd->DraggerTypeList    = NULL;
  }
  if (dnd->DraggerAskActionList != NULL) {
    Tcl_Free((char *) dnd->DraggerAskActionList);
  }
  dnd->DraggerAskActionList = NULL;
  dnd->Toplevel             = None;
  dnd->MouseWindow          = None;
  dnd->WillAcceptDropFlag   = False;
  dnd->DesiredType          = None;
  XDND_Reset(dnd);
  return True;
} /* XDND_HandleDNDLeave */

/*
 * XDND_HandleDNDDrop --
 */
int XDND_HandleDNDDrop(XDND *dnd, XClientMessageEvent clientMessage) {
  Atom actualType, actualAction;
  Window selectionOwner;
  Tk_Window tkwin;
  XEvent xevent;
  XDND_BOOL receivedEvent = False, userBored = False;
  Time time;
  int status, format;
  long read = 0;
  clock_t startTime, endTime;
  unsigned char *s, *data = NULL;
  unsigned long count, remaining;
  XDND_DEBUG("XDND_HandleDNDDrop\n");

  if (dnd->DraggerWindow != (Window) clientMessage.data.l[0]) return False;
  if ((tkwin=Tk_IdToWindow(dnd->display,dnd->MouseWindow))==NULL) goto finish;
  time = clientMessage.data.l[2];
#ifdef XDND_USE_TK_GET_SELECTION
  dnd->DesiredName = Tk_GetAtomName(tkwin, dnd->DesiredType);
  if (strncmp(dnd->DesiredName, "text", 4) == 0) {
    /* Textual data. Let tk handle them... */
    if (dnd->data != NULL) Tcl_Free(dnd->data);
    dnd->data = NULL; dnd->index = 0;
    status = Tk_GetSelection(dnd->interp, tkwin, dnd->DNDSelectionName,
             dnd->DNDStringAtom, &XDND_GetSelProc, (ClientData) dnd);
    if (status == TCL_ERROR) {
      XDND_DEBUG2("XDND_HandleDNDDrop:%s\n", Tcl_GetStringResult(dnd->interp));
      goto binary;
    }
  } else {
binary:
#endif /* XDND_USE_TK_GET_SELECTION */
    status = False;
    /* Binary data?? */
    if (dnd->data != NULL) Tcl_Free(dnd->data);
    dnd->data = NULL; dnd->index = 0;
    selectionOwner = XGetSelectionOwner(dnd->display, dnd->DNDSelectionName);
    if (selectionOwner == None) goto finish;
    XConvertSelection(dnd->display, dnd->DNDSelectionName, dnd->DesiredType,
                      dnd->DNDNonProtocolAtom, dnd->MouseWindow, CurrentTime);
    startTime = clock();
    endTime   = startTime + 5 * CLOCKS_PER_SEC;
    while (!receivedEvent && clock() < endTime) {
      receivedEvent = XCheckTypedWindowEvent(dnd->display, dnd->MouseWindow,
                                             SelectionNotify, &xevent);
      if (!userBored && clock() > startTime + 1 * CLOCKS_PER_SEC) {
        userBored = True;
        /*
         * TODO: Make the cursor the busy cursor...
         */
      }
      if (receivedEvent) break;
    }
    do {
      if (XGetWindowProperty(dnd->display, dnd->MouseWindow,
                             dnd->DNDNonProtocolAtom, read / 4, 65536, True,
                             AnyPropertyType, &actualType,
                             &format, &count, &remaining, &s) != Success) {
        XFree (s);
        goto finish;
      }
      read += count;
      if (data == NULL) {
        data = (unsigned char *) Tcl_Alloc(sizeof(unsigned char)*(read+2));
        dnd->index = 0;
        if (data == NULL) return TCL_ERROR;
      } else {
        data = (unsigned char *) Tcl_Realloc(data, 
                                             sizeof(unsigned char)*(read+2));
      }
      memcpy(&(data)[dnd->index], s, read);
      dnd->index = read;
      data[read] = '\0';
      /* printf("atom=%ld, read=%ld, binary-data:%s\n",actualType, read, s); */
      XFree (s);
    } while (remaining);
    dnd->data = data;
    dnd->index = read;
#ifdef XDND_USE_TK_GET_SELECTION
  }
#endif /* XDND_USE_TK_GET_SELECTION */
  XDND_DEBUG3("XDND_HandleDNDDrop: Dropped data: (%d) \"%s\"\n",
              dnd->index, dnd->data);
  
  /*
   * If the desired action is ask, ask the user :-)
   */
  if (dnd->SupportedAction == dnd->DNDActionAskXAtom) {
    if (dnd->Ask != NULL) {
      status = (*dnd->Ask) (dnd, dnd->DraggerWindow, dnd->MouseWindow,
                            &actualAction);
      if (!status || actualAction == None) return False;
      dnd->SupportedAction = actualAction;
    } else {
      /* TODO: What to do here? How to decide on the correct action? */
    }
  }
  
  /*
   * Call the insert widget data callback...
   */
  if (dnd->WidgetInsertDropDataCallback != NULL) {
    (*dnd->WidgetInsertDropDataCallback) (dnd, (unsigned char *) dnd->data,
    dnd->index, 0, dnd->MouseWindow, dnd->DraggerWindow, dnd->DesiredType);
  }
  if (dnd->data != NULL) Tcl_Free(dnd->data);
  dnd->data = NULL; dnd->index = 0;
finish:  
#ifdef TKDND_ENABLE_MOTIF_DROPS
  if (!dnd->Motif_DND) {
#endif /* TKDND_ENABLE_MOTIF_DROPS */
  /*
   * Send "XdndFinish"...
   */
  xevent.xclient.type         = ClientMessage;
  xevent.xclient.display      = dnd->display;
  xevent.xclient.window       = dnd->DraggerWindow;
  xevent.xclient.message_type = dnd->DNDFinishedXAtom;
  xevent.xclient.format       = 32;
  xevent.xclient.data.l[0]    = dnd->Toplevel;
  xevent.xclient.data.l[1]    = 0;
  XSendEvent(dnd->display, dnd->DraggerWindow, 0, 0, &xevent);
#ifdef TKDND_ENABLE_MOTIF_DROPS
  }
#endif /* TKDND_ENABLE_MOTIF_DROPS */
  /*XSelectInput(dnd->display, dnd->DraggerWindow, NoEventMask);*/
  
  dnd->IsDraggingFlag       = False;
  dnd->DraggerWindow        = None;
  if (dnd->DraggerTypeList != NULL) {
    Tcl_Free((char *)dnd->DraggerTypeList);
    dnd->DraggerTypeList    = NULL;
  }
  if (dnd->DraggerAskActionList != NULL) {
    Tcl_Free((char *) dnd->DraggerAskActionList);
  }
  dnd->DraggerAskActionList = NULL;
  dnd->Toplevel             = None;
  dnd->MouseWindow          = None;
  dnd->WillAcceptDropFlag   = False;
  dnd->DesiredType          = None;
  /*
   * Make sure we free any allocated memory...
   */
  if (dnd->data != NULL) Tcl_Free(dnd->data);
  dnd->data = NULL;
  XDND_Reset(dnd);
  return True;
} /* XDND_HandleDNDDrop */

/*
 * XDND_GetSelProc --
 *  This is the callback for Tk_GetSelection
 */
int XDND_GetSelProc(ClientData clientData, Tcl_Interp *interp, char *portion) {
  XDND *dnd = (XDND *) clientData;
  int length;

  if (portion == NULL) return TCL_ERROR;
  length = strlen(portion);
  if (dnd->data == NULL) {
    dnd->data = (char *) Tcl_Alloc(sizeof(char)*(length+2));
    dnd->index = 0;
    if (dnd->data == NULL) return TCL_ERROR;
  } else {
    dnd->data = Tcl_Realloc(dnd->data, sizeof(char)*(length+2));
  }
  strcpy(&(dnd->data)[dnd->index], portion);
  dnd->index += length;
  return TCL_OK;
} /* XDND_GetSelProc */

/*
 * EOF - End of File
 */
