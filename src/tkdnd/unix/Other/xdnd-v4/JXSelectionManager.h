/******************************************************************************
 JXSelectionManager.h

	Copyright © 1996-99 by John Lindal. All rights reserved.

 ******************************************************************************/

#ifndef _H_JXSelectionManager
#define _H_JXSelectionManager

#include <JPtrArray-JString.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

class JXDisplay;
class JXWindow;
class JXWidget;

const Atom kJXClipboardName = XA_PRIMARY;

class JXSelectionManager : virtual public JBroadcaster
{
	friend class JXDNDManager;

public:

	enum DeleteMethod
	{
		kArrayDelete,
		kXFree,
		kCFree
	};

	class Data;

public:

	JXSelectionManager(JXDisplay* display);

	virtual ~JXSelectionManager();

	JBoolean	GetAvailableTypes(const Atom selectionName, const Time time,
								  JArray<Atom>* typeList);
	JBoolean	GetData(const Atom selectionName, const Time time,
						const Atom requestType, Atom* returnType,
						unsigned char** data, JSize* dataLength,
						DeleteMethod* delMethod);
	void		DeleteData(unsigned char** data, const DeleteMethod delMethod);

	void	SendDeleteRequest(const Atom selectionName, const Time time);

	JBoolean	GetData(const Atom selectionName, const Time time,
						const Data** data);
	JBoolean	SetData(const Atom selectionName, Data* data);
	void		ClearData(const Atom selectionName, const Time endTime);

	Atom	GetTargetsXAtom() const;
	Atom	GetTimeStampXAtom() const;
	Atom	GetTextXAtom() const;
	Atom	GetCompoundTextXAtom() const;
	Atom	GetMultipleXAtom() const;
	Atom	GetMimePlainTextXAtom() const;
	Atom	GetURLXAtom() const;

	Atom	GetDeleteSelectionXAtom() const;
	Atom	GetNULLXAtom() const;

	// called by JXDisplay

	void	HandleSelectionRequest(const XSelectionRequestEvent& selReqEvent);

protected:

	virtual void	ReceiveWithFeedback(JBroadcaster* sender, Message* message);

private:

	JXDisplay*			itsDisplay;		// owns us
	Window				itsDataWindow;
	JPtrArray<Data>*	itsDataList;	// current + recent

	JSize		itsMaxDataChunkSize;	// max # of 4-byte blocks that we can send
	JBoolean	itsReceivedAllocErrorFlag;
	Window		itsTargetWindow;
	JBoolean	itsTargetWindowDeletedFlag;

	Atom itsSelectionWindPropXAtom;		// for receiving selection data
	Atom itsIncrementalSendXAtom;		// for sending data incrementally

	Atom itsTargetsXAtom;				// returns type XA_ATOM
	Atom itsTimeStampXAtom;				// returns type XA_INTEGER
	Atom itsTextXAtom;					//  8-bit characters
	Atom itsCompoundTextXAtom;			// 16-bit characters
	Atom itsMultipleXAtom;				// several formats at once
	Atom itsMimePlainTextXAtom;			// text/plain
	Atom itsURLXAtom;					// text/uri-list
	Atom itsDeleteSelectionXAtom;		// returns type "NULL"
	Atom itsNULLXAtom;					// "NULL"

private:

	JBoolean	RequestData(const Atom selectionName, const Time time,
							const Atom type, XSelectionEvent* selEvent);

	void		SendData(const Window requestor, const Atom property,
						 const Atom type, unsigned char* data,
						 const JSize dataLength, const JSize bitsPerBlock,
						 XEvent* returnEvent);
	JBoolean	SendData1(const Window requestor, const Atom property,
						  const Atom type, unsigned char* data,
						  const JSize dataLength, const JSize bitsPerBlock);
	JBoolean	WaitForPropertyDeleted(const Window xWindow, const Atom property);
	static Bool	GetNextPropDeletedEvent(Display* display, XEvent* event, char* arg);

	JBoolean	ReceiveDataIncr(const Atom selectionName,
								Atom* returnType, unsigned char** data,
								JSize* dataLength, DeleteMethod* delMethod);
	JBoolean	WaitForPropertyCreated(const Window xWindow, const Atom property,
									   const Window sender);
	static Bool	GetNextNewPropertyEvent(Display* display, XEvent* event, char* arg);

	JBoolean	GetData(const Atom selectionName, const Time time,
						Data** data, JIndex* index = NULL);
	void		DeleteOutdatedData();

	// called by JXDNDManager

	Window	GetDataTransferWindow();

	// not allowed

	JXSelectionManager(const JXSelectionManager& source);
	const JXSelectionManager& operator=(const JXSelectionManager& source);

public:

	class Data : public JBroadcaster
		{
		public:

			Data(JXDisplay* display);
			Data(JXWidget* widget, const JCharacter* id);

			virtual	~Data();

			JXDisplay*			GetDisplay() const;
			JXSelectionManager*	GetSelectionManager() const;
			JXDNDManager*		GetDNDManager() const;
			Atom				GetDNDSelectionName() const;

			Atom		GetSelectionName() const;
			Time		GetStartTime() const;
			JBoolean	IsCurrent() const;
			JBoolean	GetEndTime(Time* t) const;
			void		SetSelectionInfo(const Atom selectionName, const Time startTime);
			void		SetEndTime(const Time endTime);

			const JArray<Atom>&	GetTypeList() const;

			void		Resolve() const;
			JBoolean	Convert(const Atom requestType, Atom* returnType,
								unsigned char** data, JSize* dataLength,
								JSize* bitsPerBlock) const;

		protected:

			Atom	AddType(const JCharacter* name);
			void	AddType(const Atom type);
			void	RemoveType(const Atom type);

			virtual void		AddTypes(const Atom selectionName) = 0;
			virtual JBoolean	ConvertData(const Atom requestType, Atom* returnType,
											unsigned char** data, JSize* dataLength,
											JSize* bitsPerBlock) const = 0;

			virtual void	ReceiveGoingAway(JBroadcaster* sender);

		private:

			JXDisplay*		itsDisplay;			// not owned
			Atom			itsSelectionName;
			Time			itsStartTime;
			Time			itsEndTime;
			JArray<Atom>*	itsTypeList;		// sorted

			// used for delayed evaluation -- NULL after Resolve()

			JXWidget*	itsDataSource;			// not owned
			JString*	itsDataSourceID;

		private:

			void	DataX();

			static JOrderedSetT::CompareResult
				CompareAtoms(const Atom& atom1, const Atom& atom2);

			// not allowed

			Data(const Data& source);
			const Data& operator=(const Data& source);
		};
};


/******************************************************************************
 Get atom

 ******************************************************************************/

inline Atom
JXSelectionManager::GetTargetsXAtom()
	const
{
	return itsTargetsXAtom;
}

inline Atom
JXSelectionManager::GetTimeStampXAtom()
	const
{
	return itsTimeStampXAtom;
}

inline Atom
JXSelectionManager::GetTextXAtom()
	const
{
	return itsTextXAtom;
}

inline Atom
JXSelectionManager::GetCompoundTextXAtom()
	const
{
	return itsCompoundTextXAtom;
}

inline Atom
JXSelectionManager::GetMultipleXAtom()
	const
{
	return itsMultipleXAtom;
}

inline Atom
JXSelectionManager::GetMimePlainTextXAtom()
	const
{
	return itsMimePlainTextXAtom;
}

inline Atom
JXSelectionManager::GetURLXAtom()
	const
{
	return itsURLXAtom;
}

inline Atom
JXSelectionManager::GetDeleteSelectionXAtom()
	const
{
	return itsDeleteSelectionXAtom;
}

inline Atom
JXSelectionManager::GetNULLXAtom()
	const
{
	return itsNULLXAtom;
}

/******************************************************************************
 GetDataTransferWindow (private)

 ******************************************************************************/

inline Window
JXSelectionManager::GetDataTransferWindow()
{
	return itsDataWindow;
}

/******************************************************************************
 GetDisplay

 ******************************************************************************/

inline JXDisplay*
JXSelectionManager::Data::GetDisplay()
	const
{
	return itsDisplay;
}

/******************************************************************************
 GetSelectionName

 ******************************************************************************/

inline Atom
JXSelectionManager::Data::GetSelectionName()
	const
{
	return itsSelectionName;
}

/******************************************************************************
 GetStartTime

 ******************************************************************************/

inline Time
JXSelectionManager::Data::GetStartTime()
	const
{
	return itsStartTime;
}

/******************************************************************************
 IsCurrent

 ******************************************************************************/

inline JBoolean
JXSelectionManager::Data::IsCurrent()
	const
{
	return JI2B( itsEndTime == CurrentTime );
}

/******************************************************************************
 End time

 ******************************************************************************/

inline JBoolean
JXSelectionManager::Data::GetEndTime
	(
	Time* t
	)
	const
{
	*t = itsEndTime;
	return JI2B( itsEndTime != CurrentTime );
}

inline void
JXSelectionManager::Data::SetEndTime
	(
	const Time endTime
	)
{
	itsEndTime = endTime - 1;	// given time is when it changed
}

/******************************************************************************
 GetTypeList

 ******************************************************************************/

inline const JArray<Atom>&
JXSelectionManager::Data::GetTypeList()
	const
{
	return *itsTypeList;
}

/******************************************************************************
 AddType (protected)

	Add the target to the list if it is not already included.

 ******************************************************************************/

inline void
JXSelectionManager::Data::AddType
	(
	const Atom type
	)
{
	itsTypeList->InsertSorted(type, kFalse);
}

#endif
