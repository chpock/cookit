/*
 * tkOleDND.cpp --
 * 
 *    This file implements the windows portion of the drag&drop mechanish
 *    for the tk toolkit. The protocol in use under windows is the
 *    OLE protocol. Based on code wrote by Gordon Chafee.
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

#include <tcl.h>
#include <tk.h>

#include "tkDND.h"
#include "OleDND.h"

#ifndef CF_DIBV5	/* this is only in the latest Windows headers */
#define CF_DIBV5 17
#endif


extern Tcl_HashTable   TkDND_TargetTable;
extern Tcl_HashTable   TkDND_SourceTable;
#ifdef DND_DEBUG
FILE                  *TkDND_Log;
#endif
DndClass              *dnd;

extern int   TkDND_DelHandler(DndInfo *infoPtr, char *typeStr, char *eventStr,
                    unsigned long eventType, unsigned long eventMask);
extern int   TkDND_DelHandlerByName(Tcl_Interp *interp, Tk_Window topwin,
                    Tcl_HashTable *table, char *windowPath, char *typeStr,
                    unsigned long eventType, unsigned long eventMask);
extern void  TkDND_DestroyEventProc(ClientData clientData, XEvent *eventPtr);
extern int   TkDND_FindMatchingScript(Tk_Window tkwin, Tcl_HashTable *table,
                    char *windowPath, char *typeStr, CLIPFORMAT type,
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
extern char *TkDND_GetDataAccordingToType(DndInfo *info, 
                    Tcl_Obj *ResultObj, DndType *typePtr, int *length);  
extern Tcl_Obj 
            *TkDND_CreateDataObjAccordingToType(char *type,
                    unsigned char *data, int length);
extern int   TkDND_Update(Display *display, int idle);

#ifdef TKDND_ENABLE_SHAPE_EXTENSION
int          Shape_Init(Tcl_Interp *interp);
#endif /* TKDND_ENABLE_SHAPE_EXTENSION */

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
  static char typeName[256];

  strcpy(typeName, "");
  switch (type) {
    case CF_TEXT           : {strcpy(typeName, "CF_TEXT");            break;}
    case CF_BITMAP         : {strcpy(typeName, "CF_BITMAP");          break;}
    case CF_METAFILEPICT   : {strcpy(typeName, "CF_METAFILEPICT");    break;}
    case CF_SYLK           : {strcpy(typeName, "CF_SYLK");            break;}
    case CF_DIF            : {strcpy(typeName, "CF_DIF");             break;}
    case CF_TIFF           : {strcpy(typeName, "CF_TIFF");            break;}
    case CF_OEMTEXT        : {strcpy(typeName, "CF_OEMTEXT");         break;}
    case CF_DIB            : {strcpy(typeName, "CF_DIB");             break;}
    case CF_PALETTE        : {strcpy(typeName, "CF_PALETTE");         break;}
    case CF_PENDATA        : {strcpy(typeName, "CF_PENDATA");         break;}
    case CF_RIFF           : {strcpy(typeName, "CF_RIFF");            break;}
    case CF_WAVE           : {strcpy(typeName, "CF_WAVE");            break;}
    case CF_UNICODETEXT    : {strcpy(typeName, "CF_UNICODETEXT");     break;}
    case CF_ENHMETAFILE    : {strcpy(typeName, "CF_ENHMETAFILE");     break;}
    case CF_HDROP          : {strcpy(typeName, "CF_HDROP");           break;}
    case CF_LOCALE         : {strcpy(typeName, "CF_LOCALE");          break;}
    case CF_DIBV5          : {strcpy(typeName, "CF_DIBV5");           break;}
    case CF_OWNERDISPLAY   : {strcpy(typeName, "CF_OWNERDISPLAY");    break;}
    case CF_DSPTEXT        : {strcpy(typeName, "CF_DSPTEXT");         break;}
    case CF_DSPBITMAP      : {strcpy(typeName, "CF_DSPBITMAP");       break;}
    case CF_DSPMETAFILEPICT: {strcpy(typeName, "CF_DSPMETAFILEPICT"); break;}
    case CF_DSPENHMETAFILE : {strcpy(typeName, "CF_DSPENHMETAFILE");  break;}
    default: {
      if (((CLIPFORMAT) type) == dnd->UniformResourceLocator) {
        strcpy(typeName, "UniformResourceLocator");
      } else if (((CLIPFORMAT) type) == dnd->FileName) {
        strcpy(typeName, "FileName");
      } else if (((CLIPFORMAT) type) == dnd->HTML_Format) {
        strcpy(typeName, "HTML Format");
      } else if (((CLIPFORMAT) type) == dnd->RichTextFormat) {
        strcpy(typeName, "Rich Text Format");
      } else GetClipboardFormatName((CLIPFORMAT) type, typeName, 256);
    }
  }
  return &typeName[0];
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
CLIPFORMAT TkDND_StringToType(char *typeStr) {
  if      (strcmp(typeStr, "CF_TEXT")    == 0)      return CF_TEXT;
  else if (strcmp(typeStr, "CF_OEMTEXT") == 0)      return CF_OEMTEXT;
  else if (strcmp(typeStr, "CF_UNICODETEXT") == 0)  return CF_UNICODETEXT;
  else if (strcmp(typeStr, "CF_HDROP")   == 0)      return CF_HDROP;
  else if (strcmp(typeStr, "CF_DIB")     == 0)      return CF_DIB;
  else if (strcmp(typeStr, "CF_BITMAP")  == 0)      return CF_BITMAP;
  else if (strcmp(typeStr, "CF_METAFILEPICT") == 0) return CF_METAFILEPICT;
  else if (strcmp(typeStr, "CF_SYLK")    == 0)      return CF_SYLK;
  else if (strcmp(typeStr, "CF_TIFF")    == 0)      return CF_TIFF;
  else if (strcmp(typeStr, "CF_PALETTE") == 0)      return CF_PALETTE;
  else if (strcmp(typeStr, "CF_PENDATA") == 0)      return CF_PENDATA;
  else if (strcmp(typeStr, "CF_RIFF")    == 0)      return CF_RIFF;
  else if (strcmp(typeStr, "CF_WAVE")    == 0)      return CF_WAVE;
  else if (strcmp(typeStr, "CF_ENHMETAFILE") == 0)  return CF_ENHMETAFILE ;
  else if (strcmp(typeStr, "CF_HDROP")   == 0)      return CF_HDROP;
  else if (strcmp(typeStr, "CF_LOCALE")  == 0)      return CF_LOCALE;
  else if (strcmp(typeStr, "CF_DIBV5")   == 0)      return CF_DIBV5;
  else if (strcmp(typeStr, "CF_OWNERDISPLAY") == 0) return CF_OWNERDISPLAY;
  else if (strcmp(typeStr, "CF_DSPTEXT") == 0)      return CF_DSPTEXT;
  else if (strcmp(typeStr, "CF_DSPBITMAP") == 0)    return CF_DSPBITMAP;
  else if (strcmp(typeStr, "CF_DSPMETAFILEPICT")== 0) return CF_DSPMETAFILEPICT;
  else if (strcmp(typeStr, "CF_DSPENHMETAFILE") == 0) return CF_DSPENHMETAFILE;
  else return RegisterClipboardFormat(typeStr);
  return 0;
} /* TkDND_StringToType */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_Init --
 *
 *       Initialize the drag and drop platform specific protocol.
 *       Under windows, this is the OLE protocol.
 *
 * Results:
 *       A structure that holds platform specific information.
 *
 * Side effects:
 *       -
 *
 *----------------------------------------------------------------------
 */
void *TkDND_Init(Tcl_Interp *interp, Tk_Window topwin) {
  HRESULT hret;

#ifdef DND_DEBUG
  Tcl_Eval(interp, "file mkdir C:/Temp");
  Tcl_Eval(interp, "file native C:/Temp/tkdndLog_[pid].txt");
  TkDND_Log = fopen(Tcl_GetString(Tcl_GetObjResult(interp)), "w");
  XDND_DEBUG("\n-=-=-=-=-=-=-=-=-=-=-=-=-= Log opened =-=-=-=-=-=-=-=-=-=-=\n");
#endif

  /*
   * Initialise OLE
   */
  hret = OleInitialize(NULL);
  
  /*
   * If OleInitialize returns S_FALSE, OLE has already been initialized
   */
  if (hret != S_OK && hret != S_FALSE) {
    Tcl_AppendResult(interp, "unable to initialize OLE2",
      (char *) NULL);
    return NULL;
  }
  
  dnd             = (DndClass *) Tcl_Alloc(sizeof(DndClass));
  dnd->MainWindow = topwin;
  dnd->data       = NULL;
  dnd->length     = -1;
  Tcl_DStringInit(&(dnd->DraggerTypes));
  dnd->UniformResourceLocator = 
        RegisterClipboardFormat("UniformResourceLocator");
  dnd->FileName               = RegisterClipboardFormat("FileName");
  dnd->HTML_Format            = RegisterClipboardFormat("HTML Format");
  dnd->RichTextFormat         = RegisterClipboardFormat("Rich Text Format");
  /* The correct type to use is CFSTR_FILEDESCRIPTOR, but this depends on how
     the code gets compiled. So, we use directly the individual names... */
  dnd->FileGroupDescriptor    = RegisterClipboardFormat("FileGroupDescriptor");
  dnd->FileGroupDescriptorW   = RegisterClipboardFormat("FileGroupDescriptorW");

#ifdef TKDND_ENABLE_SHAPE_EXTENSION
  /* Load the shape extension... */
  if (Shape_Init(interp) != TCL_OK) {
    /* TODO: Provide the shape command, even if does not work... */
  }
#endif /* TKDND_ENABLE_SHAPE_EXTENSION */
  return dnd;
} /* TkDND_Init */

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
  Tk_Window tkwin;
  DndType *head, *prev, *curr, *tnew;
#define TkDND_ACTUAL_TYPE_NU 10
  int ActualType[TkDND_ACTUAL_TYPE_NU];
  int created, len, i, removed = False;
  UINT atom;
  HRESULT hret;

  tkwin = Tk_NameToWindow(interp, windowPath, topwin);
  if (tkwin == NULL) return TCL_ERROR;
  Tk_MakeWindowExist(tkwin);

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
          && curr->eventType == eventType && curr->eventMask == eventMask) {
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
   * register more than one type for each "platform independed" type requested
   * by the user...
   */
  i = 0;
  if (strcmp(typeStr, "text/plain;charset=UTF-8") == 0) {
    /*
     * Here we handle our platform independed way to transer UTF strings...
     */
    ActualType[i++] = CF_UNICODETEXT;             /* OLE DND Protocol */
    ActualType[i++] = NULL;
  } else if (strcmp(typeStr, "text/plain") == 0) {
    /*
     * Here we handle our platform independed way to transer ASCII strings...
     */
    ActualType[i++] = CF_TEXT;                    /* OLE DND Protocol */
    ActualType[i++] = CF_OEMTEXT;                 /* OLE DND Protocol */
    ActualType[i++] = NULL;
  } else if (strcmp(typeStr, "text/uri-list") == 0 ||
             strcmp(typeStr, "Files")         == 0) {
    /*
     * Here we handle our platform independed way to transer File Names...
     */
    ActualType[i++] = CF_HDROP;                   /* OLE DND  Protocol */
    ActualType[i++] = NULL;
  } else if (strcmp(typeStr, "Text") == 0) {
    /*
     * Here we handle our platform independed way to all supported text
     * formats...
     */
    ActualType[i++] = CF_UNICODETEXT;             /* OLE DND Protocol */
    ActualType[i++] = CF_OEMTEXT;                 /* OLE DND Protocol */
    ActualType[i++] = CF_TEXT;                    /* OLE DND Protocol */
    ActualType[i++] = NULL;
  } else if (strcmp(typeStr, "FilesWithContent") == 0) {
    ActualType[i++] = dnd->FileGroupDescriptorW;  /* OLE DND Protocol */
    ActualType[i++] = dnd->FileGroupDescriptor;   /* OLE DND Protocol */
    ActualType[i++] = NULL;
  } else if (strcmp(typeStr, "Image") == 0) {
    /*
     * Here we handle our platform independed way to all supported text
     * formats...
     */
    ActualType[i++] = CF_DIB;                   /* OLE DND Protocol */
    //ActualType[i++] = CF_BITMAP;                /* OLE DND Protocol */
    //ActualType[i++] = CF_TIFF;                  /* OLE DND Protocol */
    //ActualType[i++] = CF_METAFILEPICT;          /* OLE DND Protocol */
    ActualType[i++] = NULL;
  } else {
    /*
     * This is a platform specific type. Is it a known type?
     */
    if   (strchr(typeStr, '*') != NULL) ActualType[i++] = -1;
    else ActualType[i++] = TkDND_StringToType(typeStr);
    ActualType[i++] = NULL;
  }

  for (i=0; i<TkDND_ACTUAL_TYPE_NU && ActualType[i]!=NULL; i++) {
    tnew = (DndType *) Tcl_Alloc(sizeof(DndType));
    tnew->priority = priority;
    len = strlen(typeStr) + 1;
    tnew->matchedType = 0;
    tnew->typeStr = (char *) Tcl_Alloc(sizeof(char)*len);
    memcpy(tnew->typeStr, typeStr, len);
    tnew->eventType = eventType;
    tnew->eventMask = eventMask;
    len = strlen(script) + 1;
    tnew->script = (char *) Tcl_Alloc(sizeof(char)*len);
    memcpy(tnew->script, script, len);
    tnew->next = NULL;
    tnew->EnterEventSent = 0;
    if (ActualType[i] < 0) {
      atom = 0;
    } else {
      atom = (UINT) ActualType[i];
      if (atom == 0) {
        Tcl_AppendResult(interp, "unable to register clipboard format ", 
                         typeStr, (char *) NULL);
        return TCL_ERROR;
      }
    }
    tnew->type = atom;
    XDND_DEBUG5("<add handler> type='%-16s',%5d / event=%2d / mask=%08x\n",
                typeStr, atom, eventType, eventMask);

    if (created) {
      infoPtr = (DndInfo *) Tcl_Alloc(sizeof(DndInfo));
      infoPtr->head.next = NULL;
      infoPtr->interp = interp;
      infoPtr->tkwin = tkwin;
      infoPtr->hashEntry = hPtr;
      if (isDropTarget) {
        infoPtr->DropTarget = new TkDND_DropTarget(infoPtr);
        infoPtr->DropTarget->AddRef();
        hret = RegisterDragDrop(Tk_GetHWND(Tk_WindowId(tkwin)),
                                infoPtr->DropTarget);
        if (hret != S_OK) {
          infoPtr->DropTarget->Release();
          Tcl_Free((char *) infoPtr);
          Tcl_AppendResult(interp, "unable to register \"", windowPath,
                           "\" as a droppable window", (char *) NULL);
          Tcl_DeleteHashEntry(hPtr);
          return TCL_ERROR;
        }
      } else {
        infoPtr->DropTarget = NULL;
      }
      Tk_CreateEventHandler(tkwin, StructureNotifyMask,
                            TkDND_DestroyEventProc, (ClientData) infoPtr);
      Tcl_SetHashValue(hPtr, infoPtr);
      infoPtr->head.next = tnew;
      created = False;
    } else {
      infoPtr = (DndInfo *) Tcl_GetHashValue(hPtr);
      infoPtr->tkwin = tkwin;
      head = prev = &infoPtr->head;
      for (curr = head->next; curr != NULL; prev = curr, curr = curr->next) {
        if (curr->priority > priority)  break;
      }
      tnew->next = prev->next;
      prev->next = tnew;
    }
  } /* for (i=0; i<TkDND_ACTUAL_TYPE_NU && ActualType[i]!=NULL; i++) */
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
  return dnd->DesiredTypeStr;
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
  str = Tcl_Alloc(sizeof(char) * (strlen(tmp)+1));
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
  return dnd->DesiredAction;
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

  Tcl_DStringInit(&ds);
  if (GetKeyState(VK_SHIFT)   & 0x8000) Tcl_DStringAppendElement(&ds,"Shift");
  if (GetKeyState(VK_CONTROL) & 0x8000) Tcl_DStringAppendElement(&ds,"Control");
  if (GetKeyState(VK_MENU)    & 0x8000) Tcl_DStringAppendElement(&ds,"Alt");
  str = Tcl_Alloc(sizeof(char) * (Tcl_DStringLength(&ds)+1));
  memcpy(str, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)+1);
  Tcl_DStringFree(&ds);
  return str;
} /* TkDND_GetCurrentModifiers */

/*
 *----------------------------------------------------------------------
 * TkDND_GetSourceActions --
 *        Returns a string that describes the supported by the source actions.
 *
 * Results:
 *        A dynamic string. Should be freed.
 *
 *----------------------------------------------------------------------
 */
char *TkDND_GetSourceActions(void) {
  Tcl_DString ds;
  char *str;

  Tcl_DStringInit(&ds);
  if (dnd->DraggerActions & DROPEFFECT_COPY) {
    Tcl_DStringAppendElement(&ds, "copy");
  } 
  if (dnd->DraggerActions & DROPEFFECT_MOVE) {
    Tcl_DStringAppendElement(&ds, "move");
  } 
  if (dnd->DraggerActions & DROPEFFECT_LINK) {
    Tcl_DStringAppendElement(&ds, "link");
  }
  str = Tcl_Alloc(sizeof(char) * (Tcl_DStringLength(&ds)+1));
  memcpy(str, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)+1);
  Tcl_DStringFree(&ds);
  return str;
} /* TkDND_GetSourceActions */

/*
 *----------------------------------------------------------------------
 * TkDND_GetSourceActionDescriptions --
 *        Returns a string that describes the supported by the source action
 *        descriptions.
 *
 * Results:
 *        A dynamic string. Should be freed.
 *
 * Notes:
 *        
 *----------------------------------------------------------------------
 */
char *TkDND_GetSourceActionDescriptions(void) {
  char *descriptions = NULL;
  Tcl_DString ds;
  char *str;

  Tcl_DStringInit(&ds);
  if        (dnd->DraggerActions & DROPEFFECT_COPY) {
    Tcl_DStringAppendElement(&ds, "copy");
  } else if (dnd->DraggerActions & DROPEFFECT_MOVE) {
    Tcl_DStringAppendElement(&ds, "move");
  } else if (dnd->DraggerActions & DROPEFFECT_LINK) {
    Tcl_DStringAppendElement(&ds, "link");
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
  char *str;
  str = Tcl_Alloc(sizeof(char) * (Tcl_DStringLength(&(dnd->DraggerTypes))+1));
  memcpy(str, Tcl_DStringValue(&(dnd->DraggerTypes)),
              Tcl_DStringLength(&(dnd->DraggerTypes))+1);
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
  Tcl_Obj *listObj, **type;
  Tcl_DString ds;
  char tmp[64], *str, *list;
  int items, i;

  list = TkDND_GetSourceTypeList();
  listObj = Tcl_NewStringObj(list, -1);
  Tcl_Free(list);
  Tcl_DStringInit(&ds);
  
  Tcl_ListObjGetElements(NULL, listObj, &items, &type);
  for (i=0; i<items; i++) {
    sprintf(tmp, "0x%08x", TkDND_StringToType(Tcl_GetString(type[i])));
    Tcl_DStringAppendElement(&ds, tmp);
  }
  Tcl_DecrRefCount(listObj);
  str = Tcl_Alloc(sizeof(char) * (Tcl_DStringLength(&ds)+1));
  memcpy(str, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds)+1);
  Tcl_DStringFree(&ds);
  return str;
} /* TkDND_GetSourceTypeCodeList */

/*
 *----------------------------------------------------------------------
 * DndDrag --
 *        Begins an OLE drag and returns when the drop finishes or the
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
  LPDROPSOURCE      DropSource;
  TkDND_DataObject *DataObject;
  HRESULT           hr;
  DWORD             actions = 0;
  DWORD             FinalAction;
  Tcl_HashEntry    *hPtr;
  Tcl_Obj         **elem;
  DndInfo          *infoPtr;
  char             *action;
  int               elem_nu, i;
  
  /*
   * Is the window a drag source?
   */
  hPtr = Tcl_FindHashEntry(&TkDND_SourceTable, windowPath);
  if (hPtr == NULL) {
    Tcl_AppendResult(interp, "unable to begin drag operation: ",
                     "no source types have been bound to ", windowPath,
                     (char *) NULL);
    return TCL_ERROR;
  }

  /*
   * Update the button slot... (on unix is not important, but here is, as we
   * request a drop action if this button is released. If the user has
   * specified a button, this value is used. Else, this defaults to the left
   * mouse button ("1")...
   */
  if (button) {
    dnd->button = button;
  } else {
    /*
     * Get the current pressed button...
     */
    if (GetKeyState(VK_LBUTTON) & 0x8000) dnd->button = 1;
    if (GetKeyState(VK_MBUTTON) & 0x8000) dnd->button = 2;
    if (GetKeyState(VK_RBUTTON) & 0x8000) dnd->button = 3;
  }

  /*
   * Create the Data object, also required by the OLE protocol...
   */
  infoPtr = (DndInfo *) Tcl_GetHashValue(hPtr);
  DataObject = new TkDND_DataObject(infoPtr);
  if (DataObject == NULL) {
    Tcl_AppendResult(interp, "unable to create OLE Data object", (char *) NULL);
    return TCL_ERROR;
  }
  DataObject->AddRef();

  /*
   * Create the Drag Source object, required by the OLE protocol...
   */
  DropSource = new TkDND_DropSource(infoPtr);
  if (DropSource == NULL) {
    Tcl_AppendResult(interp, "unable to create OLE Drop Source object",
                     (char *) NULL);
    return TCL_ERROR;
  }
  DropSource->AddRef();
  
  /*
   * Handle User-requested Actions...
   */
  if (Actions == NULL) {
    /* The User has not specified any actions: defaults to copy... */
    actions |= DROPEFFECT_COPY;
    memset(Descriptions, '\0', TKDND_MAX_DESCRIPTIONS_LENGTH);
    strcpy(Descriptions, "Copy\0\0");
    strcpy(dnd->DesiredAction, "copy");
  } else {
    Tcl_ListObjGetElements(interp, Actions, &elem_nu, &elem);
    strcpy(dnd->DesiredAction, Tcl_GetString(elem[0]));
    for (i = 0; i < elem_nu; i++) {
      action = Tcl_GetString(elem[i]);
      if      (strcmp(action,"copy")==0) actions |= DROPEFFECT_COPY;
      else if (strcmp(action,"move")==0) actions |= DROPEFFECT_MOVE;
      else if (strcmp(action,"link")==0) actions |= DROPEFFECT_LINK;
      else if (strcmp(action,"ask" )==0) {/* "ask"     not supported */}
      else                               {/* "private" not supported */}
    }
  }

  /*
   * Finally, do the drag & drop operation...
   */
  dnd->interp         = interp;
  dnd->x              = 0;
  dnd->y              = 0;
  dnd->CursorWindow   = cursor_window;
  dnd->CursorCallback = cursor_callback;
  dnd->DraggerWindow  = infoPtr->tkwin;
  dnd->DesiredType    = infoPtr->head.next->type;
  dnd->DesiredTypeStr = infoPtr->head.next->typeStr;
  dnd->CallbackStatus = TCL_OK;
  if (dnd->length != -1 || dnd->data != NULL) {
    if (dnd->data != NULL) Tcl_DecrRefCount(dnd->data);
    dnd->data   = NULL;
    dnd->length = -1;
  }
  hr = DoDragDrop(DataObject, DropSource, actions, &FinalAction);

  /*
   * Free unwanted objects...
   */
  DropSource->Release();
  DataObject->Release();
  return TCL_OK;
} /* TkDND_DndDrag */

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
int TkDND_WidgetGetData(DndInfo *infoPtr, char **data, int *length,
                        CLIPFORMAT type) {
  Tcl_DString dString;
  DndType    *curr;
  int         ret;

  for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
    if (curr->type == type) {
      Tcl_DStringInit(&dString);
      TkDND_ExpandPercents(infoPtr, curr, curr->script, &dString,dnd->x,dnd->y);
      XDND_DEBUG2("<GetData>: %s\n", Tcl_DStringValue(&dString));
      ret = TkDND_ExecuteBinding(infoPtr->interp, Tcl_DStringValue(&dString),-1,
                                   NULL);
      Tcl_DStringFree(&dString);
      dnd->CallbackStatus = ret;
      if (ret == TCL_ERROR) {
        Tk_BackgroundError(infoPtr->interp);
      } else if (ret == TCL_BREAK) {
        *data = NULL; *length = 0;
        XDND_DEBUG("TkDND_WidgetGetData: Widget refused to send data "
                   "(returned TCL_BREAK)\n");
        return False;
      }
      if (curr->matchedType == 0) {
        curr->matchedType = type;
      }
      *data = TkDND_GetDataAccordingToType(infoPtr, 
                     Tcl_GetObjResult(infoPtr->interp), curr, length);
      curr->matchedType = 0;
      dnd->DesiredTypeStr = curr->typeStr;
      return True;
    }
  }
  return False;
} /* TkDND_WidgetGetData */

/*
 *----------------------------------------------------------------------
 *
 * TkDND_GetDataFromImage --
 *
 *       This function tries to retrieve from a Tk image handle a suitable
 *       platform specific string (possibly related to the specified type)
 *       that we can use in order to transfer images to other applications...
 *
 * Results:
 *       A boolean value, whether we have succeded or not.
 *
 * Side effects:
 *       This function must allocates a new buffer with Tcl_Alloc in order to
 *       place the image data. Also, length must be set to the data length...
 *
 *----------------------------------------------------------------------
 */
typedef union {
  int type;
  struct {int type; HWND handle; Tk_Window *winPtr;} window;
  struct {int type; HBITMAP handle; Colormap colormap; int depth;} bitmap;
  struct {int type; HDC hdc;} winDC;
} Dnd_TkWinDrawable;
HANDLE DibFromBitmap(HBITMAP hbm, DWORD biStyle, WORD biBits, HPALETTE hpal,
                     int *length);

int TkDND_GetDataFromImage(DndInfo *info, char *imageName,
                           char *type, char **result, int *length) {
  *result = NULL; *length = 0;
  if (strcmp(type, "Image") == 0 || strcmp(type, "CF_DIB") == 0) {
    /*
     * This function does nothing. The whole job is done by 
     * TkDND_DataObject::GetData in win/OleDND.cpp...
     */
    *length = strlen(imageName)+1;
    *result = Tcl_Alloc(*length*sizeof(char));
    if (*result != NULL) {
      strcpy(*result, imageName);
    }
    return 1;
  }
  return 0;
} /* TkDND_GetDataFromImage */

#define WIDTHBYTES(i)   ((i+31)/32*4)
/****************************************************************************
 *                                                                          *
 *  FUNCTION   : DibNumColors(VOID FAR * pv)                                *
 *                                                                          *
 *  PURPOSE    : Determines the number of colors in the DIB by looking at   *
 *               the BitCount filed in the info block.                      *
 *                                                                          *
 *  RETURNS    : The number of colors in the DIB.                           *
 *                                                                          *
 ****************************************************************************/
WORD DibNumColors (VOID FAR * pv)
{
    INT                 bits;
    LPBITMAPINFOHEADER  lpbi;
    LPBITMAPCOREHEADER  lpbc;

    lpbi = ((LPBITMAPINFOHEADER)pv);
    lpbc = ((LPBITMAPCOREHEADER)pv);

    /*  With the BITMAPINFO format headers, the size of the palette
     *  is in biClrUsed, whereas in the BITMAPCORE - style headers, it
     *  is dependent on the bits per pixel ( = 2 raised to the power of
     *  bits/pixel).
     */
    if (lpbi->biSize != sizeof(BITMAPCOREHEADER)){
        if (lpbi->biClrUsed != 0)
            return (WORD)lpbi->biClrUsed;
        bits = lpbi->biBitCount;
    }
    else
        bits = lpbc->bcBitCount;

    switch (bits){
        case 1:
                return 2;
        case 4:
                return 16;
        case 8:
                return 256;
        default:
                /* A 24 bitcount DIB has no color table */
                return 0;
    }
} /* DibNumColors */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   :  PaletteSize(VOID FAR * pv)                                *
 *                                                                          *
 *  PURPOSE    :  Calculates the palette size in bytes. If the info. block  *
 *                is of the BITMAPCOREHEADER type, the number of colors is  *
 *                multiplied by 3 to give the palette size, otherwise the   *
 *                number of colors is multiplied by 4.                                                          *
 *                                                                          *
 *  RETURNS    :  Palette size in number of bytes.                          *
 *                                                                          *
 ****************************************************************************/
WORD PaletteSize (VOID FAR * pv)
{
    LPBITMAPINFOHEADER lpbi;
    WORD               NumColors;

    lpbi      = (LPBITMAPINFOHEADER)pv;
    NumColors = DibNumColors(lpbi);

    if (lpbi->biSize == sizeof(BITMAPCOREHEADER))
        return (WORD)(NumColors * sizeof(RGBTRIPLE));
    else
        return (WORD)(NumColors * sizeof(RGBQUAD));
} /* PaletteSize */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : DibFromBitmap()                                            *
 *                                                                          *
 *  PURPOSE    : Will create a global memory block in DIB format that       *
 *               represents the Device-dependent bitmap (DDB) passed in.    *
 *                                                                          *
 *  RETURNS    : A handle to the DIB                                        *
 *                                                                          *
 ****************************************************************************/
HANDLE DibFromBitmap(HBITMAP hbm, DWORD biStyle, WORD biBits, HPALETTE hpal,
                     int *length) {
    BITMAP               bm;
    BITMAPINFOHEADER     bi;
    BITMAPINFOHEADER FAR *lpbi;
    DWORD                dwLen;
    HANDLE               hdib;
    HANDLE               h;
    HDC                  hdc;

    if (!hbm)
        return NULL;

    if (hpal == NULL)
        hpal = (HPALETTE) GetStockObject(DEFAULT_PALETTE);

    GetObject(hbm,sizeof(bm),(LPSTR)&bm);

    if (biBits == 0)
        biBits =  bm.bmPlanes * bm.bmBitsPixel;

    bi.biSize               = sizeof(BITMAPINFOHEADER);
    bi.biWidth              = bm.bmWidth;
    bi.biHeight             = bm.bmHeight;
    bi.biPlanes             = 1;
    bi.biBitCount           = biBits;
    bi.biCompression        = biStyle;
    bi.biSizeImage          = 0;
    bi.biXPelsPerMeter      = 0;
    bi.biYPelsPerMeter      = 0;
    bi.biClrUsed            = 0;
    bi.biClrImportant       = 0;

    dwLen  = bi.biSize + PaletteSize(&bi);

    hdc = GetDC(NULL);
    hpal = SelectPalette(hdc,hpal,FALSE);
         RealizePalette(hdc);

    hdib = GlobalAlloc(GHND,dwLen);

    if (!hdib) {
        SelectPalette(hdc,hpal,FALSE);
        ReleaseDC(NULL,hdc);
        return NULL;
    }

    lpbi = (BITMAPINFOHEADER FAR *) GlobalLock(hdib);

    *lpbi = bi;

    /*  call GetDIBits with a NULL lpBits param, so it will calculate the
     *  biSizeImage field for us
     */
    GetDIBits(hdc, hbm, 0L, (DWORD)bi.biHeight,
        (LPBYTE)NULL, (LPBITMAPINFO)lpbi, (DWORD)DIB_RGB_COLORS);

    bi = *lpbi;
    GlobalUnlock(hdib);

    /* If the driver did not fill in the biSizeImage field, make one up */
    if (bi.biSizeImage == 0){
        bi.biSizeImage = WIDTHBYTES((DWORD)bm.bmWidth * biBits) * bm.bmHeight;

        if (biStyle != BI_RGB)
            bi.biSizeImage = (bi.biSizeImage * 3) / 2;
    }

    /*  realloc the buffer big enough to hold all the bits */
    dwLen = bi.biSize + PaletteSize(&bi) + bi.biSizeImage;
    if (h = GlobalReAlloc(hdib,dwLen,0))
        hdib = h;
    else{
        GlobalFree(hdib);
        hdib = NULL;

        SelectPalette(hdc,hpal,FALSE);
        ReleaseDC(NULL,hdc);
        return hdib;
    }

    /*  call GetDIBits with a NON-NULL lpBits param, and actualy get the
     *  bits this time
     */
    lpbi = (BITMAPINFOHEADER FAR *)GlobalLock(hdib);

    if (GetDIBits( hdc,
                   hbm,
                   0L,
                   (DWORD)bi.biHeight,
                   (LPBYTE)lpbi + (WORD)lpbi->biSize + PaletteSize(lpbi),
                   (LPBITMAPINFO)lpbi, (DWORD)DIB_RGB_COLORS) == 0){
         GlobalUnlock(hdib);
         hdib = NULL;
         SelectPalette(hdc,hpal,FALSE);
         ReleaseDC(NULL,hdc);
         return NULL;
    }

    bi = *lpbi;
    GlobalUnlock(hdib);

    SelectPalette(hdc,hpal,FALSE);
    ReleaseDC(NULL,hdc);
    *length = dwLen;
    return hdib;
} /* DibFromBitmap */

/* 
 * End of tkOleDND.cpp
 */
