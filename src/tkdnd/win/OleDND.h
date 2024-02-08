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

#ifndef _OLE_DND_H
#define _OLE_DND_H

#include <windows.h>
#include <ole2.h>

#ifdef DND_ENABLE_DROP_TARGET_HELPER
#include <atlbase.h>
#include <shlobj.h>     /* for IDropTargetHelper */
#include <shlguid.h>
/* We need this declaration for CComPtr, which uses __uuidof() */
struct __declspec(uuid("{4657278B-411B-11d2-839A-00C04FD918D0}"))
  IDropTargetHelper;
#endif /* DND_ENABLE_DROP_TARGET_HELPER */

#include <tcl.h>
#include <tk.h>
#include "tkDND.h"
#include "tkOleDND_TEnumFormatEtc.h"

#ifdef DND_DEBUG
extern FILE *TkDND_Log;
#endif

typedef struct _OLEDND_Struct {
  Tk_Window    MainWindow;         /* The main window of our application */
  Tcl_Interp  *interp;             /* A Tcl Interpreter */
  Display     *display;            /* Display Pointer */
  int x;                           /* Current position of the mouse */
  int y;                           /* Current position of the mouse */
  int button;                      /* Current button used for drag operation */
  Tk_Window    CursorWindow;       /* A window to replace cursor */
  char *       CursorCallback;     /* A Callback to update cursor window */
  
  Tk_Window    DraggerWindow;      /* Window of the drag source */
  DWORD        DraggerActions;     /* Actions supported by the drag source */
  Tcl_DString  DraggerTypes;       /* The list of types of the drag source */
  CLIPFORMAT   DesiredType;        /* The drop desired type */
  char        *DesiredTypeStr;     /* The drop desired type (string) */
  char         DesiredAction[10];  /* The drop desired action */
  int          CallbackStatus;     /* The return value of last tcl callback */
  Tcl_Obj     *data;               /* The object contained data to be dropped */
  int          length;             /* length of the data */

  /* Some useful CLIPFORMATS... */
  CLIPFORMAT   UniformResourceLocator; /* Netscape, IE */
  CLIPFORMAT   FileName;               /* Windows Explorer */
  CLIPFORMAT   HTML_Format;            /* Word, IE */
  CLIPFORMAT   RichTextFormat;         /* Word, IE */
  CLIPFORMAT   FileGroupDescriptor;    /* Explorer, files not in the file */
  CLIPFORMAT   FileGroupDescriptorW;   /* system */
} OleDND;
#define DndClass OleDND

/*****************************************************************************
 * Drop Source Related Class.
 ****************************************************************************/
class TkDND_DropSource: public IDropSource {
  private:
    ULONG                m_refCnt;    /* Reference count */
    DndInfo             *infoPtr;     /* Pointer to hash table entry */

  public:
    TkDND_DropSource(DndInfo *infoPtr);
   ~TkDND_DropSource(void);
   
    /* IUnknown interface members */
    STDMETHODIMP         QueryInterface(REFIID, LPVOID *);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    /* IDropSource interface members */
    STDMETHODIMP         QueryContinueDrag(BOOL, DWORD);
    STDMETHODIMP         GiveFeedback(DWORD);
}; /* TkDND_DropSource */

/*****************************************************************************
 * Data object Related Class (needed by Drag Source for OLE DND)...
 ****************************************************************************/
class TkDND_DataObject: public IDataObject {
   private:
     ULONG                m_refCnt;    /* Reference Count */
     DndInfo             *infoPtr;     /* Pointer to hash table entry */

     /* The clipboard formats that can be handled */
     UINT                 m_numTypes;  /* Number of types in list */
     UINT                 m_maxTypes;  /* Number of types that fit */
     FORMATETC           *m_typeList;  /* List of types */

   public:
     TkDND_DataObject(DndInfo *infoPtr);
    ~TkDND_DataObject(void);

     /* IUnknown interface members */
     STDMETHODIMP         QueryInterface(REFIID, LPVOID *);
     STDMETHODIMP_(ULONG) AddRef(void);
     STDMETHODIMP_(ULONG) Release(void);

     /* IDataObject interface methods */
     STDMETHODIMP         GetData(LPFORMATETC, LPSTGMEDIUM);
     STDMETHODIMP         GetDataHere(LPFORMATETC, LPSTGMEDIUM);
     STDMETHODIMP         QueryGetData(LPFORMATETC);
     STDMETHODIMP         GetCanonicalFormatEtc(LPFORMATETC, LPFORMATETC);
     STDMETHODIMP         SetData(LPFORMATETC, LPSTGMEDIUM, BOOL);
     STDMETHODIMP         EnumFormatEtc(DWORD, LPENUMFORMATETC *);
     STDMETHODIMP         DAdvise(LPFORMATETC, DWORD, LPADVISESINK, DWORD *);
     STDMETHODIMP         DUnadvise(DWORD);
     STDMETHODIMP         EnumDAdvise(IEnumSTATDATA **);

     /* TkDND additional interface methods */
     int                  AddDataType(UINT clipFormat);
     int                  DelDataType(UINT clipFormat);
}; /* TkDND_DataObject */

/*****************************************************************************
 * Drop Target Related Class.
 ****************************************************************************/
class TkDND_DropTarget;
typedef class TkDND_DropTarget *PTDropTarget;
class TkDND_DropTarget: public IDropTarget {
  private:
    ULONG                m_refCnt;    /* Reference count */
    DndInfo             *infoPtr;     /* Pointer to hash table entry */
    DWORD                KeyState;    /* Remember KeyState for <DragLeave> */
    LPDATAOBJECT         DataObject;  /* Keep data object available */
#ifdef DND_ENABLE_DROP_TARGET_HELPER
    CComPtr<IDropTargetHelper> DropHelper; /* IDropTargetHelper support. This
                                              helper does some interesting
                                              things, like drawing explorer
                                              icons during drops... */
    int UseDropHelper;                     /* A flag whether to use the helper
                                              or not... */
#endif /* DND_ENABLE_DROP_TARGET_HELPER */

  public:
    TkDND_DropTarget(DndInfo *info);
   ~TkDND_DropTarget(void);
   
    /* IUnknown interface members */
    STDMETHODIMP         QueryInterface(REFIID, LPVOID *);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    /* IDropTarget interface members */
    STDMETHODIMP         DragEnter(LPDATAOBJECT, DWORD, POINTL,LPDWORD);
    STDMETHODIMP         DragOver(DWORD, POINTL, LPDWORD);
    STDMETHODIMP         DragLeave(void);
    STDMETHODIMP         Drop(LPDATAOBJECT, DWORD, POINTL, LPDWORD);

    /* TkDND additional interface methods */
    DWORD                ParseAction(void);
    Tcl_Obj             *GetAndConvertData(LPDATAOBJECT,
                            DndType *, char *, FORMATETC *, STGMEDIUM *);
}; /* TkDND_DropTarget */

#endif _OLE_DND_H
