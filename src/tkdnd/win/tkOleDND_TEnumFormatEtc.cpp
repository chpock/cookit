/*
 *----------------------------------------------------------------------
 * Filename: tenumformatetc.cpp
 *
 *	Implements the TEnumFormatEtc class, an IEnumFormatEtc object
 *
 * Copyright (c) 1995 LAS Incorporated.
 * All rights reserved.
 *
 * Copyright (c) 1995 University of California, Berkeley.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 * 
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * IN NO EVENT SHALL LAS INCORPORATED BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * LAS INCORPORATED SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND LAS INCORPORATED HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *----------------------------------------------------------------------
 */

#include <windows.h>
#include <ole2.h>
#include "tkOleDND_TEnumFormatEtc.h"

/*
 *----------------------------------------------------------------------
 * CreateFormatEtcEnumerator --
 *
 *	Creates an enumerator.
 *
 * Arguments:
 *	pUnknownObj	The object this enumerator is associated with
 *	numFormats	The number of format types passed in
 *	formatList	List of the formats.
 *----------------------------------------------------------------------
 */
LPENUMFORMATETC
CreateFormatEtcEnumerator(LPUNKNOWN pUnknownObj, ULONG numFormats,
			  LPFORMATETC formatList)
{
    LPENUMFORMATETC pNew;

    pNew = new TEnumFormatEtc(pUnknownObj, numFormats, formatList);
    if (pNew == NULL) {
	return NULL;
    }

    pNew->AddRef();
    pUnknownObj->AddRef();

    return pNew;
}

/*
 *----------------------------------------------------------------------
 * TEnumFormatEtc::TEnumFormatEtc
 *
 *	Constructor
 *----------------------------------------------------------------------
 */
TEnumFormatEtc::TEnumFormatEtc(LPUNKNOWN pUnknownObj, ULONG numFormats,
			       LPFORMATETC formatList)
{
    UINT i;

    m_refCnt = 0;
    m_currElement = 0;

    m_pUnknownObj = pUnknownObj;
    m_numFormats = numFormats;

    m_formatList = new FORMATETC[numFormats];

    if (m_formatList != NULL) {
        for (i=0; i < numFormats; i++) {
	    m_formatList[i] = formatList[i];
	}
    }
    pUnknownObj->AddRef();
}

/*
 *----------------------------------------------------------------------
 * TEnumFormatEtc::~TEnumFormatEtc
 *
 *	Destructor
 *----------------------------------------------------------------------
 */
TEnumFormatEtc::~TEnumFormatEtc(void)
{
    if (m_formatList == NULL) {
        delete [] m_formatList;
    }
}

/*
 *----------------------------------------------------------------------
 * TEnumFormatEtc::QueryInterface
 *
 *	Does interface negotiation.  IUnknown and IEnumFORMATETC are
 *	the supported interfaces.
 *
 * Arguments:
 *
 *	REFIID riid	A reference to the interface being queried.
 *      LPVOID *ppvObj	Out parameter to return a pointer to interface.
 *
 * Results:
 *	S_OK if interface is supported, E_NOINTERFACE if it is not
 *
 *----------------------------------------------------------------------
 */
STDMETHODIMP
TEnumFormatEtc::QueryInterface(REFIID riid, LPVOID *ppv)
{
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IEnumFORMATETC))
    {
        *ppv = (LPVOID) this;
	((LPUNKNOWN)*ppv)->AddRef();

	return S_OK;
    }

    *ppv = NULL;
    return ResultFromScode(E_NOINTERFACE);
}

/*
 *----------------------------------------------------------------------
 * TEnumFormatEtc::AddRef
 *
 *	Increment the reference count on the object as well as on
 *	the unknown object this enumerator is linked to.
 *
 * Results:
 *	The current reference count on the object.
 *----------------------------------------------------------------------
 */
STDMETHODIMP_(ULONG)
TEnumFormatEtc::AddRef(void)
{
    m_refCnt++;
    m_pUnknownObj->AddRef();
    return m_refCnt;
}

/*
 *----------------------------------------------------------------------
 * TEnumFormatEtc::Release
 *
 *	Decrement the reference count on the object as well as on
 *	the unknown object this enumerator is linked to.  If the
 *	reference count becomes 0, the object is deleted.
 *
 * Results:
 *	The current reference count on the object.
 *----------------------------------------------------------------------
 */
STDMETHODIMP_(ULONG)
TEnumFormatEtc::Release(void)
{
    m_refCnt--;
    m_pUnknownObj->Release();

    if (m_refCnt == 0L) {
        delete this;
	return 0L;
    }

    return m_refCnt;
}

/*
 *----------------------------------------------------------------------
 * TEnumFormatEtc::Next
 *
 *	Retrieves the specified number of items in the enumeration
 *	sequence
 *
 * Arguments:
 *	numFormats	Maximum number of FORMATETCs to return.
 *	pOutList	Where to store the outgoing data
 *	numOut		Returns how many were enumerated.
 *
 * Return Value:
 *	HRESULT		S_OK if successful, S_FALSE otherwise,
 *----------------------------------------------------------------------
 */
STDMETHODIMP
TEnumFormatEtc::Next(ULONG numFormats, LPFORMATETC pOutList, ULONG *pNumOut)
{
    ULONG i;
    ULONG               cReturn=0L;

    if (pNumOut) {
	*pNumOut = 0L;
    }
    if (m_formatList == NULL) {
        return ResultFromScode(S_FALSE);
    }

    if (pOutList == NULL) {
	if (numFormats == 1) {
	    return ResultFromScode(S_FALSE);
	} else {
	    return ResultFromScode(E_POINTER);
	}
    }

    if (m_currElement >= m_numFormats) {
        return ResultFromScode(S_FALSE);
    }

    for (i = 0; i < numFormats && m_currElement < m_numFormats;
	 i++, m_currElement++)
    {
        *pOutList = m_formatList[m_currElement];
	pOutList++;
    }

    if (pNumOut != NULL) {
	*pNumOut = i;
    }

    return S_OK;
}

/*
 *----------------------------------------------------------------------
 * TEnumFormatEtc::Skip
 *
 *	Skips over a specified number of elements in the enumeration
 *	sequence.
 *
 * Arguments:
 *	numSkip		ULONG number of elements to skip.
 *
 * Result:
 *	S_OK if successful, S_FALSE if we could not skip enough elements
 *----------------------------------------------------------------------
 */
STDMETHODIMP
TEnumFormatEtc::Skip(ULONG numSkip)
{
    m_currElement += numSkip;

    if (m_currElement > m_numFormats) {
	m_currElement = m_numFormats;
        return ResultFromScode(S_FALSE);
    }
    return S_OK;
}

/*
 *----------------------------------------------------------------------
 * TEnumFormatEtc::Reset
 *
 *	Resets the enumeration sequence to the beginning
 *
 * Result:
 *	S_OK
 *----------------------------------------------------------------------
 */
STDMETHODIMP
TEnumFormatEtc::Reset(void)
{
    m_currElement = 0L;
    return S_OK;
}

/*
 *----------------------------------------------------------------------
 * TEnumFormatEtc::Clone
 *
 *	Returns another enumerator containing the same state as the
 *	current container.
 *
 * Arguments:
 *	ppNewObject	New object is returned through here
 *
 * Result:
 *	S_OK if all's well that ends well, else an error value.
 *----------------------------------------------------------------------
 */
STDMETHODIMP
TEnumFormatEtc::Clone(LPENUMFORMATETC *ppNewObject)
{
    PTEnumFormatEtc pNew;

    pNew = new TEnumFormatEtc(m_pUnknownObj, m_numFormats, m_formatList);

    *ppNewObject = pNew;
    if (pNew == NULL) {
        return ResultFromScode(E_OUTOFMEMORY);
    }
    pNew->AddRef();
    pNew->m_currElement = m_currElement;

    return S_OK;
}
