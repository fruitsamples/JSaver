/* * JSaver.cp * * � 1997 Apple Computer, Inc. * * Written by Steve Zellers */#include "GraphicsModule_Types.h"#include <JManager.h>#include "NewAppletDialog.h"/** * Constants */ #define URL_BUTTON_MESSAGE	8		/* message that brings up URL chooser dialog */enum StartupStages {	eDoInitStage,			// we got our doInit msg	eCreateSessionStage,	// create a session now	eCreateContextStage,	// create an AWTContext	eLocateAppletStage,		// look for the applet	eLocateAppletNotDone,	// spin till we find it	eLocatorFailedStage,	// the locator failed to find the applet, or no applets were found.	eCreateViewerStage,		// create an applet viewer now	eRunningStage			// just give the VM time - everything is loaded};/** * Globals - note that we're specifically only going * to excecute one java applet, so we have one * Viewer, one Frame and one AWTContext. */ static JMSessionRef theSession;static JMAWTContext theAWTContext;static JMAppletLocatorRef theLocatorRef;static JMAppletViewerRef theAppletViewer;static Rect theViewerBounds;static Boolean theViewerVisible;static Boolean theFrameCreated;static JMFrameRef theJMFrame;static RgnHandle theUpdateRgn = nil;static StartupStages theStage = eDoInitStage;static GMParamBlockPtr theParams;static Str255 theExceptionString;static Boolean theFirstDrawFrameGotten = false;static OSErr checkError(OSErr err){	if (theExceptionString[0] > 0) {		if (err == noErr)			err = -1;		BlockMoveData(theExceptionString, theParams->errorMessage, theExceptionString[0] + 1);	}	return err;}static void setMessage(StringPtr left, StringPtr right){	static Rect lastRect = { 0, 0, 0, 0 };		TextFont(geneva);	TextFace(normal);	TextSize(9);	if (! EmptyRect(&lastRect))		FillRect(&lastRect, (ConstPatternParam) &theParams->qdGlobalsCopy->qdBlack);	if (! (left || right))		SetRect(&lastRect, 0, 0, 0, 0);	else {		if (left == nil)			left = "\p";		if (right == nil)			right = "\p";		int totalWidth = (StringWidth("\p��") * 2) + StringWidth(left) + StringWidth("\p ") + StringWidth(right);		TextMode(srcXor);		lastRect = theParams->monitors->monitorList[0].bounds;		lastRect.top = lastRect.bottom - 14;		lastRect.left = lastRect.left + ((lastRect.right - lastRect.left) / 2) - (totalWidth / 2);		lastRect.right = lastRect.left + totalWidth;		MoveTo(lastRect.left, lastRect.bottom - 4);		DrawString("\p��");		DrawString(left);		DrawString(right);		DrawString("\p �");		TextMode(srcCopy);		PenNormal();	}} /** * JMFrame support - these functions are used to  * fill in a paramblock of function pointers. */static void* _frameSetupPort(JMFrameRef frame){	if (theViewerVisible) {		SetPort(theParams->qdGlobalsCopy->qdThePort);		if (! theFirstDrawFrameGotten) {			theFirstDrawFrameGotten = true;			setMessage(nil, nil);		}		SetOrigin(-theViewerBounds.left, -theViewerBounds.top);		OffsetRgn(theParams->qdGlobalsCopy->qdThePort->clipRgn, -theViewerBounds.left, -theViewerBounds.top);				Rect r = theViewerBounds;		OffsetRect(&r, -r.left, -r.top);		ClipRect(&r);	}	return nil;}static void _frameRestorePort(JMFrameRef frame, void* param){	OffsetRgn(theParams->qdGlobalsCopy->qdThePort->clipRgn, theViewerBounds.left, theViewerBounds.top);	SetOrigin(0, 0);}static Boolean _frameResizeRequest(JMFrameRef frame, Rect* desired){	theViewerBounds.right = theViewerBounds.left + (desired->right - desired->left);	theViewerBounds.bottom = theViewerBounds.top + (desired->bottom - desired->top);	return true;}static void _frameInvalRect(JMFrameRef frame, const Rect* r){	RgnHandle rgn = NewRgn();	RectRgn(rgn, r);	UnionRgn(rgn, theUpdateRgn, theUpdateRgn);	DisposeRgn(rgn);}static void _frameShowHide(JMFrameRef frame, Boolean showFrameRequested){	if (theViewerVisible != showFrameRequested) {		theViewerVisible = showFrameRequested;		if (theViewerVisible) {			CopyRgn(theParams->qdGlobalsCopy->qdThePort->clipRgn, theUpdateRgn);			OffsetRgn(theUpdateRgn, -theViewerBounds.left, -theViewerBounds.top);			JMFrameUpdate(frame, theUpdateRgn);			SetEmptyRgn(theUpdateRgn);		}	}}static void _frameSetTitle(JMFrameRef frame, Str255 title){}static void _frameCheckUpdate(JMFrameRef frame){	if (! EmptyRgn(theUpdateRgn)) {		JMFrameUpdate(frame, theUpdateRgn);		SetEmptyRgn(theUpdateRgn);	}}/* * AppletViewer */ static OSStatus _ctxRequestFrame(JMAWTContextRef context, JMFrameRef newFrame, JMFrameKind kind,					UInt32 width, UInt32 height, Boolean resizeable, JMFrameCallbacks* callbacks){	callbacks->fVersion = kJMVersion;	callbacks->fSetupPort = _frameSetupPort;	callbacks->fRestorePort = _frameRestorePort;	callbacks->fResizeRequest = _frameResizeRequest;	callbacks->fInvalRect = _frameInvalRect;	callbacks->fShowHide = _frameShowHide;	callbacks->fSetTitle = _frameSetTitle;	callbacks->fCheckUpdate = _frameCheckUpdate;		if (kind == eModelessWindowFrame) {		theJMFrame = newFrame;		theFrameCreated = true;	}	return noErr;}static OSStatus _ctxReleaseFrame(JMAWTContextRef context, JMFrameRef oldFrame){	if (theJMFrame == oldFrame) {		theJMFrame = nil;		theFrameCreated = false;	}	return noErr;}static SInt16 _ctxUniqueMenuID(JMAWTContextRef context, Boolean isSubmenu){	return 0;	// we don't allow for menus}static int strlen(const char* p){	int len = 0;	while (*p++)		len++;	return len;}static void _ctxExceptionOccurred(JMAWTContextRef context, const char* exceptionName, const char* exceptionMsg, const char* stackTrace){	Handle hExceptionString = GetResource('TEXT', exceptionMsg == nil? 130 : 131);	if (hExceptionString == nil) {		theExceptionString[0] = 1;		theExceptionString[1] = 0;	} else {		Munger(hExceptionString, 0, "^0", 2, exceptionName, strlen(exceptionName));		if (exceptionMsg)			Munger(hExceptionString, 0, "^1", 2, exceptionMsg, strlen(exceptionMsg));		int maxLen = GetHandleSize(hExceptionString);		if (maxLen >= 255)			maxLen = 254;		theExceptionString[0] = maxLen;		BlockMoveData(*hExceptionString, &theExceptionString[1], maxLen);	}}/* * After Dark */ OSErr DoInitialize(Handle *storage, RgnHandle blankRgn, GMParamBlockPtr params){	// set up some globals - our actual initialization is staged accross	// subsequent doDrawFrames.	theParams = params;	theExceptionString[0] = 0;	theStage = eCreateSessionStage;	theUpdateRgn = NewRgn();	return noErr;}OSErr DoClose(Handle storage,RgnHandle blankRgn,GMParamBlockPtr params){	OSStatus result = noErr;		if (theSession != nil) {		if (theAppletViewer != nil) {			if (result == noErr) {				result = JMSuspendApplet(theAppletViewer);				if (result == noErr) {					if (theJMFrame) {						result = JMFrameGoAway(theJMFrame);						if (result == noErr) {							int i;							for (i = 0;  i < 6 && theFrameCreated && theJMFrame;  i++)								JMIdle(theSession, kDefaultJMTime);						}					}				}				result = JMDisposeAppletViewer(theAppletViewer);				theAppletViewer = nil;			}		}				if (theAWTContext != nil) {			result = JMDisposeAWTContext(theAWTContext);			theAWTContext = nil;		}				if (theLocatorRef != nil) {			result = JMDisposeAppletLocator(theLocatorRef);			theLocatorRef = nil;		}			result = checkError(JMCloseSession(theSession));	}		if (theUpdateRgn) {			DisposeRgn(theUpdateRgn);		theUpdateRgn = nil;	}	return result;}OSErr DoBlank(Handle storage, RgnHandle blankRgn, GMParamBlockPtr params){	OSErr result = noErr;	// Simply blanks out the area that we are going to draw in.	FillRgn(blankRgn, (ConstPatternParam) &params->qdGlobalsCopy->qdBlack);		return checkError(result);}// the control value is between 0 (.5s) and 100 (6s)#define getTime(xx) \	((xx + 1) * 500)static OSErr createSession(){	JMSessionCallbacks callbacks;	JMSecurityOptions security;	// fill in session callbacks	callbacks.fVersion = kJMVersion;	callbacks.fStandardOutput = nil;	callbacks.fStandardError = nil;	callbacks.fStandardIn = nil;	// fill in security story - we don't support running	// within a firewall.	security.fVersion = kJMVersion;	security.fVerifyMode = eCheckRemoteCode;	security.fUseHttpProxy = false;	security.fHttpProxy[0] = 0;	security.fHttpProxyPort = 0;	security.fUseFTPProxy = false;	security.fFTPProxy[0] = 0;	security.fFTPProxyPort = 0;	security.fUseFirewallProxy = false;	security.fFirewallProxy[0] = 0;	security.fFirewallProxyPort = 0;	security.fNetworkAccess = eUnrestrictedAccess;	security.fFileSystemAccess = eAllFSAccess;	security.fRestrictClassAccess = true;	security.fRestrictClassDefine = true;	setMessage("\pInitializing Java", nil);	/*	 * create a session	 */	return JMOpenSession(&theSession, &security, &callbacks, 0L);}static OSErr createContext(){	JMAWTContextCallbacks callbacks;	callbacks.fVersion = kJMVersion;	callbacks.fRequestFrame = _ctxRequestFrame;	callbacks.fReleaseFrame = _ctxReleaseFrame;	callbacks.fUniqueMenuID = _ctxUniqueMenuID;	callbacks.fExceptionOccurred = _ctxExceptionOccurred;		setMessage("\pCreating an AWTContext", nil);	OSErr result = JMNewAWTContext(&theAWTContext, theSession, &callbacks, 0L);	if (result == noErr) {		result = JMResumeAWTContext(theAWTContext);	}	return result;}static void _locatorCompleted(JMAppletLocatorRef ref, JMLocatorErrors status){	StringPtr pMsg = nil;		switch (status) {		case eLocatorNoErr:			break;		case eHostNotFound:			pMsg = "\pThe host could not be found";			break;		case eFileNotFound:			pMsg = "\pThe file could not be found";			break;		case eLocatorTimeout:			pMsg = "\pThe host could not be reached";			break;		case eLocatorKilled:			pMsg = "\pThe locator was killed";			break;	}		if (pMsg == nil)		theStage = eCreateViewerStage;	else {		setMessage(pMsg, nil);		theStage = eLocatorFailedStage;	}}static OSErr createAppletLocator(){	Str255 urlText;	Str255 labelText;	getSelectedURL(urlText, labelText);	urlText[urlText[0] + 1] = 0;	setMessage("\pFetching the html page for ", labelText);	JMAppletLocatorCallbacks locatorCallbacks;	locatorCallbacks.fVersion = kJMVersion;	locatorCallbacks.fCompleted = _locatorCompleted;		return JMNewAppletLocator(&theLocatorRef, theSession, &locatorCallbacks, (const char*) &urlText[1], nil, 0L);}static OSErr createViewer(){	if (theLocatorRef == nil)		return paramErr;	if (theAWTContext == nil)		return paramErr;	OSErr result = noErr;	UInt32 count;	result = JMCountApplets(theLocatorRef, &count);	if (result == noErr) {		if (count == 0)			_ctxExceptionOccurred(theAWTContext, "No applet tags were found on that page.", nil, nil);		else {			UInt32 width, height;			result = JMGetAppletDimensions(theLocatorRef, 0, &width, &height);				if (result == noErr) {				Rect* totalRect = &theParams->monitors->monitorList[0].bounds;				JMAppletViewerCallbacks callbacks;								theViewerVisible = false;				theFrameCreated = false;								theViewerBounds.left = totalRect->left + ((totalRect->right - totalRect->left) / 2) - (width / 2);				theViewerBounds.top = totalRect->top + ((totalRect->bottom - totalRect->top) / 2) - (height / 2);				theViewerBounds.right = theViewerBounds.left + width;				theViewerBounds.bottom = theViewerBounds.top + height;								callbacks.fVersion = kJMVersion;				callbacks.fShowDocument = nil;				callbacks.fSetStatusMsg = nil;				setMessage("\pCreating the applet viewer and loading classes.", "\p");								result = JMNewAppletViewer(&theAppletViewer, theAWTContext, theLocatorRef, 0, &callbacks, 0L);								if (result == noErr)					result = JMReloadApplet(theAppletViewer);			}		}	}		(void) JMDisposeAppletLocator(theLocatorRef);	theLocatorRef = nil;		return result;}static OSErr idleSession(){	OSErr result = noErr;	if (theSession != nil)		result = JMIdle(theSession, kDefaultJMTime);	return result;}OSErr DoDrawFrame(Handle storage, RgnHandle blankRgn, GMParamBlockPtr params){	OSErr result = noErr;		if (theStage == eRunningStage || theStage == eLocateAppletNotDone) {		if (theStage == eRunningStage) {			if (theJMFrame && theViewerVisible)				_frameCheckUpdate(theJMFrame);		}		result = idleSession();	} else {		switch (theStage) {			case eCreateSessionStage:				result = createSession();				if (result == noErr)					theStage = eCreateContextStage;				break;							case eCreateContextStage:				result = createContext();				if (result == noErr)					theStage = eLocateAppletStage;				break;			case eLocateAppletStage:				result = createAppletLocator();				if (result == noErr)					theStage = eLocateAppletNotDone;				break;						case eCreateViewerStage:				result = createViewer();				if (result == noErr)					theStage = eRunningStage;				break;						case eLocatorFailedStage:				result = fnfErr;				break;		}	}		return checkError(result);}OSErr DoSetUp(RgnHandle blankRgn ,short message, GMParamBlockPtr params){	Handle hText;		switch (message) {		case URL_BUTTON_MESSAGE:			newAppletDialog();			break;		default:			SysBeep(1);			break;		}	return noErr;}