/******************************************************************************
 JXSelectionManager.cc

	Global object to interface with X Selection mechanism.

	Nobody else seems to support MULTIPLE, so I see no reason to support
	it here.  If we ever need to support it, here is the basic idea:

		Pass in filled JArray<Atom>* and empty JArray<char*>*
		We remove the unconverted types from JArray<Atom> and
			fill in the JArray<char*> with the converted data

	BASE CLASS = virtual JBroadcaster

	Copyright © 1996-99 by John Lindal. All rights reserved.

 ******************************************************************************/

#include <JXSelectionManager.h>
#include <JXDNDManager.h>
#include <JXDisplay.h>
#include <JXWindow.h>
#include <JXWidget.h>
#include <jXGlobals.h>
#include <jTime.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <X11/Xlib.h>
#include <jAssert.h>

#define JXSEL_DEBUG_MSGS		0	// boolean
#define JXSEL_DEBUG_ONLY_RESULT 0	// boolean
#define JXSEL_MICRO_TRANSFER	0	// boolean

const Time kHistoryInterval = 60000;	// 1 minute (milliseconds)

const clock_t kWaitForSelectionTime = 5 * CLOCKS_PER_SEC;
const clock_t kUserBoredWaitingTime = 1 * CLOCKS_PER_SEC;

static const JCharacter* kSWPXAtomName             = "JXSelectionWindowProperty";
static const JCharacter* kIncrementalXAtomName     = "INCR";
static const JCharacter* kTargetsXAtomName         = "TARGETS";
static const JCharacter* kTimeStampXAtomName       = "TIMESTAMP";
static const JCharacter* kTextXAtomName            = "TEXT";
static const JCharacter* kCompoundTextXAtomName    = "COMPOUND_TEXT";
static const JCharacter* kMultipleXAtomName        = "MULTIPLE";
static const JCharacter* kMimePlainTextXAtomName   = "text/plain";
static const JCharacter* kURLXAtomName             = "text/uri-list";
static const JCharacter* kDeleteSelectionXAtomName = "DELETE";
static const JCharacter* kNULLXAtomName            = "NULL";

/******************************************************************************
 Constructor

 ******************************************************************************/

JXSelectionManager::JXSelectionManager
	(
	JXDisplay* display
	)
{
	itsDisplay = display;

	itsDataList = new JPtrArray<Data>;
	assert( itsDataList != NULL );

	itsMaxDataChunkSize = XMaxRequestSize(*display) * 4/5;

	itsReceivedAllocErrorFlag  = kFalse;
	itsTargetWindow            = None;
	itsTargetWindowDeletedFlag = kFalse;

	// create data transfer window

	const unsigned long valueMask = CWOverrideRedirect | CWEventMask;

	XSetWindowAttributes attr;
	attr.override_redirect = kTrue;
	attr.event_mask        = PropertyChangeMask;

	itsDataWindow = XCreateSimpleWindow(*itsDisplay, itsDisplay->GetRootWindow(),
										0,0,10,10, 0, 0,0);
	XChangeWindowAttributes(*itsDisplay, itsDataWindow, valueMask, &attr);

	// create required X atoms

	itsSelectionWindPropXAtom = itsDisplay->RegisterXAtom(kSWPXAtomName);
	itsIncrementalSendXAtom   = itsDisplay->RegisterXAtom(kIncrementalXAtomName);

	itsTargetsXAtom           = itsDisplay->RegisterXAtom(kTargetsXAtomName);
	itsTimeStampXAtom         = itsDisplay->RegisterXAtom(kTimeStampXAtomName);
	itsTextXAtom              = itsDisplay->RegisterXAtom(kTextXAtomName);
	itsCompoundTextXAtom      = itsDisplay->RegisterXAtom(kCompoundTextXAtomName);
	itsMultipleXAtom          = itsDisplay->RegisterXAtom(kMultipleXAtomName);
	itsMimePlainTextXAtom     = itsDisplay->RegisterXAtom(kMimePlainTextXAtomName);
	itsURLXAtom               = itsDisplay->RegisterXAtom(kURLXAtomName);

	itsDeleteSelectionXAtom   = itsDisplay->RegisterXAtom(kDeleteSelectionXAtomName);
	itsNULLXAtom              = itsDisplay->RegisterXAtom(kNULLXAtomName);
}

/******************************************************************************
 Destructor

 ******************************************************************************/

JXSelectionManager::~JXSelectionManager()
{
	itsDataList->DeleteAll();
	delete itsDataList;

	XDestroyWindow(*itsDisplay, itsDataWindow);
}

/******************************************************************************
 GetAvailableTypes

	time can be CurrentTime.

 ******************************************************************************/

JBoolean
JXSelectionManager::GetAvailableTypes
	(
	const Atom		selectionName,
	const Time		time,
	JArray<Atom>*	typeList
	)
{
	// Check if this application owns the selection.

	Data* data = NULL;
	if (GetData(selectionName, time, &data))
		{
		*typeList = data->GetTypeList();
		return kTrue;
		}

	// We have to go via the X server.

	typeList->RemoveAll();

	XSelectionEvent selEvent;
	if (RequestData(selectionName, time, itsTargetsXAtom, &selEvent))
		{
		Atom actualType;
		int actualFormat;
		unsigned long itemCount, remainingBytes;
		unsigned char* data = NULL;
		XGetWindowProperty(*itsDisplay, itsDataWindow, itsSelectionWindPropXAtom,
						   0, LONG_MAX, True, XA_ATOM,
						   &actualType, &actualFormat,
						   &itemCount, &remainingBytes, &data);

		if (actualType == XA_ATOM &&
			actualFormat/8 == sizeof(Atom) && remainingBytes == 0)
			{
			itemCount /= actualFormat/8;

			Atom* atomData = reinterpret_cast<Atom*>(data);
			for (JIndex i=1; i<=itemCount; i++)
				{
				typeList->AppendElement(atomData[i-1]);
				}

			XFree(data);
			return kTrue;
			}
		else
			{
//			cout << "TARGETS returned " << XGetAtomName(*itsDisplay, actualType) << endl;
//			cout << "  format:    " << actualFormat/8 << endl;
//			cout << "  count:     " << itemCount << endl;
//			cout << "  remaining: " << remainingBytes << endl;

			XFree(data);
			// fall through
			}
		}

	if (selectionName == kJXClipboardName &&
		XGetSelectionOwner(*itsDisplay, kJXClipboardName) != None)
		{
		// compensate for brain damage in rxvt, etc.

//		cout << "XA_PRIMARY owner: " << XGetSelectionOwner(*itsDisplay, XA_PRIMARY) << endl;
//		cout << "XA_SECONDARY owner: " << XGetSelectionOwner(*itsDisplay, XA_SECONDARY) << endl;
//		cout << "CLIPBOARD owner: " << XGetSelectionOwner(*itsDisplay, XInternAtom(*itsDisplay, "CLIPBOARD", False)) << endl;

		typeList->AppendElement(XA_STRING);
		return kTrue;
		}
	else
		{
		return kFalse;
		}
}

/******************************************************************************
 GetData

	time can be CurrentTime.

	*** Caller is responsible for calling DeleteData() on the data
		that is returned.

 ******************************************************************************/

JBoolean
JXSelectionManager::GetData
	(
	const Atom		selectionName,
	const Time		time,
	const Atom		requestType,
	Atom*			returnType,
	unsigned char**	data,
	JSize*			dataLength,
	DeleteMethod*	delMethod
	)
{
	// Check if this application owns the selection.

	Data* localData = NULL;
	JSize bitsPerBlock;
	if (GetData(selectionName, time, &localData))
		{
		if (localData->Convert(requestType, returnType,
							   data, dataLength, &bitsPerBlock))
			{
			*delMethod = kArrayDelete;
			return kTrue;
			}
		else
			{
			*returnType = None;
			*data       = NULL;
			*dataLength = 0;
			return kFalse;
			}
		}

	// We have to go via the X server.

	*returnType = None;
	*data       = NULL;
	*dataLength = 0;
	*delMethod  = kXFree;

	XSelectionEvent selEvent;
	if (RequestData(selectionName, time, requestType, &selEvent))
		{
		#if JXSEL_DEBUG_MSGS && ! JXSEL_DEBUG_ONLY_RESULT
		cout << "Received SelectionNotify" << endl;
		#endif

		// We need to discard all existing PropertyNotify events
		// before initiating the incremental transfer.
		{
		XEvent xEvent;
		XID checkIfEventData[] = { itsDataWindow, itsSelectionWindPropXAtom };
		while (XCheckIfEvent(*itsDisplay, &xEvent, GetNextNewPropertyEvent,
							 reinterpret_cast<char*>(checkIfEventData)))
			{
			// ignore the event
			}
		}

		// Initiate incremental transfer by deleting the property.

		int actualFormat;
		unsigned long itemCount, remainingBytes;
		XGetWindowProperty(*itsDisplay, itsDataWindow, itsSelectionWindPropXAtom,
						   0, LONG_MAX, True, AnyPropertyType,
						   returnType, &actualFormat,
						   &itemCount, &remainingBytes, data);

		if (*returnType == itsIncrementalSendXAtom)
			{
			XFree(*data);
			return ReceiveDataIncr(selectionName, returnType,
								   data, dataLength, delMethod);
			}
		else if (*returnType != None && remainingBytes == 0)
			{
			*dataLength = itemCount * actualFormat/8;
			return kTrue;
			}
		else
			{
			XFree(*data);
			*data = NULL;
			return kFalse;
			}
		}
	else
		{
		return kFalse;
		}
}

/******************************************************************************
 DeleteData

	This must be called with all data returned by GetData().
	The DeleteMethod must be the method returned by GetData().

 ******************************************************************************/

void
JXSelectionManager::DeleteData
	(
	unsigned char**		data,
	const DeleteMethod	delMethod
	)
{
	if (delMethod == kXFree)
		{
		XFree(*data);
		}
	else if (delMethod == kCFree)
		{
		free(*data);
		}
	else
		{
		assert( delMethod == kArrayDelete );
		delete [] *data;
		}

	*data = NULL;
}

/******************************************************************************
 SendDeleteRequest

	Implements the DELETE selection protocol.

	window should be any one that X can attach the data to.
	Widgets can simply pass in the result from GetWindow().

	time can be CurrentTime.

 ******************************************************************************/

void
JXSelectionManager::SendDeleteRequest
	(
	const Atom	selectionName,
	const Time	time
	)
{
	Atom returnType;
	unsigned char* data = NULL;
	JSize dataLength;
	JXSelectionManager::DeleteMethod delMethod;

	GetData(selectionName, time, itsDeleteSelectionXAtom,
			&returnType, &data, &dataLength, &delMethod);
	DeleteData(&data, delMethod);
}

/******************************************************************************
 RequestData (private)

 ******************************************************************************/

JBoolean
JXSelectionManager::RequestData
	(
	const Atom			selectionName,
	const Time			origTime,
	const Atom			type,
	XSelectionEvent*	selEvent
	)
{
	assert( type != None );

	JXDNDManager* dndMgr = itsDisplay->GetDNDManager();

	Time time = origTime;
	if (time == CurrentTime)
		{
		time = itsDisplay->GetLastEventTime();
		}

	XConvertSelection(*itsDisplay, selectionName, type,
					  itsSelectionWindPropXAtom, itsDataWindow, time);

	Bool receivedEvent = False;
	XEvent xEvent;
	const clock_t startTime = clock();
	const clock_t endTime   = startTime + kWaitForSelectionTime;
	JBoolean userBored      = kFalse;
	while (!receivedEvent && clock() < endTime)
		{
		receivedEvent =
			XCheckTypedWindowEvent(*itsDisplay, itsDataWindow,
								   SelectionNotify, &xEvent);

		if (!userBored && clock() > startTime + kUserBoredWaitingTime)
			{
			userBored = kTrue;
			(JXGetApplication())->DisplayBusyCursor();
			}
		}

	if (receivedEvent)
		{
		assert( xEvent.type == SelectionNotify );
		*selEvent = xEvent.xselection;
		return JI2B(selEvent->requestor == itsDataWindow &&
					selEvent->selection == selectionName &&
					selEvent->target    == type &&
					selEvent->property  == itsSelectionWindPropXAtom );
		}
	else
		{
		return kFalse;
		}
}

/******************************************************************************
 HandleSelectionRequest

 ******************************************************************************/

void
JXSelectionManager::HandleSelectionRequest
	(
	const XSelectionRequestEvent& selReqEvent
	)
{
	DeleteOutdatedData();

	XEvent xEvent;
	XSelectionEvent& returnEvent = xEvent.xselection;

	returnEvent.type      = SelectionNotify;
	returnEvent.display   = selReqEvent.display;
	returnEvent.requestor = selReqEvent.requestor;
	returnEvent.selection = selReqEvent.selection;
	returnEvent.target    = selReqEvent.target;
	returnEvent.property  = selReqEvent.property;
	returnEvent.time      = selReqEvent.time;

	if (returnEvent.property == None)
		{
		returnEvent.property = returnEvent.target;
		}

	Atom selectionName   = selReqEvent.selection;
	JXDNDManager* dndMgr = itsDisplay->GetDNDManager();
	if (selectionName == kJXClipboardName &&
		dndMgr->IsLastFakePasteTime(selReqEvent.time))
		{
		selectionName = dndMgr->GetDNDSelectionName();
		}

	Data* selData = NULL;
	if (GetData(selectionName, selReqEvent.time, &selData))
		{
		Atom returnType;
		unsigned char* data;
		JSize dataLength;
		JSize bitsPerBlock;
		if (selData->Convert(selReqEvent.target,
							 &returnType, &data, &dataLength, &bitsPerBlock))
			{
			#if JXSEL_DEBUG_MSGS && ! JXSEL_DEBUG_ONLY_RESULT
			cout << "Accepted selection request: ";
			cout << XGetAtomName(*itsDisplay, selReqEvent.target);
			cout << ", time=" << selReqEvent.time << endl;
			#endif

			#if JXSEL_MICRO_TRANSFER
			const JSize savedMaxSize = itsMaxDataChunkSize;
			if (selReqEvent.target != itsTargetsXAtom)
				{
				itsMaxDataChunkSize = 1;
				}
			#endif

			SendData(selReqEvent.requestor, returnEvent.property, returnType,
					 data, dataLength, bitsPerBlock, &xEvent);
			delete [] data;

			#if JXSEL_MICRO_TRANSFER
			itsMaxDataChunkSize = savedMaxSize;
			#endif

			return;
			}
		else
			{
			#if JXSEL_DEBUG_MSGS
			cout << "Rejected selection request: can't convert to ";
			cout << XGetAtomName(*itsDisplay, selReqEvent.target);
			cout << ", time=" << selReqEvent.time << endl;
			#endif
			}
		}
	else
		{
		#if JXSEL_DEBUG_MSGS
		cout << "Rejected selection request: don't own ";
		cout << XGetAtomName(*itsDisplay, selReqEvent.selection);
		cout << ", time=" << selReqEvent.time << endl;
		#endif
		}

	returnEvent.property = None;
	itsDisplay->SendXEvent(selReqEvent.requestor, &xEvent);
}

/******************************************************************************
 SendData (private)

	Sends the given data either as one chunk or incrementally.

 ******************************************************************************/

void
JXSelectionManager::SendData
	(
	const Window	requestor,
	const Atom		property,
	const Atom		type,
	unsigned char*	data,
	const JSize		dataLength,
	const JSize		bitsPerBlock,
	XEvent*			returnEvent
	)
{
	JSize chunkSize = 4*itsMaxDataChunkSize;

	// if small enough, send it one chunk

	#if JXSEL_DEBUG_MSGS && ! JXSEL_DEBUG_ONLY_RESULT
	cout << "Selection is " << dataLength << " bytes" << endl;
	if (dataLength <= chunkSize)
		{
		cout << "Attempting to send entire selection" << endl;
		}
	#endif

	if (dataLength <= chunkSize &&
		SendData1(requestor, property, type,
				  data, dataLength, bitsPerBlock))
		{
		#if JXSEL_DEBUG_MSGS
		cout << "Transfer complete" << endl;
		#endif

		itsDisplay->SendXEvent(requestor, returnEvent);
		return;
		}

	// we need to hear when the property or the window is deleted

	XSelectInput(*itsDisplay, requestor, PropertyChangeMask | StructureNotifyMask);

	// initiate transfer by sending INCR

	#if JXSEL_DEBUG_MSGS && ! JXSEL_DEBUG_ONLY_RESULT
	cout << "Initiating incremental transfer" << endl;
	#endif

	(JXGetApplication())->DisplayBusyCursor();

	XID remainingLength = dataLength;		// must be 32 bits
	XChangeProperty(*itsDisplay, requestor, property, itsIncrementalSendXAtom,
					32, PropModeReplace,
					reinterpret_cast<unsigned char*>(&remainingLength), 4);
	itsDisplay->SendXEvent(requestor, returnEvent);
	if (!WaitForPropertyDeleted(requestor, property))
		{
		#if JXSEL_DEBUG_MSGS
		cout << "No response from requestor (data length)" << endl;
		#endif

		return;
		}

	// send a chunk and wait for it to be deleted

	#if JXSEL_DEBUG_MSGS
	JIndex chunkIndex = 0;
	#endif

	while (remainingLength > 0)
		{
		#if JXSEL_DEBUG_MSGS
		chunkIndex++;
		#endif

		unsigned char* dataStart = data + dataLength - remainingLength;
		if (chunkSize > remainingLength)
			{
			chunkSize = remainingLength;
			}

		#if JXSEL_DEBUG_MSGS && ! JXSEL_DEBUG_ONLY_RESULT
		cout << "Sending " << chunkSize << " bytes" << endl;
		#endif

		SendData1(requestor, property, type,
				  dataStart, chunkSize, bitsPerBlock);
		if (itsTargetWindowDeletedFlag)
			{
			#if JXSEL_DEBUG_MSGS
			cout << "Requestor crashed on iteration ";
			cout << chunkIndex << endl;
			#endif
			return;
			}
		else if (itsReceivedAllocErrorFlag && itsMaxDataChunkSize > 1)
			{
			itsMaxDataChunkSize /= 2;
			chunkSize            = 4*itsMaxDataChunkSize;

			#if JXSEL_DEBUG_MSGS && ! JXSEL_DEBUG_ONLY_RESULT
			cout << "Reducing chunk size to " << chunkSize << " bytes" << endl;
			#endif
			}
		else if (itsReceivedAllocErrorFlag)
			{
			#if JXSEL_DEBUG_MSGS
			cout << "X server is out of memory!" << endl;
			#endif

			XSelectInput(*itsDisplay, requestor, NoEventMask);
			return;
			}

		if (!WaitForPropertyDeleted(requestor, property))
			{
			#if JXSEL_DEBUG_MSGS
			cout << "No response from requestor on iteration ";
			cout << chunkIndex << ", " << dataLength - remainingLength;
			cout << " bytes sent, " << remainingLength;
			cout << " bytes remaining, chunk size " << chunkSize << endl;
			#endif

			return;
			}

		remainingLength -= chunkSize;
		}

	// send zero-length property to signal that we are done

	SendData1(requestor, property, type, data, 0, 8);

	// we are done interacting with the requestor

	if (!itsTargetWindowDeletedFlag)
		{
		XSelectInput(*itsDisplay, requestor, NoEventMask);
		}

	#if JXSEL_DEBUG_MSGS
	cout << "Transfer complete" << endl;
	#endif
}

/******************************************************************************
 SendData1 (private)

	Put the data into the window property and check for BadAlloc and
	BadWindow errors.

 ******************************************************************************/

JBoolean
JXSelectionManager::SendData1
	(
	const Window	requestor,
	const Atom		property,
	const Atom		type,
	unsigned char*	data,
	const JSize		dataLength,
	const JSize		bitsPerBlock
	)
{
	XChangeProperty(*itsDisplay, requestor, property, type,
					bitsPerBlock, PropModeReplace, data, dataLength);

	itsReceivedAllocErrorFlag  = kFalse;
	itsTargetWindow            = requestor;
	itsTargetWindowDeletedFlag = kFalse;

	itsDisplay->Synchronize();

	ListenTo(itsDisplay);
	itsDisplay->CheckForXErrors();
	StopListening(itsDisplay);

	itsTargetWindow = None;

	return JNegate(itsReceivedAllocErrorFlag || itsTargetWindowDeletedFlag);
}

/******************************************************************************
 WaitForPropertyDeleted (private)

	Wait for the receiver to delete the window property.
	Returns kFalse if we time out or the window is deleted (receiver crash).

 ******************************************************************************/

JBoolean
JXSelectionManager::WaitForPropertyDeleted
	(
	const Window	xWindow,
	const Atom		property
	)
{
	itsDisplay->Synchronize();

	XEvent xEvent;
	XID checkIfEventData[] = { xWindow, property };
	const clock_t endTime = clock() + kWaitForSelectionTime;
	while (clock() < endTime)
		{
		const Bool receivedEvent =
			XCheckIfEvent(*itsDisplay, &xEvent, GetNextPropDeletedEvent,
						  reinterpret_cast<char*>(&checkIfEventData));

		if (receivedEvent && xEvent.type == PropertyNotify)
			{
			return kTrue;
			}
		else if (receivedEvent && xEvent.type == DestroyNotify)
			{
			return kFalse;
			}
		}

	#if JXSEL_DEBUG_MSGS

	Atom actualType;
	int actualFormat;
	unsigned long itemCount, remainingBytes;
	unsigned char* data;
	XGetWindowProperty(*itsDisplay, xWindow, property,
					   0, LONG_MAX, False, AnyPropertyType,
					   &actualType, &actualFormat,
					   &itemCount, &remainingBytes, &data);

	if (actualType == None && actualFormat == 0 && remainingBytes == 0)
		{
		cout << "Window property was deleted, but source was not notified!" << endl;
		}
	else
		{
		cout << "Window property not deleted, type " << XGetAtomName(*itsDisplay, actualType);
		cout << ", " << itemCount * actualFormat/8 << " bytes" << endl;
		}
	XFree(data);

	#endif

	XSelectInput(*itsDisplay, xWindow, NoEventMask);
	return kFalse;
}

// static

Bool
JXSelectionManager::GetNextPropDeletedEvent
	(
	Display*	display,
	XEvent*		event,
	char*		arg
	)
{
	XID* data = reinterpret_cast<XID*>(arg);

	if (event->type             == PropertyNotify &&
		event->xproperty.window == data[0] &&
		event->xproperty.atom   == data[1] &&
		event->xproperty.state  == PropertyDelete)
		{
		return True;
		}
	else if (event->type                  == DestroyNotify &&
			 event->xdestroywindow.window == data[0])
		{
		return True;
		}
	else
		{
		return False;
		}
}

/******************************************************************************
 ReceiveDataIncr (private)

	Receives the current selection data incrementally.

 ******************************************************************************/

JBoolean
JXSelectionManager::ReceiveDataIncr
	(
	const Atom		selectionName,
	Atom*			returnType,
	unsigned char**	data,
	JSize*			dataLength,
	DeleteMethod*	delMethod
	)
{
	*returnType = None;
	*data       = NULL;
	*dataLength = 0;
	*delMethod  = kCFree;

	#if JXSEL_DEBUG_MSGS && ! JXSEL_DEBUG_ONLY_RESULT
	cout << "Initiating incremental receive" << endl;
	#endif

	(JXGetApplication())->DisplayBusyCursor();

	// we need to hear when the sender crashes

	const Window sender = XGetSelectionOwner(*itsDisplay, selectionName);
	if (sender == None)
		{
		return kFalse;
		}
	XSelectInput(*itsDisplay, sender, StructureNotifyMask);

	// The transfer has already been initiated by deleting the
	// INCR property when it was retrieved.

	// wait for a chunk, retrieve it, and delete it

	#if JXSEL_DEBUG_MSGS
	JIndex chunkIndex = 0;
	#endif

	JBoolean ok = kTrue;
	while (1)
		{
		#if JXSEL_DEBUG_MSGS
		chunkIndex++;
		#endif

		if (!WaitForPropertyCreated(itsDataWindow, itsSelectionWindPropXAtom, sender))
			{
			#if JXSEL_DEBUG_MSGS
			cout << "No response from selection owner on iteration ";
			cout << chunkIndex << ", " << *dataLength << " bytes received" << endl;
			#endif

			ok = kFalse;
			break;
			}

		Atom actualType;
		int actualFormat;
		unsigned long itemCount, remainingBytes;
		unsigned char* chunk;
		XGetWindowProperty(*itsDisplay, itsDataWindow, itsSelectionWindPropXAtom,
						   0, LONG_MAX, True, AnyPropertyType,
						   &actualType, &actualFormat,
						   &itemCount, &remainingBytes, &chunk);

		if (actualType == None)
			{
			#if JXSEL_DEBUG_MSGS
			cout << "Received data of type None on iteration ";
			cout << chunkIndex << endl;
			#endif

			ok = kFalse;
			break;
			}

		// an empty property means that we are done

		if (itemCount == 0)
			{
			#if JXSEL_DEBUG_MSGS
			if (*data == NULL)
				{
				cout << "Didn't receive any data on iteration ";
				cout << chunkIndex << endl;
				}
			#endif

			XFree(chunk);
			ok = JConvertToBoolean( *data != NULL );
			break;
			}

		// otherwise, append it to *data

		else
			{
			assert( remainingBytes == 0 );

			const JSize chunkSize = itemCount * actualFormat/8;
			if (*data == NULL)
				{
				// the first chunk determines the format
				*returnType = actualType;

				*data = static_cast<unsigned char*>(malloc(chunkSize));
				assert( *data != NULL );
				memcpy(*data, chunk, chunkSize);

				#if JXSEL_DEBUG_MSGS && ! JXSEL_DEBUG_ONLY_RESULT
				cout << "Data format: " << XGetAtomName(*itsDisplay, actualType) << endl;
				#endif
				}
			else
				{
				*data = static_cast<unsigned char*>(realloc(*data, *dataLength + chunkSize));
				memcpy(*data + *dataLength, chunk, chunkSize);
				}

			*dataLength += chunkSize;
			XFree(chunk);

			#if JXSEL_DEBUG_MSGS && ! JXSEL_DEBUG_ONLY_RESULT
			cout << "Received " << chunkSize << " bytes" << endl;
			#endif
			}
		}

	// we are done interacting with the sender

	XSelectInput(*itsDisplay, sender, NoEventMask);

	// clean up

	if (!ok && *data != NULL)
		{
		free(*data);
		*data       = NULL;
		*dataLength = 0;
		*returnType = None;
		}

	#if JXSEL_DEBUG_MSGS
	if (ok)
		{
		cout << "Transfer successful" << endl;
		}
	else
		{
		cout << "Transfer failed" << endl;
		}
	#endif

	return ok;
}

/******************************************************************************
 WaitForPropertyCreated (private)

	Wait for the receiver to create the window property.
	Returns kFalse if we time out or the sender crashes.

 ******************************************************************************/

JBoolean
JXSelectionManager::WaitForPropertyCreated
	(
	const Window	xWindow,
	const Atom		property,
	const Window	sender
	)
{
	itsDisplay->Synchronize();

	XEvent xEvent;
	XID checkIfEventData[] = { xWindow, property };
	const clock_t endTime = clock() + kWaitForSelectionTime;
	while (clock() < endTime)
		{
		if (XCheckTypedWindowEvent(*itsDisplay, sender, DestroyNotify, &xEvent))
			{
			return kFalse;
			}

		if (XCheckIfEvent(*itsDisplay, &xEvent, GetNextNewPropertyEvent,
						  reinterpret_cast<char*>(checkIfEventData)))
			{
			return kTrue;
			}
		}

	return kFalse;
}

// static

Bool
JXSelectionManager::GetNextNewPropertyEvent
	(
	Display*	display,
	XEvent*		event,
	char*		arg
	)
{
	XID* data = reinterpret_cast<XID*>(arg);

	if (event->type             == PropertyNotify &&
		event->xproperty.window == data[0] &&
		event->xproperty.atom   == data[1] &&
		event->xproperty.state  == PropertyNewValue)
		{
		return True;
		}
	else
		{
		return False;
		}
}

/******************************************************************************
 ReceiveWithFeedback (virtual protected)

	This catches errors while sending and receiving data.

 ******************************************************************************/

void
JXSelectionManager::ReceiveWithFeedback
	(
	JBroadcaster*	sender,
	Message*		message
	)
{
	if (sender == itsDisplay && message->Is(JXDisplay::kXError))
		{
		JXDisplay::XError* err = dynamic_cast(JXDisplay::XError*, message);
		assert( err != NULL );

		if (err->GetType() == BadAlloc)
			{
			itsReceivedAllocErrorFlag = kTrue;
			err->SetCaught();
			}

		else if (err->GetType() == BadWindow &&
				 err->GetXID()  == itsTargetWindow)
			{
			itsTargetWindowDeletedFlag = kTrue;
			err->SetCaught();
			}
		}

	else
		{
		JBroadcaster::ReceiveWithFeedback(sender, message);
		}
}

/******************************************************************************
 SetData

	Set the data for the given selection.

	*** 'data' must be allocated on the heap.
		We take ownership of 'data' even if the function returns kFalse.

 ******************************************************************************/

JBoolean
JXSelectionManager::SetData
	(
	const Atom	selectionName,
	Data*		data
	)
{
	DeleteOutdatedData();

	const Time lastEventTime = itsDisplay->GetLastEventTime();

	// check if it already owns the selection

	Data* origData = NULL;
	const JBoolean found = GetData(selectionName, CurrentTime, &origData);
	if (found && origData != data)
		{
		origData->SetEndTime(lastEventTime);
		}
	else if (found)
		{
		itsDataList->Remove(data);
		}

	// we are required to check XGetSelectionOwner()

	XSetSelectionOwner(*itsDisplay, selectionName, itsDataWindow, lastEventTime);
	if (XGetSelectionOwner(*itsDisplay, selectionName) == itsDataWindow)
		{
		#if JXSEL_DEBUG_MSGS
		cout << "Got selection ownership: ";
		cout << XGetAtomName(*itsDisplay, selectionName);
		cout << ", time=" << lastEventTime << endl;
		#endif

		data->SetSelectionInfo(selectionName, lastEventTime);
		itsDataList->Append(data);
		return kTrue;
		}
	else
		{
		delete data;
		return kFalse;
		}
}

/******************************************************************************
 ClearData

	time can be CurrentTime.

 ******************************************************************************/

void
JXSelectionManager::ClearData
	(
	const Atom	selectionName,
	const Time	endTime
	)
{
	Data* data = NULL;
	if (GetData(selectionName, CurrentTime, &data))
		{
		const Time t = (endTime == CurrentTime ? itsDisplay->GetLastEventTime() : endTime);
		data->SetEndTime(t);

		if (XGetSelectionOwner(*itsDisplay, selectionName) == itsDataWindow)
			{
			XSetSelectionOwner(*itsDisplay, selectionName, None, t);
			}
		}
}

/******************************************************************************
 GetData

	time can be CurrentTime.

	In the private version, if index != NULL, it contains the index into itsDataList.

 ******************************************************************************/

JBoolean
JXSelectionManager::GetData
	(
	const Atom		selectionName,
	const Time		time,
	const Data**	data
	)
{
	return GetData(selectionName, time, const_cast<Data**>(data));
}

// private

JBoolean
JXSelectionManager::GetData
	(
	const Atom	selectionName,
	const Time	time,
	Data**		data,
	JIndex*		index
	)
{
	const JSize count = itsDataList->GetElementCount();
	for (JIndex i=1; i<=count; i++)
		{
		*data         = itsDataList->NthElement(i);
		const Time t1 = (**data).GetStartTime();

		Time t2;
		const JBoolean finished = (**data).GetEndTime(&t2);

		if (selectionName == (**data).GetSelectionName() &&
			((time == CurrentTime && !finished) ||
			 (time != CurrentTime && t1 != CurrentTime &&
			  t1 <= time && (!finished || time <= t2))))
			{
			if (index != NULL)
				{
				*index = i;
				}
			return kTrue;
			}
		}

	*data = NULL;
	if (index != NULL)
		{
		*index = 0;
		}
	return kFalse;
}

/******************************************************************************
 DeleteOutdatedData (private)

	time cannot be CurrentTime.

 ******************************************************************************/

void
JXSelectionManager::DeleteOutdatedData()
{
	const Time currTime = itsDisplay->GetLastEventTime();
	const Time oldTime  = currTime - kHistoryInterval;

	const JSize count = itsDataList->GetElementCount();
	for (JIndex i=count; i>=1; i--)
		{
		Data* data = itsDataList->NthElement(i);

		// toss if outdated or clock has wrapped

		Time endTime;
		if ((data->GetEndTime(&endTime) && endTime < oldTime) ||
			data->GetStartTime() > currTime)
			{
			itsDataList->DeleteElement(i);
			}
		}
}

/******************************************************************************
 JXSelectionManager::Data

	This defines the interface for all objects that encapsulate data
	placed in an X selection.  Each object is restricted to work on a
	single X display since the atom id's are different on each one.

	Remember to always provide access to the data so derived classes
	can extend your class by providing additional types.

 ******************************************************************************/

/******************************************************************************
 Constructor

	The second form is used for delayed evaluation.  The id must be something
	unique to a particular *class* so each class in the inheritance line
	that implements GetSelectionData() can either do the work or pass it to
	its base class.  In most cases, the class name is sufficient.

 ******************************************************************************/

JXSelectionManager::Data::Data
	(
	JXDisplay* display
	)
	:
	itsDisplay(display),
	itsDataSource(NULL)
{
	DataX();
}

JXSelectionManager::Data::Data
	(
	JXWidget*			widget,
	const JCharacter*	id
	)
	:
	itsDisplay(widget->GetDisplay()),
	itsDataSource(widget)
{
	assert( widget != NULL && !JStringEmpty(id) );

	DataX();

	ListenTo(itsDataSource);	// need to know if it is deleted

	itsDataSourceID = new JString(id);
	assert( itsDataSourceID != NULL );
}

// private

void
JXSelectionManager::Data::DataX()
{
	itsSelectionName = None;
	itsStartTime     = CurrentTime;
	itsEndTime       = CurrentTime;

	itsTypeList = new JArray<Atom>;
	assert( itsTypeList != NULL );
	itsTypeList->SetCompareFunction(CompareAtoms);

	itsDataSourceID = NULL;
}

/******************************************************************************
 Destructor

 ******************************************************************************/

JXSelectionManager::Data::~Data()
{
	delete itsTypeList;
	delete itsDataSourceID;
}

/******************************************************************************
 GetSelectionManager

 ******************************************************************************/

JXSelectionManager*
JXSelectionManager::Data::GetSelectionManager()
	const
{
	return itsDisplay->GetSelectionManager();
}

/******************************************************************************
 GetDNDManager

 ******************************************************************************/

JXDNDManager*
JXSelectionManager::Data::GetDNDManager()
	const
{
	return itsDisplay->GetDNDManager();
}

/******************************************************************************
 GetDNDSelectionName

 ******************************************************************************/

Atom
JXSelectionManager::Data::GetDNDSelectionName()
	const
{
	return (itsDisplay->GetDNDManager())->GetDNDSelectionName();
}

/******************************************************************************
 SetSelectionInfo

 ******************************************************************************/

void
JXSelectionManager::Data::SetSelectionInfo
	(
	const Atom	selectionName,
	const Time	startTime
	)
{
	assert( selectionName != None );

	itsSelectionName = selectionName;
	itsStartTime     = startTime;

	if (selectionName != GetDNDSelectionName())
		{
		JXSelectionManager* selMgr = GetSelectionManager();
		AddType(selMgr->GetTargetsXAtom());
		AddType(selMgr->GetTimeStampXAtom());
		}

	AddTypes(selectionName);
}

/******************************************************************************
 AddType (protected)

	Create the atom and add it to the list.

 ******************************************************************************/

Atom
JXSelectionManager::Data::AddType
	(
	const JCharacter* name
	)
{
	const Atom atom = itsDisplay->RegisterXAtom(name);
	AddType(atom);
	return atom;
}

/******************************************************************************
 RemoveType (protected)

	Remove the type from the list.

 ******************************************************************************/

void
JXSelectionManager::Data::RemoveType
	(
	const Atom type
	)
{
	JIndex index;
	if (itsTypeList->SearchSorted(type, JOrderedSetT::kAnyMatch, &index))
		{
		itsTypeList->RemoveElement(index);
		}
}

/******************************************************************************
 Resolve

	Asks its data source to set the data.  This is required for DND where
	the data should not be converted until it is needed, allowing the mouse
	drag to begin immediately.

	This cannot be called by JXSelectionManager because it doesn't know
	how the data is being used.

 ******************************************************************************/

void
JXSelectionManager::Data::Resolve()
	const
{
	if (itsDataSource != NULL && itsDataSourceID != NULL)
		{
		Data* me = const_cast<Data*>(this);

		itsDataSource->GetSelectionData(me, *itsDataSourceID);

		delete me->itsDataSourceID;
		me->itsDataSource   = NULL;
		me->itsDataSourceID = NULL;
		}
}

/******************************************************************************
 Convert

	Handles certain types and passes everything else off to ConvertData().

	When adding special types to this function, remember to update
	SetSelectionInfo() to add the new types.

 ******************************************************************************/

JBoolean
JXSelectionManager::Data::Convert
	(
	const Atom		requestType,
	Atom*			returnType,
	unsigned char**	data,
	JSize*			dataLength,
	JSize*			bitsPerBlock
	)
	const
{
	JXSelectionManager* selMgr = GetSelectionManager();

	// TARGETS

	if (requestType == selMgr->GetTargetsXAtom())
		{
		const JSize atomCount = itsTypeList->GetElementCount();
		assert( atomCount > 0 );

		*returnType   = XA_ATOM;
		*bitsPerBlock = sizeof(Atom)*8;
		*dataLength   = sizeof(Atom)*atomCount;

		*data = new unsigned char [ *dataLength ];
		if (*data == NULL)
			{
			return kFalse;
			}

		Atom* atomData = reinterpret_cast<Atom*>(*data);
		for (JIndex i=1; i<=atomCount; i++)
			{
			atomData[i-1] = itsTypeList->GetElement(i);
			}

		return kTrue;
		}

	// TIMESTAMP

	else if (requestType == selMgr->GetTimeStampXAtom())
		{
		*returnType   = XA_INTEGER;
		*bitsPerBlock = sizeof(Time)*8;
		*dataLength   = sizeof(Time);

		*data = new unsigned char [ *dataLength ];
		if (*data == NULL)
			{
			return kFalse;
			}

		*(reinterpret_cast<Time*>(*data)) = itsStartTime;

		return kTrue;
		}

	// everything else

	else
		{
		Resolve();
		return ConvertData(requestType, returnType,
						   data, dataLength, bitsPerBlock);
		}
}

/******************************************************************************
 AddTypes (virtual protected)

	Call AddType() for whatever types are appropriate for the given selection.

 ******************************************************************************/

/******************************************************************************
 ConvertData (virtual protected)

	Derived class must convert data to the specified type and return
	kTrue, or return kFalse if the conversion cannot be accomplished.

	*returnType must be actual data type.  For example, when "TEXT" is
	requested, one often returns XA_STRING.

	*data must be allocated with "new unsigned char[]" and will be deleted
	by the caller.  *dataLength must be set to the length of *data.

	*bitsPerBlock must be set to the number of bits per element of data.
	e.g.	If data is text, *bitsPerBlock=8.
			If data is an int, *bitsPerBlock=sizeof(int)*8

	Since X performs byte swapping when *bitsPerBlock > 8, mixed data should
	be packed one byte at a time to insure that it can be correctly decoded.

 ******************************************************************************/

/******************************************************************************
 ReceiveGoingAway (virtual protected)

	The given sender has been deleted.

 ******************************************************************************/

void
JXSelectionManager::Data::ReceiveGoingAway
	(
	JBroadcaster* sender
	)
{
	if (sender == itsDataSource)
		{
		delete itsDataSourceID;
		itsDataSource   = NULL;
		itsDataSourceID = NULL;
		}
	else
		{
		JBroadcaster::ReceiveGoingAway(sender);
		}
}

/******************************************************************************
 CompareAtoms (static private)

 ******************************************************************************/

JOrderedSetT::CompareResult
JXSelectionManager::Data::CompareAtoms
	(
	const Atom& atom1,
	const Atom& atom2
	)
{
	if (atom1 < atom2)
		{
		return JOrderedSetT::kFirstLessSecond;
		}
	else if (atom1 == atom2)
		{
		return JOrderedSetT::kFirstEqualSecond;
		}
	else
		{
		return JOrderedSetT::kFirstGreaterSecond;
		}
}

#define JTemplateType JXSelectionManager::Data
#include <JPtrArray.tmpls>
#undef JTemplateType
