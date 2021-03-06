/*

IEView Plugin for Miranda IM
Copyright (C) 2005  Piotr Piastucki

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "IEView.h"
#include "resource.h"
#include "Options.h"
#include "Utils.h"
#define WM_WAITWHILEBUSY (WM_USER+600)
//#define GECKO
#define DISPID_BEFORENAVIGATE2      250   // hyperlink clicked on
#define DISPID_NAVIGATECOMPLETE2    252   // UIActivate new document
#define DISPID_DOCUMENTCOMPLETE     259   // new document goes ReadyState_Complete

static const CLSID CLSID_MozillaBrowser=
{ 0x1339B54C, 0x3453, 0x11D2,
    { 0x93, 0xB9, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00 } };

IEView * IEView::list = NULL;
CRITICAL_SECTION IEView::mutex;
bool IEView::isInited = false;

static LRESULT CALLBACK IEViewServerWindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    IEView *view = IEView::get(GetParent(GetParent(hwnd)));
	if (view != NULL) {
		switch (message) {
		case WM_KEYUP:
			if (LOWORD(wParam) == VK_ESCAPE && !(GetKeyState(VK_SHIFT) & 0x8000)
			&& !(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) {
				//if (view->getBuilder() != NULL) {
					SendMessage(GetParent(GetParent(GetParent(hwnd))), WM_COMMAND, IDCANCEL, 0);
//				} else {

	//			}
			}
			break;
		case WM_KEYDOWN:
			view->translateAccelerator(message, wParam, lParam);
		   	break;
		case WM_SETFOCUS:
			{
				RECT rcWindow;
				POINT cursor;
				GetWindowRect(hwnd, &rcWindow);
				GetCursorPos(&cursor);
				if (cursor.y > rcWindow.bottom || cursor.y < rcWindow.top ||
					cursor.x > rcWindow.right || cursor.x < rcWindow.left) {
				} else {
					view->mouseActivate();
				}
				if (view->setFocus((HWND)wParam)) {
					return TRUE;
				}
			}
			break;
		case WM_LBUTTONDOWN:
		    POINT pt;
		    pt.x = LOWORD(lParam);
		    pt.y = HIWORD(lParam);
	   		if (view->mouseClick(pt)) {
	   		    return TRUE;
       		}
		    break;
		}
		return CallWindowProc(view->getServerWndProc(), hwnd, message, wParam, lParam);
    }
    return DefWindowProc (hwnd, message, wParam, lParam);
}

static LRESULT CALLBACK IEViewDocWindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
   	IEView *view = IEView::get(GetParent(hwnd));
   	if (view!=NULL) {
		WNDPROC oldWndProc = view->getDocWndProc();
    	if (message == WM_PARENTNOTIFY && wParam == WM_CREATE) {
			SetWindowLong(hwnd, GWL_WNDPROC, (LONG) oldWndProc);
			view->setServerWndProc((WNDPROC) SetWindowLong((HWND)lParam, GWL_WNDPROC, (LONG) IEViewServerWindowProcedure));
		}
		return CallWindowProc(oldWndProc, hwnd, message, wParam, lParam);
    }
    return DefWindowProc (hwnd, message, wParam, lParam);
}

static LRESULT CALLBACK IEViewWindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
   	IEView *view = IEView::get(hwnd);
   	if (view!=NULL) {
		WNDPROC oldWndProc = view->getMainWndProc();
    	if (message == WM_PARENTNOTIFY && wParam == WM_CREATE) {
			SetWindowLong(hwnd, GWL_WNDPROC, (LONG) oldWndProc);
			view->setDocWndProc((WNDPROC) SetWindowLong((HWND)lParam, GWL_WNDPROC, (LONG) IEViewDocWindowProcedure));
		}
		return CallWindowProc(oldWndProc, hwnd, message, wParam, lParam);
    }
    return DefWindowProc (hwnd, message, wParam, lParam);
}

void IEView::init() {
	if (isInited) return;
	isInited = true;
	InitializeCriticalSection(&mutex);
	if (FAILED(OleInitialize(NULL))) {
		MessageBoxA(NULL,"OleInitialize failed.","RESULT",MB_OK);
	}
}

void IEView::release() {
	while (list != NULL) {
	    delete list;
	}
    DeleteCriticalSection(&mutex);
}

#ifdef GECKO

static void __cdecl StartThread(void *vptr) {
	IEView *iev = (IEView *) vptr;
	iev->waitWhileBusy();
	return;
}
#endif

void IEView::waitWhileBusy() {
	VARIANT_BOOL busy;
	pWebBrowser->get_Busy(&busy);
	while (busy == VARIANT_TRUE) {
		Sleep(10);
		pWebBrowser->get_Busy(&busy);
	}
	PostMessage(hwnd, WM_WAITWHILEBUSY, 0, 0);
}

void IEView::setBorder() {
	LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
	LONG oldStyle = style;
	if (Options::getGeneralFlags() & Options::GENERAL_NO_BORDER) {
#ifndef GECKO
		style &= ~(WS_EX_STATICEDGE);
#endif
	} else {
		style |= (WS_EX_STATICEDGE);
	}
	if (oldStyle != style) {
		SetWindowLong(hwnd,GWL_EXSTYLE,style);
		SetWindowPos(getHWND(), NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
	}
//	RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
}

IEView::IEView(HWND parent, HTMLBuilder* builder, int x, int y, int cx, int cy) {
	MSG msg;
	IOleObject*   pOleObject = NULL;
    IOleInPlaceObject *pOleInPlace;
	this->parent = parent;
	this->builder = builder;
//	this->smileyWindow = NULL;
	prev = next = NULL;
	hwnd = NULL;
	sink = NULL;
	pWebBrowser = NULL;
	m_pConnectionPoint = NULL;
	m_cRef = 0;
	selectedText = NULL;
	getFocus = false;
	rcClient.left = x;
	rcClient.top = y;
	rcClient.right = x + cx;
	rcClient.bottom = y + cy;
#ifdef GECKO
	if (SUCCEEDED(CoCreateInstance(CLSID_MozillaBrowser, NULL, CLSCTX_INPROC, IID_IWebBrowser2, (LPVOID*)&pWebBrowser))) {
#else
	if (SUCCEEDED(CoCreateInstance(CLSID_WebBrowser, NULL, CLSCTX_INPROC, IID_IWebBrowser2, (LPVOID*)&pWebBrowser))) {
#endif
//		pWebBrowser->put_RegisterAsBrowser(VARIANT_FALSE);
		if (SUCCEEDED(pWebBrowser->QueryInterface(IID_IOleObject, (void**)&pOleObject))) {
    		pOleObject->SetClientSite(this);
    		pOleObject->DoVerb(OLEIVERB_INPLACEACTIVATE, &msg, this, 0, this->parent, &rcClient);
    		pOleObject->Release();
   		} else {
  			MessageBoxA(NULL,"IID_IOleObject failed.","RESULT",MB_OK);
   		}

		if (SUCCEEDED(pWebBrowser->QueryInterface(IID_IOleInPlaceObject, (void**)&pOleInPlace))) {
    		pOleInPlace->GetWindow(&hwnd);
    		pOleInPlace->Release();
		} else {
  			MessageBoxA(NULL,"IID_IOleInPlaceObject failed.","RESULT",MB_OK);
		}

		setBorder();
   		IConnectionPointContainer* pCPContainer;
	   // Step 1: Get a pointer to the connection point container.
   		if (SUCCEEDED(pWebBrowser->QueryInterface(IID_IConnectionPointContainer,
                                      		(void**)&pCPContainer))) {
	      // m_pConnectionPoint is defined like this:
	      // Step 2: Find the connection point.
      		if (SUCCEEDED(pCPContainer->FindConnectionPoint(DIID_DWebBrowserEvents2,
	                                             &m_pConnectionPoint))) {
	         // Step 3: Advise the connection point that you
	         // want to sink its events.
	            sink = new IEViewSink(this);
	         	if (FAILED(m_pConnectionPoint->Advise((IUnknown *)sink, &m_dwCookie)))     {
	            	MessageBoxA(NULL, "Failed to Advise", "C++ Event Sink", MB_OK);
	         	}
	      	}
      		pCPContainer->Release();
   		}
#ifndef GECKO
		setMainWndProc((WNDPROC)SetWindowLong(hwnd, GWL_WNDPROC, (LONG) IEViewWindowProcedure));
#else
//		setMainWndProc((WNDPROC)SetWindowLong(hwnd, GWL_WNDPROC, (LONG) MozillaWindowProcedure));
#endif
    }
    EnterCriticalSection(&mutex);
	next = list;
	if (next != NULL) {
	    next->prev = this;
	}
	list = this;
	LeaveCriticalSection(&mutex);
//	clear();
	pWebBrowser->put_RegisterAsDropTarget(VARIANT_FALSE);
}

IEView::~IEView() {
	IOleObject*   pOleObject = NULL;
	if (SUCCEEDED(pWebBrowser->QueryInterface(IID_IOleObject, (void**)&pOleObject))) {
		pOleObject->SetClientSite(NULL);
		pOleObject->Release();
	} else {
		MessageBoxA(NULL,"IID_IOleObject failed.","RESULT",MB_OK);
	}
    EnterCriticalSection(&mutex);
	if (list == this) {
		list = next;
	} else if (prev!=NULL) {
		prev->next = next;
	}
	if (next != NULL) {
		next->prev = prev;
	}
	LeaveCriticalSection(&mutex);
	if (builder != NULL) {
		delete builder;
		builder = NULL;
	}
	if (m_pConnectionPoint != NULL) {
        m_pConnectionPoint->Unadvise(m_dwCookie);
        m_pConnectionPoint->Release();
	}
	if (sink != NULL) {
		delete sink;
	}
	if (selectedText != NULL) {
		delete 	selectedText;
	}
#ifndef GECKO
	pWebBrowser->Release();
#endif
	DestroyWindow(hwnd);
}

void IEView::setMainWndProc(WNDPROC wndProc) {
	mainWndProc = wndProc;
}

WNDPROC IEView::getMainWndProc() {
	return mainWndProc;
}

void IEView::setDocWndProc(WNDPROC wndProc) {
	docWndProc = wndProc;
}

WNDPROC IEView::getDocWndProc() {
	return docWndProc;
}

void IEView::setServerWndProc(WNDPROC wndProc) {
	serverWndProc = wndProc;
}

WNDPROC IEView::getServerWndProc() {
	return serverWndProc;
}

// IUnknown
STDMETHODIMP IEView::QueryInterface(REFIID riid, PVOID *ppv) {
	*ppv=NULL;
	if (IID_IUnknown==riid)
		*ppv=this;
	if (IID_IOleClientSite==riid)
		*ppv=(IOleClientSite*)this;//Unknown)m_pIOleClientSite;
	if (IID_IOleWindow==riid || IID_IOleInPlaceSite==riid)
		*ppv=(IOleInPlaceSite*)this;//m_pIOleIPSite;
	if (IID_IDocHostUIHandler==riid)
		*ppv=(IDocHostUIHandler*)this;//m_pIOleIPSite;
	if (NULL!=*ppv) {
		((LPUNKNOWN)*ppv)->AddRef();
		return NOERROR;
	}
	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) IEView::AddRef(void) {
	++m_cRef;
	return m_cRef;
}

STDMETHODIMP_(ULONG) IEView::Release(void) {
	--m_cRef;
	return m_cRef;
}

// IDispatch
STDMETHODIMP IEView::GetTypeInfoCount(UINT *ptr) { return E_NOTIMPL; }
STDMETHODIMP IEView::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo) { return S_OK; }
STDMETHODIMP IEView::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) { return S_OK; }

STDMETHODIMP IEView::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid , WORD wFlags,
							DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO*pExcepInfo, UINT*puArgErr) {
 	switch (dispIdMember) {
		case  DISPID_AMBIENT_DLCONTROL:
			break;
	}
	return DISP_E_MEMBERNOTFOUND;
}

// IOleWindow
STDMETHODIMP IEView::GetWindow(HWND *phwnd) {
   *phwnd = parent;
	return S_OK;
}

STDMETHODIMP IEView::ContextSensitiveHelp(BOOL fEnterMode) {
	return E_NOTIMPL;
}

// IOleInPlace
STDMETHODIMP IEView::CanInPlaceActivate(void) {
   return S_OK;
}

STDMETHODIMP IEView::OnInPlaceActivate(void) {
	m_bInPlaceActive = TRUE;
	return S_OK;
}

STDMETHODIMP IEView::OnUIActivate(void) {
	return E_NOTIMPL;
}

STDMETHODIMP IEView::GetWindowContext(IOleInPlaceFrame **ppFrame, IOleInPlaceUIWindow **ppDoc,
							LPRECT lprcPosRect, LPRECT lprcClipRect,
						   LPOLEINPLACEFRAMEINFO lpFrameInfo) {

	lprcPosRect->left = rcClient.left;
	lprcPosRect->top = rcClient.top;
	lprcPosRect->right = rcClient.right;
	lprcPosRect->bottom = rcClient.bottom;
	lprcClipRect->left = rcClient.left;
	lprcClipRect->top = rcClient.top;
	lprcClipRect->right = rcClient.right;
	lprcClipRect->bottom = rcClient.bottom;
	return S_OK;
}

STDMETHODIMP IEView::Scroll(SIZE scrollExtant) {
	return E_NOTIMPL;
}

STDMETHODIMP IEView::OnUIDeactivate(BOOL fUndoable) {
	return E_NOTIMPL;
}

STDMETHODIMP IEView::OnInPlaceDeactivate( void) {
	m_bInPlaceActive = FALSE;
	return S_OK;
}

STDMETHODIMP IEView::DiscardUndoState( void) {
	return E_NOTIMPL;
}

STDMETHODIMP IEView::DeactivateAndUndo( void) {
	return E_NOTIMPL;
}

STDMETHODIMP IEView::OnPosRectChange(LPCRECT lprcPosRect) {
	return E_NOTIMPL;
}

// IOleClientSite
STDMETHODIMP IEView::SaveObject(void) {
	return E_NOTIMPL;
}
STDMETHODIMP IEView::GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, IMoniker **ppmk) {
	return E_NOTIMPL;
}
STDMETHODIMP IEView::GetContainer(IOleContainer **ppContainer) {
	return E_NOTIMPL;
}
STDMETHODIMP IEView::ShowObject(void) {
	return E_NOTIMPL;
}
STDMETHODIMP IEView::OnShowWindow(BOOL fShow) {
	return E_NOTIMPL;
}
STDMETHODIMP IEView::RequestNewObjectLayout(void) {
	return E_NOTIMPL;
}
// IDocHostUIHandler
STDMETHODIMP IEView::ShowContextMenu(DWORD dwID, POINT *ppt, IUnknown *pcmdTarget, IDispatch *pdispReserved) {
	IOleCommandTarget * pOleCommandTarget;
    IOleWindow * pOleWindow;
	HWND hSPWnd;
	if (builder == NULL) {
   //     return S_OK;
	}
#ifdef GECKO
	{
		return E_NOTIMPL;
/*
		HMENU hMenu;
		hMenu = GetSubMenu(LoadMenu(hInstance, MAKEINTRESOURCE(IDR_CONTEXTMENU)),0);
		CallService(MS_LANGPACK_TRANSLATEMENU,(WPARAM)hMenu,0);
		if (dwID == 6) { // anchor
			EnableMenuItem(hMenu, ID_MENU_COPYLINK, MF_BYCOMMAND | MF_ENABLED);
		} else if (dwID == 5) { // text select
			EnableMenuItem(hMenu, ID_MENU_COPY, MF_BYCOMMAND | MF_ENABLED);
		} else if (dwID == 1) { // control (image)
			EnableMenuItem(hMenu, ID_MENU_SAVEIMAGE, MF_BYCOMMAND | MF_ENABLED);
		}
		if (builder!=NULL) {

		}
		int iSelection = TrackPopupMenu(hMenu,
										  TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
										  ppt->x,
										  ppt->y,
										  0,
										  hwnd,
										  (RECT*)NULL);
		DestroyMenu(hMenu);
		if (iSelection == ID_MENU_CLEARLOG) {
			clear(NULL);
		} else {
			SendMessage(hSPWnd, WM_COMMAND, iSelection, (LPARAM) NULL);
		}
*/
	}
#else
    if (SUCCEEDED(pcmdTarget->QueryInterface(IID_IOleCommandTarget, (void**)&pOleCommandTarget))) {
		if (SUCCEEDED(pOleCommandTarget->QueryInterface(IID_IOleWindow, (void**)&pOleWindow))) {
    		pOleWindow->GetWindow(&hSPWnd);
			HMENU hMenu;
			hMenu = GetSubMenu(LoadMenu(hInstance, MAKEINTRESOURCE(IDR_CONTEXTMENU)),0);
		 	CallService(MS_LANGPACK_TRANSLATEMENU,(WPARAM)hMenu,0);
			if (dwID == 5) { // anchor
				EnableMenuItem(hMenu, ID_MENU_COPYLINK, MF_BYCOMMAND | MF_ENABLED);
			} else if (dwID == 4) { // text select
				EnableMenuItem(hMenu, ID_MENU_COPY, MF_BYCOMMAND | MF_ENABLED);
			} else if (dwID == 1) { // control (image)
				EnableMenuItem(hMenu, ID_MENU_SAVEIMAGE, MF_BYCOMMAND | MF_ENABLED);
			}
            if (builder!=NULL) {

			}
		 	int iSelection = TrackPopupMenu(hMenu,
		                                      TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
		                                      ppt->x,
		                                      ppt->y,
		                                      0,
		                                      hwnd,
		                                      (RECT*)NULL);
			DestroyMenu(hMenu);
			if (iSelection == ID_MENU_CLEARLOG) {
				clear(NULL);
			} else {
		    	SendMessage(hSPWnd, WM_COMMAND, iSelection, (LPARAM) NULL);
			}
    		pOleWindow->Release();
		}
	    pOleCommandTarget->Release();
	}
#endif
	return S_OK;
}
STDMETHODIMP IEView::GetHostInfo(DOCHOSTUIINFO *pInfo) {
 	pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER;// | DOCHOSTUIFLAG_DISABLE_SCRIPT_INACTIVE;
	if (builder == NULL) {
		pInfo->dwFlags |= DOCHOSTUIFLAG_DIALOG;
	}
 	return S_OK;
}
STDMETHODIMP IEView::ShowUI(DWORD dwID, IOleInPlaceActiveObject *pActiveObject, IOleCommandTarget *pCommandTarget,
		    				IOleInPlaceFrame *pFrame, IOleInPlaceUIWindow *pDoc) {
 	return S_OK;
}

STDMETHODIMP IEView::HideUI(void) {return S_OK;}
STDMETHODIMP IEView::UpdateUI(void) {return S_OK;}
STDMETHODIMP IEView::EnableModeless(BOOL fEnable) { return E_NOTIMPL; }
STDMETHODIMP IEView::OnDocWindowActivate(BOOL fEnable) { return E_NOTIMPL; }
STDMETHODIMP IEView::OnFrameWindowActivate(BOOL fEnable) { return E_NOTIMPL; }
STDMETHODIMP IEView::ResizeBorder(LPCRECT prcBorder, IOleInPlaceUIWindow *pUIWindow, BOOL fRameWindow) {return E_NOTIMPL;}
STDMETHODIMP IEView::TranslateAccelerator(LPMSG lpMsg, const GUID *pguidCmdGroup, DWORD nCmdID) { return S_FALSE;}
STDMETHODIMP IEView::GetOptionKeyPath(LPOLESTR *pchKey, DWORD dw) { return E_NOTIMPL; }
STDMETHODIMP IEView::GetDropTarget(IDropTarget *pDropTarget, IDropTarget **ppDropTarget) {
	*ppDropTarget = NULL;
	return S_OK;
//	return E_NOTIMPL;
}

STDMETHODIMP IEView::GetExternal(IDispatch **ppDispatch) {
	*ppDispatch = NULL;
    return S_FALSE;
}
STDMETHODIMP IEView::TranslateUrl(DWORD dwTranslate, OLECHAR *pchURLIn, OLECHAR **ppchURLOut) { return E_NOTIMPL; }
STDMETHODIMP IEView::FilterDataObject(IDataObject *pDO, IDataObject **ppDORet) { return E_NOTIMPL; }

IEViewSink::IEViewSink(IEView *smptr) {
	ieWindow = smptr;
}

IEViewSink::~IEViewSink() {}

STDMETHODIMP IEViewSink::QueryInterface(REFIID riid, PVOID *ppv) {
	*ppv=NULL;
	if (IID_IUnknown==riid) {
		*ppv=(IUnknown *)this;
	}
	if (IID_IDispatch==riid) {
		*ppv=(IDispatch *)this;
	}
	if (DIID_DWebBrowserEvents2==riid) {
		*ppv=(DWebBrowserEvents2*)this;
	}
	if (NULL!=*ppv) {
		((LPUNKNOWN)*ppv)->AddRef();
		return NOERROR;
	}
	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) IEViewSink::AddRef(void) {
	++m_cRef;
	return m_cRef;
}

STDMETHODIMP_(ULONG) IEViewSink::Release(void) {
	--m_cRef;
	return m_cRef;
}

STDMETHODIMP IEViewSink::GetTypeInfoCount(UINT *ptr) { return E_NOTIMPL; }
STDMETHODIMP IEViewSink::GetTypeInfo(UINT iTInfo, LCID lcid, LPTYPEINFO* ppTInfo) { return S_OK; }
STDMETHODIMP IEViewSink::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) { return S_OK; }

STDMETHODIMP IEViewSink::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid , WORD wFlags,
							DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO*pExcepInfo, UINT*puArgErr) {
	if (!pDispParams) return E_INVALIDARG;
 	switch (dispIdMember) {
		case DISPID_BEFORENAVIGATE2:
            BeforeNavigate2(pDispParams->rgvarg[6].pdispVal,
							pDispParams->rgvarg[5].pvarVal,
							pDispParams->rgvarg[4].pvarVal,
            				pDispParams->rgvarg[3].pvarVal,
            				pDispParams->rgvarg[2].pvarVal,
            				pDispParams->rgvarg[1].pvarVal,
            				pDispParams->rgvarg[0].pboolVal);
	    	return S_OK;
	}
	return DISP_E_MEMBERNOTFOUND;
}
// DWebBrowserEvents2

void IEViewSink::StatusTextChange(BSTR text) {}
void IEViewSink::ProgressChange(long progress, long progressMax) {}
void IEViewSink::CommandStateChange(long command, VARIANT_BOOL enable) {}
void IEViewSink::DownloadBegin() {}
void IEViewSink::DownloadComplete() {}
void IEViewSink::TitleChange(BSTR text) {}
void IEViewSink::PropertyChange(BSTR text) {}
void IEViewSink::BeforeNavigate2(IDispatch* pDisp,VARIANT* url,VARIANT* flags, VARIANT* targetFrameName,
								VARIANT* postData, VARIANT* headers, VARIANT_BOOL* cancel) {
   	int i = wcslen(url->bstrVal);
   	char *tTemp = new char[i+1];
   	WideCharToMultiByte(CP_ACP, 0, url->bstrVal, -1, tTemp, i+1, NULL, NULL);
#ifndef GECKO
	if (strcmp(tTemp, "about:blank")) {
//		if (smileyWindow==NULL) {
      		CallService(MS_UTILS_OPENURL, (WPARAM) 1, (LPARAM) tTemp);
  // 		} else {
	//		smileyWindow->choose(tTemp);
	//	}
    	*cancel = VARIANT_TRUE;
	}
#endif
   	delete tTemp;
}

void IEViewSink::NewWindow2(IDispatch** ppDisp, VARIANT_BOOL* cancel) {}
void IEViewSink::NavigateComplete(IDispatch* pDisp, VARIANT* url) {}
void IEViewSink::DocumentComplete(IDispatch* pDisp, VARIANT* url) {}
void IEViewSink::OnQuit() {}
void IEViewSink::OnVisible(VARIANT_BOOL visible) {}
void IEViewSink::OnToolBar(VARIANT_BOOL visible) {}
void IEViewSink::OnMenuBar(VARIANT_BOOL visible) {}
void IEViewSink::OnStatusBar(VARIANT_BOOL visible) {}
void IEViewSink::OnFullScreen(VARIANT_BOOL visible) {}
void IEViewSink::OnTheaterMode(VARIANT_BOOL visible) {}
void IEViewSink::WindowSetResizable(VARIANT_BOOL visible) {}
void IEViewSink::WindowSetLeft(long val) {}
void IEViewSink::WindowSetTop(long val) {}
void IEViewSink::WindowSetWidth(long val) {}
void IEViewSink::WindowSetHeight(long val) {}
void IEViewSink::WindowClosing(VARIANT_BOOL isChildWindow, VARIANT_BOOL* cancel) {}
void IEViewSink::ClientToHostWindow(long *cx, long *cy) {}
void IEViewSink::SetSecureLockIcon(long val) {}
void IEViewSink::FileDownload(VARIANT_BOOL* cancel) {}



IHTMLDocument2 *IEView::getDocument() {
	HRESULT hr = S_OK;
	IHTMLDocument2 *document = NULL;
	IDispatch *dispatch = NULL;
	if (SUCCEEDED(pWebBrowser->get_Document(&dispatch)) && (dispatch != NULL)) {
		hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void **)&document);
		dispatch->Release();
	}
	return document;
}

void IEView::setWindowPos(int x, int y, int cx, int cy) {
	rcClient.left = x;
	rcClient.top = y;
	rcClient.right = cx;
	rcClient.bottom = cy;//y + cy;
 	if (builder == NULL) {
		//scrollToTop();
	} else {
//		scrollToBottomSoft();
	}
	SetWindowPos(getHWND(), HWND_TOP, x, y, cx, cy, 0);
	/*
    IOleInPlaceObject * inPlaceObject;
   	if (SUCCEEDED(pWebBrowser->QueryInterface(IID_IOleInPlaceObject, (void **)&inPlaceObject))) {
   		inPlaceObject->SetObjectRects(&rcClient, &rcClient);
   		inPlaceObject->Release();
	}
 	*/
 	if (builder == NULL) {
		//scrollToTop();
	} else {
//		scrollToBottomSoft();
	}
}

void IEView::scrollToTop() {
	IHTMLDocument2 *document = getDocument();
	if (document != NULL) {
		IHTMLWindow2* pWindow = NULL;
		if (SUCCEEDED(document->get_parentWindow( &pWindow )) && pWindow != NULL) {
			pWindow->scrollBy( -0x01FFFFFF, -0x01FFFFFF );
			pWindow->Release();
		}
		document->Release();
	}
}

void IEView::scrollToBottomSoft() {
	IHTMLDocument2 *document = getDocument();
	if (document != NULL) {
		IHTMLWindow2* pWindow = NULL;
		if (SUCCEEDED(document->get_parentWindow( &pWindow )) && (pWindow != NULL)) {
			pWindow->scrollBy( -0x01FFFFFF, 0x01FFFFFF );
			pWindow->Release();
		}
		document->Release();
	}
}

void IEView::scrollToBottom() {/*
	IHTMLDocument2 *document = getDocument();
	if (document != NULL) {
		wchar_t *p = NULL;
		if (SUCCEEDED(document->get_readyState(&p))) {
		    int licznik = 0;
    		do {
          		if (FAILED(document->get_readyState(&p))) {
          		    break;
                }
                licznik++;
                if (licznik == 1) break;
                Sleep(10);
    		} while (!wcscmp(p, L"loading"));
		}
		IHTMLWindow2* pWindow = NULL;
		if (SUCCEEDED(document->get_parentWindow( &pWindow )) && pWindow != NULL) {
			pWindow->scrollBy( 0, 0x01FFFFFF );
		}
		document->Release();
	}*/

	IHTMLDocument2 *document = getDocument();
	if (document != NULL) {
        IHTMLElementCollection *collection;
        IHTMLElement *element;
		IDispatch *dispatch;
		if (SUCCEEDED(document->get_all(&collection)) && (collection != NULL)) {
            long len;
			if (SUCCEEDED(collection->get_length(&len))) {
				VARIANT	variant;
				variant.vt = VT_I4;
				variant.lVal = len-1;
				if (SUCCEEDED(collection->item(variant, variant, &dispatch)) && (dispatch != NULL)) {
					if (SUCCEEDED(dispatch->QueryInterface(IID_IHTMLElement,(void**)&element)) && (element != NULL)) {
 						variant.vt = VT_BOOL;
						variant.boolVal = VARIANT_FALSE;
						if (SUCCEEDED(element->scrollIntoView(variant))) {
							}
						element->Release();
					}
					dispatch->Release();
				}
			}
			collection->Release();
		}
		IHTMLWindow2* pWindow = NULL;
		if (SUCCEEDED(document->get_parentWindow( &pWindow )) && (pWindow != NULL)) {
			pWindow->scrollBy( -0x01FFFFFF, 0x01FFFFFF );
			pWindow->Release();
		}
		document->Release();
	}
}

void IEView::write(const wchar_t *text) {
	IHTMLDocument2 *document = getDocument();
	if (document != NULL) {
		SAFEARRAY *safe_array = SafeArrayCreateVector(VT_VARIANT,0,1);
		if (safe_array != NULL) {
			VARIANT	*variant;
			BSTR bstr;
			SafeArrayAccessData(safe_array,(LPVOID *)&variant);
			variant->vt = VT_BSTR;
			variant->bstrVal = bstr = SysAllocString(text);
			SafeArrayUnaccessData(safe_array);
			document->write(safe_array);
			//SysFreeString(bstr); -> SafeArrayDestroy should be enough
			SafeArrayDestroy(safe_array);
		}
		document->Release();
	}
}

void IEView::write(const char *text) {
	int textLen = strlen(text) + 1;
	wchar_t *wcsTemp = new wchar_t[textLen];
	MultiByteToWideChar(CP_UTF8, 0, text, -1, wcsTemp, textLen);
	write(wcsTemp);
	delete [] wcsTemp;
}

void IEView::writef(const char *fmt, ...) {
	char *str;
	va_list vararg;
	int strsize;
	va_start(vararg, fmt);
	str = (char *) malloc(strsize=2048);
	while (_vsnprintf(str, strsize, fmt, vararg) == -1)
		str = (char *) realloc(str, strsize+=2048);
	va_end(vararg);
	write(str);
	free(str);
}

void IEView::navigate(const char *url) {
	int textLen = strlen(url) + 1;
	WCHAR *tTemp = new WCHAR[textLen];
	MultiByteToWideChar(CP_ACP, 0, url, -1, tTemp, textLen);
	pWebBrowser->Navigate(tTemp, NULL, NULL, NULL, NULL);
	delete tTemp;
}

void IEView::navigate(const wchar_t *url) {
	pWebBrowser->Navigate((WCHAR *)url, NULL, NULL, NULL, NULL);
}

void IEView::documentClose() {

#ifdef GECKO
	/*
	IHTMLDocument2 *document = getDocument();
	if (document != NULL) {
		//write("</body></html>");
		document->close();
		document->Release();
	}
	*/
#endif
}

void IEView::appendEventOld(IEVIEWEVENT *event) {
	if (clearRequired) {
		clear(event);
	}
	if (builder!=NULL) {
		builder->appendEventOld(this, event);
	}
	getFocus = false;
}

void IEView::appendEvent(IEVIEWEVENT *event) {
	if (clearRequired) {
		clear(event);
	}
	if (builder!=NULL) {
		builder->appendEvent(this, event);
	}
	getFocus = false;
}

void IEView::clear(IEVIEWEVENT *event) {
#ifdef GECKO
    pWebBrowser->Navigate(L"about:blank", NULL, NULL, NULL, NULL);
	Utils::forkThread((void (__cdecl *)(void *))StartThread, 0, (void *) this);
	MSG msg;
	BOOL bRet;
	while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0) {
		if (bRet == -1) {
			// handle the error and possibly exit
		} else {
			if (msg.message == WM_WAITWHILEBUSY) {
				break;
			} else {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
	{
		IHTMLDocument2 *document = getDocument();
		if (document != NULL) {
			document->close();
			VARIANT		open_name;
			VARIANT		open_features;
			VARIANT		open_replace;
			IDispatch	*open_window	= NULL;
			VariantInit(&open_name);
			open_name.vt      = VT_BSTR;
			open_name.bstrVal = SysAllocString(L"_self");
			VariantInit(&open_features);
			VariantInit(&open_replace);

			HRESULT hr = document->open(SysAllocString(L"text/html"),
								open_name,
									open_features,
									open_replace,
									&open_window);
			if (hr == S_OK) {
			//	pWebBrowser->Refresh();
			}
			if (open_window != NULL) {
				open_window->Release();
			}
			document->Release();
		}
		if (builder!=NULL) {
			builder->clear(this, event);
		}
		clearRequired = false;
		getFocus = false;

	}
    return;
#endif
	IHTMLDocument2 *document = getDocument();
	if (document == NULL) {
		pWebBrowser->Navigate(L"about:blank", NULL, NULL, NULL, NULL);
		HRESULT hr = S_OK;
		IHTMLDocument2 *document = NULL;
		while ((document == NULL) && (hr == S_OK)) {
			Sleep(0);
			IDispatch *dispatch = NULL;
			if (SUCCEEDED(pWebBrowser->get_Document(&dispatch)) && (dispatch != NULL)) {
				hr = dispatch->QueryInterface(IID_IHTMLDocument2, (void **)&document);
				dispatch->Release();
			}
		}
		if (document != NULL) {
			document->Release();
		}
	} else {
		document->close();
		VARIANT		open_name;
		VARIANT		open_features;
		VARIANT		open_replace;
		IDispatch	*open_window	= NULL;
		VariantInit(&open_name);
		open_name.vt      = VT_BSTR;
		open_name.bstrVal = SysAllocString(L"_self");
		VariantInit(&open_features);
		VariantInit(&open_replace);

		HRESULT hr = document->open(SysAllocString(L"text/html"),
		                    open_name,
			                    open_features,
			                    open_replace,
			                    &open_window);
		if (hr == S_OK) {
		//	pWebBrowser->Refresh();
		}
		if (open_window != NULL) {
			open_window->Release();
		}
		document->Release();
	}
	if (builder!=NULL) {
		builder->clear(this, event);
	}
	clearRequired = false;
	getFocus = false;
	setBorder();
}

void* IEView::getSelection(IEVIEWEVENT *event) {
	if (selectedText!=NULL) delete selectedText;
	selectedText = getSelection();
	if (selectedText == NULL || wcslen(selectedText)== 0) return NULL;
	if (event->dwFlags & IEEF_NO_UNICODE) {
		int cp = CP_ACP;
		if (event->cbSize >= IEVIEWEVENT_SIZE_V2) {
			cp = event->codepage;
		}
		char *str = Utils::convertToString(selectedText, cp);
		delete selectedText;
		selectedText = (BSTR) str;
	}
	return (void *)selectedText;
}


IEView* IEView::get(HWND hwnd) {
	IEView *ptr;
	if (list == NULL) return NULL;
	EnterCriticalSection(&mutex);
	for (ptr = list; ptr !=NULL; ptr=ptr->next) {
		if (ptr->hwnd == hwnd) {
			break;
		}
	}
	LeaveCriticalSection(&mutex);
	return ptr;
}

HWND IEView::getHWND() {
	return hwnd;
}

void IEView::translateAccelerator(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    IOleInPlaceActiveObject* pIOIPAO;
    if (SUCCEEDED(pWebBrowser->QueryInterface(IID_IOleInPlaceActiveObject, (void**)&pIOIPAO))) {
       MSG msg;
       msg.message = uMsg;
       msg.wParam = wParam;
       msg.lParam = lParam;
       pIOIPAO->TranslateAccelerator(&msg);
       pIOIPAO->Release();
	}
}

/**
 * Returns the selected text within the active document
 **/
BSTR IEView::getSelection() {
	BSTR text = NULL;
	IHTMLDocument2 *document = getDocument();
	if (document != NULL) {
        IHTMLSelectionObject *pSelection = NULL;
		if (SUCCEEDED(document->get_selection( &pSelection )) && pSelection != NULL) {
			IDispatch *pDisp = NULL;
			if (SUCCEEDED(pSelection->createRange( &pDisp )) &&  pDisp != NULL) {
                IHTMLTxtRange *pRange = NULL;
			    if (SUCCEEDED(pDisp->QueryInterface(IID_IHTMLTxtRange, (void**)&pRange))) {
					if (SUCCEEDED(pRange->get_text(&text))) {
						text = Utils::dupString(text);
					}
					pRange->Release();
				}
				pDisp->Release();
			}
			pSelection->Release();
		}
		document->Release();
	}
	return text;
}


/**
 * Returns the destination url (href) of the given anchor element (or parent anchor element)
 **/
BSTR IEView::getHrefFromAnchor(IHTMLElement *element) {
    if (element != NULL) {
    	IHTMLAnchorElement * pAnchor;
        if (SUCCEEDED(element->QueryInterface(IID_IHTMLAnchorElement, (void**)&pAnchor)) && (pAnchor!=NULL)) {
			VARIANT	variant;
            BSTR url;
            if (SUCCEEDED(element->getAttribute(L"href", 2, &variant) && variant.vt == VT_BSTR)) {
				url = Utils::dupString(variant.bstrVal);
				SysFreeString(variant.bstrVal);
			}
            //pAnchor->get_href( &url );
//            if (url!=NULL) {
  //          	url2 = Utils::dupString(url);
  //          	SysFreeString(url);
    //        	url = url2;
      //     	}
            pAnchor->Release();
            return url;
        } else {
            IHTMLElement * parent;
            if (SUCCEEDED(element->get_parentElement(&parent)) && (parent!=NULL)) {
            	BSTR url = getHrefFromAnchor(parent);
            	parent->Release();
            	return url;
           	}
        }
    }
    return NULL;
}

bool IEView::mouseActivate() {
    if (GetFocus() != hwnd) {
		getFocus = true;
	}
	return false;
}

bool IEView::mouseClick(POINT pt) {
    bool result = false;
    if (GetFocus() != hwnd) {
		getFocus = true;
	}
	IHTMLDocument2 *document = getDocument();
	if (document != NULL) {
        IHTMLElement *element;
  		if (SUCCEEDED(document->elementFromPoint( pt.x, pt.y, &element ))&& element!=NULL) {
//		IHTMLAnchorElement * pAnchor;
//			if (SUCCEEDED(element->QueryInterface(IID_IHTMLAnchorElement, (void**)&pAnchor)) && (pAnchor!=NULL)) {
//				element->click();
//				result = true;
//				pAnchor->Release();
//			}
  			BSTR url = getHrefFromAnchor(element);
  			if (url != NULL) {
				if ((GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_CONTROL) & 0x8000)
				&& !(GetKeyState(VK_MENU) & 0x8000)) {
					SendMessage(GetParent(hwnd), WM_COMMAND, IDCANCEL, 0);
				}
  			    int i = wcslen(url);
  			    char *tTemp = new char[i+1];
  			    WideCharToMultiByte(CP_ACP, 0, url, -1, tTemp, i+1, NULL, NULL);
		    	CallService(MS_UTILS_OPENURL, (WPARAM) 1, (LPARAM) tTemp);
                delete tTemp;
                delete url;
                result = true;
  			}
  			element->Release();
  		}
	    document->Release();
	}
	return result;
}

bool IEView::setFocus(HWND prevFocus) {
	if (GetFocus() != hwnd && !getFocus) { // && IsChild(prevFocus, hwnd
		SetFocus(GetParent(getHWND()));
//		SetFocus(prevFocus);
		return true;
	}
	getFocus = false;
	return false;
}

void IEView::saveDocument() {
	IHTMLDocument2 *document = getDocument();
	if (document != NULL) {
		BSTR bCmd = SysAllocString(L"SaveAs");
    	VARIANT_BOOL vb;
    	VARIANT      vValue;
    	vValue.vt = VT_BOOL;
    	vValue.boolVal = TRUE;
        document->execCommand(bCmd, VARIANT_FALSE, vValue, &vb);
    	SysFreeString(bCmd);
	    document->Release();
	}
}

void IEView::navigate(IEVIEWNAVIGATE * nav) {
	if (nav->dwFlags & IENF_UNICODE) {
		navigate(nav->urlW);
	} else {
		navigate(nav->url);
	}
}
