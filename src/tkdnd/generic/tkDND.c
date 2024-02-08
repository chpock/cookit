/*
 * tkDND.c --
 * 
 *    This file implements a drag&drop mechanish for the tk toolkit.
 *    It contains the top-level command routines as well as some needed
 *    functions for defining and removing bind scripts.
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
 *
 * RCS: @(#) $Id: tkDND.c,v 1.10 2002/05/20 08:55:30 petasis Exp $
 */

#include "tkDND.h"

/* 
 * Global Variable Definitions...
 */
Tcl_HashTable   TkDND_TargetTable; /* This table hold dnd target bindings... */
Tcl_HashTable   TkDND_SourceTable; /* This table hold dnd source bindings... */
DndClass       *TkDND_dnd = NULL;  /* This structure holds platform specific 
                                      information */
static int initialized = 0;        /* > 0 if package is already initialised. */
static char TkDND_DescriptionStr[TKDND_MAX_DESCRIPTIONS_LENGTH];

/*
 * Forward declarations for procedures defined later in this file:
 */

/*
 * For C++ compilers, use extern "C"
 */
#ifdef __cplusplus
extern "C" {
#endif
DLLEXPORT int Tkdnd_Init(Tcl_Interp *interp);
DLLEXPORT int Tkdnd_SafeInit(Tcl_Interp *interp);
#ifdef __cplusplus
}
#endif

int        TkDND_DndObjCmd(ClientData clientData, Tcl_Interp *interp,
                int objc,Tcl_Obj *CONST objv[]);

void      *TkDND_Init(Tcl_Interp *interp, Tk_Window topwin);
int        TkDND_AddHandler(Tcl_Interp *interp, Tk_Window topwin,
                Tcl_HashTable *table, char *windowPath, char *typeStr,
                unsigned long eventType, unsigned long eventMask,
                char *script, int isDropTarget, int priority);
int        TkDND_ParseEventDescription(Tcl_Interp *interp,
                char *eventStringPtr, unsigned long *eventTypePtr,
                unsigned long *eventMaskPtr);
int        TkDND_DelHandler(DndInfo *infoPtr, char *typeStr,
                unsigned long eventType, unsigned long eventMask);
int        TkDND_DelHandlerByName(Tcl_Interp *interp, Tk_Window topwin,
                Tcl_HashTable *table, char *windowPath, char *typeStr,
                unsigned long eventType, unsigned long eventMask);
void       TkDND_DestroyEventProc(ClientData clientData, XEvent *eventPtr);
int        TkDND_GetCurrentTypes(Tcl_Interp *interp, Tk_Window topwin,
                Tcl_HashTable *table, char *windowPath);
char      *TkDND_GetCurrentTypeName(void);
char      *TkDND_GetCurrentTypeCode(void);
char      *TkDND_GetCurrentActionName(void);
int        TkDND_GetCurrentButton(void);
char      *TkDND_GetCurrentModifiers(Tk_Window tkwin);
char      *TkDND_GetSourceActions(void);
char      *TkDND_GetSourceActionDescriptions(void);
char      *TkDND_GetSourceTypeList(void);
char      *TkDND_GetSourceTypeCodeList(void);
char      *TkDND_TypeToString(int type);
int        TkDND_DndDrag(Tcl_Interp *interp, char *windowPath, int button,
                Tcl_Obj *Actions, char *Descriptions,
                Tk_Window cursor_window, char *cursor_callback);
int        TkDND_ExecuteBinding(Tcl_Interp *interp, char *script,
                int numBytes, Tcl_Obj *data);
char      *TkDND_GetDataAccordingToType(DndInfo *info, Tcl_Obj *ResultObj,
                DndType *typePtr, int *length);
int        TkDND_GetDataFromImage(DndInfo *info, char *imageName,
                char *type, char **result, int *length);
int        TkDND_Update(Display *display, int idle);

extern int TkDND_GetCurrentScript(Tcl_Interp *interp, Tk_Window topwin,
                Tcl_HashTable *table, char *windowPath, char *typeStr,
                unsigned long eventType, unsigned long eventMask);

/*
 *----------------------------------------------------------------------
 *
 * Tkdnd_Init --
 *
 *       Initialize the drag and drop package.
 *
 * Results:
 *       A standard Tcl result
 *
 * Side effects:
 *       "dnd" command is added into interpreter.
 *
 *----------------------------------------------------------------------
 */
int DLLEXPORT Tkdnd_Init(Tcl_Interp *interp)
{
    int major, minor, patchlevel;
    Tk_Window topwin;

    if (!initialized) {
        if (
#ifdef USE_TCL_STUBS 
            Tcl_InitStubs(interp, "8.3", 0)
#else
            Tcl_PkgRequire(interp, "Tcl", "8.3", 0)
#endif /* USE_TCL_STUBS */
            == NULL) {
            return TCL_ERROR;
        }
        if (
#ifdef USE_TK_STUBS
            Tk_InitStubs(interp, "8.3", 0)
#else
            Tcl_PkgRequire(interp, "Tk", "8.3", 0)
#endif /* USE_TK_STUBS */
            == NULL) {
            return TCL_ERROR;
        }

        /*
         * Get the version, because we really need 8.3.3+.
         */
        Tcl_GetVersion(&major, &minor, &patchlevel, NULL);
        if ((major == 8) && (minor == 3) && (patchlevel < 3)) {
            Tcl_SetResult(interp, "tkdnd requires Tk 8.3.3 or greater",
                    TCL_STATIC);
            return TCL_ERROR;
        }

        Tcl_PkgProvide(interp, TKDND_PACKAGE, TKDND_VERSION);
        Tcl_InitHashTable(&TkDND_TargetTable, TCL_STRING_KEYS);
        Tcl_InitHashTable(&TkDND_SourceTable, TCL_STRING_KEYS);
    }

    topwin = Tk_MainWindow(interp);
    if (topwin == NULL) return TCL_ERROR;

    /* Create and Initilise the DnD structure... */
    if (!initialized) {
        TkDND_dnd = (DndClass *) TkDND_Init(interp, topwin);
        if (TkDND_dnd == NULL) {
            return TCL_ERROR;
        }
    }

    /* Register the "dnd" Command */
    if (Tcl_CreateObjCommand(interp, "dnd", (Tcl_ObjCmdProc*) TkDND_DndObjCmd,
            (ClientData) topwin, (Tcl_CmdDeleteProc *) NULL) == NULL) {
        return TCL_ERROR;
    }

    initialized = 1;
    return TCL_OK;
} /* Tkdnd_Init */

int DLLEXPORT Tkdnd_SafeInit(Tcl_Interp *interp) {
    return Tkdnd_Init(interp);
} /* Tkdnd_SafeInit */

/*
 *----------------------------------------------------------------------
 * TkDND_DndObjCmd --
 *
 *       Drag and drop command.  This has several forms.
 *
 * Results:
 *       A standard TCL result.
 *----------------------------------------------------------------------
 */
int TkDND_DndObjCmd(ClientData clientData, Tcl_Interp *interp,
        int objc, Tcl_Obj *CONST objv[]) {
    Tk_Window topwin = (Tk_Window) clientData;
    Tk_Window tkwin, cursor_window = NULL;
    Tcl_Obj *Actions = NULL, **elem, *CursorCallbackObject = NULL;
    char *window, *action, *description, *Descriptions = TkDND_DescriptionStr,
        *ptr, *event, *cursor_callback = NULL;
    int button = 0, i, j, elem_nu, des_nu, des_len, index, status, priority;
    unsigned long eventType, eventMask;     /* Laurent Riesterer 06/07/2000 */
    static char *Methods[] = {
        "aware", "bindsource", "bindtarget",
        "clearsource", "cleartarget", "drag", (char *) NULL
    };
    enum methods {
        AWARE, BINDSOURCE, BINDTARGET, CLEARSOURCE, CLEARTARGET, DRAG
    };

    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "option window ?arg ...?");
        return TCL_ERROR;
    }
  
    tkwin = Tk_NameToWindow(interp, Tcl_GetString(objv[2]), topwin);
    if (tkwin == NULL) {
        return TCL_ERROR;
    }

    /* Handle Arguments... */
    if (Tcl_GetIndexFromObj(interp, objv[1], (CONST84 char **) Methods,
                            "method", 0, &index) 
            != TCL_OK) return TCL_ERROR;
    switch ((enum methods) index) {
        case AWARE: {
            if (objc != 4 && objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "window ?mime-type?");
                return TCL_ERROR;
            }
            return TCL_OK;
        }
        case BINDSOURCE: {
            if (objc > 6) {
                Tcl_WrongNumArgs(interp, 2, objv,
                        "window ?type ?script ?priority? ? ?");
                return TCL_ERROR;
            }
            
            if (objc >= 4) {
              if (strchr(Tcl_GetString(objv[3]), '*') != NULL) {
                Tcl_SetResult(interp, "the character \"*\" should not be "
                   "contained in the type of a drag source!", TCL_STATIC);
                return TCL_ERROR;
              }
            }

            if (objc == 3) {
                /* Return the registered types... */
                return TkDND_GetCurrentTypes(interp, topwin,
                        &TkDND_SourceTable, Tcl_GetString(objv[2]));
            } else if (objc == 4) {
                /* Return the script for the given type... */
                return TkDND_GetCurrentScript(interp, topwin,
                        &TkDND_SourceTable, Tcl_GetString(objv[2]),
                        Tcl_GetString(objv[3]), TKDND_GETDATA, 0);
            } else if (objc == 5) {
                /* Bind a new handler... */
                if (strlen(Tcl_GetString(objv[4])) > 0) {
                    /* Add the new handler... */
                    return TkDND_AddHandler(interp, topwin, &TkDND_SourceTable,
                            Tcl_GetString(objv[2]), Tcl_GetString(objv[3]),
                            TKDND_GETDATA, 0, Tcl_GetString(objv[4]), 0, 50);
                } else {
                    /* We got an empty string. Remove the handler... */
                    return TkDND_DelHandlerByName(interp, topwin,
                            &TkDND_SourceTable, Tcl_GetString(objv[2]),
                            Tcl_GetString(objv[3]), TKDND_GETDATA, 0);
                }
            } else if (objc == 6) {
                /* The priority... */
                if (Tcl_GetIntFromObj(interp, objv[5], &priority) != TCL_OK) {
                    return TCL_ERROR;
                }
                if (priority < 1) {
                    priority = 1;
                } else if (priority > 100) {
                    priority = 100;
                }
                if (strlen(Tcl_GetString(objv[4])) == 0) {
                    Tcl_SetResult(interp,
                            "when priority is specified an empty script "
                            "is not permitted", TCL_STATIC);
                    return TCL_ERROR;
                }
                /* Add the new handler... */
                return TkDND_AddHandler(interp, topwin, &TkDND_SourceTable,
                        Tcl_GetString(objv[2]), Tcl_GetString(objv[3]),
                        TKDND_GETDATA, 0, Tcl_GetString(objv[4]), 0,
                        priority);
            }
        } /* BINDSOURCE */
        case BINDTARGET :{
            if (objc > 7) {
                Tcl_WrongNumArgs(interp, 2, objv,
                        "window ?type ?event ?script? ?priority? ? ?");
                return TCL_ERROR;
            }

            if (objc == 3) {
                /* Return the registered types... */
                return TkDND_GetCurrentTypes(interp, topwin,
                        &TkDND_TargetTable, Tcl_GetString(objv[2]));
            } else if (objc == 4) {
                /* Return the script for the "<Drop>" event... */
                return TkDND_GetCurrentScript(interp, topwin,
                        &TkDND_TargetTable,
                        Tcl_GetString(objv[2]), Tcl_GetString(objv[3]),
                        TKDND_DROP, 0);
            } else {
                event = Tcl_GetString(objv[4]);
                /* Laurent Riesterer 06/07/2000 */
                if (TkDND_ParseEventDescription(interp, event, &eventType,
                        &eventMask) != TCL_OK) {
                    return TCL_ERROR;
                }
                /* XDND_DEBUG4("<DndCmd> parse result (%s) type = %2d,
                   mask = %08x\n", event, eventType, eventMask); */

                if (objc == 5) {
                    /* Return the script for the specified event... */
                    return TkDND_GetCurrentScript(interp, topwin,
                            &TkDND_TargetTable,
                            Tcl_GetString(objv[2]), Tcl_GetString(objv[3]),
                            eventType, eventMask);
                } else if (objc == 6) {
                    /* Bind a new handler... */
                    if (strlen(Tcl_GetString(objv[5])) > 0) {
                        /* Add the new handler... */
                        return TkDND_AddHandler(interp, topwin,
                                &TkDND_TargetTable,
                                Tcl_GetString(objv[2]), Tcl_GetString(objv[3]),
                                eventType, eventMask, Tcl_GetString(objv[5]),
                                1, 50);
                    } else {
                        /* We got an empty string. Remove the handler... */
                        return TkDND_DelHandlerByName(interp, topwin,
                                &TkDND_TargetTable,
                                Tcl_GetString(objv[2]), Tcl_GetString(objv[3]),
                                eventType, eventMask);
                    }
                } else {
                    /* The priority... */
                    if (Tcl_GetIntFromObj(interp, objv[6], &priority)
                            != TCL_OK) {
                        return TCL_ERROR;
                    }
                    if (priority < 1) {
                        priority = 1;
                    } else if (priority > 100) {
                        priority = 100;
                    }
                    return TkDND_AddHandler(interp, topwin, &TkDND_TargetTable,
                            Tcl_GetString(objv[2]), Tcl_GetString(objv[3]),
                            eventType, eventMask,
                            Tcl_GetString(objv[5]), 1, priority);
                }
            }
        } /* BINDTARGET */
        case CLEARSOURCE: {
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "window");
                return TCL_ERROR;
            }
            return TkDND_DelHandlerByName(interp, topwin, &TkDND_SourceTable,
                    Tcl_GetString(objv[2]), NULL,
                    TKDND_SOURCE, 0);
        } /* CLEARSOURCE */
        case CLEARTARGET: {
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "window");
                return TCL_ERROR;
            }
            return TkDND_DelHandlerByName(interp, topwin, &TkDND_TargetTable,
                    Tcl_GetString(objv[2]), NULL,
                    TKDND_TARGET, 0);
        } /* CLEARTARGET */
        case DRAG: {
            if (objc != 3 && objc != 5 && objc != 7 &&
                    objc != 9 && objc != 11 && objc != 13) {
                Tcl_WrongNumArgs(interp, 2, objv, "window ?-button button? "
                        "?-actions action-list? "
                        "?-descriptions description-list? "
                        "?-cursorwindow window? ?-callback script?");
                return TCL_ERROR;
            }

            /*
             * Get window. This is a valid one, as we have already check it...
             */
            window = Tcl_GetString(objv[2]);

            /*
             * Parse optional parameters...
             */
            for (i = 3; i < objc; i += 2) {
                if (strncmp(Tcl_GetString(objv[i]), "-b", 2) == 0) {
                    /* -button */
                    if (Tcl_GetIntFromObj(interp, objv[i+1], &button)
                            != TCL_OK) {
                        return TCL_ERROR;
                    }
                } else if (strncmp(Tcl_GetString(objv[i]), "-cu", 3) == 0) {
                    /* -cursor */
                    cursor_window = Tk_NameToWindow(interp,
                            Tcl_GetString(objv[i+1]), topwin);
                    if (cursor_window == NULL) {
                        return TCL_ERROR;
                    }
                } else if (strncmp(Tcl_GetString(objv[i]), "-ca", 3) == 0) {
                    /* -callback */
                    CursorCallbackObject = objv[i+1];
                    Tcl_IncrRefCount(CursorCallbackObject);
                    cursor_callback = Tcl_GetString(objv[i+1]);
                } else if (strncmp(Tcl_GetString(objv[i]), "-a", 2) == 0) {
                    Actions = objv[i+1];
                    /*
                     * Check the validity of actions. Valid actions are:
                     * "copy", "move", "link", "ask", "private"
                     */
                    status = Tcl_ListObjGetElements(interp, Actions,
                            &elem_nu, &elem);
                    if (status != TCL_OK) return status;
                    if (elem_nu > 5) {
                        Tcl_SetResult(interp,
                                "too many actions specified: at most 5 are "
                                "allowed, which must be from the following"
                                " ones:\ncopy, move, link, ask, private",
                                TCL_STATIC);
                        if (CursorCallbackObject != NULL)
                            Tcl_DecrRefCount(CursorCallbackObject);
                        return TCL_ERROR;
                    }
                    for (j = 0; j < elem_nu; j++) {
                        action = Tcl_GetString(elem[j]);
                        if (strcmp(action, "copy") != 0 &&
                                strcmp(action, "move") != 0 &&
                                strcmp(action, "link") != 0 &&
                                strcmp(action, "ask" ) != 0 &&
                                strcmp(action, "private") != 0) {
                            Tcl_SetResult(interp, "unknown action \"",
                                    TCL_STATIC);
                            Tcl_AppendResult(interp, action, "\"!",
                                    (char *) NULL);
                            if (CursorCallbackObject != NULL)
                                Tcl_DecrRefCount(CursorCallbackObject);
                            return TCL_ERROR;
                        }
                    }
                    Tcl_IncrRefCount(Actions);
                } else if (strncmp(Tcl_GetString(objv[i]), "-d", 2) == 0) {
                    if (Actions == NULL) {
                        Tcl_SetResult(interp,
                                "-actions option must be specified before "
                                "the -descriptions options", TCL_STATIC);
                        if (CursorCallbackObject != NULL) {
                            Tcl_DecrRefCount(CursorCallbackObject);
                        }
                        return TCL_ERROR;
                    }
                    /*
                     * Check the length of the descriptions...
                     */
                    if (strlen(Tcl_GetString(objv[i+1])) >= 
                            TKDND_MAX_DESCRIPTIONS_LENGTH - 10) {
                        Tcl_SetResult(interp,
                                "the total length of descriptions cannot "
                                "exceed " TKDND_MAX_DESCRIPTIONS_LENGTH_STR
                                " characters", TCL_STATIC);
                        if (CursorCallbackObject != NULL) {
                            Tcl_DecrRefCount(CursorCallbackObject);
                        }
                        return TCL_ERROR;
                    }
                    /*
                     * Get the descriptions...
                     */
                    ptr = Descriptions;
                    memset(ptr, '\0', TKDND_MAX_DESCRIPTIONS_LENGTH);
                    status = Tcl_ListObjGetElements(interp, objv[i+1],
                            &des_nu, &elem);
                    if (status != TCL_OK) return status;
                    if (elem_nu != des_nu) {
                        Tcl_SetResult(interp,
                                "description number must be equal to the "
                                "action number, as they describe the actions.",
                                TCL_STATIC);
                        if (CursorCallbackObject != NULL) {
                            Tcl_DecrRefCount(CursorCallbackObject);
                        }
                        return TCL_ERROR;
                    }
                    for (j = 0; j < elem_nu; j++) {
                        description = Tcl_GetString(elem[j]);
                        des_len = strlen(description);
                        memcpy(ptr, description, des_len+1);
                        ptr += des_len+1;
                    }
                    *ptr = '\0';
                } else {
                    Tcl_ResetResult(interp);
                    Tcl_AppendResult(interp, "unknown option \"",
                            Tcl_GetString(objv[i]), "\"", (char *) NULL);
                    if (CursorCallbackObject != NULL) {
                        Tcl_DecrRefCount(CursorCallbackObject);
                    }
                    return TCL_ERROR;
                }
            }
            status = TkDND_DndDrag(interp, Tcl_GetString(objv[2]), button,
                    Actions, Descriptions, cursor_window, cursor_callback);
            if (Actions != NULL) Tcl_DecrRefCount(Actions);
            if (CursorCallbackObject != NULL) {
                Tcl_DecrRefCount(CursorCallbackObject);
            }
            return status;
        } /* DRAG */
    }
    return TCL_OK;
} /* TkDND_DndObjCmd */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_DelHandler --
 *
 *        Delete a handler.  Assumes everything has already been looked
 *        up elsewhere
 *
 * Results:
 *        A standard TCL results
 *
 *----------------------------------------------------------------------
 */

int TkDND_DelHandler(DndInfo *infoPtr, char *typeStr,
        unsigned long eventType, unsigned long eventMask) {
    DndType *head, *prev, *curr, *next;
    int match;


    head = prev = &infoPtr->head;
    for (curr = head->next; curr != NULL; curr = next) {
        next = curr->next;
        match = 0;
        if ((typeStr == NULL) || (strcmp(curr->typeStr, typeStr) == 0)) {
            /* Modified by Laurent Riesterer 06/07/2000 */
            if (eventType == TKDND_SOURCE || eventType == TKDND_TARGET 
                    || (curr->eventType == eventType &&
                            curr->eventMask == eventMask)) {
                match = 1;
            }
            if (match) {
                Tcl_Free(curr->typeStr);
                Tcl_Free(curr->script);
                prev->next = next;
            }
        } else {
            prev = curr;
        }
    }
        
    if (head->next == NULL) {
#ifdef __WIN32__
        if (infoPtr->DropTarget) {
            RevokeDragDrop(Tk_GetHWND(Tk_WindowId(infoPtr->tkwin)));
        }
#endif /* __WIN32__ */
        Tk_DeleteEventHandler(infoPtr->tkwin, StructureNotifyMask,
                TkDND_DestroyEventProc, (ClientData) infoPtr);
        Tcl_DeleteHashEntry(infoPtr->hashEntry);
        Tcl_Free((char *) infoPtr);
    }
    return TCL_OK;
} /* TkDND_DelHandler */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_DelHandlerByName --
 *
 *        Remove a drag&drop handler window, freeing up any memory and objects
 *        that need to be cleaned up.
 *
 * Arguments:
 *        interp:         Tcl interpreter
 *        topwin:         The main window
 *        table:          Hash table we are removing the window/type from
 *        windowPath:     String pathname of window
 *        typeStr:        The data type to remove.
 *        eventType:      Type of event to remove (Riesterer 06/07/2000)
 *        eventMask:      Mask of event to remove (Riesterer 06/07/2000)
 *
 * Results:
 *        A standard Tcl result
 *
 * Notes:
 *        The table that is passed in must have been initialized.
 *        If type == NULL, then everything is cleaned up.
 *
 *----------------------------------------------------------------------
 */
int TkDND_DelHandlerByName(Tcl_Interp *interp, Tk_Window topwin,
        Tcl_HashTable *table, char *windowPath,
        char *typeStr, unsigned long eventType, unsigned long eventMask)
{
    Tcl_HashEntry *hPtr;
    Tk_Window tkwin;
        
    tkwin = Tk_NameToWindow(interp, windowPath, topwin);
    if (tkwin == NULL) return TCL_ERROR;
        
    hPtr = Tcl_FindHashEntry(table, windowPath);
    if (hPtr == NULL) return TCL_OK;
        
    return TkDND_DelHandler((DndInfo *)Tcl_GetHashValue(hPtr), typeStr,
            eventType, eventMask);
} /* TkDND_DelHandlerByName */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_DestroyEventProc --
 *
 *        This routine gets called on events that match the StructureNotify
 *        mask.  We only care about DestroyNotify events.
 *
 * Results:
 *        None
 *
 * Side Effects:
 *        Removes the window from being either a source or a target.
 *
 *----------------------------------------------------------------------
 */
void TkDND_DestroyEventProc(ClientData clientData, XEvent *eventPtr)
{
    DndInfo *infoPtr = (DndInfo *) clientData;
        
    if (eventPtr->type == DestroyNotify) {
        /* XDND_DEBUG2("Deleting %s\n", Tk_PathName(infoPtr->tkwin)); */
        /* Laurent Riesterer 06/07/2000 */
        TkDND_DelHandler(infoPtr, NULL, TKDND_SOURCE, 0);
        /* XDND_DEBUG("done\n"); */
    }
} /* TkDND_DestroyEventProc */

/*
 *----------------------------------------------------------------------
 * TkDND_GetCurrentTypes --
 *
 *        Append a list of the current types that are handled by the given
 *        to the result.
 *
 * Results:
 *        A standard TCL result.
 *
 *----------------------------------------------------------------------
 */
int TkDND_GetCurrentTypes(Tcl_Interp *interp, Tk_Window topwin,
        Tcl_HashTable *table, char *windowPath)
{
    Tcl_HashEntry *hPtr;
    DndInfo *infoPtr;
    Tk_Window tkwin;
    DndType *curr;
  
    if (interp == NULL) {
        return TCL_ERROR;
    }

    Tcl_ResetResult(interp);
    tkwin = Tk_NameToWindow(interp, windowPath, topwin);
    if (tkwin == NULL) {
        return TCL_ERROR;
    }
  
    hPtr = Tcl_FindHashEntry(table, windowPath);
    if (hPtr == NULL) {
        return TCL_OK;
    }
  
    infoPtr = (DndInfo *) Tcl_GetHashValue(hPtr);
    for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
        Tcl_AppendElement(interp, curr->typeStr);
    }
    return TCL_OK;
} /* TkDND_GetCurrentTypes */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_ExpandPercents --
 *
 *        Given a command and an event, produce a new command
 *        by replacing % constructs in the original command
 *        with information from the drop event
 *
 * Results:
 *        The new expanded command is appended to the dynamic string
 *        given by dsPtr.
 *
 * Side effects:
 *        None.
 *
 *--------------------------------------------------------------
 */
void TkDND_ExpandPercents(DndInfo *infoPtr, DndType *typePtr,
        char *before, Tcl_DString *dsPtr, LONG x, LONG y)
{
    /* Used to substitute string as proper Tcl list element. */
    int spaceNeeded, cvtFlags;
    int number, length;
    register char *string;
    char numStorage[41];
    int wx, wy, free_string = False;


    Tk_GetRootCoords(infoPtr->tkwin, &wx, &wy);
    while (1) {
        /*
         * Find everything up to the next % character and append it
         * to the result string.
         */
        for (string = before; (*string != 0) && (*string != '%'); string++) {
            /* Empty loop body. */
        }
        if (string != before) {
            Tcl_DStringAppend(dsPtr, before, string-before);
            before = string;
        }
        if (*before == 0) break;
                
        /*
         * There's a percent sequence here.  Process it.
         */
        number = 0;
        string = "??";
        switch (before[1]) {
            case 'A':
                if (typePtr->script == NULL && *typePtr->typeStr == '\0') {
                    /*
                     * This will only happen when we are processing the Cursor
                     * callback fo a drag source window and the cursor is over
                     * a window that does not supports dnd. So, return an empty
                     * action...
                     */
                    string = "";
                } else {
                    string = TkDND_GetCurrentActionName();
                }
                free_string = False;
                goto doString;
            case 'a':
                string = TkDND_GetSourceActions();
                free_string = True;
                goto doString;
            case 'b': /*  Laurent Riesterer 24/06/2000 */
                number = TkDND_GetCurrentButton();
                free_string = False;
                goto doNumber;
            case 'c': /* The list of supported by the source type codes */
                string = TkDND_GetSourceTypeCodeList();
                free_string = True;
                goto doString;
            case 'C': /* Type code */
                string = TkDND_GetCurrentTypeCode();
                free_string = True;
                goto doString;
            case 'D':
                /* 
                 * This is a special case. %D must be substituted by the
                 * dropped data.  We must leave it as %D. It will be
                 * substituted during execution...
                 */
                string = "%D";
                free_string = False;
                goto doString;
            case 'd':
                string = TkDND_GetSourceActionDescriptions();
                free_string = True;
                goto doString;
            case 'm': /*  Laurent Riesterer 24/06/2000 */
                string = TkDND_GetCurrentModifiers(infoPtr->tkwin);
                free_string = True;
                goto doString;
            case 'T':
                /* string = typePtr->typeStr; */
                string = TkDND_GetCurrentTypeName();
                free_string = False;
                goto doString;
            case 't': /* The list of supported by the source types */
                string = TkDND_GetSourceTypeList();
                free_string = True;
                goto doString;
            case 'W':
                string = Tk_PathName(infoPtr->tkwin);
                free_string = False;
                goto doString;
            case 'X':
                number = x;
                free_string = False;
                goto doNumber;
            case 'x':
                /*if (! converted) {
                  sPt.x = x;
                  sPt.y = y;
                  ScreenToClient(Tk_GetHWND(Tk_WindowId(infoPtr->tkwin)), &sPt);
                  }
                  number = sPt.x;*/
                number = x - wx;
                free_string = False;
                goto doNumber;
            case 'Y':
                number = y;
                free_string = False;
                goto doNumber;
            case 'y':
                /*if (! converted) {
                  sPt.x = x;
                  sPt.y = y;
                  ScreenToClient(Tk_GetHWND(Tk_WindowId(infoPtr->tkwin)), &sPt);
                  }
                  number = sPt.y;*/
                number = y - wy;
                free_string = False;
                goto doNumber;
            default:
                numStorage[0] = before[1];
                numStorage[1] = '\0';
                string = numStorage;
                free_string = False;
                goto doString;
        }
        doNumber:
        sprintf(numStorage, "%d", number);
        string = numStorage;
                
        doString:
        spaceNeeded = Tcl_ScanElement(string, &cvtFlags);
        length = Tcl_DStringLength(dsPtr);
        Tcl_DStringSetLength(dsPtr, length + spaceNeeded);
        spaceNeeded = Tcl_ConvertElement(string,
                Tcl_DStringValue(dsPtr) + length,
                cvtFlags | TCL_DONT_USE_BRACES);
        Tcl_DStringSetLength(dsPtr, length + spaceNeeded);
        before += 2;
        if (free_string) Tcl_Free(string);
    }
} /* TkDND_ExpandPercents */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_ExecuteBinding --
 *
 *        This function will execute a drop binding script and will insert
 *        dropped data into the proper place in the script. It will arrange 
 *        so as the binary data to be passed as a byte-array object to tcl.
 *
 * Results:
 *        The status of the execution...
 *
 * Side effects:
 *        What ever the script does.
 *
 *--------------------------------------------------------------
 */
int TkDND_ExecuteBinding(Tcl_Interp *interp, char *script, int numBytes,
                         Tcl_Obj *objPtr)
{
    Tcl_DString ds;
    char *start, *data_insert;
    int status;

    /*
     * Getting a NULL interp is for sure a bug in tkdnd, but at least
     * avoid the crash for now :-)
     * TODO: Find out why we get sometimes a NULL interp...
     */
    if (interp == NULL) return TCL_ERROR;

    /*
     * Just a simple check. If %D is not in the script, just execute the
     * binding...don't try to execute
     */
    data_insert = strstr(script, "%D");
    if (data_insert == NULL) {
        return Tcl_EvalEx(interp, script, numBytes, TCL_EVAL_GLOBAL);
    } else {
        /*
         * Actually, here we have a small problem, if the objPtr holds
         * something in binary. Actually, this may be a bug of the Img
         * extension. We have somehow to convert the objPtr we have into an
         * object holding binary data.
         */
        start = script;
        Tcl_DStringInit(&ds);
        if (objPtr) {
            (void) Tcl_GetByteArrayFromObj(objPtr, NULL);
        }
        while (data_insert != NULL) {
            Tcl_DStringAppend(&ds, start, data_insert-start);
            if (objPtr == NULL) {
                Tcl_DStringAppend(&ds, "{}", 2);
            } else {
#if 1
                Tcl_DStringAppendElement(&ds, Tcl_GetString(objPtr));
#else
                /*
                 * This solutions was proposed by Paul Duffin.  We just add a
                 * layer with "binary scan" to convert string to byte-array.
                 */
                Tcl_DStringAppend(&ds, "[::dnd::ConvertToBinary ", 24);
                Tcl_DStringAppendElement(&ds, Tcl_GetString(objPtr));
                Tcl_DStringAppend(&ds, "]", 1);
#endif
            }
            start = data_insert + 2;
            data_insert = strstr(start, "%D");
        }
        if (*start != '\0') Tcl_DStringAppend(&ds, start, -1);
        status = Tcl_EvalEx(interp, Tcl_DStringValue(&ds),
                Tcl_DStringLength(&ds), TCL_EVAL_GLOBAL);
        Tcl_DStringFree(&ds);
        return status;
    }
    return TCL_OK;
} /* TkDND_ExecuteBinding */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_GetDataAccordingToType --
 *
 *        This function will retrieve the data that the <Drop> binding script 
 *        has placed into the given object. This will be done in a "clever"
 *        way if the type is one that we know. On unkown types, the data will
 *        be handled as binary data...
 *
 * Results:
 *        The data in a newly allocated buffer. It must be freed with Tcl_Free
 *        when we are done with it...
 *
 * Side effects:
 *        -
 *
 *--------------------------------------------------------------
 */
char *TkDND_GetDataAccordingToType(DndInfo *info, Tcl_Obj *ResultObj,
        DndType *typePtr, int *length)
{
    Tcl_DString ds;
    char *data, *result, *type = typePtr->typeStr;

    /*
     * If the current DndType structure holds a "generic" type (i.e. a type
     * with wildcards or one of the cross-platform ones, Files, Text, Image),
     * we want to normalise this type into something meaningfull...
     */
    if (strchr(type, '*')     != NULL || strcmp(type, "Text")  == 0 ||
        strcmp(type, "Files") == 0    || strcmp(type, "Image") == 0) {
      type = TkDND_TypeToString(typePtr->matchedType);
    }
#if 0
    printf("TkDND_GetDataAccordingToType: Type=\"%s\"\n", type);
#endif

    /*
     * Search the type for a known one...
     */
    Tcl_DStringInit(&ds);
    if (strcmp(type, "text/plain;charset=UTF-8") == 0 ||   /* XDND */
            strcmp(type, "CF_UNICODETEXT") == 0 ) {  /* Windows */
        /* ###################################################################
         * # text/plain;charset=UTF-8
         * ###################################################################
         */
        /*
         * We will transfer a string in "UTF-8"...
         */
        data = Tcl_GetString(ResultObj);
        *length = strlen(data);
#ifdef __WIN32__
        {
            LPWSTR buffer;
            int nWCharNeeded;
            /*
             * We will transfer a string in Unicode...
             */

            /*
             * Tcl fails to convert our code to Unicode. Who knows why...
             */
#if 0
            data = (char *) Tcl_WinUtfToTChar(data, -1, &ds);
            data = (char *) Tcl_UtfToUniCharDString(data, -1, &ds);
            *length = Tcl_DStringLength(&ds);
#endif

            /*
             * Use directly windows functions to convert to Unicode from
             * UTF-8...
             */
            /* Query the number of WChar required to store the Dest string */
            nWCharNeeded = MultiByteToWideChar(CP_UTF8, 0,
                    (LPCSTR) data, -1, NULL, 0);
            /* Allocate the required amount of space */
            /* We need more 2 bytes for '\0' */
            buffer = (LPWSTR) Tcl_Alloc(sizeof(char)*(nWCharNeeded+1)*2);
            /* Do the conversion */
            nWCharNeeded = MultiByteToWideChar(CP_UTF8, 0,
                    (LPCSTR) data, -1, (LPWSTR) buffer, nWCharNeeded);
            *(LPWSTR)((LPWSTR)buffer + nWCharNeeded) = L'\0';
            /* MultiByteToWideChar returns # WCHAR, so multiply by 2 */
            *length = 2*nWCharNeeded; 
            Tcl_DStringFree(&ds);
            return (char *) buffer;
        }
#endif /* __WIN32__ */
    } else if (strcmp(type,  "CF_OEMTEXT")    == 0 ) {  /* Windows */
        /*********************************************************************
         * # CF_OEMTEXT (Windows specific format)...
         *********************************************************************
         */
        data = Tcl_GetString(ResultObj);
        data = Tcl_UtfToExternalDString(NULL, data, -1, &ds);
        /* Convert data to the system encoding... */
#ifdef __WIN32__
        /*
         * We have to convert data to OEM code page...
         */
        CharToOem((LPCTSTR) data, (LPSTR ) Tcl_DStringValue(&ds));
        data = Tcl_DStringValue(&ds);
#endif /* __WIN32__ */
        *length = strlen(data);
    } else if (strcmp(type,  "text/plain")    == 0 ||      /* XDND */
            strcmp(type,  "text/uri-list") == 0 ||      /* XDND */
            strcmp(type,  "text/file")     == 0 ||      /* XDND */
            strcmp(type,  "url/url")       == 0 ||      /* XDND */
            strcmp(type,  "STRING")        == 0 ||      /* Motif */
            strcmp(type,  "TEXT")          == 0 ||      /* Motif */
            strcmp(type,  "XA_STRING")     == 0 ||      /* Motif */
            strcmp(type,  "FILE_NAME")     == 0 ||      /* Motif */
            strcmp(type,  "CF_TEXT")       == 0 ||      /* Windows */
            strcmp(type,  "CF_HDROP")      == 0 ||      /* Windows */
            strncmp(type, "text/", 5)      == 0 ) {     /* Who knows :-) */
        /**********************************************************************
         * # text/plain and other ASCII textual formats...
         **********************************************************************
         */
        data = Tcl_GetString(ResultObj);
        /* Convert data to the system encoding... */
        data = Tcl_UtfToExternalDString(NULL, data, -1, &ds);
        *length = Tcl_DStringLength(&ds);
    } else if (strcmp(type, "Images")    == 0 ||         /* Cross Platform */
            strcmp(type, "CF_DIB")    == 0   ) {      /* Windows */
        /**********************************************************************
         * # Images...
         **********************************************************************
         */
        Tk_PhotoHandle tkImage;

        /*
         * Here, we mainly expect the name of a Tk image...
         */
        tkImage = Tk_FindPhoto(info->interp, Tcl_GetString(ResultObj));
        if (tkImage == NULL) {
            /* TODO: expect raw image data? What format? Img package loaded?
             * How to convert? :-) */
            data = NULL; *length = 0;
        }
        TkDND_GetDataFromImage(info, Tcl_GetString(ResultObj), type,
                &result, length);
        Tcl_DStringFree(&ds);
        return result;
    } else {
        /**********************************************************************
         * # Anything else (Unhandled)...
         **********************************************************************
         */
        data = (char *) Tcl_GetByteArrayFromObj(ResultObj, length);
    }
    result = (char *) Tcl_Alloc(sizeof(char) * (*length)+2);
    if (result == NULL) {
        Tcl_DStringFree(&ds);
        *length = 0;
        return NULL;
    }
    memcpy(result, data, (*length)+2);
    XDND_DEBUG3("Final: \"%s\" (%d)\n", result, *length);
    /* Laurent, 9/7/2000 */
    result[*length] = '\0';
    Tcl_DStringFree(&ds);
    return result;
} /* TkDND_GetDataAccordingToType */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_CreateDataObjAccordingToType --
 *
 *        This function will create a proper Tcl object from the data.
 *        This will be done in a "clever" way if the type is one that we know.
 *        On unkown types, the data will be placed on a byte array, just to be
 *        sure...
 *        Note that in parameter info we may have placed needed information in
 *        order to properly translate the data. For example, for textual data
 *        the encoding will be stored... (petasis, 03/01/2001)
 *
 * Results:
 *        The data in a newly allocated Object. The reference count of the
 *        object will be 0. The caller should be free the object when done
 *        with it...
 *
 * Side effects:
 *        -
 *
 *--------------------------------------------------------------
 */
Tcl_Obj *TkDND_CreateDataObjAccordingToType(DndType *typePtr, void *info,
        unsigned char *data, int length)
{
    Tcl_Encoding encoding;
    Tcl_DString ds;
    Tcl_Obj *result;
    char *conv_data, *element_start, *type = typePtr->typeStr;
    int i, end, element_number = 0;
  
    /*
     * If the current DndType structure holds a "generic" type (i.e. a type
     * with wildcards or one of the cross-platform ones, Files, Text, Image),
     * we want to normalise this type into something meaningfull...
     */
    if (strchr(type, '*')     != NULL || strcmp(type, "Text")  == 0 ||
        strcmp(type, "Files") == 0    || strcmp(type, "Image") == 0) {
      type = TkDND_TypeToString(typePtr->matchedType);
    }

#if 0
    /*
     * Print the received data. As it is very hard to find information about
     * how a specific application organises a list of data, printing it may help
     * us to gather such information...
     */
    printf("TkDND_CreateDataObjAccordingToType: Type=\"%s\" length=%d:",
           type, length);
    for (i=0; i<length; i++) {
      switch (data[i]) {
        case '\0': {printf("\\0"); break;}
        case '\a': {printf("\\a"); break;}
        case '\b': {printf("\\b"); break;}
        case '\f': {printf("\\f"); break;}
        case '\n': {printf("\\n"); break;}
        case '\r': {printf("\\r"); break;}
        case '\t': {printf("\\t"); break;}
        case '\v': {printf("\\v"); break;}
        default:   {printf("%c", data[i]); break;}
      }
    }
    printf("\"\n");
#endif

    /*
     * Search the type for a known one...
     */
    Tcl_DStringInit(&ds);
    if (strcmp(type, "text/plain;charset=UTF-8") == 0 ||   /* XDND */
        strcmp(type, "CF_UNICODETEXT")           == 0 ) {  /* Windows */
#ifdef __WIN32__
        /*
         * Under Windows we actually use Unicode...
         */
        data = (unsigned char *) 
            Tcl_UniCharToUtfDString((unsigned short *) data, length, &ds);
        length = Tcl_DStringLength(&ds);
#endif /* __WIN32__ */
        /*
         * We will transfer a string in "UTF-8"...
         */
        result = Tcl_NewStringObj((char *) data, length);
    } else if (strcmp(type,  "text/uri-list") == 0 ||     /* XDND */
               strcmp(type,  "text/file")     == 0 ||     /* XDND */
               strcmp(type,  "FILE_NAME")     == 0 ||     /* XDND */
               strcmp(type,  "url/url")       == 0 ||     /* Motif */
               strcmp(type,  "CF_HDROP")      == 0   ) {  /* Windows */
        /*
         * If multiple files are dropped, the file names will be seperated with
         * "\r\n" if XDND is used or "\0" if Motif is used.
         * We have to create a proper tcl list from this...
         */
        result = Tcl_NewListObj(0, NULL);

        /*
         * Convert the data from specified locale to utf-8.
         */
        if (info != NULL) {
            encoding = Tcl_GetEncoding(NULL, (char *) info);
        } else {
            encoding = NULL;
        }
        /* Convert data to the specified encoding... */
        conv_data = Tcl_ExternalToUtfDString(encoding, (char *) data,
                length, &ds);
        if (encoding) Tcl_FreeEncoding(encoding);

        end = Tcl_DStringLength(&ds);
        for (i = 0, element_start = conv_data; i < end; i++) {
            if (conv_data[i] == '\r' && conv_data[i+1] == '\n') {
                /*
                 * We are using XDND...
                 */
                if (element_start != &conv_data[i]) {
                    /*
                     * Avoid adding an empty list element...
                     */
                    Tcl_ListObjAppendElement(NULL, result,
                            Tcl_NewStringObj(element_start,
                                    &conv_data[i]-element_start));
                }
                i += 1; element_number++;
                element_start = &conv_data[i+1];
            } else if ((unsigned char) conv_data[i]   == 192 &&
                       (unsigned char) conv_data[i+1] == 128) {
                /*
                 * We are using Motif...
                 */
                if (element_start != &conv_data[i]) {
                    /*
                     * Avoid adding an empty list element...
                     */
                    Tcl_ListObjAppendElement(NULL, result,
                            Tcl_NewStringObj(element_start,
                                    &conv_data[i]-element_start));
                }
                i += 2; element_number++;
                element_start = &conv_data[i];
            } else if (conv_data[i] == '\n') {
                /*
                 * We are using Motif from Star Office...
                 */
                if (element_start != &conv_data[i]) {
                    /*
                     * Avoid adding an empty list element...
                     */
                    Tcl_ListObjAppendElement(NULL, result,
                            Tcl_NewStringObj(element_start,
                                    &conv_data[i]-element_start));
                }
                element_number++;
                element_start = &conv_data[i+1];
            } else if (conv_data[i] == '\0') {
                /*
                 * We have found a NULL. Stop here...
                 */
                break;
            }
            XDND_DEBUG5("i=%d(%d), (%c) %d\n", i, end, conv_data[i],
                    (char) conv_data[i]);
        }
        if (!element_number) {
            /*
             * If we have added no elements, only a single file was dropped.
             * We will add as a single element all the data up to the first
             * NULL. Its better to not use strlen to get the string length, as
             * the string may not be NULL terminated. Its better to do it with
             * a simple loop...
             */
            for (i=0; i<=end, conv_data[i] != '\0'; i++) ;
            Tcl_SetStringObj(result, conv_data, i);
        } else {
            /* We have added many elements, so we have to add the last one. */
#if 0
            if (conv_data[i] != '\0' && *element_start != '\n' ) {
                Tcl_ListObjAppendElement(NULL, result,
                        Tcl_NewStringObj(element_start, -1));
            }
#endif
        }
    } else if (strcmp(type,  "text/plain")    == 0 ||     /* XDND */
               strcmp(type,  "STRING")        == 0 ||     /* Motif */
               strcmp(type,  "TEXT")          == 0 ||     /* Motif */
               strcmp(type,  "XA_STRING")     == 0 ||     /* Motif */
               strcmp(type,  "CF_OEMTEXT")    == 0 ||     /* Windows */
               strcmp(type,  "CF_TEXT")       == 0 ||     /* Windows */
               strncmp(type, "text/", 5)      == 0   ) {
        /*
         * Convert the data from specified locale to utf-8.
         */
        if (info != NULL) {
            encoding = Tcl_GetEncoding(NULL, (char *) info);
        } else {
            encoding = NULL;
        }
        /* Convert data to the specified encoding... */
        conv_data = Tcl_ExternalToUtfDString(encoding, (char *) data,
                length, &ds);
        if (encoding) Tcl_FreeEncoding(encoding);
        result = Tcl_NewStringObj(conv_data, -1);
    } else {
        result = Tcl_NewByteArrayObj(data, length);
    }
    Tcl_DStringFree(&ds);
    return result;
} /* TkDND_CreateDataObjAccordingToType */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_Update --
 *
 *        This function immitates the "update" Tk command... 
 *
 * Results:
 *        Always TCL_OK
 *
 * Side effects:
 *        What ever "update" does :-)
 *
 *--------------------------------------------------------------
 */
int TkDND_Update(Display *display, int idle)
{
    int flags;
  
    if (idle) flags = TCL_IDLE_EVENTS;
    else      flags = TCL_DONT_WAIT;

    while (1) {
        while (Tcl_DoOneEvent(flags) != 0) {
            /* Empty loop body */
        }
#ifndef __WIN32__
        XSync(display, False);
#endif /* __WIN32__ */
        if (Tcl_DoOneEvent(flags) == 0) {
            break;
        }
    }
    return TCL_OK;
} /* TkDND_Update */

/* 
 * End of tkDND.c
 */
