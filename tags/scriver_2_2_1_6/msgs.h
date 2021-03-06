/*
SRMM

Copyright 2000-2003 Miranda ICQ/IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

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
#ifndef SRMM_MSGS_H
#define SRMM_MSGS_H

#include <richedit.h>
#include <richole.h>
#define MSGERROR_CANCEL	0
#define MSGERROR_RETRY	    1

struct ErrorWindowData
{
	char *	szDescription;
	char *	szText;
	HWND	hwndParent;
	int		sendIdx;
};

struct ParentWindowData
{
	HWND	hwnd;
	HANDLE	hContact;
	HWND *	children;
	int	    childrenCount;
	HWND	hwndActive;
	HWND	hwndStatus;
	HWND	hwndTabs;
	DWORD	flags;
	POINT	mouseLBDownPos;
	int		mouseLBDown;
	int		lastClickTab;
	int		lastClickTime;
	int		nFlash;
	int		nFlashMax;
	int		bMinimized;
	int		windowWasCascaded;
};

struct NewMessageWindowLParam
{
	HANDLE	hContact;
	const char *szInitialText;
};

struct MessageSendInfo
{
	HANDLE hSendId;
	int		state;
	char *	sendBuffer;
	int		sendBufferSize;
};


struct MessageWindowData
{
	HWND hwnd;
	int	tabId;
	HANDLE hContact;
	struct ParentWindowData *parent;
	HWND hwndParent;
	HWND hwndLog;
	HANDLE hDbEventFirst, hDbEventLast;
	struct MessageSendInfo *sendInfo;
	int sendCount;
	HBRUSH hBkgBrush;
	int splitterPos, originalSplitterPos;
	char *sendBuffer;
	SIZE minEditBoxSize;
	SIZE minTopSize;
	RECT minEditInit;
	int	minLogBoxHeight;
	int	minEditBoxHeight;
	int toolbarHeight;
	int windowWasCascaded;
	int nFlash;
	int nFlashMax;
	int nTypeSecs;
	int nTypeMode;
	int avatarWidth;
	int avatarHeight;
	int limitAvatarMaxH;
	int limitAvatarMinH;
	HBITMAP avatarPic;
	DWORD nLastTyping;
	int showTyping;
	int showUnread;
	HWND hwndStatus;
	DWORD lastMessage;
	char *szProto;
	WORD wStatus;
	WORD wOldStatus;
	TCmdList *cmdList;
	TCmdList *cmdListCurrent;
	time_t 	lastEventTime;
	int    	lastEventType;
	DWORD	flags;		
	int		messagesInProgress;
};

#define HM_EVENTSENT         (WM_USER+10)
#define DM_REMAKELOG         (WM_USER+11)
#define HM_DBEVENTADDED      (WM_USER+12)
#define DM_CASCADENEWWINDOW  (WM_USER+13)
#define DM_OPTIONSAPPLIED    (WM_USER+14)
#define DM_SPLITTERMOVED     (WM_USER+15)
#define DM_UPDATETITLE       (WM_USER+16)
#define DM_APPENDTOLOG       (WM_USER+17)
#define DM_ERRORDECIDED      (WM_USER+18)
#define DM_SCROLLLOGTOBOTTOM (WM_USER+19)
#define DM_TYPING            (WM_USER+20)
#define DM_UPDATEWINICON     (WM_USER+21)
#define DM_UPDATELASTMESSAGE (WM_USER+22)
#define DM_USERNAMETOCLIP    (WM_USER+23)
#define DM_CHANGEICONS		 (WM_USER+24)

#define DM_AVATARCALCSIZE    (WM_USER+25)
#define DM_GETAVATAR         (WM_USER+26)
#define HM_AVATARACK         (WM_USER+28)
#define HM_ACKEVENT          (WM_USER+29)

#define DM_ADDCHILD          (WM_USER+30)
#define DM_REMOVECHILD		 (WM_USER+31)
#define DM_ACTIVATECHILD	 (WM_USER+32)
#define DM_UPDATESTATUSBAR	 (WM_USER+35)
#define DM_ACTIVATEPREV		 (WM_USER+40)
#define DM_ACTIVATENEXT		 (WM_USER+41)
#define DM_STARTFLASHING	 (WM_USER+45)
#define DM_CLEARLOG			 (WM_USER+46)
#define DM_SWITCHSTATUSBAR	 (WM_USER+47)
#define DM_SWITCHTOOLBAR	 (WM_USER+48)
#define DM_SWITCHTITLEBAR	 (WM_USER+49)
#define DM_SWITCHRTL		 (WM_USER+50)
#define DM_SWITCHUNICODE	 (WM_USER+51)
#define DM_MESSAGESENDING	 (WM_USER+52)


#define EVENTTYPE_STATUSCHANGE 25368

struct CREOleCallback
{
	IRichEditOleCallbackVtbl *lpVtbl;
	unsigned refCount;
	IStorage *pictStg;
	int nextStgId;
};

BOOL CALLBACK DlgProcParentWindow(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DlgProcMessage(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
int InitOptions(void);
BOOL CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
int DbEventIsShown(DBEVENTINFO * dbei, struct MessageWindowData *dat);
void StreamInEvents(HWND hwndDlg, HANDLE hDbEventFirst, int count, int fAppend);
void LoadMsgLogIcons(void);
void FreeMsgLogIcons(void);

#define MSGFONTID_MYMSG		  0
#define MSGFONTID_YOURMSG	  1
#define MSGFONTID_MYNAME	  2
#define MSGFONTID_MYTIME	  3
#define MSGFONTID_MYCOLON	  4
#define MSGFONTID_YOURNAME	  5
#define MSGFONTID_YOURTIME	  6
#define MSGFONTID_YOURCOLON	  7
#define MSGFONTID_MESSAGEAREA 8
#define MSGFONTID_NOTICE      9

void LoadMsgDlgFont(int i, LOGFONTA * lf, COLORREF * colour);
extern const int msgDlgFontCount;

#define LOADHISTORY_UNREAD    0
#define LOADHISTORY_COUNT     1
#define LOADHISTORY_TIME      2

#define SRMMMOD                    "SRMM"

#define SRMSGSET_SHOWTITLEBAR	   "ShowTitleBar"
#define SRMSGDEFSET_SHOWTITLEBAR  1
#define SRMSGSET_SHOWSTATUSBAR	   "ShowStatusBar"
#define SRMSGDEFSET_SHOWSTATUSBAR  0
#define SRMSGSET_USETABS		   "UseTabs"
#define SRMSGDEFSET_USETABS		   1
#define SRMSGSET_TABSATBOTTOM	   "TabsPosition"
#define SRMSGDEFSET_TABSATBOTTOM   0
#define SRMSGSET_LIMITNAMES		   "LimitNamesOnTabs"
#define SRMSGDEFSET_LIMITNAMES		1
#define SRMSGSET_SHOWBUTTONLINE    "ShowButtonLine"
#define SRMSGDEFSET_SHOWBUTTONLINE 1
#define SRMSGSET_SHOWINFOLINE      "ShowInfoLine"
#define SRMSGDEFSET_SHOWINFOLINE   1
#define SRMSGSET_SHOWPROGRESS	   "ShowProgress"
#define SRMSGDEFSET_SHOWPROGRESS   0
#define SRMSGSET_AUTOPOPUP         "AutoPopupMsg"
#define SRMSGDEFSET_AUTOPOPUP      0
#define SRMSGSET_AUTOMIN           "AutoMin"
#define SRMSGDEFSET_AUTOMIN        0
#define SRMSGSET_AUTOCLOSE         "AutoClose"
#define SRMSGDEFSET_AUTOCLOSE      0
#define SRMSGSET_SAVEPERCONTACT    "SavePerContact"
#define SRMSGDEFSET_SAVEPERCONTACT 0
#define SRMSGSET_SAVESPLITTERPERCONTACT "SaveSplitterPerContact"
#define SRMSGDEFSET_SAVESPLITTERPERCONTACT 0
#define SRMSGSET_CASCADE           "Cascade"
#define SRMSGDEFSET_CASCADE        1
#define SRMSGSET_SENDONENTER       "SendOnEnter"
#define SRMSGDEFSET_SENDONENTER    1
#define SRMSGSET_SENDONDBLENTER    "SendOnDblEnter"
#define SRMSGDEFSET_SENDONDBLENTER 0
#define SRMSGSET_STATUSICON        "UseStatusWinIcon"
#define SRMSGDEFSET_STATUSICON     0
#define SRMSGSET_SENDBUTTON        "UseSendButton"
#define SRMSGDEFSET_SENDBUTTON     0
#define SRMSGSET_CHARCOUNT         "ShowCharCount"
#define SRMSGDEFSET_CHARCOUNT      0
#define SRMSGSET_CTRLSUPPORT       "SupportCtrlUpDn"
#define SRMSGDEFSET_CTRLSUPPORT    1
#define SRMSGSET_DELTEMP           "DeleteTempCont"
#define SRMSGDEFSET_DELTEMP        0
#define SRMSGSET_MSGTIMEOUT        "MessageTimeout"
#define SRMSGDEFSET_MSGTIMEOUT     10000
#define SRMSGSET_MSGTIMEOUT_MIN    4000 // minimum value (4 seconds)
#define SRMSGSET_FLASHCOUNT        "FlashMax"
#define SRMSGDEFSET_FLASHCOUNT     5

#define SRMSGSET_LOADHISTORY       "LoadHistory"
#define SRMSGDEFSET_LOADHISTORY    LOADHISTORY_UNREAD
#define SRMSGSET_LOADCOUNT         "LoadCount"
#define SRMSGDEFSET_LOADCOUNT      10
#define SRMSGSET_LOADTIME          "LoadTime"
#define SRMSGDEFSET_LOADTIME       10

#define SRMSGSET_USELONGDATE       "UseLongDate"
#define SRMSGDEFSET_USELONGDATE    0
#define SRMSGSET_SHOWSECONDS       "ShowSeconds"
#define SRMSGDEFSET_SHOWSECONDS    1
#define SRMSGSET_USERELATIVEDATE   "UseRelativeDate"
#define SRMSGDEFSET_USERELATIVEDATE 0

#define SRMSGSET_GROUPMESSAGES     "GroupMessages"
#define SRMSGDEFSET_GROUPMESSAGES	0
#define SRMSGSET_MARKFOLLOWUPS		"MarkFollowUps"
#define SRMSGDEFSET_MARKFOLLOWUPS	0

#define SRMSGSET_SHOWLOGICONS      "ShowLogIcon"
#define SRMSGDEFSET_SHOWLOGICONS   1
#define SRMSGSET_HIDENAMES         "HideNames"
#define SRMSGDEFSET_HIDENAMES      1
#define SRMSGSET_SHOWTIME          "ShowTime"
#define SRMSGDEFSET_SHOWTIME       1
#define SRMSGSET_SHOWDATE          "ShowDate"
#define SRMSGDEFSET_SHOWDATE       0
#define SRMSGSET_SHOWSTATUSCH      "ShowStatusChanges"
#define SRMSGDEFSET_SHOWSTATUSCH   1
#define SRMSGSET_BKGCOLOUR         "BkgColour"
#define SRMSGDEFSET_BKGCOLOUR      GetSysColor(COLOR_WINDOW)

#define SRMSGSET_TYPING             "SupportTyping"
#define SRMSGSET_TYPINGNEW          "DefaultTyping"
#define SRMSGDEFSET_TYPINGNEW       1
#define SRMSGSET_TYPINGUNKNOWN      "UnknownTyping"
#define SRMSGDEFSET_TYPINGUNKNOWN   0
#define SRMSGSET_SHOWTYPING         "ShowTyping"
#define SRMSGDEFSET_SHOWTYPING      1
#define SRMSGSET_SHOWTYPINGWIN      "ShowTypingWin"
#define SRMSGDEFSET_SHOWTYPINGWIN   1
#define SRMSGSET_SHOWTYPINGNOWIN    "ShowTypingTray"
#define SRMSGDEFSET_SHOWTYPINGNOWIN 0
#define SRMSGSET_SHOWTYPINGCLIST    "ShowTypingClist"
#define SRMSGDEFSET_SHOWTYPINGCLIST 1


#define SRMSGSET_AVATARENABLE       "AvatarEnable"
#define SRMSGDEFSET_AVATARENABLE    1
#define SRMSGSET_LIMITAVHEIGHT      "AvatarLimitHeight"
#define SRMSGDEFSET_LIMITAVHEIGHT   0
#define SRMSGSET_AVHEIGHT           "AvatarHeight"
#define SRMSGDEFSET_AVHEIGHT        60
#define SRMSGSET_AVHEIGHTMIN        "AvatarHeightMin"
#define SRMSGDEFSET_AVHEIGHTMIN     20
#define SRMSGSET_AVATAR             "Avatar"

#endif
