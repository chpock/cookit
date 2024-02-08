/******************************************************************************
 JXDNDManager.h

	Copyright © 1997-99 by John Lindal. All rights reserved.

 ******************************************************************************/

#ifndef _H_JXDNDManager
#define _H_JXDNDManager

#include <JXSelectionManager.h>
#include <JXCursor.h>
#include <JRect.h>

class JXDisplay;
class JXContainer;
class JXWidget;
class JXButtonStates;
class JXKeyModifiers;
class JXDNDChooseDropActionDialog;

class JXDNDManager : virtual public JBroadcaster
{
public:

	JXDNDManager(JXDisplay* display);

	virtual ~JXDNDManager();

	Atom			GetDNDSelectionName() const;
	JCursorIndex	GetDefaultDNDCursor() const;
	JCursorIndex	GetDefaultDNDCopyCursor() const;
	JCursorIndex	GetDefaultDNDMoveCursor() const;
	JCursorIndex	GetDefaultDNDLinkCursor() const;
	JCursorIndex	GetDefaultDNDAskCursor() const;

	Atom	GetDNDActionCopyXAtom() const;
	Atom	GetDNDActionMoveXAtom() const;
	Atom	GetDNDActionLinkXAtom() const;
	Atom	GetDNDActionAskXAtom() const;
	Atom	GetDNDActionPrivateXAtom() const;

	JBoolean	GetAskActions(JArray<Atom>* actionList,
							  JPtrArray<JString>* descriptionList) const;
	JBoolean	ChooseDropAction(const JArray<Atom>& actionList,
								 const JPtrArray<JString>& descriptionList,
								 Atom* action);

	JBoolean	IsDragging() const;
	Window		GetDraggerWindow() const;
	JBoolean	TargetWillAcceptDrop() const;

	Atom	GetDNDDirectSave0XAtom() const;

	// called by JXWidget

	JBoolean	BeginDND(JXWidget* widget, const JPoint& pt,
						 const JXButtonStates& buttonStates,
						 const JXKeyModifiers& modifiers,
						 JXSelectionManager::Data* data);
	void		HandleDND(const JPoint& pt,
						  const JXButtonStates& buttonStates,
						  const JXKeyModifiers& modifiers);
	void		FinishDND();

	// called by JXWindow

	void		EnableDND(const Window xWindow) const;
	JBoolean	CancelDND();

	// called by JXDisplay

	JBoolean	HandleClientMessage(const XClientMessageEvent& clientMessage);
	JBoolean	HandleDestroyNotify(const XDestroyWindowEvent& xEvent);

	// called by JXSelectionManager

	JBoolean	IsLastFakePasteTime(const Time time) const;

protected:

	virtual void	Receive(JBroadcaster* sender, const Message& message);
	virtual void	ReceiveWithFeedback(JBroadcaster* sender, Message* message);

private:

	enum
	{
		kCopyCursorIndex,
		kMoveCursorIndex,
		kLinkCursorIndex,
		kAskCursorIndex,

		kDefDNDCursorCount
	};

private:

	JXDisplay*			itsDisplay;					// not owned
	JBoolean			itsIsDraggingFlag;			// kTrue until FinishDND() is called
	JXWidget*			itsDragger;					// widget initiating drag; not owned
	Window				itsDraggerWindow;			// window of itsDragger; not owned
	JArray<Atom>*		itsDraggerTypeList;			// data types supported by itsDragger
	JArray<Atom>*		itsDraggerAskActionList;	// actions accepted by itsDragger
	JPtrArray<JString>*	itsDraggerAskDescripList;	// description of each action

	Atom			itsDNDVersion;			// version being used
	Window			itsMouseWindow;			// window that mouse is in; not owned
	JBoolean		itsMouseWindowIsAware;	// kTrue if itsMouseWindow has XdndAware
	JXContainer*	itsMouseContainer;		// widget that mouse is in; NULL if inter-app; not owned
	Window			itsMsgWindow;			// window that receives messages (not itsMouseWindow if proxy)

	JBoolean		itsWillAcceptDropFlag;	// kTrue if target will accept drop
	JBoolean		itsWaitForStatusFlag;	// kTrue if waiting for XdndStatus message
	JBoolean		itsSendHereMsgFlag;		// kTrue if need another XdndPosition with itsPrevHandleDNDPt
	JBoolean		itsReceivedStatusFlag;	// kTrue if received any XdndStatus from current target
	JBoolean		itsUseMouseRectFlag;	// kTrue if use itsMouseRectR
	JRect			itsMouseRectR;			// don't send another XdndPosition while inside here

	JPoint			itsPrevMousePt;			// last XdndPosition coordinates (local coords of target)
	Atom			itsPrevHereAction;		// last XdndPosition action
	Atom			itsPrevStatusAction;	// last XdndStatus action
	JPoint			itsPrevHandleDNDPt;		// last JPoint sent to HandleDND()
	Atom			itsPrevHandleDNDAction;	// last action computed inside HandleDND()

	JXDNDChooseDropActionDialog*	itsChooseDropActionDialog;
	Atom*							itsUserDropAction;		// NULL unless waiting for GetDropActionDialog

	JBoolean	itsSentFakePasteFlag;		// kTrue if times are valid
	Time		itsFakeButtonPressTime;
	Time		itsFakeButtonReleaseTime;

	JCursorIndex	itsDefDNDCursor[kDefDNDCursorCount];

	Atom itsDNDSelectionName;

	Atom itsDNDProxyXAtom;
	Atom itsDNDAwareXAtom;
	Atom itsDNDTypeListXAtom;

	Atom itsDNDEnterXAtom;
	Atom itsDNDHereXAtom;
	Atom itsDNDStatusXAtom;
	Atom itsDNDLeaveXAtom;
	Atom itsDNDDropXAtom;
	Atom itsDNDFinishedXAtom;

	Atom itsDNDActionCopyXAtom;
	Atom itsDNDActionMoveXAtom;
	Atom itsDNDActionLinkXAtom;
	Atom itsDNDActionAskXAtom;
	Atom itsDNDActionPrivateXAtom;

	Atom itsDNDActionListXAtom;
	Atom itsDNDActionDescriptionXAtom;

	Atom itsDNDDirectSave0XAtom;

private:

	void	InitCursors();

	void	SendDNDEnter(const Window xWindow, const Window msgWindow, JXContainer* widget,
						 const JBoolean isAware, const Atom vers);
	void	SendDNDHere(const JPoint& pt, const Atom action);
	void	SendDNDLeave(const JBoolean sendPasteClick = kFalse);
	void	SendDNDDrop();
	void	PrepareForDrop(const JXContainer* target);
	void	SendDNDStatus(const JBoolean willAcceptDrop, const Atom action);
	void	SendDNDFinished();

	JBoolean	IsDNDAware(const Window xWindow, Window* proxy, JSize* vers) const;
	void		AnnounceTypeList(const Window xWindow, const JArray<Atom>& list) const;
	void		AnnounceAskActions(const JXButtonStates& buttonStates,
								   const JXKeyModifiers& modifiers) const;
	JBoolean	DraggerCanProvideText() const;

	void	HandleDNDEnter(const XClientMessageEvent& clientMessage);
	void	HandleDNDHere(const XClientMessageEvent& clientMessage);
	void	HandleDNDLeave(const XClientMessageEvent& clientMessage);
	void	HandleDNDLeave1();
	void	HandleDNDDrop(const XClientMessageEvent& clientMessage);
	void	HandleDNDFinished();
	void	HandleDNDStatus(const XClientMessageEvent& clientMessage);

	JBoolean	FindTarget(const JXContainer* coordOwner, const JPoint& pt,
						   Window* xWindow, Window* msgWindow,
						   JXContainer** target, Atom* vers) const;
	JBoolean	StayWithCurrentTarget(const JBoolean isDNDAware, const Window xWindow,
									  JXContainer* dropWidget, const JPoint& pt,
									  const JXButtonStates& buttonStates,
									  const JXKeyModifiers& modifiers) const;

	void		FinishDND1();
	JBoolean	WaitForLastStatusMsg();
	static Bool	GetNextStatusEvent(Display* display, XEvent* event, char* arg);

	long	PackPoint(const JPoint& pt) const;
	JPoint	UnpackPoint(const long data) const;
	JRect	UnpackRect(const long pt, const long area) const;

	// not allowed

	JXDNDManager(const JXDNDManager& source);
	const JXDNDManager& operator=(const JXDNDManager& source);
};


/******************************************************************************
 GetDNDSelectionName

 ******************************************************************************/

inline Atom
JXDNDManager::GetDNDSelectionName()
	const
{
	return itsDNDSelectionName;
}

/******************************************************************************
 GetDNDDirectSave0XAtom

 ******************************************************************************/

inline Atom
JXDNDManager::GetDNDDirectSave0XAtom()
	const
{
	return itsDNDDirectSave0XAtom;
}

/******************************************************************************
 Get actions

 ******************************************************************************/

inline Atom
JXDNDManager::GetDNDActionCopyXAtom()
	const
{
	return itsDNDActionCopyXAtom;
}

inline Atom
JXDNDManager::GetDNDActionMoveXAtom()
	const
{
	return itsDNDActionMoveXAtom;
}

inline Atom
JXDNDManager::GetDNDActionLinkXAtom()
	const
{
	return itsDNDActionLinkXAtom;
}

inline Atom
JXDNDManager::GetDNDActionAskXAtom()
	const
{
	return itsDNDActionAskXAtom;
}

inline Atom
JXDNDManager::GetDNDActionPrivateXAtom()
	const
{
	return itsDNDActionPrivateXAtom;
}

/******************************************************************************
 IsDragging

	Returns kTrue while dragging.

 ******************************************************************************/

inline JBoolean
JXDNDManager::IsDragging()
	const
{
	return JI2B( itsDragger != NULL && itsIsDraggingFlag );
}

/******************************************************************************
 GetDraggerWindow

	This should only be called while dragging.

 ******************************************************************************/

inline Window
JXDNDManager::GetDraggerWindow()
	const
{
	return itsDraggerWindow;
}

/******************************************************************************
 TargetWillAcceptDrop

	This should only be called from JXWidget::HandleDNDHere().

 ******************************************************************************/

inline JBoolean
JXDNDManager::TargetWillAcceptDrop()
	const
{
	return JI2B( itsDragger != NULL && itsWillAcceptDropFlag );
}

/******************************************************************************
 IsLastFakePasteTime

 ******************************************************************************/

inline JBoolean
JXDNDManager::IsLastFakePasteTime
	(
	const Time time
	)
	const
{
	return JI2B( itsSentFakePasteFlag &&
				 (time == itsFakeButtonPressTime ||
				  time == itsFakeButtonReleaseTime) );
}

/******************************************************************************
 Default cursors

 ******************************************************************************/

inline JCursorIndex
JXDNDManager::GetDefaultDNDCursor()
	const
{
	return itsDefDNDCursor [ kMoveCursorIndex ];
}

inline JCursorIndex
JXDNDManager::GetDefaultDNDCopyCursor()
	const
{
	return itsDefDNDCursor [ kCopyCursorIndex ];
}

inline JCursorIndex
JXDNDManager::GetDefaultDNDMoveCursor()
	const
{
	return itsDefDNDCursor [ kMoveCursorIndex ];
}

inline JCursorIndex
JXDNDManager::GetDefaultDNDLinkCursor()
	const
{
	return itsDefDNDCursor [ kLinkCursorIndex ];
}

inline JCursorIndex
JXDNDManager::GetDefaultDNDAskCursor()
	const
{
	return itsDefDNDCursor [ kAskCursorIndex ];
}

/******************************************************************************
 Packing/unpacking (private)

 ******************************************************************************/

inline long
JXDNDManager::PackPoint
	(
	const JPoint& pt
	)
	const
{
	return ((pt.x << 16) | pt.y);
}

inline JPoint
JXDNDManager::UnpackPoint
	(
	const long data
	)
	const
{
	return JPoint((data >> 16) & 0xFFFF, data & 0xFFFF);
}

inline JRect
JXDNDManager::UnpackRect
	(
	const long pt,
	const long area
	)
	const
{
	const JPoint topLeft = UnpackPoint(pt);
	return JRect(topLeft.y, topLeft.x,
				 topLeft.y + (area & 0xFFFF),
				 topLeft.x + ((area >> 16) & 0xFFFF));
}

#endif
