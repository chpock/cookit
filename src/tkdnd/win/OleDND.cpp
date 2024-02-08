/*
 * OleDND.cpp --
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
                    char *windowPath, char *typeStr, CLIPFORMAT typelist[],
                    unsigned long eventType, unsigned long eventMask,
                    int matchExactly, DndType **typePtrPtr,
                    DndInfo **infoPtrPtr);
extern int   TkDND_GetCurrentTypes(Tcl_Interp *interp, Tk_Window topwin,
                    Tcl_HashTable *table, char *windowPath);
extern char *TkDND_GetSourceActions(void);
extern void  TkDND_ExpandPercents(DndInfo *infoPtr, DndType *typePtr,
                    char *before, Tcl_DString *dsPtr,
                    LONG x, LONG y);
extern int   TkDND_ExecuteBinding(Tcl_Interp *interp, char *script,
                    int numBytes, Tcl_Obj *data);
extern char *TkDND_GetDataAccordingToType(Tcl_Obj *ResultObj, DndType *typePtr,
                    int *length);
extern Tcl_Obj 
            *TkDND_CreateDataObjAccordingToType(DndType *typePtr, void *info,
                    unsigned char *data, int length);
extern int   TkDND_Update(Display *display, int idle);
extern int   TkDND_WidgetGetData(DndInfo *infoPtr, char **data, int *length,
                    CLIPFORMAT type);
extern char *TkDND_TypeToString(int type);

static CLIPFORMAT TypeList[100];
static char MatchedTypeStr[256];




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
 *        LPVOID *ppvObj       Out parameter to return a pointer to interface.
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
        type.typeStr = (char *) Tk_GetAtomName(info.tkwin, dnd->DesiredType);
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
     * Finally, add an additional null terminator, as per CF_HDROP Format
     * specs.
     */
    ((LPSTR)pDropFiles)[CurPosition]=0;
    GlobalUnlock(hMem);
  } else if (pFormatEtcIn->cfFormat == dnd->FileGroupDescriptor ||
           pFormatEtcIn->cfFormat == dnd->FileGroupDescriptorW) {
    /************************************************************************
     * ######################################################################
     * # FileGroupDescriptor (text/uri-list, Files)
     * ######################################################################
     ***********************************************************************/
    /* We cannot easily support this format. We must have the *contents* of
       the file(s) to be transferred... */
     return S_FALSE;
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
    //MessageBox(NULL, data, "Error", MB_OK);
    XDND_DEBUG2("UTF-8: allocating %d\n", length);
    hMem = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, length+2);
    if (hMem == NULL) return ResultFromScode(STG_E_MEDIUMFULL);
    newStr = (char *) GlobalLock(hMem);
    lstrcpyW((LPWSTR) newStr, (LPWSTR) data);
    //MessageBox(NULL, Tcl_DStringValue(&ds), "Error", MB_OK);
    GlobalUnlock(hMem);
  } else if (pFormatEtcIn->cfFormat == CF_DIB) {
     /*********************************************************************
     * ###################################################################
     * #  CF_DIB
     * ###################################################################
     ********************************************************************/
    /*
     * In order to create a DIB, we must allocate a BITMAPINFO structure,
     * followed by the bitmap bits. The caller data should be a tk image...
     */
    Tk_PhotoHandle photo;
    Tk_PhotoImageBlock block;
    HANDLE hDIB;
    BITMAPINFOHEADER *bmi;
    unsigned char *pData, *pixel;
    int x, y, lineSize;

    photo = Tk_FindPhoto(dnd->interp, data);
    if (photo == NULL) {
      /*
       * The data is not a Tk image. Pass them as is to the caller...
       */
      length = lstrlen(data) + 1;
      hMem = GlobalAlloc(GMEM_SHARE | GMEM_MOVEABLE, length);
      if (hMem == NULL) return ResultFromScode(STG_E_MEDIUMFULL);
      newStr = (char *) GlobalLock(hMem);
      memcpy(newStr, data, length);
      GlobalUnlock(hMem);
    } else {
      Tk_PhotoGetImage(photo, &block);
      lineSize = ((block.width*3-1)/4+1)*4;
      hDIB = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE,
             sizeof(BITMAPINFOHEADER)+lineSize*block.height);
      if (hDIB == NULL) return ResultFromScode(STG_E_MEDIUMFULL);
      /*
       * Fill Header info...
       */
      bmi = (BITMAPINFOHEADER *) GlobalLock (hDIB);
      if (bmi == NULL) {
        GlobalUnlock(hDIB);
        return ResultFromScode(STG_E_MEDIUMFULL);
      }
      bmi->biSize          = sizeof(BITMAPINFOHEADER);
      bmi->biWidth         = block.width;
      bmi->biHeight        = block.height;
      bmi->biPlanes        = 1;
      bmi->biBitCount      = 24;     /* We pass only 24-bit images */
      bmi->biCompression   = BI_RGB; /* none */
      bmi->biSizeImage     = 0;      /* not calculated/needed */
      bmi->biXPelsPerMeter = 0;
      bmi->biYPelsPerMeter = 0;
      bmi->biClrUsed       = 0;
      /*
       * Fill the bitmap bits...
       */
      pData = (unsigned char *) bmi + sizeof(BITMAPINFOHEADER);
      for (y=0; y<block.height; y++) {
        for (x=0; x<block.width; x++) {
          pixel = block.pixelPtr + 
                  (block.height-y-1)*block.pitch+x*block.pixelSize;
          pData[x*3+y*lineSize]   = *(pixel+block.offset[2]); /* blue */
          pData[x*3+y*lineSize+1] = *(pixel+block.offset[1]); /* green */
          pData[x*3+y*lineSize+2] = *(pixel+block.offset[0]); /* red */
        }
      }
      GlobalUnlock(hDIB);
    }
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
#ifdef DND_ENABLE_DROP_TARGET_HELPER
  if (SUCCEEDED(DropHelper.CoCreateInstance(CLSID_DragDropHelper,
                                            NULL, CLSCTX_INPROC_SERVER))) {
    UseDropHelper = 1;
  } else {
    UseDropHelper = 0;
  }
#endif /* DND_ENABLE_DROP_TARGET_HELPER */
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
  IEnumFORMATETC *pEF;
  FORMATETC fetc;
  HRESULT   hret;
  DndInfo  *info;
  DndType  *curr;
  char      *formatName, *typeStringMatch = NULL;
  int i, j, found;
#ifdef DND_DEBUG
  char name[1024];
  int n;
#endif /* DND_DEBUG */
        
  XDND_DEBUG("############ ---- Drag Enter ---- ##############\n");
  if (DataObject != NULL) DataObject->Release();
  DataObject          = NULL;
  XDND_DEBUG3("grfKeyState = 0x%08x  --  KeyState = 0x%08x\n",
              grfKeyState, KeyState);
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
  else if (*pdwEffect & DROPEFFECT_LINK) strcpy(dnd->DesiredAction, "link");
  else                                   strcpy(dnd->DesiredAction, "");

  /*
   * Iterate over all the types supported by the drag source...
   */
  Tcl_DStringFree(&(dnd->DraggerTypes));
  Tcl_DStringInit(&(dnd->DraggerTypes));
  if (pIDataSource->EnumFormatEtc(DATADIR_GET, &pEF) == S_OK) {
    i = 0; TypeList[0] = 0;
    while (pEF->Next(1, &fetc, NULL) == S_OK) {
      if (pIDataSource->QueryGetData(&fetc) == S_OK) {
        /* Get the format name from windows */
        formatName = TkDND_TypeToString(fetc.cfFormat);
        Tcl_DStringAppendElement(&(dnd->DraggerTypes), formatName);
#ifdef DND_DEBUG
        XDND_DEBUG3("Checking for Drag source type: \"%s\" (0x%08x):\n",
                    formatName, fetc.cfFormat);
#endif /* DND_DEBUG */
        /* Iterate over the types supported by the drop target... */
        for (curr=infoPtr->head.next; i<99 && curr!=NULL;curr=curr->next) {
          if (curr->type == 0) {
            /* If the type is zero, then we should compare the strings with
             * string match... */
            if (Tcl_StringCaseMatch(formatName, curr->typeStr, 1)) {
              for (found=0,j=0;j<=i;j++) if (TypeList[j]==fetc.cfFormat) {
                found = 1; break;}
              if (!found) {
                TypeList[i++] = fetc.cfFormat;
                /* Do not remove this, we don't have initialise the array */
                TypeList[i]=0;
              }
#ifdef DND_DEBUG
              XDND_DEBUG3("  Drop target type: \"%s\" (0x%08x): "
                          "string match!\n", curr->typeStr, curr->type);
#endif /* DND_DEBUG */
            }
          } else if (curr->type == fetc.cfFormat) {
            for (found=0,j=0;j<=i;j++) if (TypeList[j]==fetc.cfFormat) {
              found = 1; break;}
            if (!found) {
              TypeList[i++] = fetc.cfFormat;
              /* Do not remove this, we don't have initialise the array */
              TypeList[i]=0;
            }
#ifdef DND_DEBUG
            XDND_DEBUG3("  Drop target type: \"%s\" (0x%08x): type equality!\n",
            curr->typeStr, curr->type);
#endif /* DND_DEBUG */
          }
        }
      }
    }
    pEF->Release();
  } else {
    /* We failed to enumerate the drag source data object. Check what types of
     * the drop target are also supported by the drop source... */
    for (i=0, curr=infoPtr->head.next; i<99 && curr!=NULL; curr=curr->next) {
      if (curr->type == 0) continue;
      fetc.cfFormat = curr->type;
      hret = pIDataSource->QueryGetData(&fetc);
      if (hret == S_OK) {
        for (found=0,j=0;j<=i;j++) if (TypeList[j]==fetc.cfFormat) {
          found = 1; break;}
        if (!found) TypeList[i++] = fetc.cfFormat;
        Tcl_DStringAppendElement(&(dnd->DraggerTypes), curr->typeStr);
      }
  
#ifdef DND_DEBUG
      XDND_DEBUG3("Checking for type: \"%s\" (0x%08x): ", curr->typeStr,
                  curr->type);
      if (hret == S_OK) {
        n = GetClipboardFormatName(fetc.cfFormat, name, sizeof(name));
        XDND_DEBUG2("supported (%s)\n", n ? name : "???");
      } else {
        XDND_DEBUG("not supported\n");
      }
#endif /* DND_DEBUG */
    } /* for (curr = infoPtr->head.next;... */
  }
  TypeList[i] = 0;
  dnd->DesiredTypeStr = "";
#ifdef DND_DEBUG
  XDND_DEBUG2("Checking done: %d types compatibles\n", i);
  for (j=0; TypeList && TypeList[j]; j++) {
    XDND_DEBUG4("Index=%d Type=\"%s\", Code=%d\n",
                j, TkDND_TypeToString(TypeList[j]), TypeList[j]);
  }
  XDND_DEBUG2("Drag Source Type List: %s\n",
               Tcl_DStringValue(&(dnd->DraggerTypes)));
  formatName = TkDND_GetSourceActions();
  XDND_DEBUG2("Drag Source Action List: %s\n", formatName);
  Tcl_Free(formatName);
#endif /* DND_DEBUG */


  info = infoPtr;
  if (TkDND_FindMatchingScript(NULL, NULL, NULL, TypeList,
            TKDND_DRAGENTER, grfKeyState, False, &curr, &info) != TCL_OK) {
    dnd->CallbackStatus = TCL_ERROR;
    Tk_BackgroundError(infoPtr->interp);
    TkDND_Update(dnd->display, 0);
    return ResultFromScode(E_FAIL);
  }
  /*
   * Execute the binding...
   */
  XDND_DEBUG3("<DragEnter>: search result: info=%p, type=%p\n", info, curr);
  if (info != NULL && curr != NULL) {
    Tcl_Obj *dataObj = NULL;
    Tcl_DString dString;
    int ret;
    STGMEDIUM stgMedium;

    /* Memorize the parameters */
    if      (strcmp(dnd->DesiredAction,"copy")==0) *pdwEffect = DROPEFFECT_COPY;
    else if (strcpy(dnd->DesiredAction,"move")==0) *pdwEffect = DROPEFFECT_MOVE;
    else if (strcpy(dnd->DesiredAction,"link")==0) *pdwEffect = DROPEFFECT_LINK;
    else                                           *pdwEffect = DROPEFFECT_COPY;
    dnd->DesiredType    = curr->type;
    dnd->DesiredTypeStr = curr->typeStr;
    DataObject          = pIDataSource;
    DataObject->AddRef();
    if (dnd->DesiredType == 0) {
      strcpy(MatchedTypeStr, TkDND_TypeToString(dnd->DesiredType));
      dnd->DesiredType    = curr->matchedType;
      dnd->DesiredTypeStr = MatchedTypeStr;
    }

    /*
     * Try to retrieve the data to be dropped.
     */
    stgMedium.tymed = TYMED_HGLOBAL;
    fetc.cfFormat = curr->type;
    if (pIDataSource->GetData(&fetc, &stgMedium) == S_OK) {
      char *str = (char *) GlobalLock(stgMedium.hGlobal);
      if (str != NULL) {
        dataObj = GetAndConvertData((IDataObject *) pIDataSource, curr,
                                    str, &fetc, &stgMedium);
        
      }
      GlobalUnlock(stgMedium.hGlobal);
      if (!stgMedium.pUnkForRelease) {
        GlobalFree(stgMedium.hGlobal);
      }
    }
    
    /*
     * Send the <DragEnter> event...
     */
    dnd->interp = infoPtr->interp;
    Tcl_DStringInit(&dString);
    TkDND_ExpandPercents(info, curr, curr->script, &dString, dnd->x,dnd->y);
    XDND_DEBUG2("<DragEnter>: execute binding '%s'\n",
                        Tcl_DStringValue(&dString));
    if (dataObj != NULL) Tcl_IncrRefCount(dataObj);
    ret = TkDND_ExecuteBinding(dnd->interp, Tcl_DStringValue(&dString),
                               -1, dataObj);
    if (dataObj != NULL) Tcl_DecrRefCount(dataObj);
    Tcl_DStringFree(&dString);
    if (ret == TCL_ERROR) {
      dnd->CallbackStatus = TCL_ERROR;
      Tk_BackgroundError(infoPtr->interp);
      TkDND_Update(dnd->display, 0);
      return ResultFromScode(E_FAIL);
    }
    *pdwEffect = ParseAction();

  } else if (info == NULL && curr != NULL) {
    XDND_DEBUG("<DragEnter>: no <Enter> but a <Drop>\n");
        /* Supported type but no script for <Enter> */
    dnd->DesiredType    = curr->type;
    dnd->DesiredTypeStr = curr->typeStr;
    DataObject          = pIDataSource;
    DataObject->AddRef();
    if (dnd->DesiredType == 0) {
      strcpy(MatchedTypeStr, TkDND_TypeToString(dnd->DesiredType));
      dnd->DesiredType    = curr->matchedType;
      dnd->DesiredTypeStr = MatchedTypeStr;
    }

  } else {
    /*
     * The data type is not supported by our target. No drop is allowed...
     */
    XDND_DEBUG3("<DragEnter>: Script not found, info=%p, type=%p\n", info,curr);
    dnd->DesiredType    = -1;
    dnd->DesiredTypeStr = "";
    *pdwEffect          = DROPEFFECT_NONE;
  }

#ifdef DND_ENABLE_DROP_TARGET_HELPER
  /*
   * If the <DragEnter> is not rejected, try to call the IDropTargetHelper...
   */
  if (UseDropHelper) {
    //IDataObject* pdo = pIDataSource->GetIDataObject(FALSE);
    DropHelper->DragEnter(Tk_GetHWND(Tk_WindowId(infoPtr->tkwin)),
                          (IDataObject *) pIDataSource,
                          (POINT*) &pt, *pdwEffect);
  }
#endif /* DND_ENABLE_DROP_TARGET_HELPER */

  XDND_DEBUG("<DragEnter>: returning\n");
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
#ifdef DND_ENABLE_DROP_TARGET_HELPER
    if (UseDropHelper) {
      DropHelper->DragOver((POINT*) &pt, *pdwEffect);
    }
#endif /* DND_ENABLE_DROP_TARGET_HELPER */
    return NOERROR;
  }

  XDND_DEBUG3("grfKeyState = 0x%08x  --  KeyState = 0x%08x\n",
              grfKeyState, KeyState);
  KeyState = grfKeyState;
  dnd->x   = pt.x;
  dnd->y   = pt.y;
  
  /* Laurent Riesterer 06/07/2000: support for extended bindings */
  info = infoPtr;
  if (TkDND_FindMatchingScript(NULL, NULL, NULL, TypeList,
            TKDND_DRAG, KeyState, False, &curr, &info) != TCL_OK) {
    dnd->CallbackStatus = TCL_ERROR;
#ifdef DND_ENABLE_DROP_TARGET_HELPER
    if (UseDropHelper) {
      DropHelper->DragOver((POINT*) &pt, *pdwEffect);
    }
#endif /* DND_ENABLE_DROP_TARGET_HELPER */
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
    if (strcmp(dnd->DesiredAction, "copy") == 0) {
      *pdwEffect = DROPEFFECT_COPY;
    } else if (strcmp(dnd->DesiredAction, "move") == 0) {
      *pdwEffect = DROPEFFECT_MOVE;
    } else if (strcmp(dnd->DesiredAction, "link") == 0) {
      *pdwEffect = DROPEFFECT_LINK;
    } else {
      *pdwEffect = DROPEFFECT_COPY;
    }
    XDND_DEBUG3("<Drag>: Script not found, info=%p, type=%p\n",
                info, curr);
  } else {
    Tcl_Obj *dataObj = NULL;
    STGMEDIUM stgMedium;
    FORMATETC fetc;
    Tcl_DString dString;
    int ret;
    
    /*
     * Try to retrieve the data to be dropped.
     */
    stgMedium.tymed = TYMED_HGLOBAL;
    fetc.ptd        = NULL;
    fetc.dwAspect   = DVASPECT_CONTENT;
    fetc.lindex     = -1;
    fetc.tymed      = TYMED_HGLOBAL;
    fetc.cfFormat   = curr->type;
    if (DataObject->GetData(&fetc, &stgMedium) == S_OK) {
      char *str = (char *) GlobalLock(stgMedium.hGlobal);
      if (str != NULL) {
        dataObj = GetAndConvertData((IDataObject *) DataObject, curr,
                                    str, &fetc, &stgMedium);
        
      }
      GlobalUnlock(stgMedium.hGlobal);
      if (!stgMedium.pUnkForRelease) {
        GlobalFree(stgMedium.hGlobal);
      }
    }
    
    /*
     * Send the <Drag> event...
     */
    dnd->interp = infoPtr->interp;
    Tcl_DStringInit(&dString);
    TkDND_ExpandPercents(info, curr, curr->script, &dString, dnd->x,dnd->y);
    XDND_DEBUG2("<Drag>: Executing (%s)\n", Tcl_DStringValue(&dString));
    if (dataObj != NULL) Tcl_IncrRefCount(dataObj);
    ret = TkDND_ExecuteBinding(dnd->interp, Tcl_DStringValue(&dString),
                               -1, dataObj);
    if (dataObj != NULL) Tcl_DecrRefCount(dataObj);
    Tcl_DStringFree(&dString);
    if (ret == TCL_ERROR) {
      dnd->CallbackStatus = TCL_ERROR;
      Tk_BackgroundError(infoPtr->interp);
      TkDND_Update(dnd->display, 0);
      return ResultFromScode(E_FAIL);
    }
    *pdwEffect = ParseAction();
  }
#ifdef DND_ENABLE_DROP_TARGET_HELPER
    if (UseDropHelper) {
      DropHelper->DragOver((POINT*) &pt, *pdwEffect);
    }
#endif /* DND_ENABLE_DROP_TARGET_HELPER */
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

#ifdef DND_ENABLE_DROP_TARGET_HELPER
  if (UseDropHelper) {
    DropHelper->DragLeave();
  }
#endif /* DND_ENABLE_DROP_TARGET_HELPER */

  if (DataObject == NULL || dnd->DesiredType == -1) {
    strcpy(dnd->DesiredAction, "");
    return NOERROR;
  }
  
  XDND_DEBUG2("grfKeyState = ----------  --  KeyState = 0x%08x\n", KeyState);
  /* Laurent Riesterer 06/07/2000: support for extended bindings */
  info = infoPtr;
  if (TkDND_FindMatchingScript(NULL, NULL, NULL, TypeList,
            TKDND_DRAGLEAVE, KeyState, False, &curr, &info) != TCL_OK) {
    dnd->CallbackStatus = TCL_ERROR;
    Tk_BackgroundError(infoPtr->interp);
    TkDND_Update(dnd->display, 0);
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
    Tcl_Obj *dataObj = NULL;
    STGMEDIUM stgMedium;
    FORMATETC fetc;
    Tcl_DString dString;
    int ret;

    /*
     * Try to retrieve the data to be dropped.
     */
    stgMedium.tymed = TYMED_HGLOBAL;
    fetc.ptd        = NULL;
    fetc.dwAspect   = DVASPECT_CONTENT;
    fetc.lindex     = -1;
    fetc.tymed      = TYMED_HGLOBAL;
    fetc.cfFormat   = curr->type;
    if (DataObject->GetData(&fetc, &stgMedium) == S_OK) {
      char *str = (char *) GlobalLock(stgMedium.hGlobal);
      if (str != NULL) {
        dataObj = GetAndConvertData((IDataObject *) DataObject, curr,
                                    str, &fetc, &stgMedium);
        
      }
      GlobalUnlock(stgMedium.hGlobal);
      if (!stgMedium.pUnkForRelease) {
        GlobalFree(stgMedium.hGlobal);
      }
    }
    
    /*
     * Send the <DragLeave> event...
     */
    dnd->interp = infoPtr->interp;
    Tcl_DStringInit(&dString);
    TkDND_ExpandPercents(info, curr, curr->script, &dString, dnd->x, dnd->y);
    XDND_DEBUG2("<DragLeave>: Executing (%s)\n", Tcl_DStringValue(&dString));
    if (dataObj != NULL) Tcl_IncrRefCount(dataObj);
    ret = TkDND_ExecuteBinding(dnd->interp, Tcl_DStringValue(&dString),
                               -1, dataObj);
    if (dataObj != NULL) Tcl_DecrRefCount(dataObj);
    Tcl_DStringFree(&dString);
    if (ret == TCL_ERROR) {
      dnd->CallbackStatus = TCL_ERROR;
      Tk_BackgroundError(infoPtr->interp);
      TkDND_Update(dnd->display, 0);
      DataObject->Release();
      DataObject = NULL;
      return ResultFromScode(E_FAIL);
    }
  }
  DataObject->Release();
  DataObject = NULL;
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
#ifdef DND_ENABLE_DROP_TARGET_HELPER
    if (UseDropHelper) {
      DropHelper->Drop((IDataObject *) pIDataSource, (POINT*) &pt, *pdwEffect);
    }
#endif /* DND_ENABLE_DROP_TARGET_HELPER */
    return ResultFromScode(E_FAIL);
  }

  /* the grfKeyState has the button release : we need to "reinject" it in order
     to correctly match the bindings */
  grfKeyState |= (dnd->button == 1 ? MK_LBUTTON :
                  dnd->button == 2 ? MK_MBUTTON :
                  dnd->button == 3 ? MK_RBUTTON : 0);
  XDND_DEBUG3("grfKeyState = 0x%08x  --  KeyState = 0x%08x\n",
              grfKeyState, KeyState);
  /* Laurent Riesterer 06/07/2000: support for extended bindings */
  info = infoPtr;
  if (TkDND_FindMatchingScript(NULL, NULL, NULL, TypeList,
            TKDND_DROP, grfKeyState, False, &curr, &info) != TCL_OK) {
    dnd->CallbackStatus = TCL_ERROR;
    Tk_BackgroundError(infoPtr->interp);
    TkDND_Update(dnd->display, 0);
    *pdwEffect = DROPEFFECT_NONE;
    DataObject->Release();
    DataObject = NULL;
#ifdef DND_ENABLE_DROP_TARGET_HELPER
    if (UseDropHelper) {
      DropHelper->Drop((IDataObject *) pIDataSource, (POINT*) &pt, *pdwEffect);
    }
#endif /* DND_ENABLE_DROP_TARGET_HELPER */
    return ResultFromScode(E_FAIL);
  }

#ifdef DND_ENABLE_DROP_TARGET_HELPER
  if (UseDropHelper) {
    DropHelper->Drop((IDataObject *) pIDataSource, (POINT*) &pt, *pdwEffect);
  }
#endif /* DND_ENABLE_DROP_TARGET_HELPER */

  /*
   * Execute the binding...
   */
  XDND_DEBUG3("Ready to retrieve the data: Type= \"%s\" (0x%08x)\n",
              dnd->DesiredTypeStr, dnd->DesiredType);
  if (info == NULL || curr == NULL) {
    XDND_DEBUG3("<Drop>: Script not found, info=%p, type=%p\n",
                infoPtr, curr);
  } else {
    FORMATETC fetc;
    HRESULT hret;
    STGMEDIUM stgMedium;
    UINT type;
    Tcl_Obj *dataObj = NULL;
    Tcl_DString dString;
    char *str;
    int ret;

    /*
     * Time to get the data from the data object...
     */
    if (curr->type) type = curr->type;
    else type = curr->matchedType;
    fetc.cfFormat   = type;
    fetc.ptd        = NULL;
    fetc.dwAspect   = DVASPECT_CONTENT;
    fetc.lindex     = -1;
    fetc.tymed      = TYMED_HGLOBAL;
    stgMedium.tymed = TYMED_HGLOBAL;
    
    if (type != dnd->DesiredType) {
      /*
       * We have a script for another type, that best fits the current
       * buttons/modifiers. Is the type supported by the drag source?
       */
      fetc.cfFormat = type;
      hret = pIDataSource->QueryGetData(&fetc);
      if (hret != S_OK) {
        fetc.cfFormat = dnd->DesiredType;
      } else {
        dnd->DesiredTypeStr = curr->typeStr;
        dnd->DesiredType = type;
      }
    }

    hret = pIDataSource->GetData(&fetc, &stgMedium);
    if (hret != S_OK) {
      *pdwEffect = DROPEFFECT_NONE;
      DataObject->Release();
      DataObject = NULL;
#ifdef DND_ENABLE_DROP_TARGET_HELPER
      if (UseDropHelper) {
        DropHelper->Drop((IDataObject *) pIDataSource, (POINT*) &pt,*pdwEffect);
      }
#endif /* DND_ENABLE_DROP_TARGET_HELPER */
      return ResultFromScode(E_FAIL);
    }
    /*
     * Dropped Data will be pointed by str...
     */
    str = (char *) GlobalLock(stgMedium.hGlobal);

    /************************************************************************
     * ######################################################################
     * # Make the necessary data convertions...
     * ######################################################################
     ***********************************************************************/
    if (str != NULL) {
      dataObj = GetAndConvertData((IDataObject *) pIDataSource, curr,
                                   str, &fetc, &stgMedium);
    }
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
    XDND_DEBUG2("<Drop>: Executing (%s)\n", Tcl_DStringValue(&dString));
    ret = TkDND_ExecuteBinding(dnd->interp, Tcl_DStringValue(&dString),
                               -1, dataObj);
    Tcl_DStringFree(&dString);
    if (dataObj != NULL) Tcl_DecrRefCount(dataObj);
    if (ret == TCL_ERROR) {
      dnd->CallbackStatus = TCL_ERROR;
      *pdwEffect = DROPEFFECT_NONE;
      DataObject->Release();
      DataObject = NULL;
      Tk_BackgroundError(infoPtr->interp);
      TkDND_Update(dnd->display, 0);
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
 * TkDND_DropTarget::GetAndConvertData
 *
 *        Gets the data to be dropped from the data source object and tries to
 *        convert it into something we can understand.
 *
 * Arguments:
 *        pIDataSource     Object through which the data is accessible.
 *        grfKeyState      Current keyboard/mouse state.
 *        pt               Mouse screen coordinates
 *        pdwEffect        Action that took place is stored here
 *
 * Result:
 *        A Tcl object holding the converted data. Must be freed by the
 *        caller.
 *----------------------------------------------------------------------
 */
Tcl_Obj *TkDND_DropTarget::GetAndConvertData(LPDATAOBJECT pIDataSource,
                           DndType  *curr,
                           char *str, FORMATETC *fetc, STGMEDIUM *stgMedium) {
  Tcl_Obj *dataObj = NULL;
  Tcl_DString dString;
  HRESULT hret;
  char *s, *ip, *op;
  int   len;
  void *loc;

  Tcl_DStringInit(&dString);

  /*********************************************************************
   * ###################################################################
   * #  CF_TEXT & CF_OEMTEXT
   * ###################################################################
   ********************************************************************/
  if (dnd->DesiredType == CF_TEXT || dnd->DesiredType == CF_OEMTEXT) {
    int locale;
    Tcl_DString ds;
    Tcl_DStringInit(&ds);
    len = strlen(str) + 1;
    s = (char *) Tcl_Alloc(sizeof(char) * len);
    memcpy(s, str, len);

    /*
     * Can we retrieve locale information from our object?
     */
    loc = NULL;
    if (dnd->DesiredType == CF_TEXT) {
      fetc->cfFormat = CF_LOCALE;
      hret = pIDataSource->QueryGetData(fetc);
      if (hret == S_OK) {
        hret = pIDataSource->GetData(fetc, stgMedium);
        if (hret != S_OK) {
          char *data;
        
          Tcl_DStringAppend(&ds, "cp######", -1);
          data = (char *) GlobalLock(stgMedium->hGlobal);
          locale = LANGIDFROMLCID(*((int*)data));
          GetLocaleInfo(locale, LOCALE_IDEFAULTANSICODEPAGE,
                  Tcl_DStringValue(&ds)+2, Tcl_DStringLength(&ds)-2);
          GlobalUnlock(stgMedium->hGlobal);
          loc = Tcl_DStringValue(&ds);
        }
      }
    } else {
      Tcl_DStringAppend(&ds, "cp######", -1);
      locale = GetSystemDefaultLCID();
      GetLocaleInfo(locale, LOCALE_IDEFAULTANSICODEPAGE,
                  Tcl_DStringValue(&ds)+2, Tcl_DStringLength(&ds)-2);
      loc = Tcl_DStringValue(&ds);
    }
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
    curr->matchedType = dnd->DesiredType;
    dataObj = TkDND_CreateDataObjAccordingToType(curr, loc,
                    (unsigned char *) Tcl_DStringValue(&dString),
                                      Tcl_DStringLength(&dString));
    curr->matchedType = 0;
    Tcl_DStringFree(&ds);
  } else if (dnd->DesiredType == CF_UNICODETEXT) {
     /*********************************************************************
     * ###################################################################
     * #  CF_UNICODETEXT
     * ###################################################################
     ********************************************************************/
     curr->matchedType = dnd->DesiredType;
     dataObj = 
       TkDND_CreateDataObjAccordingToType(curr, NULL,
                            (unsigned char*) str, lstrlenW((LPCWSTR) str));
     curr->matchedType = 0;
  } else if (dnd->DesiredType == CF_HDROP) { /* Handle files drop... */
    /*********************************************************************
     * ###################################################################
     * #  CF_HDROP
     * ###################################################################
     ********************************************************************/
    Tcl_DString ds;
    HDROP hDrop = (HDROP) str;
    char szFile[_MAX_PATH], *p;
    UINT cFiles = DragQueryFile(hDrop, 0xFFFFFFFF, szFile, _MAX_PATH);
    UINT i;
    Tcl_DStringInit(&ds);

    for (i = 0; i < cFiles; ++i) {
      DragQueryFile(hDrop, i, szFile, _MAX_PATH);
      /*
       * Convert to forward slashes for easier access in scripts...
       */
      for (p=szFile; *p!='\0'; p=(char *) CharNext(p)) {
        if (*p == '\\') *p = '/';
      }
      Tcl_DStringAppendElement(&dString, szFile);
    }
    /*
     * Can we retrieve locale information from our object?
     */
    fetc->cfFormat = CF_LOCALE;
    loc = NULL;
    hret = pIDataSource->QueryGetData(fetc);
    if (hret == S_OK) {
      hret = pIDataSource->GetData(fetc, stgMedium);
      if (hret != S_OK) {
        char *data;
        int locale;
        
        Tcl_DStringAppend(&ds, "cp######", -1);
        data = (char *) GlobalLock(stgMedium->hGlobal);
        locale = LANGIDFROMLCID(*((int*)data));
        GetLocaleInfo(locale, LOCALE_IDEFAULTANSICODEPAGE,
                Tcl_DStringValue(&ds)+2, Tcl_DStringLength(&ds)-2);
        GlobalUnlock(stgMedium->hGlobal);
        loc = Tcl_DStringValue(&ds);
      }
    }
    curr->matchedType = dnd->DesiredType;
    dataObj = TkDND_CreateDataObjAccordingToType(curr, loc,
                    (unsigned char*) Tcl_DStringValue(&dString),
                                     Tcl_DStringLength(&dString));
    curr->matchedType = 0;
    Tcl_DStringFree(&ds);
  } else if (dnd->DesiredType == dnd->FileGroupDescriptor) {
     /*********************************************************************
     * ###################################################################
     * #  FileGroupDescriptor
     * ###################################################################
     ********************************************************************/
     LPFILEGROUPDESCRIPTOR fileGroupDescrPtr = (LPFILEGROUPDESCRIPTOR) str;
     FILEDESCRIPTOR fileDescrPtr;
     UINT i;
     Tcl_Obj *elements[2], *element;
     dataObj = Tcl_NewListObj(0, NULL);
     
     /*
      * The result of the drop will be a list of elements. Each element
      * will be also a list of two elements:
      *   1) The file name.
      *   2) The file contents as a binary object.
      */
     for (i = 0; i < fileGroupDescrPtr->cItems; ++i) {
       fileDescrPtr = fileGroupDescrPtr->fgd[i];
       elements[0] = Tcl_NewStringObj(fileDescrPtr.cFileName, -1);
       elements[1] = Tcl_NewObj();
       element = Tcl_NewListObj(2, elements);
       Tcl_ListObjAppendElement(dnd->interp, dataObj, element);
     }
  } else if (dnd->DesiredType == dnd->FileGroupDescriptorW) {
     /*********************************************************************
     * ###################################################################
     * #  FileGroupDescriptorW
     * ###################################################################
     ********************************************************************/
     LPFILEGROUPDESCRIPTORW fileGroupDescrPtr = (LPFILEGROUPDESCRIPTORW) str;
     LPFILEDESCRIPTORW fileDescrPtr;
     UINT i;
     Tcl_Obj *elements[2], *element;
     dataObj = Tcl_NewListObj(0, NULL);
     
     /*
      * The result of the drop will be a list of elements. Each element
      * will be also a list of two elements:
      *   1) The file name.
      *   2) The file contents as a binary object.
      */
     for (i = 0; i < fileGroupDescrPtr->cItems; ++i) {
       if (i == 0) {
         fileDescrPtr = &(fileGroupDescrPtr->fgd[0]);
       } else {
         ++fileDescrPtr;
       }
       elements[0] = Tcl_NewUnicodeObj((const Tcl_UniChar *)
                     fileDescrPtr->cFileName, -1);
       elements[1] = Tcl_NewObj();
       element     = Tcl_NewListObj(2, elements);
       Tcl_ListObjAppendElement(dnd->interp, dataObj, element);
     }
  } else if (dnd->DesiredType == CF_DIB) {
     /*********************************************************************
     * ###################################################################
     * #  CF_DIB
     * ###################################################################
     ********************************************************************/
    /*
     * A memory object containing a BITMAPINFO structure followed by
     * the bitmap bits.
     */
    LPBITMAPINFO bmi = (LPBITMAPINFO) str;
    Tk_PhotoHandle photo;
    Tk_PhotoImageBlock block;
    unsigned char *pixelPtr;
    char *pColor;
    unsigned char *pData;
    long width, height, bpp, colors;
    int x, y, index, lineSize;

    /*
     * Read Header...
     */

    if (bmi->bmiHeader.biSize != sizeof(BITMAPINFOHEADER) ||
          (bmi->bmiHeader.biCompression != BI_RGB &&
           bmi->bmiHeader.biCompression != BI_BITFIELDS) ) {
      char tmp_str[128];
      sprintf(tmp_str, "Unsupported bitmap format (%d)!",
              bmi->bmiHeader.biCompression);
      dataObj = Tcl_NewStringObj(tmp_str, -1); 
    } else {
      width  = bmi->bmiHeader.biWidth;
      height = bmi->bmiHeader.biHeight;
      bpp    = bmi->bmiHeader.biBitCount;
      colors = bmi->bmiHeader.biClrUsed;
      /*
       * Create a new photo image...
       */
      if (Tcl_Eval(infoPtr->interp, "image create photo") != TCL_OK) {
        return NULL;
      }
      dataObj = Tcl_GetObjResult(infoPtr->interp);
      Tcl_IncrRefCount(dataObj);
      photo = Tk_FindPhoto(infoPtr->interp, Tcl_GetString(dataObj));
      if (width != 0 && height != 0) {
        Tk_PhotoExpand(
#ifdef HAVE_TCL85
			   infoPtr->interp,
#endif
			   photo, width, (height>0) ? height: -height);
        block.width     = width;
        block.height    = 1;
        block.pitch     = width*3;
        block.pixelSize = 3; /* Have DIBs transparency? */
        block.offset[0] = 2;
        block.offset[1] = 1;
        block.offset[2] = 0;
        block.offset[3] = block.offset[0];
        lineSize = ((width*(bpp/8)-1)/4+1)*4;
        
        pColor = (char *) ((LPSTR)bmi + (WORD)(bmi->bmiHeader.biSize));
        pData  = (unsigned char *) pColor + sizeof(RGBQUAD) * colors;
        if (bmi->bmiHeader.biCompression == BI_RGB) {
          /* 
           * Uncompressed. If height is positive, the bitmap is a
           * bottom-up DIB and its origin is the lower-left corner. If
           * height is negative, the bitmap is a top-down DIB and its
           * origin is the upper-left corner. Note that each 3-byte triplet
           * in the bitmap array represents the relative intensities of
           * blue, green, and red, respectively, for a pixel.
           */
          switch (bpp) {
            case 24: {
              for (y=0; y<height; y++) {
                block.pixelPtr  = (unsigned char *) (pData+y*lineSize);
                if (height > 0) {
                      Tk_PhotoPutBlock(
#ifdef HAVE_TCL85
				       infoPtr->interp,
#endif
				       photo, &block, 0, height-y-1, width, 1,
				       TK_PHOTO_COMPOSITE_OVERLAY);
                    } else {
                      Tk_PhotoPutBlock(
#ifdef HAVE_TCL85
				       infoPtr->interp,
#endif
				       photo, &block, 0, y, width, 1,
				       TK_PHOTO_COMPOSITE_OVERLAY);
                }
              }
              break;
            }
            case 8: {
              /*
               * We have to look up pixels in the indexed palette...
               */
              block.pixelPtr = pixelPtr = (unsigned char *)
                                          ckalloc(3*width);
              for(y=0; y<height; y++) {
                for (x=0; x<width; x++) {
                  index = pData[y*lineSize+x];
                  memcpy(pixelPtr, pColor+(4*index),3);
                  pixelPtr += 3;
                }
                if (height > 0) {
                      Tk_PhotoPutBlock(
#ifdef HAVE_TCL85
				       infoPtr->interp,
#endif
				       photo, &block, 0, height-y-1, width, 1,
				       TK_PHOTO_COMPOSITE_OVERLAY);
                    } else {
                      Tk_PhotoPutBlock(
#ifdef HAVE_TCL85
				       infoPtr->interp,
#endif
				       photo, &block, 0, y, width, 1,
				       TK_PHOTO_COMPOSITE_OVERLAY);
                }
                pixelPtr = block.pixelPtr;
              }
              ckfree((char *) block.pixelPtr);
              break;
            }
            case 4: {
              /*
               * We have to look up pixels in the indexed palette...
               */
              block.pixelPtr = pixelPtr = (unsigned char *)
                                          ckalloc(3*width);
              for(y=0; y<height; y++) {
                for (x=0; x<width; x++) {
                  if (x&1) {
                    index = pData[y*lineSize+x/2] & 0x0f;
                  } else {
                    index = pData[y*lineSize+x/2] >> 4;
                  }
                  memcpy(pixelPtr, pColor+(4*index),3);
                  pixelPtr += 3;
                }
                if (height > 0) {
                      Tk_PhotoPutBlock(
#ifdef HAVE_TCL85
				       infoPtr->interp,
#endif
				       photo, &block, 0, height-y-1, width, 1,
				       TK_PHOTO_COMPOSITE_OVERLAY);
                    } else {
                      Tk_PhotoPutBlock(
#ifdef HAVE_TCL85
				       infoPtr->interp,
#endif
				       photo, &block, 0, y, width, 1,
				       TK_PHOTO_COMPOSITE_OVERLAY);
                }
                pixelPtr = block.pixelPtr;
              }
              ckfree((char *) block.pixelPtr);
              break;
            }
            case 1: {
              /*
               * Black and White image. We have to look up pixels in the
               * indexed palette...
               */
              block.pixelPtr = pixelPtr = (unsigned char *)
                                          ckalloc(3*width);
              for(y=0; y<height; y++) {
                for (x=0; x<width; x++) {
                  index = (pData[y*lineSize+x/8] >> (7-(x%8))) & 1;
                  memcpy(pixelPtr, pColor+(4*index),3);
                  pixelPtr += 3;
                }
                if (height > 0) {
                      Tk_PhotoPutBlock(
#ifdef HAVE_TCL85
				       infoPtr->interp,
#endif
				       photo, &block, 0, height-y-1, width, 1,
				       TK_PHOTO_COMPOSITE_OVERLAY);
                    } else {
                      Tk_PhotoPutBlock(
#ifdef HAVE_TCL85
				       infoPtr->interp,
#endif
				       photo, &block, 0, y, width, 1,
				       TK_PHOTO_COMPOSITE_OVERLAY);
                }
                pixelPtr = block.pixelPtr;
              }
              ckfree((char *) block.pixelPtr);
              break;
            }
            default: {
              char tmp_str[128];
              sprintf(tmp_str, "Unsupported Bitmap bits per pixel (%d)!",
                      bpp);
              dataObj = Tcl_NewStringObj(tmp_str, -1);
            }
          } /* switch (bpp) */
        } else if (bmi->bmiHeader.biCompression == BI_BITFIELDS) {
          /*
           * Specifies that the bitmap is not compressed and that the color
           * table consists of three DWORD color masks that specify the
           * red, green, and blue components, respectively, of each pixel.
           * This is valid when used with 16- and 32-bit-per-pixel bitmaps.
           */
          dataObj = 
          Tcl_NewStringObj("Unsupported bitmap format (BI_BITFIELDS)!", -1);
        }
      }
    }
  } else {
    /*********************************************************************
     * ###################################################################
     * #  UNHANDLED :-)
     * ###################################################################
     ********************************************************************/
    XDND_DEBUG2("Unhandled Data type \"%s\"\n", dnd->DesiredTypeStr);
    curr->matchedType = dnd->DesiredType;
    dataObj = TkDND_CreateDataObjAccordingToType(curr, NULL,
                                (unsigned char*) str, strlen(str));
    curr->matchedType = 0;
  }
  Tcl_DStringFree(&dString);
  return dataObj;
}; /* TkDND_DropTarget::GetAndConvertData */

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
          (CONST84 char **) Actions, "action", 0, &index) == TCL_OK) {
    switch ((enum actions) index) {
      case NONE: {
        strcpy(dnd->DesiredAction, "none");
        dnd->CallbackStatus = TCL_BREAK;
        return DROPEFFECT_NONE;
      }
      case DEFAULT: {
        if (strcmp(dnd->DesiredAction, "copy") == 0) {
          wanted_action = DROPEFFECT_COPY;
        } else if (strcmp(dnd->DesiredAction, "move") == 0) {
          wanted_action = DROPEFFECT_MOVE;
        } else if (strcmp(dnd->DesiredAction, "link") == 0) {
          wanted_action = DROPEFFECT_LINK;
        } else {
          wanted_action = DROPEFFECT_COPY;
        }
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
