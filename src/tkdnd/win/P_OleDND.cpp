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
#include <shlobj.h>

extern Tcl_HashTable   TkDND_TargetTable;
extern Tcl_HashTable   TkDND_SourceTable;
extern DndClass       *dnd;

extern int   TkDND_DelHandler(DndInfo *infoPtr, char *typeStr, char *eventStr,
                    unsigned long eventType, unsigned long eventMask);
extern int   TkDND_DelHandlerByName(Tcl_Interp *interp, Tk_Window topwin,
                    Tcl_HashTable *table, char *windowPath, char *typeStr,
                    unsigned long eventType, unsigned long eventMask);
extern void  TkDND_DestroyEventProc(ClientData clientData, XEvent *eventPtr);
extern int   TkDND_FindMatchingScript(Tcl_HashTable *table,
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
extern char *TkDND_GetDataAccordingToType(Tcl_Obj *ResultObj, char *type,
                    int *length);  
extern Tcl_Obj 
            *TkDND_CreateDataObjAccordingToType(char *type,
                    unsigned char *data, int length);
extern int   TkDND_Update(Display *display, int idle);
extern int   TkDND_WidgetGetData(DndInfo *infoPtr, char **data, int *length,
                    CLIPFORMAT type);

/*****************************************************************************
 *############################################################################
 *# Drop Source Related Class.
 *############################################################################
 ****************************************************************************/

/*
 *----------------------------------------------------------------------
 * TkDND_DropSource::TkDND_DropSource
 *
 *        Constructor
 *----------------------------------------------------------------------
 */
TkDND_DropSource::TkDND_DropSource(DndInfo *info) {
  m_refCnt = 0;         /* Reference count */
  infoPtr  = info;
  return;
} /* TkDND_DropSource */

/*
 *----------------------------------------------------------------------
 * TkDND_DropSource::~TkDND_DropSource
 *
 *        Destructor
 *----------------------------------------------------------------------
 */
TkDND_DropSource::~TkDND_DropSource(void) {
  return;
} /* ~TkDND_DropSource */

/*
 *----------------------------------------------------------------------
 * TkDND_DropSource::QueryInterface
 *
 *        Does interface negotiation.  IUnknown and IDropSource are
 *        the supported interfaces.
 *
 * Arguments:
 *
 *        REFIID riid        A reference to the interface being queried.
 *      LPVOID *ppvObj       Out parameter to return a pointer to interface.
 *
 * Results:
 *        NOERROR if interface is supported, E_NOINTERFACE if it is not
 *
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DropSource::QueryInterface(REFIID riid, LPVOID *ppv) {
  *ppv=NULL;

  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropSource)) {
    *ppv=(LPVOID)this;
  }

  if (NULL!=*ppv) {
    ((LPUNKNOWN)*ppv)->AddRef();
    return NOERROR;
  }

  return ResultFromScode(E_NOINTERFACE);
} /* TkDND_DropSource::QueryInterface */

/*
 *----------------------------------------------------------------------
 * TkDND_DropSource::AddRef
 *
 *        Increment the reference count on the object as well as on
 *        the unknown object this enumerator is linked to.
 *
 * Results:
 *        The current reference count on the object.
 *----------------------------------------------------------------------
 */
STDMETHODIMP_(ULONG) TkDND_DropSource::AddRef(void) {
  return ++m_refCnt;
} /* TkDND_DropSource::AddRef */

/*
 *----------------------------------------------------------------------
 * TkDND_DropSource::Release
 *
 *        Decrement the reference count on the object as well as on
 *        the unknown object this enumerator is linked to.  If the
 *        reference count becomes 0, the object is deleted.
 *
 * Results:
 *        The current reference count on the object.
 *----------------------------------------------------------------------
 */
STDMETHODIMP_(ULONG) TkDND_DropSource::Release(void) {
   m_refCnt--;

  if (m_refCnt == 0L) {
    delete this;
    return 0L;
  }

  return m_refCnt;
} /* TkDND_DropSource::Release */

/*
 *----------------------------------------------------------------------
 * TkDND_DropSource::QueryContinueDrag
 *
 *        Determines whether a drag operation should continue
 *
 * Arguments:
 *        fEscapePressed          Was the escape key pressed? (in)
 *        grfKeyState             Identifies the present state of the modifier
 *                                keys on the keyboard (in)
 *
 * Result:
 *        DRAGDROP_S_CANCEL to stop the drag, DRAGDROP_S_DROP to drop the
 *        data where it is, or NOERROR to continue.
 *
 * Notes:
 *
 *----------------------------------------------------------------------
 */
STDMETHODIMP
TkDND_DropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD keyState) {
  /*
   * If <Escape> was pressed or an error has occured in any of our tcl
   * callbacks, cancel drag...
   */
  if (fEscapePressed || dnd->CallbackStatus == TCL_ERROR) {
    return ResultFromScode(DRAGDROP_S_CANCEL);
  }

  /*
   * If mouse button initiated drag was released, request a drop...
   */
  if ((dnd->button == 1 && !(keyState & MK_LBUTTON)) ||
      (dnd->button == 2 && !(keyState & MK_MBUTTON)) ||
      (dnd->button == 3 && !(keyState & MK_RBUTTON))    ) {
    return ResultFromScode(DRAGDROP_S_DROP);
  }

  return NOERROR;
} /* TkDND_DropSource::QueryContinueDrag */

/*
 *----------------------------------------------------------------------
 * TkDND_DropSource::GiveFeedback
 *
 *        Enables a source application to provide cursor feedback during a 
 *        drag and drop operation.  
 *
 * Arguments
 *        dwEffect        The DROPEFFECT returned from the most recent call
 *                        to IDropTarget::DragEnter or IDropTarget::DragOver
 *
 * Result:
 *        DRAGDROP_S_USEDEFAULTCURSORS: OLE should use the default cursors
 *        to provide feedback...
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DropSource::GiveFeedback(DWORD dwEffect) {
  POINT CursorPoint;
  int ret;
  Tcl_DString dString;
  DndInfo info;
  DndType type;
  
  /*
   * Get Mouse coordinates...
   */
  GetCursorPos(&CursorPoint);

  /*
   * Get the desired action...
   */
  if      (dwEffect == DROPEFFECT_COPY) strcpy(dnd->DesiredAction, "copy");
  else if (dwEffect == DROPEFFECT_MOVE) strcpy(dnd->DesiredAction, "move");
  else if (dwEffect == DROPEFFECT_LINK) strcpy(dnd->DesiredAction, "link");
  else if (dwEffect == DROPEFFECT_NONE) strcpy(dnd->DesiredAction, "");
  else                                  strcpy(dnd->DesiredAction, "private");
  
  /*
   * If a cursor window is specified by the user, move it to follow cursor...
   */
  if (dnd->CursorWindow != NULL) {
    if (dnd->x != CursorPoint.x || dnd->y != CursorPoint.y) {
      Tk_MoveToplevelWindow(dnd->CursorWindow, CursorPoint.x+10, CursorPoint.y);
      Tk_RestackWindow(dnd->CursorWindow, Above, NULL);
    }
    /*
     * Has also the user registered a Cursor Callback?
     */
    if (dnd->CursorCallback != NULL) {
      info.tkwin   = dnd->DraggerWindow;
      if (dnd->DesiredTypeStr == NULL) {
        type.typeStr = Tk_GetAtomName(info.tkwin, dnd->DesiredType);
      } else {
        type.typeStr = dnd->DesiredTypeStr;
      }
      type.script  = "";
      Tcl_DStringInit(&dString);
      TkDND_ExpandPercents(&info, &type, dnd->CursorCallback, &dString,
                           CursorPoint.x, CursorPoint.y);
      ret = TkDND_ExecuteBinding(dnd->interp, Tcl_DStringValue(&dString),
                                 dnd->length, dnd->data);
      Tcl_DStringFree(&dString);
      if (ret == TCL_ERROR) {
        Tk_BackgroundError(dnd->interp);
        dnd->CallbackStatus = TCL_ERROR;
      }
      TkDND_Update(dnd->display, 0);
    }
  }
  dnd->x = CursorPoint.x;
  dnd->y = CursorPoint.y;
  return ResultFromScode(DRAGDROP_S_USEDEFAULTCURSORS);
} /* TkDND_DropSource::GiveFeedback */

/*****************************************************************************
 *############################################################################
 *# Data object Related Class (needed by Drag Source for OLE DND)...
 *############################################################################
 ****************************************************************************/

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::TkDND_DataObject
 *
 *        Constructor
 *----------------------------------------------------------------------
 */
TkDND_DataObject::TkDND_DataObject(DndInfo *info) {
  DndType     *curr;
  
  m_refCnt   = 0;
  m_numTypes = 0;
  m_maxTypes = 0;
  m_typeList = NULL;
  infoPtr    = info;

  /*
   * Get the supported platform specific types register for the window, and
   * register them in the data object, to make them available...
   */
  for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
    AddDataType(curr->type);
  }
} /* TkDND_DataObject::TkDND_DataObject */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::~TkDND_DataObject
 *
 *        Destructor
 *----------------------------------------------------------------------
 */
TkDND_DataObject::~TkDND_DataObject(void) {
  if (m_numTypes > 0) {
    delete [] m_typeList;
  }
} /* TkDND_DataObject::~TkDND_DataObject */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::QueryInterface
 *
 *        Does interface negotiation.
 *
 * Arguments:
 *
 *        REFIID riid           A reference to the interface being queried.
 *        LPVOID *ppvObj        Out parameter to return a pointer to interface.
 *
 * Results:
 *        NOERROR if interface is supported, E_NOINTERFACE if it is not
 *
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DataObject::QueryInterface(REFIID riid, LPVOID *ppv) {
  *ppv=NULL;

  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDataObject)) {
    *ppv=(LPVOID)this;
  }

  if (NULL!=*ppv) {
    ((LPUNKNOWN)*ppv)->AddRef();
    return NOERROR;
  }

  return ResultFromScode(E_NOINTERFACE);
} /* TkDND_DataObject::QueryInterface */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::AddRef
 *
 *        Increment the reference count.
 *----------------------------------------------------------------------
 */
STDMETHODIMP_(ULONG) TkDND_DataObject::AddRef() {
  return ++m_refCnt;
} /* TkDND_DataObject::AddRef */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::Release
 *
 *        Decrement the reference count, destroy if count reaches 0.
 *----------------------------------------------------------------------
 */
STDMETHODIMP_(ULONG) TkDND_DataObject::Release() {
  m_refCnt--;
  if (m_refCnt == 0L) {
    delete this;
    return 0L;
  }
  return m_refCnt;
} /* TkDND_DataObject::Release */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::GetData
 *
 *        Gets the data into the storage medium if possible.
 *----------------------------------------------------------------------
 */
STDMETHODIMP
TkDND_DataObject::GetData(LPFORMATETC pFormatEtcIn, STGMEDIUM *pMedium) {
  HGLOBAL hMem;
  UINT  i;
  Tcl_DString ds;
  char *data, *newStr;
  int   length;
  
  /*
   * Is the requested format supported by our object?
   */
  for (i = 0; i < m_numTypes; i++) {
    if (m_typeList[i].cfFormat == pFormatEtcIn->cfFormat) {
      if (pFormatEtcIn->tymed & TYMED_HGLOBAL) {
        break;
      }
    }
  }
  if (i == m_numTypes) {
    /* The requested format was not found... */
    return ResultFromScode(DV_E_FORMATETC);
  }

  /*
   * Execute the Tcl binding to get the data that are to be dropped...
   */
  if (!TkDND_WidgetGetData(infoPtr, &data, &length, pFormatEtcIn->cfFormat)) {
    return S_FALSE;
  }

  if (pFormatEtcIn->cfFormat == CF_HDROP) {
    /************************************************************************
     * ######################################################################
     * # CF_HDROP (text/uri-list)
     * ######################################################################
     ***********************************************************************/
    /*
     * Being a file drag source is something special... The data must be a
     * special structure that holds the file list of the files to be dropped...
     */
    LPDROPFILES pDropFiles;
    Tcl_Obj *ObjResult, **File;
    Tcl_Interp *interp;
    int file_nu;

    /*
     * Forget what TkDND_WidgetGetData has returned, and access the interpreter
     * directly, in order to get the tcl list object...
     */
    interp = dnd->interp;
    ObjResult = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(ObjResult);
    if (Tcl_ListObjGetElements(interp, ObjResult, &file_nu, &File) !=TCL_OK) {
      Tcl_DecrRefCount(ObjResult);
      return S_FALSE;
    }
    
    /*
     * We now have the file names in a tcl list object...
     * Allocate space for the DROPFILE structure plus the number of files
     * and one extra byte for final NULL terminator...
     */
    hMem = GlobalAlloc(GHND|GMEM_SHARE, 
                       (DWORD) (sizeof(DROPFILES)+(_MAX_PATH)*file_nu+1));
    if (hMem == NULL) {
      Tcl_DecrRefCount(ObjResult);
      return ResultFromScode(STG_E_MEDIUMFULL);
    }
    pDropFiles = (LPDROPFILES) GlobalLock(hMem);
    /*
     * Set the offset where the starting point of the file start..
     */
    pDropFiles->pFiles = sizeof(DROPFILES);
    /*
     * File contains wide characters?
     */
    pDropFiles->fWide = FALSE;

    int CurPosition = sizeof(DROPFILES);
    for (int i=0; i<file_nu; i++) {
      TCHAR *pszFileName;
      /*
       * Convert File Name to native paths...
       */
      Tcl_DStringInit(&ds);
      pszFileName = Tcl_TranslateFileName(NULL, Tcl_GetString(File[i]), &ds);
      if (pszFileName != NULL) {
        /*
         * BUG: The following didn't work under NT!
         * pszFileName = Tcl_WinUtfToTChar(Tcl_GetString(File[i]), -1, &ds);
         */
        
        /*
         * Convert the file name in "external" string,
         * using the system encoding...
         */
        pszFileName = 
               Tcl_UtfToExternalDString(NULL, Tcl_GetString(File[i]), -1, &ds);
      }
      /*
       * Copy the file name into global memory...
       */
      lstrcpy((LPSTR)((LPSTR)(pDropFiles)+CurPosition),TEXT(pszFileName));
      /*
       * Move the current position beyond the file name copied, and
       * don't forget the Null terminator (+1)
       */
      CurPosition +=Tcl_DStringLength(&ds)+1;
      Tcl_DStringFree(&ds);
    }
    Tcl_DecrRefCount(ObjResult);
    /*
     * Finally, add an additional null terminator, as per CF_HDROP Format specs.
     */
    ((LPSTR)pDropFiles)[CurPosition]=0;
    GlobalUnlock(hMem);
  } else if (pFormatEtcIn->cfFormat == CF_UNICODETEXT) {
    /************************************************************************
     * ######################################################################
     * # CF_UNICODETEXT (text/plain;charset=UTF-8)
     * ######################################################################
     ***********************************************************************/
    /*
     * TODO: Actually, this code is buggy :-)
     * I don't know how to pass utf-8 strings. Anybody??
     */
    length = lstrlen(data) + 1;
    hMem = GlobalAlloc(GMEM_SHARE | GMEM_MOVEABLE, length + 2);
    if (hMem == NULL) return ResultFromScode(STG_E_MEDIUMFULL);
    newStr = (char *) GlobalLock(hMem);
    lstrcpy((LPSTR) newStr, TEXT(data));
    GlobalUnlock(hMem);
  } else {
    /************************************************************************
     * ######################################################################
     * # CF_TEXT (text/plain) and anything else...
     * ######################################################################
     ***********************************************************************/
    length = lstrlen(data) + 1;
    hMem = GlobalAlloc(GMEM_SHARE | GMEM_MOVEABLE, length);
    if (hMem == NULL) return ResultFromScode(STG_E_MEDIUMFULL);
    newStr = (char *) GlobalLock(hMem);
    memcpy(newStr, data, length);
    GlobalUnlock(hMem);
  }

  if (data != NULL) Tcl_Free(data);
  pMedium->hGlobal = hMem;
  pMedium->tymed = TYMED_HGLOBAL;
  pMedium->pUnkForRelease = NULL;
  return S_OK;
} /* TkDND_DataObject::GetData */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::GetDataHere
 *
 *        Not implemented and not necessary.
 *----------------------------------------------------------------------
 */
STDMETHODIMP
TkDND_DataObject::GetDataHere(LPFORMATETC pformatetc, LPSTGMEDIUM pmedium) {
  return ResultFromScode(E_NOTIMPL);
} /* TkDND_DataObject::GetDataHere */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::QueryGetData
 *
 *        TODO: Needs to be implemented.
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DataObject::QueryGetData(LPFORMATETC pFormatEtcIn) {
  UINT i;

  for (i = 0; i < m_numTypes; i++) {
    if (m_typeList[i].cfFormat == pFormatEtcIn->cfFormat) {
      if (pFormatEtcIn->tymed & TYMED_HGLOBAL) {
        return S_OK;
      }
    }
  }
  return ResultFromScode(DV_E_FORMATETC);
} /* TkDND_DataObject::QueryGetData */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::GetCanonicalFormatEtc
 *
 *        Not implemented and not necessary.
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DataObject::GetCanonicalFormatEtc(LPFORMATETC pFormatEtcIn,
                                                LPFORMATETC pFormatEtcOut) {
  return ResultFromScode(E_NOTIMPL);
} /* TkDND_DataObject::GetCanonicalFormatEtc */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::SetData
 *
 *        Not implemented and not necessary.
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DataObject::SetData(LPFORMATETC pFormatEtc,
                                       LPSTGMEDIUM pMedium, BOOL fRelease) {
  return ResultFromScode(E_NOTIMPL);
} /* TkDND_DataObject::SetData */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::EnumFormatEtc
 *
 *        Creates an enumerator. TODO
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DataObject::EnumFormatEtc(DWORD dwDirection,
                                             LPENUMFORMATETC *ppEnumFormatEtc) {
  if (ppEnumFormatEtc == NULL) {
    return ResultFromScode(E_INVALIDARG);
  }

  if (dwDirection != DATADIR_GET) {
    *ppEnumFormatEtc = NULL;
    return ResultFromScode(E_NOTIMPL);
  }

  *ppEnumFormatEtc = CreateFormatEtcEnumerator(this, m_numTypes, m_typeList);
  if (*ppEnumFormatEtc == NULL) {
    return E_OUTOFMEMORY;
  }

  return S_OK;
} /* TkDND_DataObject::EnumFormatEtc */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::DAdvise
 *
 *        Not implemented and not necessary.
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DataObject::DAdvise(LPFORMATETC pFormatEtc, DWORD advf,
                                  LPADVISESINK pAdvSink, DWORD *pdwConnection) {
  return ResultFromScode(OLE_E_ADVISENOTSUPPORTED);
} /* TkDND_DataObject::DAdvise */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::DUnadvise
 *
 *        Not implemented and not necessary.
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DataObject::DUnadvise(DWORD dwConnection) {
  return ResultFromScode(OLE_E_NOCONNECTION);
} /* TkDND_DataObject::DUnadvise */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::EnumDAdvise
 *
 *        Not implemented and not necessary.
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DataObject::EnumDAdvise(IEnumSTATDATA **ppEnumAdvise) {
  return ResultFromScode(OLE_E_ADVISENOTSUPPORTED);
} /* TkDND_DataObject::EnumDAdvise */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::AddDataType
 *
 *        Add a data type to the list of types that are supported by
 *        this instance of the TkDND_DataObject.
 *
 * Results:
 *        TCL_OK if everything is OK.  TCL_ERROR if not.
 *
 *----------------------------------------------------------------------
 */
int TkDND_DataObject::AddDataType(UINT clipFormat) {
  FORMATETC *oldTypeList;
  UINT i;

  if (clipFormat == 0) {
    return TCL_ERROR;
  }

  /*
   * Is the requested format already in our table?
   */
  for (i = 0; i < m_numTypes; i++) {
    if (m_typeList[i].cfFormat == clipFormat) {
      return TCL_OK;
    }
  }

  /*
   * Is our array full? Move it to a larger array...
   */
  if (m_maxTypes == m_numTypes) {
    m_maxTypes += 10;
    oldTypeList = m_typeList;
    m_typeList = new FORMATETC[m_maxTypes];
    if (m_typeList == NULL) {
      return TCL_ERROR;
    }
    if (m_numTypes > 0) {
      for (i = 0; i < m_numTypes; i++) {
        m_typeList[i] = oldTypeList[i];
      }
      delete [] oldTypeList;
    }
  }

  m_typeList[m_numTypes].cfFormat = clipFormat;
  m_typeList[m_numTypes].ptd = NULL;
  m_typeList[m_numTypes].dwAspect = DVASPECT_CONTENT;
  m_typeList[m_numTypes].lindex = -1;
  m_typeList[m_numTypes].tymed = TYMED_HGLOBAL;

  m_numTypes++;
  return TCL_OK;
} /* TkDND_DataObject::AddDataType */

/*
 *----------------------------------------------------------------------
 * TkDND_DataObject::DelDataType
 *
 *        Remove a data type from the list of types that are supported by
 *        this instance of the TkDND_DataObject.
 *
 * Results:
 *        TCL_OK if the type was found in the list and was successfully removed.
 *        TCL_ERROR if the type was not found.
 *
 *----------------------------------------------------------------------
 */
int TkDND_DataObject::DelDataType(UINT clipFormat) {
  UINT i, j;

  if (clipFormat == 0) {
    return TCL_ERROR;
  }
  for (i = 0; i < m_numTypes; i++) {
    if (m_typeList[i].cfFormat == clipFormat) {
      for (j = i; j < m_numTypes; j++) {
        m_typeList[j-1] = m_typeList[j];
      }
      m_numTypes--;
      return TCL_OK;
    }
  }
  return TCL_OK;
} /* TkDND_DataObject::DelDataType */

/*****************************************************************************
 *############################################################################
 *# Drop Target Related Class.
 *############################################################################ 
 ****************************************************************************/

/*
 *----------------------------------------------------------------------
 * TkDND_DropTarget::TkDND_DropTarget
 *
 *        Constructor
 *----------------------------------------------------------------------
 */
TkDND_DropTarget::TkDND_DropTarget(DndInfo *info) {
  m_refCnt   = 0;         /* Reference count */
  infoPtr    = info;
  DataObject = NULL;
  return;
} /* TkDND_DropTarget */

/*
 *----------------------------------------------------------------------
 * TkDND_DropTarget::~TkDND_DropTarget
 *
 *        Destructor
 *----------------------------------------------------------------------
 */
TkDND_DropTarget::~TkDND_DropTarget(void) {
  if (DataObject != NULL) DataObject->Release();
  return;
} /* ~TkDND_DropTarget */

/*
 *----------------------------------------------------------------------
 * TkDND_DropTarget::QueryInterface
 *
 *        Does interface negotiation.  IUnknown and IDropTarget are
 *        the supported interfaces.
 *
 * Arguments:
 *
 *      REFIID riid           A reference to the interface being queried.
 *      LPVOID *ppvObj        Out parameter to return a pointer to interface.
 *
 * Results:
 *        NOERROR if interface is supported, E_NOINTERFACE if it is not
 *
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DropTarget::QueryInterface(REFIID riid, LPVOID *ppv) {
  *ppv=NULL;

  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropTarget)) {
    *ppv=(LPVOID)this;
  }

  if (NULL!=*ppv) {
    ((LPUNKNOWN)*ppv)->AddRef();
    return NOERROR;
  }

  return ResultFromScode(E_NOINTERFACE);
} /* TkDND_DropTarget::QueryInterface */

  /*
  *----------------------------------------------------------------------
  * TkDND_DropTarget::AddRef
  *
  *        Increment the reference count on the object as well as on
  *        the unknown object this enumerator is linked to.
  *
  * Results:
  *        The current reference count on the object.
  *----------------------------------------------------------------------
*/
STDMETHODIMP_(ULONG) TkDND_DropTarget::AddRef(void) {
  return ++m_refCnt;
} /* AddRef */

/*
 *----------------------------------------------------------------------
 * TkDND_DropTarget::Release
 *
 *        Decrement the reference count on the object as well as on
 *        the unknown object this enumerator is linked to.  If the
 *        reference count becomes 0, the object is deleted.
 *
 * Results:
 *        The current reference count on the object.
 *----------------------------------------------------------------------
 */
STDMETHODIMP_(ULONG) TkDND_DropTarget::Release(void) {
  m_refCnt--;
        
  if (m_refCnt == 0L) {
    delete this;
    return 0L;
  }
        
  return m_refCnt;
} /* TkDND_DropTarget::Release */

/*
 *----------------------------------------------------------------------
 * TkDND_DropTarget::DragEnter
 *
 *        Determines whether the target window can accept the dragged object
 *        and what effect the drop would have on the target window.
 *
 * Arguments:
 *        pIDataObject      An instance of the IDataObject interface through
 *                          which dragged data is accessible. (in)
 *        grfKeyState       Present state of keyboard modifiers (in)
 *        pt                Structure that holds mouse screen coordinates (in)
 *        pdwEffect         Points to where to return the effect of this
 *                          drag operation.
 *
 * Result:
 *        S_OK if everything is OK.
 *
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DropTarget::DragEnter(LPDATAOBJECT pIDataSource,
                                         DWORD grfKeyState, POINTL pt,
                                         LPDWORD pdwEffect) {
  FORMATETC fetc;
  HRESULT   hret;
  DndInfo  *info;
  DndType  *curr;
#ifdef DND_DEBUG
  char name[1024];
#endif /* DND_DEBUG */
        
  XDND_DEBUG("############ ---- Drag Enter ---- ##############\n");
  if (DataObject != NULL) DataObject->Release();
  DataObject          = NULL;
  KeyState            = grfKeyState;
  dnd->CallbackStatus = TCL_OK;
  dnd->DraggerActions = *pdwEffect;
  dnd->x              = pt.x;
  dnd->y              = pt.y;
  fetc.ptd            = NULL;
  fetc.dwAspect       = DVASPECT_CONTENT;
  fetc.lindex         = -1;
  fetc.tymed          = TYMED_HGLOBAL;

  /*
   * Get one of the supported by the drag source actions...
   */
  if      (*pdwEffect & DROPEFFECT_COPY) strcpy(dnd->DesiredAction, "copy");
  else if (*pdwEffect & DROPEFFECT_MOVE) strcpy(dnd->DesiredAction, "move");
  else if (*pdwEffect & DROPEFFECT_MOVE) strcpy(dnd->DesiredAction, "link");
  else                                   strcpy(dnd->DesiredAction, "");
  *pdwEffect          = DROPEFFECT_NONE;

  /*
   * Iterate over the supported by the drop target types, and see if any of
   * them is supported by the drag source...
   */
  for (curr = infoPtr->head.next; curr != NULL; curr = curr->next) {
    fetc.cfFormat = curr->type;
    hret = pIDataSource->QueryGetData(&fetc);
#ifdef DND_DEBUG
    XDND_DEBUG3("Checking for type: \"%s\" (0x%08x): ", curr->typeStr,
                curr->type);
    if (hret == S_OK) {
      GetClipboardFormatName(fetc.cfFormat, name, sizeof(name));
      XDND_DEBUG2("supported (%s)\n", name);
    } else {
      XDND_DEBUG("not supported\n");
    }
#endif /* DND_DEBUG */
    if (hret == S_OK) {
      /*
       * The drop target supports this option. So, set the drop effect to a 
       * default value ("copy"), in case a binding script is not bind for the
       * specific event...
       */
      *pdwEffect          = DROPEFFECT_COPY;
      dnd->DesiredType    = curr->type;
      dnd->DesiredTypeStr = curr->typeStr;
      DataObject          = pIDataSource;
      DataObject->AddRef();
      /*
       * We have found a common supported type that also has the highest
       * priority for the drop target. Now, try to find the appropriate Tcl
       * callback which may be stored on a different point in chain, if the
       * user has bind an action with a modifier...
       */
      /* Laurent Riesterer 06/07/2000: support for extended bindings */
      info = infoPtr;
      if (TkDND_FindMatchingScript(NULL, NULL, curr->typeStr, curr->type,
                TKDND_DRAGENTER, grfKeyState, False, &curr, &info) != TCL_OK) {
        dnd->CallbackStatus = TCL_ERROR;
        Tk_BackgroundError(infoPtr->interp);
        TkDND_Update(dnd->display, 0);
        return ResultFromScode(E_FAIL);
      }
      /*
       * Execute the binding...
       */
      if (info == NULL || curr == NULL) {
        XDND_DEBUG3("<DragEnter>: Script not found, info=%p, type=%p\n",
                infoPtr, curr);
      } else {
        Tcl_DString dString;
        int ret;
        /*
         * Send the <DragEnter> event...
         */
        dnd->interp = infoPtr->interp;
        Tcl_DStringInit(&dString);
        TkDND_ExpandPercents(info, curr, curr->script, &dString, dnd->x,dnd->y);
        ret = TkDND_ExecuteBinding(dnd->interp, Tcl_DStringValue(&dString),
                                   -1, NULL);
        Tcl_DStringFree(&dString);
        if (ret == TCL_ERROR) {
          dnd->CallbackStatus = TCL_ERROR;
	  Tk_BackgroundError(infoPtr->interp);
          TkDND_Update(dnd->display, 0);
          return ResultFromScode(E_FAIL);
        }
        *pdwEffect = ParseAction();
      }
      return S_OK;
    }
  } /* for (curr = infoPtr->head.next;... */
  /*
   * The data type is not supported by our target. No drop is allowed...
   */
  dnd->DesiredType    = -1;
  dnd->DesiredTypeStr = "";
  *pdwEffect          = DROPEFFECT_NONE;
  return S_OK;
} /* TkDND_DropTarget::DragEnter */

/*
 *----------------------------------------------------------------------
 * TkDND_DropTarget::DragOver
 *
 *        Provides feedback to the user and to DoDragDrop about the state
 *        of the drag operation within a drop target application.  This
 *        gets called when the mouse has moved within a window that was
 *        already DragEnter'ed.
 *
 * Arguments:
 *        grfKeyState        Present state of the keyboard modifiers
 *        pt                 Current mouse screen coordinates.
 *        pdwEffect          Points to where to return the effect of this
 *                           drag operation.
 *
 * Result:
 *        S_OK
 *
 * Notes:
 *        This routine is called on every WM_MOUSEMOVE, so it should be
 *        quick.
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DropTarget::DragOver(DWORD grfKeyState, POINTL pt,
                                        LPDWORD pdwEffect) {
  DndInfo  *info;
  DndType  *curr;
        
  XDND_DEBUG("############ ---- Drag Over ---- ##############\n");
  if (DataObject == NULL || dnd->DesiredType == -1) {
    strcpy(dnd->DesiredAction, "");
    *pdwEffect = DROPEFFECT_NONE;
    return NOERROR;
  }

  KeyState = grfKeyState;
  dnd->x   = pt.x;
  dnd->y   = pt.y;
  
  /* Laurent Riesterer 06/07/2000: support for extended bindings */
  info = infoPtr;
  if (TkDND_FindMatchingScript(NULL, NULL,
            dnd->DesiredTypeStr, dnd->DesiredType,
            TKDND_DRAG, KeyState, False, &curr, &info) != TCL_OK) {
    dnd->CallbackStatus = TCL_ERROR;
    Tk_BackgroundError(infoPtr->interp);
    TkDND_Update(dnd->display, 0);
    return ResultFromScode(E_FAIL);
  }
  /*
   * Execute the binding...
   */
  if (info == NULL || curr == NULL) {
    /*
     * There is no binding for this event. Return the default action...
     */
    *pdwEffect = DROPEFFECT_COPY;
    XDND_DEBUG3("<Drag>: Script not found, info=%p, type=%p\n",
                info, curr);
  } else {
    Tcl_DString dString;
    int ret;
    /*
     * Send the <Drag> event...
     */
    dnd->interp = infoPtr->interp;
    Tcl_DStringInit(&dString);
    TkDND_ExpandPercents(info, curr, curr->script, &dString, dnd->x,dnd->y);
    XDND_DEBUG2("<Drag>: Executing (%s)\n", Tcl_DStringValue(&dString));
    ret = TkDND_ExecuteBinding(dnd->interp, Tcl_DStringValue(&dString),
                               -1, NULL);
    Tcl_DStringFree(&dString);
    if (ret == TCL_ERROR) {
      dnd->CallbackStatus = TCL_ERROR;
      Tk_BackgroundError(infoPtr->interp);
      TkDND_Update(dnd->display, 0);
      return ResultFromScode(E_FAIL);
    }
    *pdwEffect = ParseAction();
  }
  return NOERROR;
} /* TkDND_DropTarget::DragOver */

/*
 *----------------------------------------------------------------------
 * TkDND_DropTarget::DragLeave
 *
 *        This method is called when the mouse leaves the area of a given
 *        target while a drag is in progress or when the drag operation is
 *        canceled.  Causes the drop target to remove its feedback.
 *
 * Arguments:
 *        None
 *
 * Result:
 *        Result from Callback
 *----------------------------------------------------------------------
 */
STDMETHODIMP TkDND_DropTarget::DragLeave(void) {
  DndInfo  *info;
  DndType  *curr;
        
  XDND_DEBUG("############ ---- Drag Leave ---- ##############\n");
  if (DataObject == NULL || dnd->DesiredType == -1) {
    strcpy(dnd->DesiredAction, "");
    return NOERROR;
  }
  
  DataObject->Release();
  DataObject = NULL;
  /* Laurent Riesterer 06/07/2000: support for extended bindings */
  info = infoPtr;
  if (TkDND_FindMatchingScript(NULL, NULL,
            dnd->DesiredTypeStr, dnd->DesiredType,
            TKDND_DRAGLEAVE, KeyState, False, &curr, &info) != TCL_OK) {
    dnd->CallbackStatus = TCL_ERROR;
    Tk_BackgroundError(infoPtr->interp);
    TkDND_Update(dnd->display, 0);
    return ResultFromScode(E_FAIL);
  }
  /*
   * Execute the binding...
   */
  if (info == NULL || curr == NULL) {
    XDND_DEBUG3("<DragLeave>: Script not found, info=%p, type=%p\n",
                infoPtr, curr);
  } else {
    Tcl_DString dString;
    int ret;
    /*
     * Send the <DragLeave> event...
     */
    dnd->interp = infoPtr->interp;
    Tcl_DStringInit(&dString);
    TkDND_ExpandPercents(info, curr, curr->script, &dString, dnd->x, dnd->y);
    ret = TkDND_ExecuteBinding(dnd->interp, Tcl_DStringValue(&dString),
                               -1, NULL);
    Tcl_DStringFree(&dString);
    if (ret == TCL_ERROR) {
      dnd->CallbackStatus = TCL_ERROR;
      Tk_BackgroundError(infoPtr->interp);
      TkDND_Update(dnd->display, 0);
      return ResultFromScode(E_FAIL);
    }
  }
  return NOERROR;
} /* TkDND_DropTarget::DragLeave */

/*
 *----------------------------------------------------------------------
 * TkDND_DropTarget::Drop
 *
 *        Drops the source data, indicated by pIDataSource, on this target
 *        application.
 *
 * Arguments:
 *        pIDataSource     Object through which the data is accessible.
 *        grfKeyState      Current keyboard/mouse state.
 *        pt               Mouse screen coordinates
 *        pdwEffect        Action that took place is stored here
 *
 * Result:
 *        Result from Callback
 *----------------------------------------------------------------------
 */
STDMETHODIMP 
TkDND_DropTarget::Drop(LPDATAOBJECT pIDataSource, DWORD grfKeyState,
                       POINTL pt, LPDWORD pdwEffect) {
  DndInfo  *info;
  DndType  *curr;
        
  XDND_DEBUG("############ ---- Drop ---- ##############\n");
  if (DataObject == NULL || dnd->DesiredType == -1) {
    strcpy(dnd->DesiredAction, "");
    *pdwEffect = DROPEFFECT_NONE;
    return ResultFromScode(E_FAIL);
  }

  /* Laurent Riesterer 06/07/2000: support for extended bindings */
  info = infoPtr;
  if (TkDND_FindMatchingScript(NULL, NULL,
            dnd->DesiredTypeStr, dnd->DesiredType,
            TKDND_DROP, KeyState, False, &curr, &info) != TCL_OK) {
    dnd->CallbackStatus = TCL_ERROR;
    Tk_BackgroundError(infoPtr->interp);
    TkDND_Update(dnd->display, 0);
    *pdwEffect = DROPEFFECT_NONE;
    DataObject->Release();
    DataObject = NULL;
    return ResultFromScode(E_FAIL);
  }

  /*
   * Execute the binding...
   */
  if (info == NULL || curr == NULL) {
    XDND_DEBUG3("<DragLeave>: Script not found, info=%p, type=%p\n",
                infoPtr, curr);
  } else {
    FORMATETC fetc;
    HRESULT hret;
    STGMEDIUM stgMedium;
    Tcl_Obj *dataObj = NULL;
    Tcl_DString dString;
    char *str, *s, *ip, *op;
    int ret, len;

    /*
     * Time to get the data from the data object...
     */
    fetc.cfFormat   = dnd->DesiredType;
    fetc.ptd        = NULL;
    fetc.dwAspect   = DVASPECT_CONTENT;
    fetc.lindex     = -1;
    fetc.tymed      = TYMED_HGLOBAL;
    stgMedium.tymed = TYMED_HGLOBAL;
    
    if (curr->type != dnd->DesiredType) {
      /*
       * We have a script for another type, that best fits the current
       * buttons/modifiers. Is the type supported by the drag source?
       */
      fetc.cfFormat = curr->type;
      hret = pIDataSource->GetData(&fetc, &stgMedium);
      if (hret != S_OK) {
        fetc.cfFormat = dnd->DesiredType;
      } else {
        dnd->DesiredTypeStr = curr->typeStr;
      }
    }

    hret = pIDataSource->GetData(&fetc, &stgMedium);
    if (hret != S_OK) {
      *pdwEffect = DROPEFFECT_NONE;
      DataObject->Release();
      DataObject = NULL;
      return ResultFromScode(E_FAIL);
    }
    /*
     * Dropped Data will be pointed by str...
     */
    str = (char *) GlobalLock(stgMedium.hGlobal);
    Tcl_DStringInit(&dString);

    /************************************************************************
     * ######################################################################
     * # Make the necessary data convertions...
     * ######################################################################
     ***********************************************************************/
    if (str != NULL) {
      if (dnd->DesiredType == CF_TEXT) {
        len = lstrlen(str) + 1;
        s = (char *) Tcl_Alloc(sizeof(char) * len);
        memcpy(s, str, len);
                        
        for (ip = str, op = s; *ip != 0; ) {
          if (*ip == '\r') {
            if (ip[1] != '\n') {
              *op++ = '\n';
            }
            ip++;
          } else {
            *op++ = *ip++;
          }
        }
        *op = 0;
        Tcl_DStringAppend(&dString, s, -1);
        Tcl_Free(s);
        dataObj = TkDND_CreateDataObjAccordingToType(dnd->DesiredTypeStr,
                        (unsigned char *) Tcl_DStringValue(&dString),
                                          Tcl_DStringLength(&dString));
      } else if (dnd->DesiredType == CF_HDROP) { /* Handle files drop... */
        HDROP hDrop = (HDROP) str;
        char szFile[_MAX_PATH], *p;
        UINT cFiles = DragQueryFile(hDrop, 0xFFFFFFFF, szFile, _MAX_PATH);
        UINT i;
                        
        for (i = 0; i < cFiles; ++i) {
          DragQueryFile(hDrop, i, szFile, _MAX_PATH);
          /*
           * Convert to forward slashes for easier access in scripts...
           */
          for (p=szFile; *p!='\0'; CharNext(p)) {
            if (*p == '\\') *p = '/';
          }
          Tcl_DStringAppendElement(&dString, szFile);
        }
        dataObj = TkDND_CreateDataObjAccordingToType(dnd->DesiredTypeStr,
                        (unsigned char*) Tcl_DStringValue(&dString),
                                         Tcl_DStringLength(&dString));
      } else {
        dataObj = TkDND_CreateDataObjAccordingToType(dnd->DesiredTypeStr,
                                    (unsigned char*) str, lstrlen(str));
      }
    }
    Tcl_DStringFree(&dString);
    GlobalUnlock(stgMedium.hGlobal);
    if (!stgMedium.pUnkForRelease) {
      GlobalFree(stgMedium.hGlobal);
    }

    /*
     * Send the <Drop> event...
     */
    dnd->interp = infoPtr->interp;
    Tcl_DStringInit(&dString);
    TkDND_ExpandPercents(info, curr, curr->script, &dString, dnd->x, dnd->y);
    if (dataObj != NULL) Tcl_IncrRefCount(dataObj);
    ret = TkDND_ExecuteBinding(dnd->interp, Tcl_DStringValue(&dString),
                               -1, dataObj);
    Tcl_DStringFree(&dString);
    if (dataObj != NULL) Tcl_DecrRefCount(dataObj);
    if (ret == TCL_ERROR) {
      dnd->CallbackStatus = TCL_ERROR;
      *pdwEffect = DROPEFFECT_NONE;
      DataObject->Release();
      DataObject = NULL;
      return ResultFromScode(E_FAIL);
    }
  }
  *pdwEffect = DROPEFFECT_NONE;
  DataObject->Release();
  DataObject = NULL;
  return NOERROR;
} /* TkDND_DropTarget::Drop */

/*
 *----------------------------------------------------------------------
 *
 * ParseAction -- (Laurent Riesterer 06/07/2000)
 *
 *       Analyze the result of a script to find the requested action
 *
 * Results:
 *       True if OK / False else.
 *
 *----------------------------------------------------------------------
 */
DWORD TkDND_DropTarget::ParseAction(void) {
  DWORD wanted_action;
  static char *Actions[] = {
    "none", "default", "copy", "move", "link", "ask", "private",
    (char *) NULL
  };
  enum actions {NONE, DEFAULT, COPY, MOVE, LINK, ASK, PRIVATE};
  int index;

  XDND_DEBUG2("<<ParseAction>>: Wanted Action = '%s'\n",
              Tcl_GetStringResult(dnd->interp));

  if (Tcl_GetIndexFromObj(infoPtr->interp, Tcl_GetObjResult(infoPtr->interp),
                          Actions, "action", 0, &index) == TCL_OK) {
    switch ((enum actions) index) {
      case NONE: {
        strcpy(dnd->DesiredAction, "none");
        dnd->CallbackStatus = TCL_BREAK;
        return DROPEFFECT_NONE;
      }
      case DEFAULT: {
        strcpy(dnd->DesiredAction, "copy");
        wanted_action = DROPEFFECT_COPY;
        break;
      }
      case COPY: {
        strcpy(dnd->DesiredAction, "copy");
        wanted_action = DROPEFFECT_COPY;
        break;
      }
      case MOVE: {
        strcpy(dnd->DesiredAction, "move");
        wanted_action = DROPEFFECT_MOVE;
        break;
      }
      case LINK: {
        strcpy(dnd->DesiredAction, "link");
        wanted_action = DROPEFFECT_LINK;
        break;
      }
      case ASK: {
        strcpy(dnd->DesiredAction, "copy");
        wanted_action = DROPEFFECT_COPY;
        break;
      }
      case PRIVATE: {
        strcpy(dnd->DesiredAction, "copy");
        wanted_action = DROPEFFECT_COPY;
        break;
      }
      default: {
        strcpy(dnd->DesiredAction, "none");
        wanted_action = DROPEFFECT_NONE;
      }
    }
  } else {
    strcpy(dnd->DesiredAction, "none");
    wanted_action = DROPEFFECT_NONE;
  }
  /*
   * Is the action returned by the script supported by the drag
   * source?
   */
  if (dnd->DraggerActions & wanted_action) {
    return wanted_action;
  }
  strcpy(dnd->DesiredAction, "none");
  return DROPEFFECT_NONE;
} /* TkDND_DropTarget::ParseAction */

/*
 * OleDND.cpp: EOF
 */
