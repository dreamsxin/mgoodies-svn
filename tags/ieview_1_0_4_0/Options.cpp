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
#include "Options.h"
#include "resource.h"
#include "Smiley.h"
#include "Template.h"
#include "m_MathModule.h"

#define UM_CHECKSTATECHANGE (WM_USER+100)
HANDLE hHookOptionsChanged;
static BOOL CALLBACK IEViewOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK IEViewGeneralOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK IEViewEmoticonsOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK IEViewTemplatesOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK IEViewGroupChatsOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static int protoNum;
static char (*protoNames)[128];
static char (*protoFilenames)[MAX_PATH];
static HWND hwndEmoticons, hwndTemplates, hwndCurrentTab, hwndGeneral, hwndGroupChats;
static int currentProtoItem;
static HICON smileyIcon;

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

int IEViewOptInit(WPARAM wParam, LPARAM lParam)
{
	OPTIONSDIALOGPAGE odp;

	ZeroMemory(&odp, sizeof(odp));
	odp.cbSize = sizeof(odp);
	odp.position = 0;
	odp.hInstance = hInstance;
	odp.pszGroup = Translate("Message Sessions");
	odp.pszTemplate = MAKEINTRESOURCE(IDD_OPTIONS);
	odp.pszTitle = Translate("IEView plugin");
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
			tc = GetDlgItem(hwndDlg, IDC_TABS);
			tci.mask = TCIF_TEXT;
			tci.pszText = Translate("General");
			TabCtrl_InsertItem(tc, 0, &tci);
			tci.pszText = Translate("Message Log");
			TabCtrl_InsertItem(tc, 1, &tci);
			tci.pszText = Translate("Emoticons");
			TabCtrl_InsertItem(tc, 2, &tci);
			tci.pszText = Translate("Group Chats");
			TabCtrl_InsertItem(tc, 3, &tci);

			hwndEmoticons = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_EMOTICONS_OPTIONS), hwndDlg, IEViewEmoticonsOptDlgProc, (LPARAM) NULL);
			SetWindowPos(hwndEmoticons, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
			hwndTemplates = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_TEMPLATES_OPTIONS), hwndDlg, IEViewTemplatesOptDlgProc, (LPARAM) NULL);
			SetWindowPos(hwndTemplates, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
			hwndGeneral = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_GENERAL_OPTIONS), hwndDlg, IEViewGeneralOptDlgProc, (LPARAM) NULL);
			SetWindowPos(hwndTemplates, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
			hwndGroupChats = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_GROUPCHATS_OPTIONS), hwndDlg, IEViewGroupChatsOptDlgProc, (LPARAM) NULL);
			SetWindowPos(hwndTemplates, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
			hwndCurrentTab = hwndGeneral;
			ShowWindow(hwndGeneral, SW_SHOW);
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
						HWND hwnd;
						switch (TabCtrl_GetCurSel(GetDlgItem(hwndDlg, IDC_TABS))) {
						default:
						case 0:
							hwnd = hwndGeneral;
							break;
						case 1:
							hwnd = hwndTemplates;
							break;
						case 2:
							hwnd = hwndEmoticons;
							break;
						case 3:
							hwnd = hwndGroupChats;
							break;
						}
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
				SendMessage(hwndGeneral, WM_NOTIFY, wParam, lParam);
				SendMessage(hwndEmoticons, WM_NOTIFY, wParam, lParam);
				SendMessage(hwndTemplates, WM_NOTIFY, wParam, lParam);
				SendMessage(hwndGroupChats, WM_NOTIFY, wParam, lParam);
				NotifyEventHooks(hHookOptionsChanged, 0, 0);
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
			EnableWindow(GetDlgItem(hwndDlg, IDC_ENABLE_MATHMODULE), Options::isMathModule());
			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {
			case IDC_ENABLE_BBCODES:
            case IDC_ENABLE_FLASH:
            case IDC_ENABLE_MATHMODULE:
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

static void updateSmileyInfo(HWND hwndDlg, int proto) {
	HWND hProtoList = GetDlgItem(hwndDlg, IDC_PROTOLIST);
	SetDlgItemText(hwndDlg, IDC_SMILEYS_FILENAME, protoFilenames[proto]);
	SmileyMap *map = SmileyMap::getLibraryInfo(protoFilenames[proto]);
	TreeView_SetCheckState(hProtoList, TreeView_GetSelection(hProtoList), map!=NULL);
	if (map != NULL) {
		SetDlgItemText(hwndDlg, IDC_LIBNAME, map->getDescription());
		SetDlgItemText(hwndDlg, IDC_LIBAUTHOR, map->getAuthor());
		SetDlgItemText(hwndDlg, IDC_LIBVERSION, map->getVersion());
		delete map;
	} else {
		SetDlgItemText(hwndDlg, IDC_LIBNAME, Translate("Not loaded"));
		SetDlgItemText(hwndDlg, IDC_LIBAUTHOR, "");
		SetDlgItemText(hwndDlg, IDC_LIBVERSION, "");
	}
	currentProtoItem = proto;
}

static void buildProtoList() {
	int protoCount;
	PROTOCOLDESCRIPTOR **pProtos;
	CallService(MS_PROTO_ENUMPROTOCOLS, (WPARAM)&protoCount, (LPARAM)&pProtos);
	protoNum = protoCount+1;
	protoNames = new char[protoCount+1][128];
	protoFilenames = new char[protoCount+1][MAX_PATH];
	for (int i = 0; i < protoNum; i++) {
		protoNames[i][0] = '\0';
		if (i==0) {
			strcpy(protoNames[i], "");
		} else if ((pProtos[i-1]->type == PROTOTYPE_PROTOCOL) && strcmp(pProtos[i-1]->szName,"MetaContacts")) {
			strcpy(protoNames[i], pProtos[i-1]->szName);
		} else {
			continue;
		}
		char * path = (char *) Options::getSmileyFile(protoNames[i]);
		if (path != NULL) {
			strcpy (protoFilenames[i], path);
		} else {
			strcpy (protoFilenames[i], "");
		}
	}
}

static void refreshProtoList(HWND hwndDlg, BOOL protoSmileys) {
    HTREEITEM hItem = NULL;
	HWND hProtoList = GetDlgItem(hwndDlg, IDC_PROTOLIST);
	TreeView_DeleteAllItems(hProtoList);
	updateSmileyInfo(hwndDlg, 0);
	for (int i = 0; i < protoNum; i++) {
		if (i==0 || (protoSmileys && protoNames[i][0]!= '\0')) {
			char protoName[128];
			TVINSERTSTRUCT tvi = {0};
			tvi.hParent = TVI_ROOT;
			tvi.hInsertAfter = TVI_LAST;
			tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_STATE | TVIF_SELECTEDIMAGE;
			tvi.item.stateMask = TVIS_SELECTED | TVIS_STATEIMAGEMASK;
			if (i==0) {
				strcpy(protoName, Translate("Standard"));
			} else {
				CallProtoService(protoNames[i], PS_GETNAME, sizeof(protoName), (LPARAM)protoName);
			}
			strcat(protoName, " ");
			strcat(protoName, Translate("smileys"));
			tvi.item.pszText = protoName;
			tvi.item.iImage = i;
			tvi.item.iSelectedImage = i;
			tvi.item.state = INDEXTOSTATEIMAGEMASK(strlen(protoFilenames[i]) > 0 ? 2 : 1);
			if (i==0) {
                hItem = TreeView_InsertItem(hProtoList, &tvi);
			} else {
				TreeView_InsertItem(hProtoList, &tvi);
			}
			 
		}
	}
	TreeView_SelectItem(hProtoList, hItem);
}

static int getSelProto(HWND hLstView, HTREEITEM hItem) {
	TVITEM tvi = {0};
	tvi.mask = TVIF_IMAGE;
	tvi.hItem = hItem == NULL ? TreeView_GetSelection(hLstView) : hItem;
	TreeView_GetItem(hLstView, &tvi);
	return tvi.iImage;
}

static bool browseSmileys(HWND hwndDlg) {
	char path[MAX_PATH];
	OPENFILENAME ofn={0};
	GetDlgItemText(hwndDlg, IDC_SMILEYS_FILENAME, path, sizeof(path));
	ofn.lStructSize = sizeof(OPENFILENAME);//_SIZE_VERSION_400;
	ofn.hwndOwner = hwndDlg;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "Smiley Library (*.asl)\0*.asl\0All Files\0*.*\0\0";
	ofn.lpstrFile = path;
	ofn.Flags = OFN_FILEMUSTEXIST;
	ofn.nMaxFile = sizeof(path);
	ofn.nMaxFileTitle = MAX_PATH;
	ofn.lpstrDefExt = "asl";
	if(GetOpenFileName(&ofn)) {
        strcpy(protoFilenames[currentProtoItem], path);
		return true;
	}
	return false;
}

static BOOL CALLBACK IEViewEmoticonsOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	int i;
	BOOL bChecked;
	switch (msg) {
	case WM_INITDIALOG:
		{
			TranslateDialogDefault(hwndDlg);
//			SendDlgItemMessage(hwndDlg, IDC_SMILEYS_PREVIEW, BM_SETIMAGE, IMAGE_ICON, (LPARAM) smileyIcon);

			bChecked = FALSE;
			i = Options::getSmileyFlags();
			if (i&Options::SMILEY_ENABLED) {
			    bChecked = TRUE;
				CheckDlgButton(hwndDlg, IDC_SMILEYS, TRUE);
			}
			EnableWindow(GetDlgItem(hwndDlg, IDC_SMILEYS_FILENAME), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_SMILEYS), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_ISOLATED_SMILEYS), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_SURROUND_SMILEYS), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_SMILEYS_IN_NAMES), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_SMILEYS_PREVIEW), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_PROTOLIST), bChecked);
//			EnableWindow(GetDlgItem(hwndDlg, IDC_REPLACE_SMILEYADD), bChecked);
			SendDlgItemMessage(hwndDlg, IDC_BKGCOLOR, CPM_SETCOLOUR, 0, Options::getSmileyBackground());
//			SendDlgItemMessage(hwndDlg, IDC_BKGCOLOUR, CPM_SETDEFAULTCOLOUR, 0, SRMSGDEFSET_BKGCOLOUR);
			if (Options::getSmileyFlags() & Options::SMILEY_PROTOCOLS) {
				CheckDlgButton(hwndDlg, IDC_PROTO_SMILEYS, TRUE);
			} else {
				bChecked = FALSE;
			}
			if (i&Options::SMILEY_ISOLATED) {
				CheckDlgButton(hwndDlg, IDC_ISOLATED_SMILEYS, TRUE);
			}
			if (i&Options::SMILEY_SURROUND) {
				CheckDlgButton(hwndDlg, IDC_SURROUND_SMILEYS, TRUE);
			}
			if (i&Options::SMILEY_SMILEYINNAMES) {
				CheckDlgButton(hwndDlg, IDC_SMILEYS_IN_NAMES, TRUE);
			}
			buildProtoList();

			{
				int protoCount;
				PROTOCOLDESCRIPTOR **pProtos;
				CallService(MS_PROTO_ENUMPROTOCOLS, (WPARAM)&protoCount, (LPARAM)&pProtos);
				HWND hProtoList = GetDlgItem(hwndDlg, IDC_PROTOLIST);
				HIMAGELIST hImageList = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
					ILC_MASK | ILC_COLOR32, protoCount+1, 0);

				for (i = 0; i<protoCount+1; i++)  {
					HICON hIcon = NULL;
					if (i > 0 ) {
						if (pProtos[i-1]->type == PROTOTYPE_PROTOCOL) {
							hIcon=(HICON)CallProtoService(pProtos[i-1]->szName, PS_LOADICON, PLI_PROTOCOL | PLIF_SMALL, 0);
							if (hIcon == NULL)  {
								hIcon=(HICON)CallProtoService(pProtos[i-1]->szName, PS_LOADICON, PLI_PROTOCOL, 0);
							}
						}
					}
					if (hIcon == NULL) {
						hIcon = (HICON) LoadImage(hInstance, MAKEINTRESOURCE(IDI_SMILEY), IMAGE_ICON,
										GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
					}
					ImageList_AddIcon(hImageList, hIcon);
					DestroyIcon(hIcon);
				}
				TreeView_SetImageList(hProtoList, hImageList, TVSIL_NORMAL);
			}
			refreshProtoList(hwndDlg, IsDlgButtonChecked(hwndDlg, IDC_PROTO_SMILEYS));
			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {
			case IDC_SMILEYS_FILENAME:
				if (HIWORD(wParam)==EN_KILLFOCUS) {
					GetDlgItemText(hwndDlg, IDC_SMILEYS_FILENAME, protoFilenames[currentProtoItem], MAX_PATH);
					updateSmileyInfo(hwndDlg, currentProtoItem);
				}
				if ((HWND)lParam==GetFocus() && HIWORD(wParam)==EN_CHANGE) {
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
			case IDC_SMILEYS_PREVIEW:
				{
					char path[MAX_PATH];
					HWND hLstView = GetDlgItem(hwndDlg, IDC_PROTOLIST);
					int iItem = getSelProto(hLstView, TreeView_GetSelection(hLstView));
					GetDlgItemText(hwndDlg, IDC_SMILEYS_FILENAME, path, sizeof(path));
					if (SmileyMap::loadLibrary("IEVIewPreview", path)) {
					    SmileyMap *map = SmileyMap::getSmileyMap("IEVIewPreview");
					    if (map!=NULL) {
	                        RECT rc;
						    GetWindowRect(GetDlgItem(hwndDlg, IDC_SMILEYS_PREVIEW), &rc);
							map->getWindow()->show(NULL, 0, 0, rc.left, rc.bottom);
						}
					} else {
	                	SetDlgItemText(hwndDlg, IDC_SMILEYS_FILENAME, protoFilenames[iItem]);
					}
				}
			    break;
			case IDC_ISOLATED_SMILEYS:
			case IDC_SURROUND_SMILEYS:
			case IDC_SMILEYS_IN_NAMES:
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_SMILEYS:
				bChecked = IsDlgButtonChecked(hwndDlg, IDC_SMILEYS);
				EnableWindow(GetDlgItem(hwndDlg, IDC_SMILEYS_FILENAME), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_SMILEYS), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_ISOLATED_SMILEYS), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_SURROUND_SMILEYS), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_SMILEYS_IN_NAMES), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_PROTO_SMILEYS), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_SMILEYS_PREVIEW), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_PROTOLIST), bChecked);
			case IDC_PROTO_SMILEYS:
				bChecked = IsDlgButtonChecked(hwndDlg, IDC_PROTO_SMILEYS) & IsDlgButtonChecked(hwndDlg, IDC_SMILEYS);
				if (!bChecked ) {
					GetDlgItemText(hwndDlg, IDC_SMILEYS_FILENAME, protoFilenames[currentProtoItem], sizeof(MAX_PATH));
				}
				refreshProtoList(hwndDlg, IsDlgButtonChecked(hwndDlg, IDC_PROTO_SMILEYS));
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_BROWSE_SMILEYS:
				if (browseSmileys(hwndDlg)) {
					updateSmileyInfo(hwndDlg, currentProtoItem);
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				}
				break;
   			case IDC_BKGCOLOR:
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			}
		}
		break;
	case UM_CHECKSTATECHANGE:
		{
			if (TreeView_GetCheckState((HWND)wParam, (HTREEITEM) lParam)) {
				browseSmileys(hwndDlg);
			} else {
				strcpy(protoFilenames[getSelProto((HWND)wParam, (HTREEITEM) lParam)], "");
			}
			if ((HTREEITEM) lParam != TreeView_GetSelection((HWND)wParam)) {
				TreeView_SelectItem((HWND)wParam, (HTREEITEM) lParam);
			} else {
				updateSmileyInfo(hwndDlg, currentProtoItem);
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
							LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) lParam;
							if (pnmtv->itemNew.state & TVIS_SELECTED) {
								HWND hLstView = GetDlgItem(hwndDlg, IDC_PROTOLIST);
								TVITEM tvi = {0};
								tvi.mask = TVIF_IMAGE;
								tvi.hItem = TreeView_GetSelection(hLstView);
								TreeView_GetItem(hLstView, &tvi);
								int iItem = tvi.iImage;
								GetDlgItemText(hwndDlg, IDC_SMILEYS_FILENAME, protoFilenames[currentProtoItem], MAX_PATH);
								updateSmileyInfo(hwndDlg, iItem);
							}
						}
						break;
				}
				break;
			}
			switch (((LPNMHDR) lParam)->code) {
			case PSN_APPLY:
				i = 0;
				if (IsDlgButtonChecked(hwndDlg, IDC_SMILEYS)) {
					i |= Options::SMILEY_ENABLED;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_ISOLATED_SMILEYS)) {
					i |= Options::SMILEY_ISOLATED;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_SURROUND_SMILEYS)) {
					i |= Options::SMILEY_SURROUND;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_SMILEYS_IN_NAMES)) {
					i |= Options::SMILEY_SMILEYINNAMES;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_PROTO_SMILEYS)) {
					i |= Options::SMILEY_PROTOCOLS;
				}
				Options::setSmileyFlags(i);
				Options::setSmileyBackground((DWORD)SendDlgItemMessage(hwndDlg, IDC_BKGCOLOR, CPM_GETCOLOUR, 0, 0));
				SmileyMap::setSelectorBackground(Options::getSmileyBackground());
				GetDlgItemText(hwndDlg, IDC_SMILEYS_FILENAME, protoFilenames[currentProtoItem], MAX_PATH);
				for (i = 0; i < protoNum; i++) {
					if (i==0 || protoNames[i][0] != '\0') {
                    	Options::setSmileyFile(protoNames[i], protoFilenames[i]);
					}
				}
				return TRUE;

			}
		}
		break;
	case WM_DESTROY:
		if (protoNames != NULL) {
			delete[] protoNames;
			protoNames = NULL;
		}
		if (protoFilenames != NULL) {
			delete[] protoFilenames;
			protoFilenames = NULL;
		}
		break;
	}
	return FALSE;
}

static BOOL CALLBACK IEViewTemplatesOptDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	int i;
	BOOL bChecked;
	char path[MAX_PATH];
	switch (msg) {
	case WM_INITDIALOG:
		{
			TranslateDialogDefault(hwndDlg);

			bChecked = FALSE;
			if (Options::getSRMMFlags() & Options::TEMPLATES_ENABLED) {
			    bChecked = TRUE;
				CheckDlgButton(hwndDlg, IDC_TEMPLATES, TRUE);
			}
			if (Options::getSRMMFlags() & Options::LOG_SHOW_FILE) {
				CheckDlgButton(hwndDlg, IDC_LOG_SHOW_FILE, TRUE);
			}
			if (Options::getSRMMFlags() & Options::LOG_SHOW_URL) {
				CheckDlgButton(hwndDlg, IDC_LOG_SHOW_URL, TRUE);
			}
			if (Options::getSRMMFlags() & Options::LOG_SHOW_STATUSCHANGE) {
				CheckDlgButton(hwndDlg, IDC_LOG_SHOW_STATUSCHANGE, TRUE);
			}
			if (Options::getSRMMFlags() & Options::LOG_SHOW_NICKNAMES) {
				CheckDlgButton(hwndDlg, IDC_LOG_SHOW_NICKNAMES, TRUE);
			}
			if (Options::getSRMMFlags() & Options::LOG_SHOW_TIME) {
				CheckDlgButton(hwndDlg, IDC_LOG_SHOW_TIME, TRUE);
			}
			if (Options::getSRMMFlags() & Options::LOG_SHOW_DATE) {
				CheckDlgButton(hwndDlg, IDC_LOG_SHOW_DATE, TRUE);
			}
			if (Options::getSRMMFlags() & Options::LOG_SHOW_SECONDS) {
				CheckDlgButton(hwndDlg, IDC_LOG_SHOW_SECONDS, TRUE);
			}
			if (Options::getSRMMFlags() & Options::LOG_LONG_DATE) {
				CheckDlgButton(hwndDlg, IDC_LOG_LONG_DATE, TRUE);
			}
			if (Options::getSRMMFlags() & Options::LOG_RELATIVE_DATE) {
				CheckDlgButton(hwndDlg, IDC_LOG_RELATIVE_DATE, TRUE);
			}
			if (Options::getSRMMFlags() & Options::LOG_GROUP_MESSAGES) {
				CheckDlgButton(hwndDlg, IDC_LOG_GROUP_MESSAGES, TRUE);
			}

			EnableWindow(GetDlgItem(hwndDlg, IDC_TEMPLATES_FILENAME), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_TEMPLATES), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_TEMPLATES_FILENAME_RTL), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_TEMPLATES_RTL), bChecked);

			EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_FILE), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_URL), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_STATUSCHANGE), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_NICKNAMES), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_TIME), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_DATE), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_SECONDS), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_LONG_DATE), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_RELATIVE_DATE), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_GROUP_MESSAGES), bChecked);

			if (Options::getTemplatesFile() != NULL) {
                SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, Options::getTemplatesFile());
			}
			if (Options::getTemplatesFileRTL() != NULL) {
                SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, Options::getTemplatesFileRTL());
			}

			bChecked = !IsDlgButtonChecked(hwndDlg, IDC_TEMPLATES);
			EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNALCSS), bChecked);
			if (Options::getSRMMFlags() & Options::CSS_ENABLED) {
				CheckDlgButton(hwndDlg, IDC_EXTERNALCSS, TRUE);
			} else {
                bChecked = FALSE;
			}
			EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNALCSS_FILENAME), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_EXTERNALCSS), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_EXTERNALCSS_RTL), bChecked);
			if (Options::getExternalCSSFile() != NULL) {
                SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, Options::getExternalCSSFile());
			}
			if (Options::getExternalCSSFileRTL() != NULL) {
                SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, Options::getExternalCSSFileRTL());
			}

			bChecked = !IsDlgButtonChecked(hwndDlg, IDC_TEMPLATES) && !IsDlgButtonChecked(hwndDlg, IDC_EXTERNALCSS);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BACKGROUND_IMAGE), bChecked);
			if (Options::getSRMMFlags() & Options::IMAGE_ENABLED) {
				CheckDlgButton(hwndDlg, IDC_BACKGROUND_IMAGE, TRUE);
			} else {
				bChecked = FALSE;
			}
			EnableWindow(GetDlgItem(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_BACKGROUND_IMAGE), bChecked);
			if (Options::getSRMMFlags() & Options::IMAGE_SCROLL) {
				CheckDlgButton(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE, TRUE);
			}
			if (Options::getBkgImageFile() != NULL) {
                SetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, Options::getBkgImageFile());
			}
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
			case IDC_LOG_SHOW_FILE:
			case IDC_LOG_SHOW_URL:
			case IDC_LOG_SHOW_STATUSCHANGE:
			case IDC_LOG_SHOW_NICKNAMES:
			case IDC_LOG_SHOW_TIME:
			case IDC_LOG_SHOW_DATE:
			case IDC_LOG_SHOW_SECONDS:
			case IDC_LOG_LONG_DATE:
			case IDC_LOG_RELATIVE_DATE:
			case IDC_LOG_GROUP_MESSAGES:
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_TEMPLATES:
				bChecked = IsDlgButtonChecked(hwndDlg, IDC_TEMPLATES);
				EnableWindow(GetDlgItem(hwndDlg, IDC_TEMPLATES_FILENAME), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_TEMPLATES), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_TEMPLATES_FILENAME_RTL), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_TEMPLATES_RTL), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_FILE), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_URL), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_STATUSCHANGE), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_NICKNAMES), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_TIME), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_DATE), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_SHOW_SECONDS), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_LONG_DATE), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_RELATIVE_DATE), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_LOG_GROUP_MESSAGES), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNALCSS), !bChecked);
//				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
	//			break;
			case IDC_EXTERNALCSS:
                bChecked = IsDlgButtonChecked(hwndDlg, IDC_TEMPLATES) || IsDlgButtonChecked(hwndDlg, IDC_EXTERNALCSS);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BACKGROUND_IMAGE), !bChecked);
               	bChecked = !IsDlgButtonChecked(hwndDlg, IDC_TEMPLATES) && IsDlgButtonChecked(hwndDlg, IDC_EXTERNALCSS);
				EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNALCSS_FILENAME), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_EXTERNALCSS), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_EXTERNALCSS_RTL), bChecked);
		//		SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
			//	break;
			case IDC_BACKGROUND_IMAGE:
                bChecked = !IsDlgButtonChecked(hwndDlg, IDC_TEMPLATES) && !IsDlgButtonChecked(hwndDlg, IDC_EXTERNALCSS) && IsDlgButtonChecked(hwndDlg, IDC_BACKGROUND_IMAGE);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BROWSE_BACKGROUND_IMAGE), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE), bChecked);
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_BROWSE_TEMPLATES:
				{
					OPENFILENAME ofn={0};
					GetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, path, sizeof(path));
					ofn.lStructSize = sizeof(OPENFILENAME);//_SIZE_VERSION_400;
					ofn.hwndOwner = hwndDlg;
					ofn.hInstance = NULL;
					ofn.lpstrFilter = "Templates (*.ivt)\0*.ivt\0All Files\0*.*\0\0";
					ofn.lpstrFile = path;
					ofn.Flags = OFN_FILEMUSTEXIST;
					ofn.nMaxFile = sizeof(path);
					ofn.nMaxFileTitle = MAX_PATH;
					ofn.lpstrDefExt = "ivt";
					if(GetOpenFileName(&ofn)) {
						SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, path);
						SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
					}
				}
				break;
			case IDC_BROWSE_TEMPLATES_RTL:
				{
					OPENFILENAME ofn={0};
					GetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, path, sizeof(path));
					ofn.lStructSize = sizeof(OPENFILENAME);//_SIZE_VERSION_400;
					ofn.hwndOwner = hwndDlg;
					ofn.hInstance = NULL;
					ofn.lpstrFilter = "Templates (*.ivt)\0*.ivt\0All Files\0*.*\0\0";
					ofn.lpstrFile = path;
					ofn.Flags = OFN_FILEMUSTEXIST;
					ofn.nMaxFile = sizeof(path);
					ofn.nMaxFileTitle = MAX_PATH;
					ofn.lpstrDefExt = "ivt";
					if(GetOpenFileName(&ofn)) {
						SetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, path);
						SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
					}
				}
				break;
			case IDC_BROWSE_BACKGROUND_IMAGE:
				{
					OPENFILENAME ofn={0};
					GetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, path, sizeof(path));
					ofn.lStructSize = sizeof(OPENFILENAME);
					ofn.hwndOwner = hwndDlg;
					ofn.hInstance = NULL;
					ofn.lpstrFilter = "All Images (*.jpg,*.gif,*.png,*.bmp)\0*.jpg;*.gif;*.png;*.bmp\0All Files\0*.*\0\0";
					ofn.lpstrFile = path;
					ofn.Flags = OFN_FILEMUSTEXIST;
					ofn.nMaxFile = sizeof(path);
					ofn.nMaxFileTitle = MAX_PATH;
					ofn.lpstrDefExt = "jpg";
					if(GetOpenFileName(&ofn)) {
						SetDlgItemText(hwndDlg,IDC_BACKGROUND_IMAGE_FILENAME,path);
						SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
					}
				}
				break;
			case IDC_BROWSE_EXTERNALCSS:
				{
					OPENFILENAME ofn={0};
					GetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, path, sizeof(path));
					ofn.lStructSize = sizeof(OPENFILENAME);//_SIZE_VERSION_400;
					ofn.hwndOwner = hwndDlg;
					ofn.hInstance = NULL;
					ofn.lpstrFilter = "Style Sheet (*.css)\0*.css\0All Files\0*.*\0\0";
					ofn.lpstrFile = path;
					ofn.Flags = OFN_FILEMUSTEXIST;
					ofn.nMaxFile = sizeof(path);
					ofn.nMaxFileTitle = MAX_PATH;
					ofn.lpstrDefExt = "css";
					if(GetOpenFileName(&ofn)) {
						SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, path);
						SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
					}
				}
				break;
			case IDC_BROWSE_EXTERNALCSS_RTL:
				{
					OPENFILENAME ofn={0};
					GetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, path, sizeof(path));
					ofn.lStructSize = sizeof(OPENFILENAME);//_SIZE_VERSION_400;
					ofn.hwndOwner = hwndDlg;
					ofn.hInstance = NULL;
					ofn.lpstrFilter = "Style Sheet (*.css)\0*.css\0All Files\0*.*\0\0";
					ofn.lpstrFile = path;
					ofn.Flags = OFN_FILEMUSTEXIST;
					ofn.nMaxFile = sizeof(path);
					ofn.nMaxFileTitle = MAX_PATH;
					ofn.lpstrDefExt = "css";
					if(GetOpenFileName(&ofn)) {
						SetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, path);
						SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
					}
				}
				break;
			}
		}
		break;
	case WM_NOTIFY:
		{
			switch (((LPNMHDR) lParam)->code) {
			case PSN_APPLY:
				i = 0;
				if (IsDlgButtonChecked(hwndDlg, IDC_TEMPLATES)) {
					i |= Options::TEMPLATES_ENABLED;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_FILE)) {
					i |= Options::LOG_SHOW_FILE;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_URL)) {
					i |= Options::LOG_SHOW_URL;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_STATUSCHANGE)) {
					i |= Options::LOG_SHOW_STATUSCHANGE;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_NICKNAMES)) {
					i |= Options::LOG_SHOW_NICKNAMES;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_TIME)) {
					i |= Options::LOG_SHOW_TIME;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_DATE)) {
					i |= Options::LOG_SHOW_DATE;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_LOG_SHOW_SECONDS)) {
					i |= Options::LOG_SHOW_SECONDS;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_LOG_LONG_DATE)) {
					i |= Options::LOG_LONG_DATE;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_LOG_RELATIVE_DATE)) {
					i |= Options::LOG_RELATIVE_DATE;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_LOG_GROUP_MESSAGES)) {
					i |= Options::LOG_GROUP_MESSAGES;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_BACKGROUND_IMAGE)) {
					i |= Options::IMAGE_ENABLED;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_SCROLL_BACKGROUND_IMAGE)) {
					i |= Options::IMAGE_SCROLL;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_EXTERNALCSS)) {
					i |= Options::CSS_ENABLED;
				}
				Options::setSRMMFlags(i);
				GetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME, path, sizeof(path));
				Options::setTemplatesFile(path);
				GetDlgItemText(hwndDlg, IDC_TEMPLATES_FILENAME_RTL, path, sizeof(path));
				Options::setTemplatesFileRTL(path);
				GetDlgItemText(hwndDlg, IDC_BACKGROUND_IMAGE_FILENAME, path, sizeof(path));
				Options::setBkgImageFile(path);
				GetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME, path, sizeof(path));
				Options::setExternalCSSFile(path);
				GetDlgItemText(hwndDlg, IDC_EXTERNALCSS_FILENAME_RTL, path, sizeof(path));
				Options::setExternalCSSFileRTL(path);
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
	int i;
	BOOL bChecked;
	char path[MAX_PATH];
	switch (msg) {
	case WM_INITDIALOG:
		{
			TranslateDialogDefault(hwndDlg);
			if (Options::getGroupChatCSSFile() != NULL) {
                SetDlgItemText(hwndDlg, IDC_GROUPCHAT_CSS_FILENAME, Options::getGroupChatCSSFile());
			}
			bChecked = FALSE;
			if (Options::getGroupChatFlags() & Options::TEMPLATES_ENABLED) {
				CheckDlgButton(hwndDlg, IDC_GROUPCHAT_TEMPLATES, TRUE);
			    bChecked = TRUE;
			}
			EnableWindow(GetDlgItem(hwndDlg, IDC_GROUPCHAT_TEMPLATES_FILENAME), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_GROUPCHAT_TEMPLATES_BROWSE), bChecked);
			if (Options::getGroupChatTemplatesFile() != NULL) {
                SetDlgItemText(hwndDlg, IDC_GROUPCHAT_TEMPLATES_FILENAME, Options::getGroupChatTemplatesFile());
			}
			bChecked = !IsDlgButtonChecked(hwndDlg, IDC_GROUPCHAT_TEMPLATES);
			EnableWindow(GetDlgItem(hwndDlg, IDC_GROUPCHAT_CSS), bChecked);
			if (Options::getGroupChatFlags() & Options::CSS_ENABLED) {
				CheckDlgButton(hwndDlg, IDC_GROUPCHAT_CSS, TRUE);
			} else {
                bChecked = FALSE;
			}
			EnableWindow(GetDlgItem(hwndDlg, IDC_GROUPCHAT_CSS_FILENAME), bChecked);
			EnableWindow(GetDlgItem(hwndDlg, IDC_GROUPCHAT_CSS_BROWSE), bChecked);
			if (Options::getGroupChatCSSFile() != NULL) {
                SetDlgItemText(hwndDlg, IDC_GROUPCHAT_CSS_FILENAME, Options::getGroupChatCSSFile());
			}
			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {
            case IDC_GROUPCHAT_CSS_FILENAME:
				if ((HWND)lParam==GetFocus() && HIWORD(wParam)==EN_CHANGE)
					SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_GROUPCHAT_TEMPLATES:
				bChecked = IsDlgButtonChecked(hwndDlg, IDC_GROUPCHAT_TEMPLATES);
				EnableWindow(GetDlgItem(hwndDlg, IDC_GROUPCHAT_TEMPLATES_FILENAME), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_GROUPCHAT_TEMPLATES_BROWSE), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_GROUPCHAT_CSS), !bChecked);
			case IDC_GROUPCHAT_CSS:
               	bChecked = !IsDlgButtonChecked(hwndDlg, IDC_GROUPCHAT_TEMPLATES) && IsDlgButtonChecked(hwndDlg, IDC_GROUPCHAT_CSS);
				EnableWindow(GetDlgItem(hwndDlg, IDC_GROUPCHAT_CSS_FILENAME), bChecked);
				EnableWindow(GetDlgItem(hwndDlg, IDC_GROUPCHAT_CSS_BROWSE), bChecked);
				SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
				break;
			case IDC_GROUPCHAT_CSS_BROWSE:
				{
					OPENFILENAME ofn={0};
					GetDlgItemText(hwndDlg, IDC_GROUPCHAT_CSS_FILENAME, path, sizeof(path));
					ofn.lStructSize = sizeof(OPENFILENAME);//_SIZE_VERSION_400;
					ofn.hwndOwner = hwndDlg;
					ofn.hInstance = NULL;
					ofn.lpstrFilter = "Style Sheet (*.css)\0*.css\0All Files\0*.*\0\0";
					ofn.lpstrFile = path;
					ofn.Flags = OFN_FILEMUSTEXIST;
					ofn.nMaxFile = sizeof(path);
					ofn.nMaxFileTitle = MAX_PATH;
					ofn.lpstrDefExt = "css";
					if(GetOpenFileName(&ofn)) {
						SetDlgItemText(hwndDlg, IDC_GROUPCHAT_CSS_FILENAME, path);
						SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
					}
				}
				break;
			case IDC_GROUPCHAT_TEMPLATES_BROWSE:
				{
					OPENFILENAME ofn={0};
					GetDlgItemText(hwndDlg, IDC_GROUPCHAT_TEMPLATES_FILENAME, path, sizeof(path));
					ofn.lStructSize = sizeof(OPENFILENAME);//_SIZE_VERSION_400;
					ofn.hwndOwner = hwndDlg;
					ofn.hInstance = NULL;
					ofn.lpstrFilter = "Templates (*.ivt)\0*.ivt\0All Files\0*.*\0\0";
					ofn.lpstrFile = path;
					ofn.Flags = OFN_FILEMUSTEXIST;
					ofn.nMaxFile = sizeof(path);
					ofn.nMaxFileTitle = MAX_PATH;
					ofn.lpstrDefExt = "ivt";
					if(GetOpenFileName(&ofn)) {
						SetDlgItemText(hwndDlg, IDC_GROUPCHAT_TEMPLATES_FILENAME, path);
						SendMessage(GetParent(GetParent(hwndDlg)), PSM_CHANGED, 0, 0);
					}
				}
				break;
			}
		}
		break;
	case WM_NOTIFY:
		{
			switch (((LPNMHDR) lParam)->code) {
			case PSN_APPLY:
				GetDlgItemText(hwndDlg, IDC_GROUPCHAT_CSS_FILENAME, path, sizeof(path));
				Options::setGroupChatCSSFile(path);
				i = 0;
				if (IsDlgButtonChecked(hwndDlg, IDC_GROUPCHAT_TEMPLATES)) {
					i |= Options::TEMPLATES_ENABLED;
				}
				if (IsDlgButtonChecked(hwndDlg, IDC_GROUPCHAT_CSS)) {
					i |= Options::CSS_ENABLED;
				}
				Options::setGroupChatFlags(i);
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
int Options::generalFlags;
char *Options::bkgFilename = NULL;
int Options::smileyFlags;
int Options::groupChatFlags;
char *Options::groupChatCSSFilename = NULL;
char *Options::externalCSSFilename = NULL;
char *Options::externalCSSFilenameRTL = NULL;
char *Options::templatesFilename = NULL;
char *Options::templatesFilenameRTL = NULL;
char *Options::groupChatTemplatesFilename = NULL;
DWORD Options::smileyBackground;
int Options::srmmFlags;

void Options::init() {
	if (isInited) return;
	isInited = true;
	bMathModule = (bool) ServiceExists(MTH_GET_GIF_UNICODE);
	DBVARIANT dbv;
	generalFlags = DBGetContactSettingDword(NULL, ieviewModuleName, DBS_BASICFLAGS, 0);
	srmmFlags = DBGetContactSettingDword(NULL, ieviewModuleName, DBS_SRMMFLAGS, FALSE);
	smileyFlags = DBGetContactSettingDword(NULL, ieviewModuleName, DBS_SMILEYSFLAGS, 1);
	smileyBackground = DBGetContactSettingDword(NULL, ieviewModuleName, DBS_SMILEYBACKGROUND, 0xFFFFFF);
	if (!DBGetContactSetting(NULL,  ieviewModuleName, DBS_BACKGROUNDIMAGEFILE, &dbv)) {
    	char tmpPath[MAX_PATH];
    	strcpy(tmpPath, dbv.pszVal);
    	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
   	    	CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
   		}
		bkgFilename = new char[strlen(tmpPath)+1];
		strcpy(bkgFilename, tmpPath);
		DBFreeVariant(&dbv);
	} else {
        bkgFilename = new char[1];
        strcpy(bkgFilename, "");
	}
	PROTOCOLDESCRIPTOR **protoList;
	int protoCount;
	CallService(MS_PROTO_ENUMPROTOCOLS, (WPARAM)&protoCount, (LPARAM)&protoList);
	for (int i = 0; i < protoCount; i++) {
		if (protoList[i]->type == PROTOTYPE_PROTOCOL && strcmp(protoList[i]->szName,"MetaContacts")) {
			char dbName[1024];
			strcpy(dbName, protoList[i]->szName);
			strcat(dbName, "SmileyFile");
			if (!DBGetContactSetting(NULL,  ieviewModuleName, dbName, &dbv)) {
			   	char tmpPath[MAX_PATH];
            	strcpy(tmpPath, dbv.pszVal);
            	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE)) {
           	    	CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
           		}
				SmileyMap::loadLibrary(protoList[i]->szName, tmpPath);
				DBFreeVariant(&dbv);
			}
		}
	}
	if (!DBGetContactSetting(NULL,  ieviewModuleName, "SmileyFile", &dbv)) {
    	char tmpPath[MAX_PATH];
		strcpy(tmpPath, dbv.pszVal);
    	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE)) {
   	    	CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
   		}
		SmileyMap::loadLibrary("", tmpPath);
		DBFreeVariant(&dbv);
	}
	if (!DBGetContactSetting(NULL,  ieviewModuleName, DBS_EXTERNALCSSFILE, &dbv)) {
    	char tmpPath[MAX_PATH];
    	strcpy(tmpPath, dbv.pszVal);
    	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
   	    	CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
   		}
		externalCSSFilename = new char[strlen(tmpPath)+1];
		strcpy(externalCSSFilename, tmpPath);
		DBFreeVariant(&dbv);
	} else {
        externalCSSFilename = new char[1];
        strcpy(externalCSSFilename, "");
	}
	if (!DBGetContactSetting(NULL,  ieviewModuleName, DBS_EXTERNALCSSFILE_RTL, &dbv)) {
    	char tmpPath[MAX_PATH];
    	strcpy(tmpPath, dbv.pszVal);
    	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
   	    	CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
   		}
		externalCSSFilenameRTL = new char[strlen(tmpPath)+1];
		strcpy(externalCSSFilenameRTL, tmpPath);
		DBFreeVariant(&dbv);
	} else {
        externalCSSFilenameRTL = new char[1];
        strcpy(externalCSSFilenameRTL, "");
	}
	if (!DBGetContactSetting(NULL,  ieviewModuleName, DBS_TEMPLATESFILE, &dbv)) {
    	char tmpPath[MAX_PATH];
    	strcpy(tmpPath, dbv.pszVal);
    	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE)) {
   	    	CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
   		}
		templatesFilename = new char[strlen(tmpPath)+1];
		strcpy(templatesFilename, tmpPath);
		DBFreeVariant(&dbv);
	} else {
        templatesFilename = new char[1];
        strcpy(templatesFilename, "");
	}
	if (!DBGetContactSetting(NULL,  ieviewModuleName, DBS_TEMPLATESFILE_RTL, &dbv)) {
    	char tmpPath[MAX_PATH];
    	strcpy(tmpPath, dbv.pszVal);
    	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE)) {
   	    	CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
   		}
		templatesFilenameRTL = new char[strlen(tmpPath)+1];
		strcpy(templatesFilenameRTL, tmpPath);
		DBFreeVariant(&dbv);
	} else {
        templatesFilenameRTL = new char[1];
        strcpy(templatesFilenameRTL, "");
	}
	groupChatFlags = DBGetContactSettingDword(NULL, ieviewModuleName, DBS_GROUPCHATFLAGS, FALSE);
	if (!DBGetContactSetting(NULL,  ieviewModuleName, DBS_GROUPCHATCSSFILE, &dbv)) {
    	char tmpPath[MAX_PATH];
    	strcpy(tmpPath, dbv.pszVal);
    	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE) && strncmp(tmpPath, "http://", 7)) {
   	    	CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
   		}
		groupChatCSSFilename = new char[strlen(tmpPath)+1];
		strcpy(groupChatCSSFilename, tmpPath);
		DBFreeVariant(&dbv);
	} else {
        groupChatCSSFilename = new char[1];
        strcpy(groupChatCSSFilename, "");
	}
	if (!DBGetContactSetting(NULL,  ieviewModuleName, DBS_GROUPCHATTEMPLATESFILE, &dbv)) {
    	char tmpPath[MAX_PATH];
    	strcpy(tmpPath, dbv.pszVal);
    	if (ServiceExists(MS_UTILS_PATHTOABSOLUTE)) {
   	    	CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)dbv.pszVal, (LPARAM)tmpPath);
   		}
		groupChatTemplatesFilename = new char[strlen(tmpPath)+1];
		strcpy(groupChatTemplatesFilename, tmpPath);
		DBFreeVariant(&dbv);
	} else {
        groupChatTemplatesFilename = new char[1];
        strcpy(groupChatTemplatesFilename, "");
	}
	TemplateMap::loadTemplates("default", templatesFilename);
	TemplateMap::loadTemplates("default_rtl", templatesFilenameRTL);
	TemplateMap::loadTemplates("groupchat", groupChatTemplatesFilename);
	smileyIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_SMILEY), IMAGE_ICON, 0, 0, 0);
//	mathModuleFlags = ServiceExists(MTH_GET_HTML_SOURCE) ? GENERAL_ENABLE_MATHMODULE : 0;
}

void Options::setBkgImageFile(const char *filename) {
	if (bkgFilename != NULL) {
		delete [] bkgFilename;
	}
	bkgFilename = new char[strlen(filename)+1];
	strcpy(bkgFilename, filename);
    char tmpPath[MAX_PATH];
    strcpy (tmpPath, filename);
    if (ServiceExists(MS_UTILS_PATHTORELATIVE)&& strncmp(tmpPath, "http://", 7)) {
    	CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)filename, (LPARAM)tmpPath);
   	}
	DBWriteContactSettingString(NULL, ieviewModuleName, DBS_BACKGROUNDIMAGEFILE, tmpPath);
}

const char * Options::getBkgImageFile() {
	return bkgFilename;
}

void Options::setGeneralFlags(int flags) {
	generalFlags = flags;
	DBWriteContactSettingDword(NULL, ieviewModuleName, DBS_BASICFLAGS, (DWORD) flags);
}

int	Options::getGeneralFlags() {
	return generalFlags;
}

void Options::setSmileyFile(const char *proto, const char *filename) {
	char dbName[1024];
	strcpy(dbName, proto);
	strcat(dbName, "SmileyFile");
    char tmpPath[MAX_PATH];
    strcpy (tmpPath, filename);
    if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
    	CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)filename, (LPARAM)tmpPath);
   	}
	DBWriteContactSettingString(NULL, ieviewModuleName, dbName, tmpPath);
	SmileyMap::loadLibrary(proto, filename);
}

const char *Options::getSmileyFile(const char *proto) {
	SmileyMap *map = SmileyMap::getSmileyMap(proto);
	if (map != NULL) {
		return map->getFilename();
	}
    return NULL;
}

void Options::setSmileyFlags(int flags) {
	smileyFlags = flags;
	DBWriteContactSettingDword(NULL, ieviewModuleName, DBS_SMILEYSFLAGS, (DWORD) flags);
}

int	Options::getSmileyFlags() {
	return smileyFlags;
}

void Options::setSmileyBackground(DWORD color) {
	smileyBackground = color;
	DBWriteContactSettingDword(NULL, ieviewModuleName, DBS_SMILEYBACKGROUND, color);
}

DWORD Options::getSmileyBackground() {
	return smileyBackground;
}

void Options::setExternalCSSFile(const char *filename) {
	if (externalCSSFilename != NULL) {
		delete [] externalCSSFilename;
	}
	externalCSSFilename = new char[strlen(filename)+1];
	strcpy(externalCSSFilename, filename);
    char tmpPath[MAX_PATH];
    strcpy (tmpPath, filename);
    if (ServiceExists(MS_UTILS_PATHTORELATIVE) && strncmp(tmpPath, "http://", 7)) {
    	CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)filename, (LPARAM)tmpPath);
   	}
	DBWriteContactSettingString(NULL, ieviewModuleName, DBS_EXTERNALCSSFILE, tmpPath);
}

const char *Options::getExternalCSSFile() {
	return externalCSSFilename;
}

void Options::setExternalCSSFileRTL(const char *filename) {
	if (externalCSSFilenameRTL != NULL) {
		delete [] externalCSSFilenameRTL;
	}
	externalCSSFilenameRTL = new char[strlen(filename)+1];
	strcpy(externalCSSFilenameRTL, filename);
    char tmpPath[MAX_PATH];
    strcpy (tmpPath, filename);
    if (ServiceExists(MS_UTILS_PATHTORELATIVE) && strncmp(tmpPath, "http://", 7)) {
    	CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)filename, (LPARAM)tmpPath);
   	}
	DBWriteContactSettingString(NULL, ieviewModuleName, DBS_EXTERNALCSSFILE_RTL, tmpPath);
}

const char *Options::getExternalCSSFileRTL() {
	return externalCSSFilenameRTL;
}

void Options::setTemplatesFile(const char *filename) {
	if (templatesFilename != NULL) {
		delete [] templatesFilename;
	}
	templatesFilename = new char[strlen(filename)+1];
	strcpy(templatesFilename, filename);
    char tmpPath[MAX_PATH];
    strcpy (tmpPath, filename);
    if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
    	CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)filename, (LPARAM)tmpPath);
   	}
	DBWriteContactSettingString(NULL, ieviewModuleName, DBS_TEMPLATESFILE, tmpPath);

	TemplateMap::loadTemplates("default", templatesFilename);
}

void Options::setTemplatesFileRTL(const char *filename) {
	if (templatesFilenameRTL != NULL) {
		delete [] templatesFilenameRTL;
	}
	templatesFilenameRTL = new char[strlen(filename)+1];
	strcpy(templatesFilenameRTL, filename);
    char tmpPath[MAX_PATH];
    strcpy (tmpPath, filename);
    if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
    	CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)filename, (LPARAM)tmpPath);
   	}
	DBWriteContactSettingString(NULL, ieviewModuleName, DBS_TEMPLATESFILE_RTL, tmpPath);

	TemplateMap::loadTemplates("default_rtl", templatesFilenameRTL);
}

const char *Options::getTemplatesFile() {
	return templatesFilename;
}

const char *Options::getTemplatesFileRTL() {
	return templatesFilenameRTL;
}

void Options::setSRMMFlags(int flags) {
	srmmFlags = flags;
	DBWriteContactSettingDword(NULL, ieviewModuleName, DBS_SRMMFLAGS, (DWORD) flags);
}

int	Options::getSRMMFlags() {
	return srmmFlags;
}

void Options::setGroupChatCSSFile(const char *filename) {
	if (groupChatCSSFilename != NULL) {
		delete [] groupChatCSSFilename;
	}
	groupChatCSSFilename = new char[strlen(filename)+1];
	strcpy(groupChatCSSFilename, filename);
    char tmpPath[MAX_PATH];
    strcpy (tmpPath, filename);
    if (ServiceExists(MS_UTILS_PATHTORELATIVE) && strncmp(tmpPath, "http://", 7)) {
    	CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)filename, (LPARAM)tmpPath);
   	}
	DBWriteContactSettingString(NULL, ieviewModuleName, DBS_GROUPCHATCSSFILE, tmpPath);
}

const char *Options::getGroupChatCSSFile() {
	return groupChatCSSFilename;
}

void Options::setGroupChatFlags(int flags) {
	groupChatFlags = flags;
	DBWriteContactSettingDword(NULL, ieviewModuleName, DBS_GROUPCHATFLAGS, (DWORD) flags);
}

int	Options::getGroupChatFlags() {
	return groupChatFlags;
}

void Options::setGroupChatTemplatesFile(const char *filename) {
	if (groupChatTemplatesFilename != NULL) {
		delete [] groupChatTemplatesFilename;
	}
	groupChatTemplatesFilename = new char[strlen(filename)+1];
	strcpy(groupChatTemplatesFilename, filename);
    char tmpPath[MAX_PATH];
    strcpy (tmpPath, filename);
    if (ServiceExists(MS_UTILS_PATHTORELATIVE)) {
    	CallService(MS_UTILS_PATHTORELATIVE, (WPARAM)filename, (LPARAM)tmpPath);
   	}
	DBWriteContactSettingString(NULL, ieviewModuleName, DBS_GROUPCHATTEMPLATESFILE, tmpPath);
	TemplateMap::loadTemplates("groupchat", groupChatTemplatesFilename);
}

const char *Options::getGroupChatTemplatesFile() {
	return groupChatTemplatesFilename;
}

bool Options::isMathModule() {
	return bMathModule;
}
