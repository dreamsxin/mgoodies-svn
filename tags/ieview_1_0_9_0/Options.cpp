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
#include <shlobj.h>
#include "Options.h"
#include "resource.h"
//#include "Smiley.h"
#include "Template.h"
#include "Utils.h"
#include "m_MathModule.h"
#include "m_avatars.h"

#define UM_CHECKSTATECHANGE (WM_USER+100)
HANDLE hHookOptionsChanged;
static BOOL CALLBACK IEViewOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK IEViewGeneralOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
//static BOOL CALLBACK IEViewEmoticonsOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK IEViewSRMMOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK IEViewGroupChatsOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK IEViewHistoryOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static HWND hwndCurrentTab, hwndPages[4];
static ProtocolSettings *srmmCurrentProtoItem = NULL;
static ProtocolSettings *chatCurrentProtoItem = NULL;
static ProtocolSettings *historyCurrentProtoItem = NULL;
static HIMAGELIST hProtocolImageList = NULL;

#ifndef _MSC_VER
typedef struct tagTVKEYDOWN {
    NMHDR hdr;
    WORD wVKey;
    UINT flags;
} NMTVKEYDOWN, FAR *LPNMTVKEYDOWN;

BOOL TreeView_SetCheckState(HWND hwndTreeView, HTREEITEM hItem, BOOL fCheck)
{
    TVITEM tvItem;

    tvItem.mask = TVIF_HANDLE | TVIF_STATE;
    tvItem.hItem = hItem;
    tvItem.stateMask = TVIS_STATEIMAGEMASK;

    // Image 1 in the tree-view check box image list is the
    // unchecked box. Image 2 is the checked box.

    tvItem.state = INDEXTOSTATEIMAGEMASK((fCheck ? 2 : 1));

    return TreeView_SetItem(hwndTreeView, &tvItem);
}

BOOL TreeView_GetCheckState(HWND hwndTreeView, HTREEITEM hItem)
{
    TVITEM tvItem;

    // Prepare to receive the desired information.
    tvItem.mask = TVIF_HANDLE | TVIF_STATE;
    tvItem.hItem = hItem;
    tvItem.stateMask = TVIS_STATEIMAGEMASK;

    // Request the information.
    TreeView_GetItem(hwndTreeView, &tvItem);

    // Return zero if it's not checked, or nonzero otherwise.
    return ((BOOL)(tvItem.state >> 12) -1);
}
#endif

static LPARAM GetItemParam(HWND hwndTreeView, HTREEITEM hItem) {
	TVITEM tvi = {0};
	tvi.mask = TVIF_PARAM;
	tvi.hItem = hItem == NULL ? TreeView_GetSelection(hwndTreeView) : hItem;
	TreeView_GetItem(hwndTreeView, &tvi);
	return tvi.lParam;
}

static void SaveSRMMProtoSettings(HWND hwndDlg, ProtocolSettings *proto) {
	if (proto != NULL) {
		char path[MAX_PATH];
		int i;
		i = Options::MODE_COMPATIBLE;
		if (IsDlgButtonChecked(hwndDlg, IDC_MODE_TEMPLATE)) {
			i = Options::MODE_TEMPLATE;
		} else if (IsDlgButtonChecked(hwndDlg, IDC_MODE_CSS)) {
			i = Options::MODE_CSS;
		}
		proto->setSRMMModeTemp(i);
		i = IsDlgButtonChecked(hwndDlg, IDC_BACKGROUND_IMAGE) ? Options::LOG_IMAGE_ENABLED : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE) ? Options::LOG_IMAGE_SCROLL : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_NICKNAMES) ? Options::LOG_SHOW_NICKNAMES : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_TIME) ? Options::LOG_SHOW_TIME : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_DATE) ? Options::LOG_SHOW_DATE : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_SECONDS) ? Options::LOG_SHOW_SECONDS : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_LONG_DATE) ? Options::LOG_LONG_DATE : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_RELATIVE_DATE) ? Options::LOG_RELATIVE_DATE : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_GROUP_MESSAGES) ? Options::LOG_GROUP_MESSAGES : 0;
		proto->setSRMMFlagsTemp(i);
		GetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, path, sizeof(path));
		proto->setSRMMBackgroundFilenameTemp(path);
		GetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, path, sizeof(path));
		proto->setSRMMCssFilenameTemp(path);
		GetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, path, sizeof(path));
		proto->setSRMMCssFilenameRtl(path);
		GetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, path, sizeof(path));
		proto->setSRMMTemplateFilenameTemp(path);
		GetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, path, sizeof(path));
		proto->setSRMMTemplateFilenameRtlTemp(path);
	}
}

static void SaveChatProtoSettings(HWND hwndDlg, ProtocolSettings *proto) {
	if (proto != NULL) {
		char path[MAX_PATH];
		int i;
		i = Options::MODE_COMPATIBLE;
		if (IsDlgButtonChecked(hwndDlg, IDC_MODE_TEMPLATE)) {
			i = Options::MODE_TEMPLATE;
		} else if (IsDlgButtonChecked(hwndDlg, IDC_MODE_CSS)) {
			i = Options::MODE_CSS;
		}
		proto->setChatModeTemp(i);
		i = IsDlgButtonChecked(hwndDlg, IDC_BACKGROUND_IMAGE) ? Options::LOG_IMAGE_ENABLED : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE) ? Options::LOG_IMAGE_SCROLL : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_NICKNAMES) ? Options::LOG_SHOW_NICKNAMES : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_TIME) ? Options::LOG_SHOW_TIME : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_DATE) ? Options::LOG_SHOW_DATE : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_SECONDS) ? Options::LOG_SHOW_SECONDS : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_LONG_DATE) ? Options::LOG_LONG_DATE : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_RELATIVE_DATE) ? Options::LOG_RELATIVE_DATE : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_GROUP_MESSAGES) ? Options::LOG_GROUP_MESSAGES : 0;
		proto->setChatFlagsTemp(i);
		GetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, path, sizeof(path));
		proto->setChatBackgroundFilenameTemp(path);
		GetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, path, sizeof(path));
		proto->setChatCssFilenameTemp(path);
		GetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, path, sizeof(path));
		proto->setChatCssFilenameRtl(path);
		GetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, path, sizeof(path));
		proto->setChatTemplateFilenameTemp(path);
		GetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, path, sizeof(path));
		proto->setChatTemplateFilenameRtlTemp(path);
	}
}

static void SaveHistoryProtoSettings(HWND hwndDlg, ProtocolSettings *proto) {
	if (proto != NULL) {
		char path[MAX_PATH];
		int i;
		i = Options::MODE_COMPATIBLE;
		if (IsDlgButtonChecked(hwndDlg, IDC_MODE_TEMPLATE)) {
			i = Options::MODE_TEMPLATE;
		} else if (IsDlgButtonChecked(hwndDlg, IDC_MODE_CSS)) {
			i = Options::MODE_CSS;
		}
		proto->setHistoryModeTemp(i);
		i = IsDlgButtonChecked(hwndDlg, IDC_BACKGROUND_IMAGE) ? Options::LOG_IMAGE_ENABLED : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE) ? Options::LOG_IMAGE_SCROLL : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_NICKNAMES) ? Options::LOG_SHOW_NICKNAMES : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_TIME) ? Options::LOG_SHOW_TIME : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_DATE) ? Options::LOG_SHOW_DATE : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_SECONDS) ? Options::LOG_SHOW_SECONDS : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_LONG_DATE) ? Options::LOG_LONG_DATE : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_RELATIVE_DATE) ? Options::LOG_RELATIVE_DATE : 0;
		i |= IsDlgButtonChecked(hwndDlg, IDC_LOG_GROUP_MESSAGES) ? Options::LOG_GROUP_MESSAGES : 0;
		proto->setHistoryFlagsTemp(i);
		GetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, path, sizeof(path));
		proto->setHistoryBackgroundFilenameTemp(path);
		GetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, path, sizeof(path));
		proto->setHistoryCssFilenameTemp(path);
		GetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, path, sizeof(path));
		proto->setHistoryCssFilenameRtl(path);
		GetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, path, sizeof(path));
		proto->setHistoryTemplateFilenameTemp(path);
		GetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, path, sizeof(path));
		proto->setHistoryTemplateFilenameRtlTemp(path);
	}
}

static void UpdateControlsState(HWND hwndDlg) {

	BOOL bChecked = IsDlgButtonChecked(hwndDlg, IDC_MODE_TEMPLATE);
	EnableWindow(GetDlgItem(hwndDlg, IDC_TEMPLATES_FILENAME), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_TEMPLATES), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_TEMPLATES_FILENAME_RTL), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_TEMPLATES_RTL), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_NICKNAMES), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_TIME), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_DATE), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_SECONDS), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_LONG_DATE), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_RELATIVE_DATE), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_GROUP_MESSAGES), bChecked);

	bChecked = IsDlgButtonChecked(hwndDlg, IDC_MODE_CSS);
	EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNALCSS_FILENAME), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_EXTERNALCSS), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_EXTERNALCSS_RTL), bChecked);

	bChecked = IsDlgButtonChecked(hwndDlg, IDC_MODE_COMPATIBLE);
	EnableWindow(GetDlgItem(hwndDlg, IDC_BACKGROUND_IMAGE), bChecked);
	bChecked &= IsDlgButtonChecked(hwndDlg, IDC_BACKGROUND_IMAGE);
	EnableWindow(GetDlgItem(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE), bChecked);
	EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_BACKGROUND_IMAGE), bChecked);
}

static void UpdateSRMMProtoInfo(HWND hwndDlg, ProtocolSettings *proto) {
	if (proto != NULL) {
		HWND hProtoList = GetDlgItem(hwndDlg, IDC_PROTOLIST);
		TreeView_SetCheckState(hProtoList, TreeView_GetSelection(hProtoList), proto->isSRMMEnableTemp());
		CheckDlgButton(hwndDlg, IDC_MODE_TEMPLATE, proto->getSRMMModeTemp() == Options::MODE_TEMPLATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_MODE_CSS, proto->getSRMMModeTemp() == Options::MODE_CSS ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_MODE_COMPATIBLE, proto->getSRMMModeTemp() == Options::MODE_COMPATIBLE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_BACKGROUND_IMAGE, proto->getSRMMFlagsTemp() & Options::LOG_IMAGE_ENABLED ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE, proto->getSRMMFlagsTemp() & Options::LOG_IMAGE_SCROLL ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_NICKNAMES, proto->getSRMMFlagsTemp() & Options::LOG_SHOW_NICKNAMES ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_TIME, proto->getSRMMFlagsTemp() & Options::LOG_SHOW_TIME ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_DATE, proto->getSRMMFlagsTemp() & Options::LOG_SHOW_DATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_SECONDS, proto->getSRMMFlagsTemp() & Options::LOG_SHOW_SECONDS ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_LONG_DATE, proto->getSRMMFlagsTemp() & Options::LOG_LONG_DATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_RELATIVE_DATE, proto->getSRMMFlagsTemp() & Options::LOG_RELATIVE_DATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_GROUP_MESSAGES, proto->getSRMMFlagsTemp() & Options::LOG_GROUP_MESSAGES ? TRUE : FALSE);
		if (proto->getSRMMBackgroundFilenameTemp() != NULL) {
			SetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, proto->getSRMMBackgroundFilenameTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, "");
		}
		if (proto->getSRMMCssFilename() != NULL) {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, proto->getSRMMCssFilenameTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, "");
		}
		if (proto->getSRMMCssFilenameRtl() != NULL) {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, proto->getSRMMCssFilenameRtlTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, "");
		}
		if (proto->getSRMMTemplateFilenameTemp() != NULL) {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, proto->getSRMMTemplateFilenameTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, "");
		}
		if (proto->getSRMMTemplateFilenameRtlTemp() != NULL) {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, proto->getSRMMTemplateFilenameRtlTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, "");
		}
		srmmCurrentProtoItem = proto;
		UpdateControlsState(hwndDlg);
	}
}

static void UpdateChatProtoInfo(HWND hwndDlg, ProtocolSettings *proto) {
	if (proto != NULL) {
		HWND hProtoList = GetDlgItem(hwndDlg, IDC_PROTOLIST);
		TreeView_SetCheckState(hProtoList, TreeView_GetSelection(hProtoList), proto->isChatEnableTemp());
		CheckDlgButton(hwndDlg, IDC_MODE_TEMPLATE, proto->getChatModeTemp() == Options::MODE_TEMPLATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_MODE_CSS, proto->getChatModeTemp() == Options::MODE_CSS ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_MODE_COMPATIBLE, proto->getChatModeTemp() == Options::MODE_COMPATIBLE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_BACKGROUND_IMAGE, proto->getChatFlagsTemp() & Options::LOG_IMAGE_ENABLED ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE, proto->getChatFlagsTemp() & Options::LOG_IMAGE_SCROLL ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_NICKNAMES, proto->getChatFlagsTemp() & Options::LOG_SHOW_NICKNAMES ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_TIME, proto->getChatFlagsTemp() & Options::LOG_SHOW_TIME ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_DATE, proto->getChatFlagsTemp() & Options::LOG_SHOW_DATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_SECONDS, proto->getChatFlagsTemp() & Options::LOG_SHOW_SECONDS ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_LONG_DATE, proto->getChatFlagsTemp() & Options::LOG_LONG_DATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_RELATIVE_DATE, proto->getChatFlagsTemp() & Options::LOG_RELATIVE_DATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_GROUP_MESSAGES, proto->getChatFlagsTemp() & Options::LOG_GROUP_MESSAGES ? TRUE : FALSE);
		if (proto->getChatBackgroundFilenameTemp() != NULL) {
			SetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, proto->getChatBackgroundFilenameTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, "");
		}
		if (proto->getChatCssFilename() != NULL) {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, proto->getChatCssFilenameTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, "");
		}
		if (proto->getChatCssFilenameRtl() != NULL) {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, proto->getChatCssFilenameRtlTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, "");
		}
		if (proto->getChatTemplateFilenameTemp() != NULL) {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, proto->getChatTemplateFilenameTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, "");
		}
		if (proto->getChatTemplateFilenameRtlTemp() != NULL) {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, proto->getChatTemplateFilenameRtlTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, "");
		}
		chatCurrentProtoItem = proto;
		UpdateControlsState(hwndDlg);
	}
}

static void UpdateHistoryProtoInfo(HWND hwndDlg, ProtocolSettings *proto) {
	if (proto != NULL) {
		HWND hProtoList = GetDlgItem(hwndDlg, IDC_PROTOLIST);
		TreeView_SetCheckState(hProtoList, TreeView_GetSelection(hProtoList), proto->isHistoryEnableTemp());
		CheckDlgButton(hwndDlg, IDC_MODE_TEMPLATE, proto->getHistoryModeTemp() == Options::MODE_TEMPLATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_MODE_CSS, proto->getHistoryModeTemp() == Options::MODE_CSS ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_MODE_COMPATIBLE, proto->getHistoryModeTemp() == Options::MODE_COMPATIBLE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_BACKGROUND_IMAGE, proto->getHistoryFlagsTemp() & Options::LOG_IMAGE_ENABLED ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE, proto->getHistoryFlagsTemp() & Options::LOG_IMAGE_SCROLL ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_NICKNAMES, proto->getHistoryFlagsTemp() & Options::LOG_SHOW_NICKNAMES ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_TIME, proto->getHistoryFlagsTemp() & Options::LOG_SHOW_TIME ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_DATE, proto->getHistoryFlagsTemp() & Options::LOG_SHOW_DATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_SHOW_SECONDS, proto->getHistoryFlagsTemp() & Options::LOG_SHOW_SECONDS ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_LONG_DATE, proto->getHistoryFlagsTemp() & Options::LOG_LONG_DATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_RELATIVE_DATE, proto->getHistoryFlagsTemp() & Options::LOG_RELATIVE_DATE ? TRUE : FALSE);
		CheckDlgButton(hwndDlg, IDC_LOG_GROUP_MESSAGES, proto->getHistoryFlagsTemp() & Options::LOG_GROUP_MESSAGES ? TRUE : FALSE);
		if (proto->getHistoryBackgroundFilenameTemp() != NULL) {
			SetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, proto->getHistoryBackgroundFilenameTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, "");
		}
		if (proto->getHistoryCssFilename() != NULL) {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, proto->getHistoryCssFilenameTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, "");
		}
		if (proto->getHistoryCssFilenameRtl() != NULL) {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, proto->getHistoryCssFilenameRtlTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, "");
		}
		if (proto->getHistoryTemplateFilenameTemp() != NULL) {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, proto->getHistoryTemplateFilenameTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, "");
		}
		if (proto->getHistoryTemplateFilenameRtlTemp() != NULL) {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, proto->getHistoryTemplateFilenameRtlTemp());
		} else {
			SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, "");
		}
		historyCurrentProtoItem = proto;
		UpdateControlsState(hwndDlg);
	}
}

static void RefreshProtoIcons(HWND hwndDlg) {
	int i;
	ProtocolSettings *proto;
	if (hProtocolImageList != NULL) {
		ImageList_RemoveAll(hProtocolImageList);
	} else {
		for (i=0,proto=Options::getProtocolSettings();proto!=NULL;proto=proto->getNext(),i++);
		hProtocolImageList = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
			ILC_MASK | ILC_COLOR32, i, 0);
	}
	for (i=0,proto=Options::getProtocolSettings();proto!=NULL;proto=proto->getNext(),i++) {
		HICON hIcon = NULL;
		if (i > 0 ) {
			hIcon=(HICON)CallProtoService(proto->getProtocolName(), PS_LOADICON, PLI_PROTOCOL | PLIF_SMALL, 0);
			if (hIcon == NULL)  {
				hIcon=(HICON)CallProtoService(proto->getProtocolName(), PS_LOADICON, PLI_PROTOCOL, 0);
			}
		}
		if (hIcon == NULL) {
			hIcon = (HICON) LoadImage(hInstance, MAKEINTRESOURCE(IDI_SMILEY), IMAGE_ICON,
							GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
		}
		ImageList_AddIcon(hProtocolImageList, hIcon);
		DestroyIcon(hIcon);
	}
//	refreshProtoList(hwndDlg, IsDlgButtonChecked(hwndDlg, IDC_PROTO_SMILEYS));
}

static void RefreshProtoList(HWND hwndDlg, int mode, bool protoTemplates) {
	int i;
    HTREEITEM hItem = NULL;
	HWND hProtoList = GetDlgItem(hwndDlg, IDC_PROTOLIST);
	TreeView_DeleteAllItems(hProtoList);
	TreeView_SetImageList(hProtoList, hProtocolImageList, TVSIL_NORMAL);
	ProtocolSettings *proto;
	for (i=0,proto=Options::getProtocolSettings();proto!=NULL;proto=proto->getNext(),i++) {
		char protoName[128];
		TVINSERTSTRUCT tvi = {0};
		tvi.hParent = TVI_ROOT;
		tvi.hInsertAfter = TVI_LAST;
		tvi.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_STATE | TVIF_SELECTEDIMAGE;
		tvi.item.stateMask = TVIS_SELECTED | TVIS_STATEIMAGEMASK;
		if (i==0) {
			strcpy(protoName, Translate("Default"));
		} else {
			CallProtoService(proto->getProtocolName(), PS_GETNAME, sizeof(protoName), (LPARAM)protoName);
//			strcat(protoName, " ");
	//		strcat(protoName, Translate("protocol"));
		}
		tvi.item.pszText = protoName;
		tvi.item.lParam = (LPARAM)proto;
		tvi.item.iImage = i;
		tvi.item.iSelectedImage = i;
		switch (mode) {
			case 0:
				tvi.item.state = INDEXTOSTATEIMAGEMASK(proto->isSRMMEnableTemp() ? 2 : 1);
				break;
			case 1:
				tvi.item.state = INDEXTOSTATEIMAGEMASK(proto->isChatEnableTemp() ? 2 : 1);
				break;
			case 2:
				tvi.item.state = INDEXTOSTATEIMAGEMASK(proto->isHistoryEnableTemp() ? 2 : 1);
				break;
		}
		if (i==0) {
			hItem = TreeView_InsertItem(hProtoList, &tvi);
		} else {
			TreeView_InsertItem(hProtoList, &tvi);
		}
		if (!protoTemplates) break;
	}
//	UpdateSRMMProtoInfo(hwndDlg, Options::getProtocolSettings());
	TreeView_SelectItem(hProtoList, hItem);
}

static bool BrowseFile(HWND hwndDlg, TCHAR *filter, TCHAR *defExt,  TCHAR *path, int maxLen) {
	OPENFILENAMEA ofn={0};
	GetWindowText(hwndDlg, path, maxLen);
	ofn.lStructSize = sizeof(OPENFILENAME);//_SIZE_VERSION_400;
	ofn.hwndOwner = hwndDlg;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = filter;//"Templates (*.ivt)\0*.ivt\0All Files\0*.*\0\0";
	ofn.lpstrFile = path;
	ofn.Flags = OFN_FILEMUSTEXIST;
	ofn.nMaxFile = maxLen;
	ofn.nMaxFileTitle = maxLen;
	ofn.lpstrDefExt = defExt;//"ivt";
	if(GetOpenFileName(&ofn)) {
		SetWindowText(hwndDlg, path);
		return true;
	}
	return false;
}

int IEViewOptInit(WPARAM wParam, LPARAM lParam)
{
	OPTIONSDIALOGPAGE odp;

	ZeroMemory(&odp, sizeof(odp));
	odp.cbSize = sizeof(odp);
	odp.position = 0;
	odp.hInstance = hInstance;
	odp.pszGroup = Translate("Message Sessions");
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPTIONS);
	odp.pszTitle = Translate("IEView");
	odp.flags = ODPF_BOLDGROUPS;
	odp.pfnDlgProc = IEViewOptDlgProc;
	odp.nIDBottomSimpleControl = 0;
	CallService(MS_OPT_ADDPAGE, wParam, (LPARAM) &odp);
	return 0;
}

static BOOL CALLBACK IEViewOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_INITDIALOG:
		{
            HWND tc;
			TCITEM tci;
			RefreshProtoIcons(hwndDlg);
			Options::resetProtocolSettings();

			tc = GetDlgItem(hwndDlg, IDC_TABS);
			tci.mask = TCIF_TEXT;
			tci.pszText = TranslateT("General");
			TabCtrl_InsertItem(tc, 0, &tci);
			tci.pszText = TranslateT("Message Log");
			TabCtrl_InsertItem(tc, 1, &tci);
			tci.pszText = TranslateT("Group Chats");
			TabCtrl_InsertItem(tc, 2, &tci);
			tci.pszText = TranslateT("History");
			TabCtrl_InsertItem(tc, 3, &tci);
//			hwndEmoticons = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_EMOTICONS_OPTIONS), hwndDlg, IEViewEmoticonsOptDlgProc, (LPARAM) NULL);
	//		SetWindowPos(hwndEmoticons, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
			hwndPages[0] = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_GENERAL_OPTIONS), hwndDlg, IEViewGeneralOptDlgProc, (LPARAM) NULL);
			SetWindowPos(hwndPages[0], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
			hwndPages[1] = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_SRMM_OPTIONS), hwndDlg, IEViewSRMMOptDlgProc, (LPARAM) NULL);
			SetWindowPos(hwndPages[1], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
			hwndPages[2] = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_GROUPCHATS_OPTIONS), hwndDlg, IEViewGroupChatsOptDlgProc, (LPARAM) NULL);
			SetWindowPos(hwndPages[2], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
			hwndPages[3] = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_HISTORY_OPTIONS), hwndDlg, IEViewHistoryOptDlgProc, (LPARAM) NULL);
			SetWindowPos(hwndPages[3], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
			hwndCurrentTab = hwndPages[0];
			ShowWindow(hwndPages[0], SW_SHOW);
			return TRUE;
		}
	case WM_COMMAND:
		break;
	case WM_NOTIFY:
		{
			switch (((LPNMHDR) lParam)->code) {
			case TCN_SELCHANGE:
                switch (wParam) {
				case IDC_TABS:
					{
						HWND hwnd = hwndPages[TabCtrl_GetCurSel(GetDlgItem(hwndDlg, IDC_TABS))];
						if (hwnd!=hwndCurrentTab) {
	                    	ShowWindow(hwnd, SW_SHOW);
	                    	ShowWindow(hwndCurrentTab, SW_HIDE);
	                    	hwndCurrentTab = hwnd;
						}
					}
					break;
				}
				break;
			case PSN_APPLY:
				for (int i = 0; i < 4; i++) {
                    SendMessage(hwndPages[i], WM_NOTIFY, wParam, lParam);
				}
				NotifyEventHooks(hHookOptionsChanged, 0, 0);
				Options::saveProtocolSettings();
				return TRUE;
			}
		}
		break;
	case WM_DESTROY:
		break;
	}
	return FALSE;
}

static BOOL CALLBACK IEViewGeneralOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	int i;
	switch (msg) {
	case WM_INITDIALOG:
		{
			TranslateDialogDefault(hwndDlg);
			if (Options::getGeneralFlags() & Options::GENERAL_ENABLE_BBCODES) {
				CheckDlgButton(hwndDlg, IDC_ENABLE_BBCODES, TRUE);
			}
			if (Options::getGeneralFlags() & Options::GENERAL_ENABLE_FLASH) {
				CheckDlgButton(hwndDlg, IDC_ENABLE_FLASH, TRUE);
			}
			if (Options::getGeneralFlags() & Options::GENERAL_ENABLE_MATHMODULE) {
				CheckDlgButton(hwndDlg, IDC_ENABLE_MATHMODULE, TRUE);
			}
			if (Options::getGeneralFlags() & Options::GENERAL_ENABLE_PNGHACK) {
				CheckDlgButton(hwndDlg, IDC_ENABLE_PNGHACK, TRUE);
			}
			if (Options::getGeneralFlags() & Options::GENERAL_SMILEYINNAMES) {
				CheckDlgButton(hwndDlg, IDC_SMILEYS_IN_NAMES, TRUE);
			}
			EnableWindow(GetDlgItem(hwndDlg, IDC_ENABLE_MATHMODULE), Options::isMathModule());
			EnableWindow(GetDlgItem(hwndDlg, IDC_SMILEYS_IN_NAMES), Options::isSmileyAdd());
			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {
			case IDC_ENABLE_BBCODES:
            case IDC_ENABLE_FLASH:
            case IDC_ENABLE_MATHMODULE:
            case IDC_ENABLE_PNGHACK:
            case IDC_SMILEYS_IN_NAMES:
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			}
		}
		break;
	case WM_NOTIFY:
		{
			switch (((LPNMHDR) lParam)->code) {
			case PSN_APPLY:
				i = 0;
				if (IsDlgButtonChecked(hwndDlg, IDC_ENABLE_BBCODES)) {
					i |= Options::GENERAL_ENABLE_BBCODES;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_ENABLE_FLASH)) {
					i |= Options::GENERAL_ENABLE_FLASH;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_ENABLE_MATHMODULE)) {
					i |= Options::GENERAL_ENABLE_MATHMODULE;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_ENABLE_PNGHACK)) {
					i |= Options::GENERAL_ENABLE_PNGHACK;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_SMILEYS_IN_NAMES)) {
					i |= Options::GENERAL_SMILEYINNAMES;
				}
				Options::setGeneralFlags(i);
				return TRUE;
			}
		}
		break;
	case WM_DESTROY:
		break;
	}
	return FALSE;
}

static BOOL CALLBACK IEViewSRMMOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	BOOL bChecked;
	char path[MAX_PATH];
	switch (msg) {
	case WM_INITDIALOG:
		{
			TranslateDialogDefault(hwndDlg);
			srmmCurrentProtoItem = NULL;
			RefreshProtoList(hwndDlg, 0, true);
			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {
			case IDC_BACKGROUND_IMAGE_FILENAME:
            case IDC_EXTERNALCSS_FILENAME:
            case IDC_EXTERNALCSS_FILENAME_RTL:
            case IDC_TEMPLATES_FILENAME:
            case IDC_TEMPLATES_FILENAME_RTL:
				if ((HWND)lParam==GetFocus() && HIWORD(wParam)==EN_CHANGE)
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_SCROLL_BACKGROUND_IMAGE:
			case IDC_LOG_SHOW_NICKNAMES:
			case IDC_LOG_SHOW_TIME:
			case IDC_LOG_SHOW_DATE:
			case IDC_LOG_SHOW_SECONDS:
			case IDC_LOG_LONG_DATE:
			case IDC_LOG_RELATIVE_DATE:
			case IDC_LOG_GROUP_MESSAGES:
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_BACKGROUND_IMAGE:
                bChecked = IsDlgButtonChecked(hwndDlg, IDC_MODE_COMPATIBLE) && IsDlgButtonChecked(hwndDlg, IDC_BACKGROUND_IMAGE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_BACKGROUND_IMAGE), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE), bChecked);
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_BROWSE_TEMPLATES:
				if (BrowseFile(hwndDlg, "Template (*.ivt)\0*.ivt\0All Files\0*.*\0\0", "ivt", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_TEMPLATES_FILENAME, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_TEMPLATES_RTL:
				if (BrowseFile(hwndDlg, "Template (*.ivt)\0*.ivt\0All Files\0*.*\0\0", "ivt", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_BACKGROUND_IMAGE:
				if (BrowseFile(hwndDlg, "All Images (*.jpg,*.gif,*.png,*.bmp)\0*.jpg;*.gif;*.png;*.bmp\0All Files\0*.*\0\0", "jpg", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg,IDC_BACKGROUND_IMAGE_FILENAME,path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_EXTERNALCSS:
				if (BrowseFile(hwndDlg, "Style Sheet (*.css)\0*.css\0All Files\0*.*\0\0", "css", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_EXTERNALCSS_FILENAME, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_EXTERNALCSS_RTL:
				if (BrowseFile(hwndDlg, "Style Sheet (*.css)\0*.css\0All Files\0*.*\0\0", "css", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_MODE_COMPATIBLE:
			case IDC_MODE_CSS:
			case IDC_MODE_TEMPLATE:
				UpdateControlsState(hwndDlg);
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			}
		}
		break;
	case UM_CHECKSTATECHANGE:
		{
			ProtocolSettings *proto = (ProtocolSettings *)GetItemParam((HWND)wParam, (HTREEITEM) lParam);
			if (proto != NULL) {
				if (strcmpi(proto->getProtocolName(), "_default_")) {
					proto->setSRMMEnableTemp(TreeView_GetCheckState((HWND)wParam, (HTREEITEM) lParam));
				}
			}
			if ((HTREEITEM) lParam != TreeView_GetSelection((HWND)wParam)) {
				TreeView_SelectItem((HWND)wParam, (HTREEITEM) lParam);
			} else {
				UpdateSRMMProtoInfo(hwndDlg, proto);
			}
			SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
		}
		break;
	case WM_NOTIFY:
		{
			if (((LPNMHDR)lParam)->idFrom == IDC_PROTOLIST) {
				switch (((LPNMHDR)lParam)->code) {
					case NM_CLICK:
						{
							TVHITTESTINFO ht = {0};
							DWORD dwpos = GetMessagePos();
							POINTSTOPOINT(ht.pt, MAKEPOINTS(dwpos));
							MapWindowPoints(HWND_DESKTOP, ((LPNMHDR)lParam)->hwndFrom, &ht.pt, 1);
							TreeView_HitTest(((LPNMHDR)lParam)->hwndFrom, &ht);
							if (TVHT_ONITEMSTATEICON & ht.flags) {
                                PostMessage(hwndDlg, UM_CHECKSTATECHANGE, (WPARAM)((LPNMHDR)lParam)->hwndFrom, (LPARAM)ht.hItem);
                                return FALSE;
							}
						}
						break;
					case TVN_KEYDOWN:
						 if (((LPNMTVKEYDOWN) lParam)->wVKey == VK_SPACE)
								PostMessage(hwndDlg, UM_CHECKSTATECHANGE, (WPARAM)((LPNMHDR)lParam)->hwndFrom,
								(LPARAM)TreeView_GetSelection(((LPNMHDR)lParam)->hwndFrom));
						break;
					case TVN_SELCHANGED:
						{
							HWND hLstView = GetDlgItem(hwndDlg, IDC_PROTOLIST);
							ProtocolSettings *proto = (ProtocolSettings *)GetItemParam(hLstView, (HTREEITEM) NULL);
							SaveSRMMProtoSettings(hwndDlg, srmmCurrentProtoItem);
							UpdateSRMMProtoInfo(hwndDlg, proto);
						}
						break;
				}
				break;
			}
			switch (((LPNMHDR) lParam)->code) {
			case PSN_APPLY:
				SaveSRMMProtoSettings(hwndDlg, srmmCurrentProtoItem);
				return TRUE;
			}
		}
		break;
	case WM_DESTROY:
		break;
	}
	return FALSE;
}

static BOOL CALLBACK IEViewHistoryOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	BOOL bChecked = FALSE;
	char path[MAX_PATH];
	switch (msg) {
	case WM_INITDIALOG:
		{
			TranslateDialogDefault(hwndDlg);
			historyCurrentProtoItem = NULL;
			RefreshProtoList(hwndDlg, 2, true);
			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {
			case IDC_BACKGROUND_IMAGE_FILENAME:
            case IDC_EXTERNALCSS_FILENAME:
            case IDC_EXTERNALCSS_FILENAME_RTL:
            case IDC_TEMPLATES_FILENAME:
            case IDC_TEMPLATES_FILENAME_RTL:
				if ((HWND)lParam==GetFocus() && HIWORD(wParam)==EN_CHANGE)
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_SCROLL_BACKGROUND_IMAGE:
			case IDC_LOG_SHOW_NICKNAMES:
			case IDC_LOG_SHOW_TIME:
			case IDC_LOG_SHOW_DATE:
			case IDC_LOG_SHOW_SECONDS:
			case IDC_LOG_LONG_DATE:
			case IDC_LOG_RELATIVE_DATE:
			case IDC_LOG_GROUP_MESSAGES:
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_BACKGROUND_IMAGE:
                bChecked = IsDlgButtonChecked(hwndDlg, IDC_MODE_COMPATIBLE) && IsDlgButtonChecked(hwndDlg, IDC_BACKGROUND_IMAGE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_BACKGROUND_IMAGE), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE), bChecked);
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_BROWSE_TEMPLATES:
				if (BrowseFile(hwndDlg, "Template (*.ivt)\0*.ivt\0All Files\0*.*\0\0", "ivt", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_TEMPLATES_FILENAME, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_TEMPLATES_RTL:
				if (BrowseFile(hwndDlg, "Template (*.ivt)\0*.ivt\0All Files\0*.*\0\0", "ivt", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_BACKGROUND_IMAGE:
				if (BrowseFile(hwndDlg, "All Images (*.jpg,*.gif,*.png,*.bmp)\0*.jpg;*.gif;*.png;*.bmp\0All Files\0*.*\0\0", "jpg", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg,IDC_BACKGROUND_IMAGE_FILENAME,path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_EXTERNALCSS:
				if (BrowseFile(hwndDlg, "Style Sheet (*.css)\0*.css\0All Files\0*.*\0\0", "css", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_EXTERNALCSS_FILENAME, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_EXTERNALCSS_RTL:
				if (BrowseFile(hwndDlg, "Style Sheet (*.css)\0*.css\0All Files\0*.*\0\0", "css", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_MODE_COMPATIBLE:
			case IDC_MODE_CSS:
			case IDC_MODE_TEMPLATE:
				UpdateControlsState(hwndDlg);
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			}
		}
		break;
	case UM_CHECKSTATECHANGE:
		{
			ProtocolSettings *proto = (ProtocolSettings *)GetItemParam((HWND)wParam, (HTREEITEM) lParam);
			if (proto != NULL) {
				if (strcmpi(proto->getProtocolName(), "_default_")) {
					proto->setHistoryEnableTemp(TreeView_GetCheckState((HWND)wParam, (HTREEITEM) lParam));
				}
			}
			if ((HTREEITEM) lParam != TreeView_GetSelection((HWND)wParam)) {
				TreeView_SelectItem((HWND)wParam, (HTREEITEM) lParam);
			} else {
				UpdateHistoryProtoInfo(hwndDlg, proto);
			}
			SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
		}
		break;
	case WM_NOTIFY:
		{
			if (((LPNMHDR)lParam)->idFrom == IDC_PROTOLIST) {
				switch (((LPNMHDR)lParam)->code) {
					case NM_CLICK:
						{
							TVHITTESTINFO ht = {0};
							DWORD dwpos = GetMessagePos();
							POINTSTOPOINT(ht.pt, MAKEPOINTS(dwpos));
							MapWindowPoints(HWND_DESKTOP, ((LPNMHDR)lParam)->hwndFrom, &ht.pt, 1);
							TreeView_HitTest(((LPNMHDR)lParam)->hwndFrom, &ht);
							if (TVHT_ONITEMSTATEICON & ht.flags) {
                                PostMessage(hwndDlg, UM_CHECKSTATECHANGE, (WPARAM)((LPNMHDR)lParam)->hwndFrom, (LPARAM)ht.hItem);
                                return FALSE;
							}
						}
						break;
					case TVN_KEYDOWN:
						 if (((LPNMTVKEYDOWN) lParam)->wVKey == VK_SPACE)
								PostMessage(hwndDlg, UM_CHECKSTATECHANGE, (WPARAM)((LPNMHDR)lParam)->hwndFrom,
								(LPARAM)TreeView_GetSelection(((LPNMHDR)lParam)->hwndFrom));
						break;
					case TVN_SELCHANGED:
						{
							HWND hLstView = GetDlgItem(hwndDlg, IDC_PROTOLIST);
							ProtocolSettings *proto = (ProtocolSettings *)GetItemParam(hLstView, (HTREEITEM) NULL);
							SaveHistoryProtoSettings(hwndDlg, historyCurrentProtoItem);
							UpdateHistoryProtoInfo(hwndDlg, proto);
						}
						break;
				}
				break;
			}
			switch (((LPNMHDR) lParam)->code) {
			case PSN_APPLY:
				SaveHistoryProtoSettings(hwndDlg, historyCurrentProtoItem);
				return TRUE;
			}
		}
		break;
	case WM_DESTROY:
		break;
	}
	return FALSE;
}

static BOOL CALLBACK IEViewGroupChatsOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	BOOL bChecked;
	char path[MAX_PATH];
	switch (msg) {
	case WM_INITDIALOG:
		{
			TranslateDialogDefault(hwndDlg);
			chatCurrentProtoItem = NULL;
			RefreshProtoList(hwndDlg, 1, true);
			return TRUE;
		}
		break;
	case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {
			case IDC_BACKGROUND_IMAGE_FILENAME:
            case IDC_EXTERNALCSS_FILENAME:
            case IDC_EXTERNALCSS_FILENAME_RTL:
            case IDC_TEMPLATES_FILENAME:
            case IDC_TEMPLATES_FILENAME_RTL:
				if ((HWND)lParam==GetFocus() && HIWORD(wParam)==EN_CHANGE)
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_SCROLL_BACKGROUND_IMAGE:
			case IDC_LOG_SHOW_NICKNAMES:
			case IDC_LOG_SHOW_TIME:
			case IDC_LOG_SHOW_DATE:
			case IDC_LOG_SHOW_SECONDS:
			case IDC_LOG_LONG_DATE:
			case IDC_LOG_RELATIVE_DATE:
			case IDC_LOG_GROUP_MESSAGES:
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_BACKGROUND_IMAGE:
                bChecked = IsDlgButtonChecked(hwndDlg, IDC_MODE_COMPATIBLE) && IsDlgButtonChecked(hwndDlg, IDC_BACKGROUND_IMAGE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_BACKGROUND_IMAGE), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE), bChecked);
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_BROWSE_TEMPLATES:
				if (BrowseFile(hwndDlg, "Template (*.ivt)\0*.ivt\0All Files\0*.*\0\0", "ivt", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_TEMPLATES_FILENAME, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_TEMPLATES_RTL:
				if (BrowseFile(hwndDlg, "Template (*.ivt)\0*.ivt\0All Files\0*.*\0\0", "ivt", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_BACKGROUND_IMAGE:
				if (BrowseFile(hwndDlg, "All Images (*.jpg,*.gif,*.png,*.bmp)\0*.jpg;*.gif;*.png;*.bmp\0All Files\0*.*\0\0", "jpg", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg,IDC_BACKGROUND_IMAGE_FILENAME,path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_EXTERNALCSS:
				if (BrowseFile(hwndDlg, "Style Sheet (*.css)\0*.css\0All Files\0*.*\0\0", "css", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_EXTERNALCSS_FILENAME, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_BROWSE_EXTERNALCSS_RTL:
				if (BrowseFile(hwndDlg, "Style Sheet (*.css)\0*.css\0All Files\0*.*\0\0", "css", path, sizeof(path))) {
					SetDlgItemTextA(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, path);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_MODE_COMPATIBLE:
			case IDC_MODE_CSS:
			case IDC_MODE_TEMPLATE:
				UpdateControlsState(hwndDlg);
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			}
		}
		break;
	case UM_CHECKSTATECHANGE:
		{
			ProtocolSettings *proto = (ProtocolSettings *)GetItemParam((HWND)wParam, (HTREEITEM) lParam);
			if (proto != NULL) {
				if (strcmpi(proto->getProtocolName(), "_default_")) {
					proto->setChatEnableTemp(TreeView_GetCheckState((HWND)wParam, (HTREEITEM) lParam));
				}
			}
			if ((HTREEITEM) lParam != TreeView_GetSelection((HWND)wParam)) {
				TreeView_SelectItem((HWND)wParam, (HTREEITEM) lParam);
			} else {
				UpdateChatProtoInfo(hwndDlg, proto);
			}
			SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
		}
		break;
	case WM_NOTIFY:
		{
			if (((LPNMHDR)lParam)->idFrom == IDC_PROTOLIST) {
				switch (((LPNMHDR)lParam)->code) {
					case NM_CLICK:
						{
							TVHITTESTINFO ht = {0};
							DWORD dwpos = GetMessagePos();
							POINTSTOPOINT(ht.pt, MAKEPOINTS(dwpos));
							MapWindowPoints(HWND_DESKTOP, ((LPNMHDR)lParam)->hwndFrom, &ht.pt, 1);
							TreeView_HitTest(((LPNMHDR)lParam)->hwndFrom, &ht);
							if (TVHT_ONITEMSTATEICON & ht.flags) {
                                PostMessage(hwndDlg, UM_CHECKSTATECHANGE, (WPARAM)((LPNMHDR)lParam)->hwndFrom, (LPARAM)ht.hItem);
                                return FALSE;
							}
						}
						break;
					case TVN_KEYDOWN:
						 if (((LPNMTVKEYDOWN) lParam)->wVKey == VK_SPACE)
								PostMessage(hwndDlg, UM_CHECKSTATECHANGE, (WPARAM)((LPNMHDR)lParam)->hwndFrom,
								(LPARAM)TreeView_GetSelection(((LPNMHDR)lParam)->hwndFrom));
						break;
					case TVN_SELCHANGED:
						{
							HWND hLstView = GetDlgItem(hwndDlg, IDC_PROTOLIST);
							ProtocolSettings *proto = (ProtocolSettings *)GetItemParam(hLstView, (HTREEITEM) NULL);
							SaveChatProtoSettings(hwndDlg, chatCurrentProtoItem);
							UpdateChatProtoInfo(hwndDlg, proto);
						}
						break;
				}
				break;
			}
			switch (((LPNMHDR) lParam)->code) {
			case PSN_APPLY:
				SaveChatProtoSettings(hwndDlg, chatCurrentProtoItem);
				return TRUE;
			}
		}
		break;
	case WM_DESTROY:
		break;
	}
	return FALSE;
}

bool Options::isInited = false;
bool Options::bMathModule = false;
bool  Options::bSmileyAdd = false;
int  Options::avatarServiceFlags = 0;
int Options::generalFlags;

ProtocolSettings *Options::protocolList = NULL;

ProtocolSettings::ProtocolSettings(const char *protocolName) {
	this->protocolName = Utils::dupString(protocolName);
	next = NULL;
	srmmEnable = false;
	srmmMode = Options::MODE_COMPATIBLE;
	srmmFlags = 0;
	srmmBackgroundFilename = Utils::dupString("");
	srmmCssFilename = Utils::dupString("");
	srmmCssFilenameRtl = Utils::dupString("");
	srmmTemplateFilename = Utils::dupString("");
	srmmTemplateFilenameRtl = Utils::dupString("");

	srmmBackgroundFilenameTemp = Utils::dupString("");
	srmmCssFilenameTemp = Utils::dupString("");
	srmmCssFilenameRtlTemp = Utils::dupString("");
	srmmTemplateFilenameTemp = Utils::dupString("");
	srmmTemplateFilenameRtlTemp = Utils::dupString("");

	chatEnable = false;
	chatMode = Options::MODE_COMPATIBLE;
	chatFlags = 0;
	chatBackgroundFilename = Utils::dupString("");
	chatCssFilename = Utils::dupString("");
	chatCssFilenameRtl = Utils::dupString("");
	chatTemplateFilename = Utils::dupString("");
	chatTemplateFilenameRtl = Utils::dupString("");

	chatBackgroundFilenameTemp = Utils::dupString("");
	chatCssFilenameTemp = Utils::dupString("");
	chatCssFilenameRtlTemp = Utils::dupString("");
	chatTemplateFilenameTemp = Utils::dupString("");
	chatTemplateFilenameRtlTemp = Utils::dupString("");

	historyEnable = false;
	historyMode = Options::MODE_COMPATIBLE;
	historyFlags = 0;
	historyBackgroundFilename = Utils::dupString("");
	historyCssFilename = Utils::dupString("");
	historyCssFilenameRtl = Utils::dupString("");
	historyTemplateFilename = Utils::dupString("");
	historyTemplateFilenameRtl = Utils::dupString("");

	historyBackgroundFilenameTemp = Utils::dupString("");
	historyCssFilenameTemp = Utils::dupString("");
	historyCssFilenameRtlTemp = Utils::dupString("");
	historyTemplateFilenameTemp = Utils::dupString("");
	historyTemplateFilenameRtlTemp = Utils::dupString("");

}

ProtocolSettings::~ProtocolSettings() {
	if (srmmBackgroundFilename != NULL) {
		delete srmmBackgroundFilename;
	}
	if (srmmBackgroundFilenameTemp != NULL) {
		delete srmmBackgroundFilenameTemp;
	}
	if (srmmCssFilename != NULL) {
		delete srmmCssFilename;
	}
	if (srmmCssFilenameRtl != NULL) {
		delete srmmCssFilenameRtl;
	}
	if (srmmCssFilenameTemp != NULL) {
		delete srmmCssFilenameTemp;
	}
	if (srmmCssFilenameRtlTemp != NULL) {
		delete srmmCssFilenameRtlTemp;
	}
	if (srmmTemplateFilename != NULL) {
		delete srmmTemplateFilename;
	}
	if (srmmTemplateFilenameRtl != NULL) {
		delete srmmTemplateFilenameRtl;
	}
	if (srmmTemplateFilenameTemp != NULL) {
		delete srmmTemplateFilenameTemp;
	}
	if (srmmTemplateFilenameRtlTemp != NULL) {
		delete srmmTemplateFilenameRtlTemp;
	}

	if (chatBackgroundFilename != NULL) {
		delete chatBackgroundFilename;
	}
	if (chatBackgroundFilenameTemp != NULL) {
		delete chatBackgroundFilenameTemp;
	}
	if (chatCssFilename != NULL) {
		delete chatCssFilename;
	}
	if (chatCssFilenameRtl != NULL) {
		delete chatCssFilenameRtl;
	}
	if (chatCssFilenameTemp != NULL) {
		delete chatCssFilenameTemp;
	}
	if (chatCssFilenameRtlTemp != NULL) {
		delete chatCssFilenameRtlTemp;
	}
	if (chatTemplateFilename != NULL) {
		delete chatTemplateFilename;
	}
	if (chatTemplateFilenameRtl != NULL) {
		delete chatTemplateFilenameRtl;
	}
	if (chatTemplateFilenameTemp != NULL) {
		delete chatTemplateFilenameTemp;
	}
	if (chatTemplateFilenameRtlTemp != NULL) {
		delete chatTemplateFilenameRtlTemp;
	}

	if (historyBackgroundFilename != NULL) {
		delete historyBackgroundFilename;
	}
	if (historyBackgroundFilenameTemp != NULL) {
		delete historyBackgroundFilenameTemp;
	}
	if (historyCssFilename != NULL) {
		delete historyCssFilename;
	}
	if (historyCssFilenameRtl != NULL) {
		delete historyCssFilenameRtl;
	}
	if (historyCssFilenameTemp != NULL) {
		delete historyCssFilenameTemp;
	}
	if (historyCssFilenameRtlTemp != NULL) {
		delete historyCssFilenameRtlTemp;
	}
	if (historyTemplateFilename != NULL) {
		delete historyTemplateFilename;
	}
	if (historyTemplateFilenameRtl != NULL) {
		delete historyTemplateFilenameRtl;
	}
	if (historyTemplateFilenameTemp != NULL) {
		delete historyTemplateFilenameTemp;
	}
	if (historyTemplateFilenameRtlTemp != NULL) {
		delete historyTemplateFilenameRtlTemp;
	}
}

void ProtocolSettings::copyToTemp() {
	setSRMMModeTemp(getSRMMMode());
	setSRMMFlagsTemp(getSRMMFlags());
	setSRMMBackgroundFilenameTemp(getSRMMBackgroundFilename());
	setSRMMCssFilenameTemp(getSRMMCssFilename());
	setSRMMCssFilenameRtlTemp(getSRMMCssFilenameRtl());
	setSRMMTemplateFilenameTemp(getSRMMTemplateFilename());
	setSRMMTemplateFilenameRtlTemp(getSRMMTemplateFilenameRtl());
	setSRMMEnableTemp(isSRMMEnable());

	setChatModeTemp(getChatMode());
	setChatFlagsTemp(getChatFlags());
	setChatBackgroundFilenameTemp(getChatBackgroundFilename());
	setChatCssFilenameTemp(getChatCssFilename());
	setChatCssFilenameRtlTemp(getChatCssFilenameRtl());
	setChatTemplateFilenameTemp(getChatTemplateFilename());
	setChatTemplateFilenameRtlTemp(getChatTemplateFilenameRtl());
	setChatEnableTemp(isChatEnable());

	setHistoryModeTemp(getHistoryMode());
	setHistoryFlagsTemp(getHistoryFlags());
	setHistoryBackgroundFilenameTemp(getHistoryBackgroundFilename());
	setHistoryCssFilenameTemp(getHistoryCssFilename());
	setHistoryCssFilenameRtlTemp(getHistoryCssFilenameRtl());
	setHistoryTemplateFilenameTemp(getHistoryTemplateFilename());
	setHistoryTemplateFilenameRtlTemp(getHistoryTemplateFilenameRtl());
	setHistoryEnableTemp(isHistoryEnable());
}

void ProtocolSettings::copyFromTemp() {
	setSRMMMode(getSRMMModeTemp());
	setSRMMFlags(getSRMMFlagsTemp());
	setSRMMBackgroundFilename(getSRMMBackgroundFilenameTemp());
	setSRMMCssFilename(getSRMMCssFilenameTemp());
	setSRMMCssFilenameRtl(getSRMMCssFilenameRtlTemp());
	setSRMMTemplateFilename(getSRMMTemplateFilenameTemp());
	setSRMMTemplateFilenameRtl(getSRMMTemplateFilenameRtlTemp());
	setSRMMEnable(isSRMMEnableTemp());

	setChatMode(getChatModeTemp());
	setChatFlags(getChatFlagsTemp());
	setChatBackgroundFilename(getChatBackgroundFilenameTemp());
	setChatCssFilename(getChatCssFilenameTemp());
	setChatCssFilenameRtl(getChatCssFilenameRtlTemp());
	setChatTemplateFilename(getChatTemplateFilenameTemp());
	setChatTemplateFilenameRtl(getChatTemplateFilenameRtlTemp());
	setChatEnable(isChatEnableTemp());

	setHistoryMode(getHistoryModeTemp());
	setHistoryFlags(getHistoryFlagsTemp());
	setHistoryBackgroundFilename(getHistoryBackgroundFilenameTemp());
	setHistoryCssFilename(getHistoryCssFilenameTemp());
	setHistoryCssFilenameRtl(getHistoryCssFilenameRtlTemp());
	setHistoryTemplateFilename(getHistoryTemplateFilenameTemp());
	setHistoryTemplateFilenameRtl(getHistoryTemplateFilenameRtlTemp());
	setHistoryEnable(isHistoryEnableTemp());
}

void ProtocolSettings::setNext(ProtocolSettings *next) {
	this->next = next;
}

const char *ProtocolSettings::getProtocolName() {
	return protocolName;
}

ProtocolSettings * ProtocolSettings::getNext() {
	return next;
}

void ProtocolSettings::setSRMMBackgroundFilename(const char *filename) {
	if (srmmBackgroundFilename != NULL) {
		delete srmmBackgroundFilename;
	}
	srmmBackgroundFilename = Utils::dupString(filename);
}

void ProtocolSettings::setSRMMBackgroundFilenameTemp(const char *filename) {
	if (srmmBackgroundFilenameTemp != NULL) {
		delete srmmBackgroundFilenameTemp;
	}
	srmmBackgroundFilenameTemp = Utils::dupString(filename);
}

void ProtocolSettings::setSRMMCssFilename(const char *filename) {
	if (srmmCssFilename != NULL) {
		delete srmmCssFilename;
	}
	srmmCssFilename = Utils::dupString(filename);
}

void ProtocolSettings::setSRMMCssFilenameRtl(const char *filename) {
	if (srmmCssFilenameRtl != NULL) {
		delete srmmCssFilenameRtl;
	}
	srmmCssFilenameRtl = Utils::dupString(filename);
}

void ProtocolSettings::setSRMMCssFilenameTemp(const char *filename) {
	if (srmmCssFilenameTemp != NULL) {
		delete srmmCssFilenameTemp;
	}
	srmmCssFilenameTemp = Utils::dupString(filename);
}

void ProtocolSettings::setSRMMCssFilenameRtlTemp(const char *filename) {
	if (srmmCssFilenameRtlTemp != NULL) {
		delete srmmCssFilenameRtlTemp;
	}
	srmmCssFilenameRtlTemp = Utils::dupString(filename);
}

void ProtocolSettings::setSRMMTemplateFilename(const char *filename) {
	if (srmmTemplateFilename != NULL) {
		delete srmmTemplateFilename;
	}
	srmmTemplateFilename = Utils::dupString(filename);
	TemplateMap::loadTemplates(getSRMMTemplateFilename(), getSRMMTemplateFilename());
}

void ProtocolSettings::setSRMMTemplateFilenameRtl(const char *filename) {
	if (srmmTemplateFilenameRtl != NULL) {
		delete srmmTemplateFilenameRtl;
	}
	srmmTemplateFilenameRtl = Utils::dupString(filename);
	TemplateMap::loadTemplates(getSRMMTemplateFilenameRtl(), getSRMMTemplateFilenameRtl());
}

void ProtocolSettings::setSRMMTemplateFilenameTemp(const char *filename) {
	if (srmmTemplateFilenameTemp != NULL) {
		delete srmmTemplateFilenameTemp;
	}
	srmmTemplateFilenameTemp = Utils::dupString(filename);
}

void ProtocolSettings::setSRMMTemplateFilenameRtlTemp(const char *filename) {
	if (srmmTemplateFilenameRtlTemp != NULL) {
		delete srmmTemplateFilenameRtlTemp;
	}
	srmmTemplateFilenameRtlTemp = Utils::dupString(filename);
}

const char *ProtocolSettings::getSRMMBackgroundFilename() {
	return srmmBackgroundFilename;
}

const char *ProtocolSettings::getSRMMBackgroundFilenameTemp() {
	return srmmBackgroundFilenameTemp;
}

const char *ProtocolSettings::getSRMMCssFilename() {
	return srmmCssFilename;
}

const char *ProtocolSettings::getSRMMCssFilenameRtl() {
	return srmmCssFilenameRtl;
}

const char *ProtocolSettings::getSRMMCssFilenameTemp() {
	return srmmCssFilenameTemp;
}

const char *ProtocolSettings::getSRMMCssFilenameRtlTemp() {
	return srmmCssFilenameRtlTemp;
}

const char *ProtocolSettings::getSRMMTemplateFilename() {
	return srmmTemplateFilename;
}

const char *ProtocolSettings::getSRMMTemplateFilenameRtl() {
	return srmmTemplateFilenameRtl;
}

const char *ProtocolSettings::getSRMMTemplateFilenameTemp() {
	return srmmTemplateFilenameTemp;
}

const char *ProtocolSettings::getSRMMTemplateFilenameRtlTemp() {
	return srmmTemplateFilenameRtlTemp;
}

void ProtocolSettings::setSRMMEnable(bool enable) {
	this->srmmEnable = enable;
}

bool ProtocolSettings::isSRMMEnable() {
	return srmmEnable;
}

void ProtocolSettings::setSRMMEnableTemp(bool enable) {
	this->srmmEnableTemp = enable;
}

bool ProtocolSettings::isSRMMEnableTemp() {
	return srmmEnableTemp;
}

void ProtocolSettings::setSRMMMode(int mode) {
	this->srmmMode = mode;
}

int ProtocolSettings::getSRMMMode() {
	return srmmMode;
}

void ProtocolSettings::setSRMMModeTemp(int mode) {
	this->srmmModeTemp = mode;
}

int ProtocolSettings::getSRMMModeTemp() {
	return srmmModeTemp;
}

void ProtocolSettings::setSRMMFlags(int flags) {
	this->srmmFlags = flags;
}

int ProtocolSettings::getSRMMFlags() {
	return srmmFlags;
}

void ProtocolSettings::setSRMMFlagsTemp(int flags) {
	this->srmmFlagsTemp = flags;
}

int ProtocolSettings::getSRMMFlagsTemp() {
	return srmmFlagsTemp;
}

/* */

void ProtocolSettings::setChatBackgroundFilename(const char *filename) {
	if (chatBackgroundFilename != NULL) {
		delete chatBackgroundFilename;
	}
	chatBackgroundFilename = Utils::dupString(filename);
}

void ProtocolSettings::setChatBackgroundFilenameTemp(const char *filename) {
	if (chatBackgroundFilenameTemp != NULL) {
		delete chatBackgroundFilenameTemp;
	}
	chatBackgroundFilenameTemp = Utils::dupString(filename);
}

void ProtocolSettings::setChatCssFilename(const char *filename) {
	if (chatCssFilename != NULL) {
		delete chatCssFilename;
	}
	chatCssFilename = Utils::dupString(filename);
}

void ProtocolSettings::setChatCssFilenameRtl(const char *filename) {
	if (chatCssFilenameRtl != NULL) {
		delete chatCssFilenameRtl;
	}
	chatCssFilenameRtl = Utils::dupString(filename);
}

void ProtocolSettings::setChatCssFilenameTemp(const char *filename) {
	if (chatCssFilenameTemp != NULL) {
		delete chatCssFilenameTemp;
	}
	chatCssFilenameTemp = Utils::dupString(filename);
}

void ProtocolSettings::setChatCssFilenameRtlTemp(const char *filename) {
	if (chatCssFilenameRtlTemp != NULL) {
		delete chatCssFilenameRtlTemp;
	}
	chatCssFilenameRtlTemp = Utils::dupString(filename);
}

void ProtocolSettings::setChatTemplateFilename(const char *filename) {
	if (chatTemplateFilename != NULL) {
		delete chatTemplateFilename;
	}
	chatTemplateFilename = Utils::dupString(filename);
	TemplateMap::loadTemplates(getChatTemplateFilename(), getChatTemplateFilename());
}

void ProtocolSettings::setChatTemplateFilenameRtl(const char *filename) {
	if (chatTemplateFilenameRtl != NULL) {
		delete chatTemplateFilenameRtl;
	}
	chatTemplateFilenameRtl = Utils::dupString(filename);
	TemplateMap::loadTemplates(getChatTemplateFilenameRtl(), getChatTemplateFilenameRtl());
}

void ProtocolSettings::setChatTemplateFilenameTemp(const char *filename) {
	if (chatTemplateFilenameTemp != NULL) {
		delete chatTemplateFilenameTemp;
	}
	chatTemplateFilenameTemp = Utils::dupString(filename);
}

void ProtocolSettings::setChatTemplateFilenameRtlTemp(const char *filename) {
	if (chatTemplateFilenameRtlTemp != NULL) {
		delete chatTemplateFilenameRtlTemp;
	}
	chatTemplateFilenameRtlTemp = Utils::dupString(filename);
}

const char *ProtocolSettings::getChatBackgroundFilename() {
	return chatBackgroundFilename;
}

const char *ProtocolSettings::getChatBackgroundFilenameTemp() {
	return chatBackgroundFilenameTemp;
}

const char *ProtocolSettings::getChatCssFilename() {
	return chatCssFilename;
}

const char *ProtocolSettings::getChatCssFilenameRtl() {
	return chatCssFilenameRtl;
}

const char *ProtocolSettings::getChatCssFilenameTemp() {
	return chatCssFilenameTemp;
}

const char *ProtocolSettings::getChatCssFilenameRtlTemp() {
	return chatCssFilenameRtlTemp;
}

const char *ProtocolSettings::getChatTemplateFilename() {
	return chatTemplateFilename;
}

const char *ProtocolSettings::getChatTemplateFilenameRtl() {
	return chatTemplateFilenameRtl;
}

const char *ProtocolSettings::getChatTemplateFilenameTemp() {
	return chatTemplateFilenameTemp;
}

const char *ProtocolSettings::getChatTemplateFilenameRtlTemp() {
	return chatTemplateFilenameRtlTemp;
}

void ProtocolSettings::setChatEnable(bool enable) {
	this->chatEnable = enable;
}

bool ProtocolSettings::isChatEnable() {
	return chatEnable;
}

void ProtocolSettings::setChatEnableTemp(bool enable) {
	this->chatEnableTemp = enable;
}

bool ProtocolSettings::isChatEnableTemp() {
	return chatEnableTemp;
}

void ProtocolSettings::setChatMode(int mode) {
	this->chatMode = mode;
}

int ProtocolSettings::getChatMode() {
	return chatMode;
}

void ProtocolSettings::setChatModeTemp(int mode) {
	this->chatModeTemp = mode;
}

int ProtocolSettings::getChatModeTemp() {
	return chatModeTemp;
}

void ProtocolSettings::setChatFlags(int flags) {
	this->chatFlags = flags;
}

int ProtocolSettings::getChatFlags() {
	return chatFlags;
}

void ProtocolSettings::setChatFlagsTemp(int flags) {
	this->chatFlagsTemp = flags;
}

int ProtocolSettings::getChatFlagsTemp() {
	return chatFlagsTemp;
}

/* */

void ProtocolSettings::setHistoryBackgroundFilename(const char *filename) {
	if (historyBackgroundFilename != NULL) {
		delete historyBackgroundFilename;
	}
	historyBackgroundFilename = Utils::dupString(filename);
}

void ProtocolSettings::setHistoryBackgroundFilenameTemp(const char *filename) {
	if (historyBackgroundFilenameTemp != NULL) {
		delete historyBackgroundFilenameTemp;
	}
	historyBackgroundFilenameTemp = Utils::dupString(filename);
}

void ProtocolSettings::setHistoryCssFilename(const char *filename) {
	if (historyCssFilename != NULL) {
		delete historyCssFilename;
	}
	historyCssFilename = Utils::dupString(filename);
}

void ProtocolSettings::setHistoryCssFilenameRtl(const char *filename) {
	if (historyCssFilenameRtl != NULL) {
		delete historyCssFilenameRtl;
	}
	historyCssFilenameRtl = Utils::dupString(filename);
}

void ProtocolSettings::setHistoryCssFilenameTemp(const char *filename) {
	if (historyCssFilenameTemp != NULL) {
		delete historyCssFilenameTemp;
	}
	historyCssFilenameTemp = Utils::dupString(filename);
}

void ProtocolSettings::setHistoryCssFilenameRtlTemp(const char *filename) {
	if (historyCssFilenameRtlTemp != NULL) {
		delete historyCssFilenameRtlTemp;
	}
	historyCssFilenameRtlTemp = Utils::dupString(filename);
}

void ProtocolSettings::setHistoryTemplateFilename(const char *filename) {
	if (historyTemplateFilename != NULL) {
		delete historyTemplateFilename;
	}
	historyTemplateFilename = Utils::dupString(filename);
	TemplateMap::loadTemplates(getHistoryTemplateFilename(), getHistoryTemplateFilename());
}

void ProtocolSettings::setHistoryTemplateFilenameRtl(const char *filename) {
	if (historyTemplateFilenameRtl != NULL) {
		delete historyTemplateFilenameRtl;
	}
	historyTemplateFilenameRtl = Utils::dupString(filename);
	TemplateMap::loadTemplates(getHistoryTemplateFilenameRtl(), getHistoryTemplateFilenameRtl());
}

void ProtocolSettings::setHistoryTemplateFilenameTemp(const char *filename) {
	if (historyTemplateFilenameTemp != NULL) {
		delete historyTemplateFilenameTemp;
	}
	historyTemplateFilenameTemp = Utils::dupString(filename);
}

void ProtocolSettings::setHistoryTemplateFilenameRtlTemp(const char *filename) {
	if (historyTemplateFilenameRtlTemp != NULL) {
		delete historyTemplateFilenameRtlTemp;
	}
	historyTemplateFilenameRtlTemp = Utils::dupString(filename);
}

const char *ProtocolSettings::getHistoryBackgroundFilename() {
	return historyBackgroundFilename;
}

const char *ProtocolSettings::getHistoryBackgroundFilenameTemp() {
	return historyBackgroundFilenameTemp;
}

const char *ProtocolSettings::getHistoryCssFilename() {
	return historyCssFilename;
}

const char *ProtocolSettings::getHistoryCssFilenameRtl() {
	return historyCssFilenameRtl;
}

const char *ProtocolSettings::getHistoryCssFilenameTemp() {
	return historyCssFilenameTemp;
}

const char *ProtocolSettings::getHistoryCssFilenameRtlTemp() {
	return historyCssFilenameRtlTemp;
}

const char *ProtocolSettings::getHistoryTemplateFilename() {
	return historyTemplateFilename;
}

const char *ProtocolSettings::getHistoryTemplateFilenameRtl() {
	return historyTemplateFilenameRtl;
}

const char *ProtocolSettings::getHistoryTemplateFilenameTemp() {
	return historyTemplateFilenameTemp;
}

const char *ProtocolSettings::getHistoryTemplateFilenameRtlTemp() {
	return historyTemplateFilenameRtlTemp;
}

void ProtocolSettings::setHistoryEnable(bool enable) {
	this->historyEnable = enable;
}

bool ProtocolSettings::isHistoryEnable() {
	return historyEnable;
}

void ProtocolSettings::setHistoryEnableTemp(bool enable) {
	this->historyEnableTemp = enable;
}

bool ProtocolSettings::isHistoryEnableTemp() {
	return historyEnableTemp;
}

void ProtocolSettings::setHistoryMode(int mode) {
	this->historyMode = mode;
}

int ProtocolSettings::getHistoryMode() {
	return historyMode;
}

void ProtocolSettings::setHistoryModeTemp(int mode) {
	this->historyModeTemp = mode;
}

int ProtocolSettings::getHistoryModeTemp() {
	return historyModeTemp;
}

void ProtocolSettings::setHistoryFlags(int flags) {
	this->historyFlags = flags;
}

int ProtocolSettings::getHistoryFlags() {
	return historyFlags;
}

void ProtocolSettings::setHistoryFlagsTemp(int flags) {
	this->historyFlagsTemp = flags;
}

int ProtocolSettings::getHistoryFlagsTemp() {
	return historyFlagsTemp;
}

void Options::init() {
	if (isInited) return;
	isInited = true;
	DBVARIANT dbv;

	generalFlags = DBGetContactSettingDword(NULL, ieviewModuleName, DBS_BASICFLAGS, 0);

	/* TODO: move to buildProtocolList method */
	int protoCount;
	PROTOCOLDESCRIPTOR **pProtos;
	ProtocolSettings *lastProto = NULL;
	CallService(MS_PROTO_ENUMPROTOCOLS, (WPARAM)&protoCount, (LPARAM)&pProtos);
	for (int i = 0; i < protoCount+1; i++) {
		ProtocolSettings *proto;
		char tmpPath[MAX_PATH];
		char dbsName[256];
		if (i==0) {
			proto = new ProtocolSettings("_default_");
			proto->setSRMMEnable(true);
		} else if ((pProtos[i-1]->type == PROTOTYPE_PROTOCOL) && strcmp(pProtos[i-1]->szName,"MetaContacts")) {
			proto = new ProtocolSettings(pProtos[i-1]->szName);
		} else {
			continue;
		}
		/* SRMM settings */
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_ENABLE);
		proto->setSRMMEnable(i==0 ? true : DBGetContactSettingByte(NULL, ieviewModuleName, dbsName, FALSE));
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_MODE);
		proto->setSRMMMode(DBGetContactSettingByte(NULL, ieviewModuleName, dbsName, FALSE));
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_FLAGS);
		proto->setSRMMFlags(DBGetContactSettingDword(NULL, ieviewModuleName, dbsName, FALSE));
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_BACKGROUND);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setSRMMBackgroundFilename(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_CSS);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setSRMMCssFilename(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_CSS_RTL);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
	    	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setSRMMCssFilenameRtl(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_TEMPLATE);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setSRMMTemplateFilename(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_TEMPLATE_RTL);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setSRMMTemplateFilenameRtl(tmpPath);
			DBFreeVariant(&dbv);
		}

		/* Group chat settings */
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_ENABLE);
		proto->setChatEnable(i==0 ? true : DBGetContactSettingByte(NULL, ieviewModuleName, dbsName, FALSE));
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_MODE);
		proto->setChatMode(DBGetContactSettingByte(NULL, ieviewModuleName, dbsName, FALSE));
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_FLAGS);
		proto->setChatFlags(DBGetContactSettingDword(NULL, ieviewModuleName, dbsName, FALSE));
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_BACKGROUND);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setChatBackgroundFilename(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_CSS);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setChatCssFilename(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_CSS_RTL);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
	    	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setChatCssFilenameRtl(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_TEMPLATE);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setChatTemplateFilename(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_TEMPLATE_RTL);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setChatTemplateFilenameRtl(tmpPath);
			DBFreeVariant(&dbv);
		}

		/* History settings */
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_ENABLE);
		proto->setHistoryEnable(i==0 ? true : DBGetContactSettingByte(NULL, ieviewModuleName, dbsName, FALSE));
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_MODE);
		proto->setHistoryMode(DBGetContactSettingByte(NULL, ieviewModuleName, dbsName, FALSE));
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_FLAGS);
		proto->setHistoryFlags(DBGetContactSettingDword(NULL, ieviewModuleName, dbsName, FALSE));
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_BACKGROUND);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setHistoryBackgroundFilename(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_CSS);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setHistoryCssFilename(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_CSS_RTL);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
	    	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setHistoryCssFilenameRtl(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_TEMPLATE);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setHistoryTemplateFilename(tmpPath);
			DBFreeVariant(&dbv);
		}
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_TEMPLATE_RTL);
		if (!DBGetContactSetting(NULL,  ieviewModuleName, dbsName, &dbv)) {
			strcpy(tmpPath, dbv.pszVal);
			if (ServiceExists(MS_UTILS_PATHTOABSOLUTE)) {
				CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
			}
			proto->setHistoryTemplateFilenameRtl(tmpPath);
			DBFreeVariant(&dbv);
		}

		proto->copyToTemp();
		if (lastProto != NULL) {
			lastProto->setNext(proto);
		} else {
			protocolList = proto;
		}
		lastProto = proto;
	}

	bMathModule = (bool) ServiceExists(MTH_GET_GIF_UNICODE);
	bSmileyAdd = (bool) ServiceExists(MS_SMILEYADD_BATCHPARSE);
	avatarServiceFlags = 0;
	if (ServiceExists(MS_AV_GETAVATARBITMAP)) {
		avatarServiceFlags = AVATARSERVICE_PRESENT;
	}


//	mathModuleFlags = ServiceExists(MTH_GET_HTML_SOURCE) ? GENERAL_ENABLE_MATHMODULE : 0;
}

void Options::setGeneralFlags(int flags) {
	generalFlags = flags;
	DBWriteContactSettingDword(NULL, ieviewModuleName, DBS_BASICFLAGS, (DWORD) flags);
}

int	Options::getGeneralFlags() {
	return generalFlags;
}

bool Options::isMathModule() {
	return bMathModule;
}

bool Options::isSmileyAdd() {
	return bSmileyAdd;
}

int Options::getAvatarServiceFlags() {
	return avatarServiceFlags;
}

ProtocolSettings * Options::getProtocolSettings() {
	return protocolList;
}

ProtocolSettings * Options::getProtocolSettings(const char *protocolName) {
	for (ProtocolSettings *proto=protocolList;proto!=NULL;proto=proto->getNext()) {
		if (!strcmpi(proto->getProtocolName(), protocolName)) {
			return proto;
		}
	}
	return NULL;
}

void Options::resetProtocolSettings() {
	for (ProtocolSettings *proto=Options::getProtocolSettings();proto!=NULL;proto=proto->getNext()) {
		proto->copyToTemp();
	}
}

void Options::saveProtocolSettings() {
	ProtocolSettings *proto;
	int i;
	for (i=0,proto=Options::getProtocolSettings();proto!=NULL;proto=proto->getNext(),i++) {
		char dbsName[256];
		char tmpPath[MAX_PATH];
		proto->copyFromTemp();
		/* SRMM settings */
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_ENABLE);
		DBWriteContactSettingByte(NULL, ieviewModuleName, dbsName, proto->isSRMMEnable());
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_MODE);
		DBWriteContactSettingByte(NULL, ieviewModuleName, dbsName, proto->getSRMMMode());
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_FLAGS);
		DBWriteContactSettingDword(NULL, ieviewModuleName, dbsName, proto->getSRMMFlags());
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_BACKGROUND);
		strcpy (tmpPath, proto->getSRMMBackgroundFilename());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getSRMMBackgroundFilename(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_CSS);
		strcpy (tmpPath, proto->getSRMMCssFilename());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getSRMMCssFilename(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_CSS_RTL);
		strcpy (tmpPath, proto->getSRMMCssFilenameRtl());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getSRMMCssFilenameRtl(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_TEMPLATE);
		strcpy (tmpPath, proto->getSRMMTemplateFilename());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getSRMMTemplateFilename(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_SRMM_TEMPLATE_RTL);
		strcpy (tmpPath, proto->getSRMMTemplateFilenameRtl());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getSRMMTemplateFilenameRtl(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		/* Group Chat settings */
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_ENABLE);
		DBWriteContactSettingByte(NULL, ieviewModuleName, dbsName, proto->isChatEnable());
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_MODE);
		DBWriteContactSettingByte(NULL, ieviewModuleName, dbsName, proto->getChatMode());
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_FLAGS);
		DBWriteContactSettingDword(NULL, ieviewModuleName, dbsName, proto->getChatFlags());
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_BACKGROUND);
		strcpy (tmpPath, proto->getChatBackgroundFilename());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getChatBackgroundFilename(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_CSS);
		strcpy (tmpPath, proto->getChatCssFilename());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getChatCssFilename(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_CSS_RTL);
		strcpy (tmpPath, proto->getChatCssFilenameRtl());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getChatCssFilenameRtl(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_TEMPLATE);
		strcpy (tmpPath, proto->getChatTemplateFilename());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getChatTemplateFilename(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_CHAT_TEMPLATE_RTL);
		strcpy (tmpPath, proto->getChatTemplateFilenameRtl());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getChatTemplateFilenameRtl(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		/* History settings */
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_ENABLE);
		DBWriteContactSettingByte(NULL, ieviewModuleName, dbsName, proto->isHistoryEnable());
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_MODE);
		DBWriteContactSettingByte(NULL, ieviewModuleName, dbsName, proto->getHistoryMode());
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_FLAGS);
		DBWriteContactSettingDword(NULL, ieviewModuleName, dbsName, proto->getHistoryFlags());
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_BACKGROUND);
		strcpy (tmpPath, proto->getHistoryBackgroundFilename());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getHistoryBackgroundFilename(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_CSS);
		strcpy (tmpPath, proto->getHistoryCssFilename());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getHistoryCssFilename(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_CSS_RTL);
		strcpy (tmpPath, proto->getHistoryCssFilenameRtl());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getHistoryCssFilenameRtl(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_TEMPLATE);
		strcpy (tmpPath, proto->getHistoryTemplateFilename());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getHistoryTemplateFilename(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);
		sprintf(dbsName, "%s.%s", proto->getProtocolName(), DBS_HISTORY_TEMPLATE_RTL);
		strcpy (tmpPath, proto->getHistoryTemplateFilenameRtl());
		if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
			CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)proto->getHistoryTemplateFilenameRtl(), (LPARAM)tmpPath);
		}
		DBWriteContactSettingString(NULL, ieviewModuleName, dbsName, tmpPath);

	}
}
