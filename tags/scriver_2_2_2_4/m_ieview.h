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

#ifndef M_IEVIEW_INCLUDED
#define M_IEVIEW_INCLUDED

#define MS_IEVIEW_WINDOW  "IEVIEW/NewWindow"
#define MS_IEVIEW_EVENT	  "IEVIEW/Event"
#define MS_IEVIEW_UTILS   "IEVIEW/Utils"
#define MS_IEVIEW_SHOWSMILEYSELECTION  "IEVIEW/ShowSmileySelection"

#define ME_IEVIEW_NOTIFICATION  "IEVIEW/Notification"

#define IEW_CREATE  1               // create new window (control)
#define IEW_DESTROY 2               // destroy control
#define IEW_SETPOS  3               // set window position and size

#define IEWM_SRMM     0             // regular SRMM
#define IEWM_TABSRMM  1             // TabSRMM-compatible HTML builder
#define IEWM_HTML     2             // HTML
#define IEWM_SCRIVER  3             // HTML
#define IEWM_MUCC     4             // MUCC group chats GUI
#define IEWM_CHAT     5             // chat.dll group chats GUI

typedef struct {
	int			cbSize;             // size of the strusture
	int			iType;				// one of IEW_* values
	DWORD		dwMode;				// compatibility mode - one of IEWM_* values
	DWORD		dwFlags;			// flags, one of IEWF_* values
	HWND		parent;             // parent window HWND
	HWND 		hwnd;               // IEW_CREATE returns WebBrowser control's HWND here
	int			x;                  // IE control horizontal position
	int			y;                  // IE control vertical position
	int			cx;                 // IE control horizontal size
	int			cy;                 // IE control vertical size
} IEVIEWWINDOW;

#define IEE_LOG_EVENTS  	1       // log specified number of DB events
#define IEE_CLEAR_LOG		2       // clear log
#define IEE_GET_SELECTION	3       // get selected text

#define IEEF_RTL          1           // turn on RTL support
#define IEEF_NO_UNICODE   2           // disable Unicode support
#define IEEF_NO_SCROLLING 4           // do not scroll logs to bottom

typedef struct {
	int			cbSize;             // size of the strusture
	int			iType;				// one of IEE_* values
	DWORD		dwFlags;			// one of IEEF_* values
	HWND		hwnd;               // HWND returned by IEW_CREATE
	HANDLE      hContact;           // contact
	HANDLE 		hDbEventFirst;      // first event to log, when IEE_LOG_EVENTS returns it will contain
	                                // the last event actually logged or NULL if no event was logged
	int 		count;              // number of events to log
	int         codepage;           // ANSI codepage
} IEVIEWEVENT;

typedef struct {
	int cbSize;                //size of the structure
	const char* Protocolname;  //protocol to use... if you have defined a protocol, u can
                             //use your own protocol name. Smiley add wil automatically
                             //select the smileypack that is defined for your protocol.
                             //Or, use "Standard" for standard smiley set. Or "ICQ", "MSN"
                             //if you prefer those icons.
                             //If not found or NULL: "Standard" will be used
	int xPosition;             //Postition to place the selectwindow
	int yPosition;             // "
	int Direction;             //Direction (i.e. size upwards/downwards/etc) of the window 0, 1, 2, 3
  	HWND hwndTarget;           //Window, where to send the message when smiley is selected.
	UINT targetMessage;        //Target message, to be sent.
	LPARAM targetWParam;       //Target WParam to be sent (LParam will be char* to select smiley)
                             //see the example file.
} IEVIEWSHOWSMILEYSEL;

#define IEEDF_UNICODE 1             // if set pszText is a pointer to wchar_t string instead of char string
#define IEED_EVENT_MESSAGE			0x0001 // message
#define IEED_EVENT_STATUS			0x0002 // status change
#define IEED_EVENT_ERROR			0x0004 // error
#define IEED_EVENT_TOPIC			0x0005 // topic change
#define IEED_EVENT_JOIN				0x0080 // user joined
#define IEED_EVENT_LEAVE			0x0081 // user left


typedef struct tagIEVIEWEVENTDATA {
	int			cbSize;
	int			iType;				// Event type, one of MUCC_EVENT_* values
	DWORD		dwFlags;			// Event flags - IEEF_*
	int			font;				// Text font index
	int			fontSize;			// Text font size
	COLORREF	color;				// Text color
	const char *pszProto;			// Name of the protocol
//	const char *pszID;				// Unique identifier of the chat room corresponding to the event,
//	const char *pszName;			// Name of the chat room visible to the user
//	const char *pszUID;				// User identifier, usage depends on type of event
	const char *pszNick;			// Nick, usage depends on type of event
	const char *pszText;			// Text, usage depends on type of event
	DWORD		dwData;				// DWORD data e.g. status
	BOOL		bIsMe;				// TRUE if the event is related to the user
	time_t		time;				// Time of the event
	struct tagIEVIEWEVENTDATA *next;
} IEVIEWEVENTDATA;

#endif

