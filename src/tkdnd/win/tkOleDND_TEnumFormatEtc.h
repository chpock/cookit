/*
 *----------------------------------------------------------------------
 * File: tenumformatetc.h
 *
 *	This file defines the TEnumFormatEtc, an enerator class for the
 *	IEnumFORMATETC object.  The IEnumFORMATETC object has the standard
 *	enumerator interface.   The only creation oddity it has it the
 *	IUnknown interface it takes as its first parameter.  This allows
 *	the object to increase the reference count of the object that is
 *	being enumerated.  Only after this enumerator is destroyed can the
 *	object that created it be destroyed.  This object should only be
 *	created by the IDataObject::EnumFormatEtc function.
 *----------------------------------------------------------------------
 */
 
#ifndef _TENUMFORMATETC_H_
#define _TENUMFORMATETC_H_

class TEnumFormatEtc;
typedef class TEnumFormatEtc *PTEnumFormatEtc;

class TEnumFormatEtc: public IEnumFORMATETC
{
  private:
    ULONG           m_refCnt;		/* Reference count */
    LPUNKNOWN       m_pUnknownObj;	/* IUnknown for ref counting */
    ULONG           m_currElement;	/* Current element */
    ULONG           m_numFormats;	/* Number of FORMATETCs in us */
    LPFORMATETC	    m_formatList;	/* List of formats */

  public:
    TEnumFormatEtc(LPUNKNOWN, ULONG, LPFORMATETC);
    ~TEnumFormatEtc(void);

    /* IUnknown members */
    STDMETHODIMP	 QueryInterface(REFIID, LPVOID *);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    /* IEnumFORMATETC members */
    STDMETHODIMP	Next(ULONG, LPFORMATETC, ULONG *);
    STDMETHODIMP	Skip(ULONG);
    STDMETHODIMP	Reset(void);
    STDMETHODIMP	Clone(IEnumFORMATETC **);
};

extern LPENUMFORMATETC
CreateFormatEtcEnumerator(LPUNKNOWN, ULONG, LPFORMATETC);

#endif /* _TENUMFORMATETC_H_ */
