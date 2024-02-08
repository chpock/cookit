/*
 * Motif.c -- Tk Motif Drag'n'Drop Protocol Implementation
 * 
 *    This file implements the unix portion of the drag&drop mechanism
 *    for the tk toolkit. The protocol in use under unix is the
 *    XDND protocol. This file adds the possibility for Motif drops...
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
 * Static variable that holds the drag source valid actions...
 */
Atom MotifDND_DraggerActionList[4];

int MotifDND_HandleClientMessage(XDND *dnd, XEvent xevent) {
  XClientMessageEvent cm;
  XRectangle rectangle;
  XDND_BOOL aware;
  DndData *dnd_data = &dnd->Motif_DND_Data;
  Window cur_window = None, src_window = None, temp_win;
  Atom *src_targets, Selection, DesiredType, temp_atom;
/*XDND_BOOL receivedEvent = False, userBored = False;
  clock_t startTime, endTime; */
  char receiver;
  int status;
  unsigned short num_src_targets, i;
#ifdef DND_DEBUG
  char *AtomName;
#endif /* DND_DEBUG */

  if (!(DndParseClientMessage((XClientMessageEvent*) &xevent, dnd_data,
                                 &receiver))) {
    return False;
  }

  MotifDND_DraggerActionList[0] = dnd->DNDActionCopyXAtom;
  MotifDND_DraggerActionList[1] = None;
  XDND_DEBUG2("    -->> dnd_data->reason = %d\n", dnd_data->reason);
  if (dnd_data->reason == DND_DRAG_MOTION) {
    /*
     * We receive these events as the mouse moves inside the toplevel.
     * We have to find the actual window we are over, emulate
     * Enter/Leave events and query widget whether it will accept the
     * drop or not :-). Simple stuff...
     */
    
    /*
     * Set the desired by the drag source action...
     * TODO:
     */
          
    /*
     * Reset drag source supported actions...
     * TODO:
     */
    /*for (i=0; i<4; i++) {
      MotifDND_DraggerActionList[i] = None;
    }*/
    
    dnd->x = dnd_data->x;
    dnd->y = dnd_data->y;
    dnd->Toplevel = xevent.xclient.window;
    src_window = dnd->DraggerWindow;
    XDND_FindTarget(dnd, dnd->x, dnd->y, &temp_win, &temp_win, &cur_window,
                    &aware, &temp_atom);
    XDND_DEBUG2("We are at window %ld\n", cur_window);
        if (cur_window == None) return True;
        if (dnd->MouseWindow != cur_window) {
          /*
           * Send a Leave event to old window...
           */
          XDND_DEBUG2("MotifDND_HandleClientMessage: Sending Leave to window "
                      "%ld...\n", dnd->MouseWindow);
          if (dnd->WidgetApplyLeaveCallback != NULL) {
            (*dnd->WidgetApplyLeaveCallback) (dnd, dnd->MouseWindow);
          }
          dnd->MouseWindow        = cur_window;
          dnd->WillAcceptDropFlag = False;
          /*
           * Send an Enter event to the new window...
           */
          XDND_DEBUG2("MotifDND_HandleClientMessage: Sending Enter to window "
                      "%ld...\n", dnd->MouseWindow);
          if (dnd->WidgetApplyEnterCallback != NULL) {
            status = (*dnd->WidgetApplyEnterCallback)
              (dnd, dnd->MouseWindow, dnd->DraggerWindow,
               dnd->DNDActionCopyXAtom, dnd->x, dnd->y, CurrentTime,
               dnd->DraggerTypeList);
          }
        }

        /*
         * Now, simulate XdndPosition and ask widget whether it will accept
         * the data at that particular point...
         */
        if (!status) return True;
        if (dnd->WidgetApplyPositionCallback != NULL) {
          status = (*dnd->WidgetApplyPositionCallback) (dnd, 
                     dnd->MouseWindow, dnd->DraggerWindow,
                     dnd->DNDActionCopyXAtom, MotifDND_DraggerActionList,
                     dnd->x, dnd->y, CurrentTime, dnd->DraggerTypeList, &status,
                     &(dnd->SupportedAction), &(dnd->DesiredType),
                     &rectangle);
          if (dnd->CallbackStatus == TCL_ERROR || !status) {
            if (dnd->WillAcceptDropFlag) {
              /* We have send a drop site enter. Send a drop site leave... */
              XDND_DEBUG("MotifDND_HandleClientMessage: sending drop site "
                         "leave\n");
              dnd_data->reason     = DND_DROP_SITE_LEAVE;
              dnd_data->time       = CurrentTime;
              DndFillClientMessage(xevent.xclient.display, src_window, &cm,
                                   dnd_data, 0);
              XSendEvent(dnd->display, src_window, False, 0, (XEvent *)&cm);
              XDND_DEBUG2("MotifDND_HandleClientMessage: XSendEvent "
                          "DND_DROP_SITE_LEAVE %ld\n", src_window);
              dnd->WillAcceptDropFlag = False;
            }
            return True;
          }

          /* 
           * if dnd->WillAcceptDropFlag == True, then we have already send a
           * drop site enter. So, repeat the drag motion event...
           */
          if (dnd->WillAcceptDropFlag) {
            XDND_DEBUG("MotifDND_HandleClientMessage: repeating drag motion "
                       "...\n");
            dnd_data->reason     = DND_DRAG_MOTION;
            dnd_data->time       = CurrentTime;
            if (dnd->SupportedAction == dnd->DNDActionCopyXAtom) {
              dnd_data->operation = DND_COPY;
            } else if (dnd->SupportedAction == dnd->DNDActionMoveXAtom) {
              dnd_data->operation = DND_MOVE;
            } else if (dnd->SupportedAction == dnd->DNDActionLinkXAtom) {
              dnd_data->operation = DND_LINK;
            } else {
              dnd_data->operation = DND_MOVE|DND_COPY|DND_LINK;
            }
            dnd_data->operations = DND_MOVE|DND_COPY|DND_LINK;
            DndFillClientMessage(xevent.xclient.display, src_window, &cm,
                                 dnd_data, 0);
            XSendEvent(dnd->display, src_window, False, 0, (XEvent *)&cm);
            XDND_DEBUG2("MotifDND_HandleClientMessage: XSendEvent "
                        "DND_DRAG_MOTION %ld\n", src_window);
            return True;
          }
          dnd->WillAcceptDropFlag = status;
          XDND_DEBUG("MotifDND_HandleClientMessage: sending drop site "
                     "enter\n");
          dnd_data->reason     = DND_DROP_SITE_ENTER;
          dnd_data->time       = CurrentTime;
          dnd_data->operation  = DND_MOVE|DND_COPY|DND_LINK;
          dnd_data->operations = DND_MOVE|DND_COPY|DND_LINK;
          DndFillClientMessage(xevent.xclient.display, src_window, &cm,
                               dnd_data, 0);
          XSendEvent(dnd->display, src_window, False, 0, (XEvent *)&cm);
          XDND_DEBUG2("MotifDND_HandleClientMessage: XSendEvent "
                      "DND_DROP_SITE_ENTER %ld\n", src_window);
        }
      } else if (dnd_data->reason == DND_TOP_LEVEL_ENTER) {
        /*
         * The mouse has entered in a new toplevel...
         */
        dnd->DraggerWindow = src_window = dnd_data->src_window;
        dnd->MouseWindow = None;
        XDND_DEBUG2("source sending a top level enter %ld\n", src_window);
        /* no answer needed, just read source property */
        DndReadSourceProperty(xevent.xclient.display, src_window,
                     dnd_data->property, &src_targets, &num_src_targets);
        if (dnd->DraggerTypeList!=NULL) Tcl_Free((char *) dnd->DraggerTypeList);
        /*
         * Sometimes, we get numbers of supported types, like 41072, and Atom
         * numbers like 7000000...
         */
        for (i = 0; i < num_src_targets; i++) {
          if (src_targets[i] > 6000) {
            num_src_targets = i; break;
          }
        }
        dnd->DraggerTypeList = 
            (Atom *) Tcl_Alloc(sizeof(Atom) * (num_src_targets+2));
        XDND_DEBUG2("MotifDND_HandleClientMessage: Drag source supports "
                    "%d types:\n", num_src_targets);
        for (i = 0; i < num_src_targets; i++) {
          dnd->DraggerTypeList[i] = src_targets[i];
#ifdef DND_DEBUG
          XDND_DEBUG2("  (%ld)", src_targets[i]);
          if (src_targets[i] == None) continue;
          AtomName = XGetAtomName(dnd->display, src_targets[i]);
          XDND_DEBUG2("        %s\n", AtomName);
          XFree(AtomName);
#endif /* DND_DEBUG */
        }
        dnd->DraggerTypeList[num_src_targets] = None;
        if (src_targets != NULL && num_src_targets) free(src_targets);
        
      } else if (dnd_data->reason == DND_TOP_LEVEL_LEAVE) {
        if (dnd->DraggerTypeList!=NULL) Tcl_Free((char *) dnd->DraggerTypeList);
        dnd->DraggerTypeList = NULL;
        XDND_DEBUG("source sending a top level leave\n");
        /* no need to reply to this message... */
      } else if (dnd_data->reason == DND_DROP_SITE_ENTER) {
        XDND_DEBUG("receiver sending drop site enter\n");
      } else if (dnd_data->reason == DND_DROP_SITE_LEAVE) {
        XDND_DEBUG("receiver sending drop site leave\n");
      } else if (dnd_data->reason == DND_OPERATION_CHANGED) {
        XDND_DEBUG("receiver sending operation changed\n");
      } else if (dnd_data->reason == DND_DROP_START) {
        cur_window = dnd->MouseWindow;
        src_window = dnd->DraggerWindow;
        /*
         * Does the widget wants the drop?
         */
        if (!dnd->WillAcceptDropFlag) {
          XDND_DEBUG("MotifDND_HandleClientMessage: sending drop failure!\n");
          XConvertSelection(dnd->display, dnd_data->property,
                          dnd->Motif_DND_FailureAtom, dnd_data->property,
                          src_window, dnd_data->time);
        } else {
          XDND_DEBUG("MotifDND_HandleClientMessage: sending drop start!\n");
          dnd_data->reason     = DND_DROP_START;
          dnd_data->status     = DND_VALID_DROP_SITE;
          dnd_data->operation  = DND_COPY;
          dnd_data->completion = DND_DROP;
          DndFillClientMessage(xevent.xclient.display, cur_window, &cm,
                               dnd_data, 0);
          XSendEvent(dnd->display, src_window, False, 0, (XEvent *)&cm);
          XDND_DEBUG2("MotifDND_HandleClientMessage: XSendEvent "
                      "DND_DROP_START %ld\n", src_window);
          Selection   = dnd->DNDSelectionName;
          DesiredType = dnd->DesiredType;
#ifdef DND_DEBUG
          AtomName = XGetAtomName(dnd->display, dnd_data->property);
          XDND_DEBUG2("Getting Selection %s\n", AtomName);
          XFree(AtomName);
#endif /* DND_DEBUG */
          dnd->DNDSelectionName = dnd_data->property;
          xevent.xclient.data.l[0] = dnd->DraggerWindow;
          xevent.xclient.data.l[2] = CurrentTime;
          dnd->Motif_DND = True;
          XDND_HandleDNDDrop(dnd, xevent.xclient);
          dnd->Motif_DND = False;
          /* Report success, so as to notify the source application that we have
           * finished... */
          XConvertSelection(dnd->display, dnd_data->property,
                            dnd->Motif_DND_SuccessAtom, dnd_data->property,
                            src_window, dnd_data->time);
          /*
           * Wait for notification from drag source...
           * Desided not to include it. Does not offer anything more than
           * delay :-). According to the protocol, it should be there...
           */
       /* startTime = clock();
          endTime   = startTime + 5 * CLOCKS_PER_SEC;
          while (!receivedEvent && clock() < endTime) {
            receivedEvent=XCheckTypedWindowEvent(dnd->display, dnd->MouseWindow,
                                                 SelectionNotify, &xevent);
            if (!userBored && clock() > startTime + 1 * CLOCKS_PER_SEC) {
              userBored = True;
            }
            if (receivedEvent) break;
          } */
          dnd->DNDSelectionName = Selection;
        }
        /*
         * Reset all changed values...
         */
        if (dnd->DraggerTypeList!=NULL) Tcl_Free((char *) dnd->DraggerTypeList);
        dnd->DraggerTypeList = NULL;
        dnd->WillAcceptDropFlag = False;
        dnd->DraggerWindow = None;
        dnd->MouseWindow = None;
        dnd->SupportedAction = None;
        dnd->DesiredType = None;
        dnd->CallbackStatus = TCL_OK;
      }
  return True;
} /* MotifDND_HandleClientMessage */
