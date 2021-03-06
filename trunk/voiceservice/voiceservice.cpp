/* 
Copyright (C) 2006 Ricardo Pescuma Domenecci

This is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this file; see the file license.txt.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  
*/

#include "commons.h"


// Prototypes ///////////////////////////////////////////////////////////////////////////


PLUGININFOEX pluginInfo = {
	sizeof(PLUGININFOEX),
#ifdef UNICODE
	"Voice Service (Unicode)",
#else
	"Voice Service",
#endif
	PLUGIN_MAKE_VERSION(0,1,2,0),
	"Provide services for protocols that support voice calls",
	"Ricardo Pescuma Domenecci",
	"pescuma@miranda-im.org",
	"� 2007-2009 Ricardo Pescuma Domenecci",
	"http://pescuma.org/miranda/voiceservice",
	UNICODE_AWARE,
	0,		//doesn't replace anything built-in
#ifdef UNICODE
	{ 0x1bfc449d, 0x8f6f, 0x4080, { 0x8f, 0x35, 0xf9, 0x40, 0xb3, 0xde, 0x12, 0x84 } } // {1BFC449D-8F6F-4080-8F35-F940B3DE1284}
#else
	{ 0x1bbe5b21, 0x238d, 0x4cbc, { 0xaf, 0xb8, 0xe, 0xef, 0xab, 0x1b, 0xf2, 0x69 } } // {1BBE5B21-238D-4cbc-AFB8-0EEFAB1BF269}
#endif
};


HINSTANCE hInst;
PLUGINLINK *pluginLink;
MM_INTERFACE mmi;
UTF8_INTERFACE utfi;
LIST_INTERFACE li;

static vector<HANDLE> hHooks;

static vector<HANDLE> hCMCalls;
static HANDLE hCMCall = NULL;
static HANDLE hCMAnswer = NULL;
static HANDLE hCMDrop = NULL;
static HANDLE hCMHold = NULL;

char *metacontacts_proto = NULL;


static INT_PTR ModulesLoaded(WPARAM wParam, LPARAM lParam);
static INT_PTR PreShutdown(WPARAM wParam, LPARAM lParam);
static INT_PTR ProtoAck(WPARAM wParam, LPARAM lParam);
static INT_PTR PreBuildContactMenu(WPARAM wParam, LPARAM lParam);
static INT_PTR AccListChanged(WPARAM wParam, LPARAM lParam);

static INT_PTR VoiceRegister(WPARAM wParam, LPARAM lParam);
static INT_PTR VoiceUnregister(WPARAM wParam, LPARAM lParam);
static INT_PTR VoiceState(WPARAM wParam, LPARAM lParam);

VoiceProvider * FindModule(const char *szModule);
VoiceCall * FindVoiceCall(const char *szModule, const char *id, BOOL add);
VoiceCall * FindVoiceCall(HANDLE hContact);


template<class TYPE>
static int sttCompare(const TYPE *p1, const TYPE *p2)
{
	return (int) p1 - (int) p2;
}
OBJLIST<VoiceProvider> modules(1, &sttCompare<VoiceProvider>);
OBJLIST<VoiceCall> calls(1, &sttCompare<VoiceCall>);

HFONT fonts[NUM_FONTS] = {0};
COLORREF font_colors[NUM_FONTS] = {0};
int font_max_height;

COLORREF bkg_color = {0};
HBRUSH bk_brush = NULL;



static INT_PTR CListDblClick(WPARAM wParam, LPARAM lParam);

static INT_PTR Service_CanCall(WPARAM wParam, LPARAM lParam);
static INT_PTR Service_Call(WPARAM wParam, LPARAM lParam); 
static INT_PTR CMAnswer(WPARAM wParam, LPARAM lParam); 
static INT_PTR CMHold(WPARAM wParam, LPARAM lParam);
static INT_PTR CMDrop(WPARAM wParam, LPARAM lParam);

static INT_PTR IconsChanged(WPARAM wParam, LPARAM lParam);
static INT_PTR ReloadFont(WPARAM wParam, LPARAM lParam);
static INT_PTR ReloadColor(WPARAM wParam, LPARAM lParam);
static VOID CALLBACK ClearOldVoiceCalls(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

static BOOL CALLBACK DlgProcNewCall(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);



class CallingMethod
{
public:
	VoiceProvider *provider;
	HANDLE hContact;
	TCHAR number[128];

	CallingMethod(VoiceProvider *provider, HANDLE hContact, const TCHAR *number = NULL)
		: provider(provider), hContact(hContact)
	{
		if (number == NULL)
			this->number[0] = 0;
		else
			lstrcpyn(this->number, number, MAX_REGS(this->number));
	}

	void Call()
	{
		provider->Call(hContact, number);
	}
};


static int sttCompareCallingMethods(const CallingMethod *p1, const CallingMethod *p2)
{
	if (p1->hContact != p2->hContact)
		return (int)p2->hContact - (int)p1->hContact;

	BOOL noNum1 = (IsEmpty(p1->number) ? 1 : 0);
	BOOL noNum2 = (IsEmpty(p2->number) ? 1 : 0);
	if (noNum1 != noNum2)
		return noNum2 - noNum1;

	if (!noNum1)
	{
		int numDif = lstrcmp(p1->number, p2->number);
		if (numDif != 0)
			return numDif;
	}
	
	BOOL isProto1 = (CallService(MS_PROTO_ISPROTOONCONTACT, (WPARAM) p1->hContact, (LPARAM) p1->provider->name) ? 1 : 0);
	BOOL isProto2 = (CallService(MS_PROTO_ISPROTOONCONTACT, (WPARAM) p2->hContact, (LPARAM) p2->provider->name) ? 1 : 0);
	if (isProto1 != isProto2)
		return isProto2 - isProto1;

	return lstrcmp(p1->provider->description, p2->provider->description);
}


static void AddMethodsFrom(OBJLIST<CallingMethod> *list, HANDLE hContact)
{
	for(int i = 0; i < modules.getCount(); i++)
	{
		VoiceProvider *provider = &modules[i];
		if (provider->CanCall(hContact))
			list->insert(new CallingMethod(provider, hContact));
	}
}

static void AddMethodsFrom(OBJLIST<CallingMethod> *list, HANDLE hContact, const TCHAR *number)
{
	for(int i = 0; i < modules.getCount(); i++)
	{
		VoiceProvider *provider = &modules[i];
		if (provider->CanCall(number))
			list->insert(new CallingMethod(provider, hContact, number));
	}
}

static void BuildCallingMethodsList(OBJLIST<CallingMethod> *list, HANDLE hContact)
{
	AddMethodsFrom(list, hContact);

	// Fetch contact number
	char *proto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);

	DBTString protoNumber(hContact, proto, "Number");
	if (protoNumber != NULL)
		AddMethodsFrom(list, hContact, protoNumber);

	for(int i = 0; ;i++)
	{
		char tmp[128];
		mir_snprintf(tmp, MAX_REGS(tmp), "MyPhone%d", i);

		DBTString number(hContact, "UserInfo", tmp);
		if (number != NULL)
			AddMethodsFrom(list, hContact, number);

		if (number == NULL && i >= 4)
			break;
	}
}


// Functions ////////////////////////////////////////////////////////////////////////////


extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) 
{
	hInst = hinstDLL;
	return TRUE;
}


extern "C" __declspec(dllexport) PLUGININFO* MirandaPluginInfo(DWORD mirandaVersion) 
{
	pluginInfo.cbSize = sizeof(PLUGININFO);
	return (PLUGININFO*) &pluginInfo;
}


extern "C" __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD mirandaVersion)
{
	pluginInfo.cbSize = sizeof(PLUGININFOEX);
	return &pluginInfo;
}


static const MUUID interfaces[] = { MIID_VOICESERVICE, MIID_LAST };
extern "C" __declspec(dllexport) const MUUID* MirandaPluginInterfaces(void)
{
	return interfaces;
}


extern "C" int __declspec(dllexport) Load(PLUGINLINK *link) 
{
	pluginLink = link;

	CHECK_VERSION("Voice Service")

	if (Pa_Initialize() != paNoError)
		MessageBox(NULL, TranslateT("Error initializing portaudio."), TranslateT("Voice Service"), MB_OK | MB_ICONERROR);

	// TODO Assert results here
	mir_getMMI(&mmi);
	mir_getUTFI(&utfi);
	mir_getLI(&li);

	CreateServiceFunction(MS_VOICESERVICE_CLIST_DBLCLK, CListDblClick);
	CreateServiceFunction(MS_VOICESERVICE_REGISTER, VoiceRegister);
	CreateServiceFunction(MS_VOICESERVICE_UNREGISTER, VoiceUnregister);

	// Hooks
	hHooks.push_back( HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded) );
	hHooks.push_back( HookEvent(ME_SYSTEM_PRESHUTDOWN, PreShutdown) );
	hHooks.push_back( HookEvent(ME_PROTO_ACK, ProtoAck) );

	return 0;
}


extern "C" int __declspec(dllexport) Unload(void) 
{
	if (bk_brush != NULL)
		DeleteObject(bk_brush);

	Pa_Terminate();

	return 0;
}


static void AddAccount(PROTOACCOUNT *acc)
{
	if (!IsAccountEnabled(acc))
		return;
	if (IsEmptyA(acc->szModuleName))
		return;
	if (!ProtoServiceExists(acc->szModuleName, PS_VOICE_CAPS))
		return;

	int flags = CallProtoService(acc->szModuleName, PS_VOICE_CAPS, 0, 0);

	if ((flags & VOICE_CAPS_VOICE) == 0)
		return;

	VOICE_MODULE vm = {0};
	vm.cbSize = sizeof(vm);
	vm.name = acc->szModuleName;
	vm.description = acc->tszAccountName;
	vm.flags = flags;
	VoiceRegister((WPARAM) &vm, 0);
}


// Called when all the modules are loaded
static INT_PTR ModulesLoaded(WPARAM wParam, LPARAM lParam) 
{
	// add our modules to the KnownModules list
	CallService("DBEditorpp/RegisterSingleModule", (WPARAM) MODULE_NAME, 0);

	if (ServiceExists(MS_MC_GETPROTOCOLNAME))
		metacontacts_proto = (char *) CallService(MS_MC_GETPROTOCOLNAME, 0, 0);

    // updater plugin support
    if(ServiceExists(MS_UPDATE_REGISTER))
	{
		Update upd = {0};
		char szCurrentVersion[30];

		upd.cbSize = sizeof(upd);
		upd.szComponentName = pluginInfo.shortName;

		upd.szUpdateURL = UPDATER_AUTOREGISTER;

		upd.szBetaVersionURL = "http://pescuma.org/miranda/voiceservice_version.txt";
		upd.szBetaChangelogURL = "http://pescuma.org/miranda/voiceservice#Changelog";
		upd.pbBetaVersionPrefix = (BYTE *)"Voice Service ";
		upd.cpbBetaVersionPrefix = strlen((char *)upd.pbBetaVersionPrefix);
#ifdef UNICODE
		upd.szBetaUpdateURL = "http://pescuma.org/miranda/voiceserviceW.zip";
#else
		upd.szBetaUpdateURL = "http://pescuma.org/miranda/voiceservice.zip";
#endif

		upd.pbVersion = (BYTE *)CreateVersionStringPlugin((PLUGININFO*) &pluginInfo, szCurrentVersion);
		upd.cpbVersion = strlen((char *)upd.pbVersion);

        CallService(MS_UPDATE_REGISTER, 0, (LPARAM)&upd);
	}

	// Init icons
	{
		IcoLib_Register(mainIcons[0], _T("Voice Calls"), _T("Main"), IDI_MAIN);
		IcoLib_Register(mainIcons[1], _T("Voice Calls"), _T("Dialpad"), IDI_DIALPAD);
		IcoLib_Register(mainIcons[2], _T("Voice Calls"), _T("Secure Overlay"), IDI_SECURE);

		int i;
		for(i = 0; i < MAX_REGS(stateNames); i++)
			IcoLib_Register(stateIcons[i], _T("Voice Calls"), stateNames[i], IDI_TALKING + i);

		for(i = 0; i < MAX_REGS(actionNames); i++)
			IcoLib_Register(actionIcons[i], _T("Voice Calls"), actionNames[i], IDI_ACTION_CALL + i);

		hHooks.push_back( HookEvent(ME_SKIN2_ICONSCHANGED, IconsChanged) );
	}

	// Init fonts
	{
		FontIDT fi = {0};
		fi.cbSize = sizeof(fi);
		lstrcpyn(fi.group, TranslateT("Voice Calls"), MAX_REGS(fi.group));
		strncpy(fi.dbSettingsGroup, MODULE_NAME, MAX_REGS(fi.dbSettingsGroup));

		for (int i = 0; i < NUM_FONTS; i++)
		{
			fi.order = i;
			lstrcpyn(fi.name, stateNames[i], MAX_REGS(fi.name));
			strncpy(fi.prefix, stateIcons[i], MAX_REGS(fi.prefix));

			CallService(MS_FONT_REGISTERT, (WPARAM) &fi, 0);
		}

		ReloadFont(0,0);
		hHooks.push_back( HookEvent(ME_FONT_RELOAD, ReloadFont) );
	}

	// Init bkg color
	{
		ColourIDT ci = {0};
		ci.cbSize = sizeof(ci);
		lstrcpyn(ci.group, TranslateT("Voice Calls"), MAX_REGS(ci.group));
		lstrcpyn(ci.name, TranslateT("Background"), MAX_REGS(ci.name));
		strncpy(ci.dbSettingsGroup, MODULE_NAME, MAX_REGS(ci.dbSettingsGroup));
		strncpy(ci.setting, "BkgColor", MAX_REGS(ci.setting));
		ci.defcolour = GetSysColor(COLOR_BTNFACE);

		CallService(MS_COLOUR_REGISTERT, (WPARAM) &ci, 0);

		ReloadColor(0,0);
		hHooks.push_back( HookEvent(ME_COLOUR_RELOAD, ReloadColor) );
	}

	// Init history
	if (ServiceExists(MS_HISTORYEVENTS_REGISTER))
	{
		char *templates[] = {
			"Talking\nCall from %number% has started\n%number%\tOther side of the call",
			"Ringing\nCall from %number% is ringing\n%number%\tOther side of the call",
			"Calling\nCalling %number%\n%number%\tOther side of the call",
			"On Hold\nCall from %number% is on hold\n%number%\tOther side of the call",
			"Ended\nCall from %number% has ended\n%number%\tOther side of the call",
			"Busy\n%number% is busy\n%number%\tOther side of the call",
		};

		HISTORY_EVENT_HANDLER heh = {0};
		heh.cbSize = sizeof(heh);
		heh.module = MODULE_NAME;
		heh.name = "VoiceCall";
		heh.description = "Voice calls";
		heh.eventType = EVENTTYPE_VOICE_CALL;
		heh.defaultIconName = "vca_call";
		heh.supports = HISTORYEVENTS_FORMAT_TCHAR;
		heh.flags = HISTORYEVENTS_FLAG_SHOW_IM_SRMM 
					| HISTORYEVENTS_FLAG_USE_SENT_FLAG
					| HISTORYEVENTS_REGISTERED_IN_ICOLIB;
		heh.templates = templates;
		heh.numTemplates = MAX_REGS(templates);
		CallService(MS_HISTORYEVENTS_REGISTER, (WPARAM) &heh, 0);
	}

	InitOptions();
	InitFrames();

	// Add menu items
	CLISTMENUITEM mi = {0};
	mi.cbSize = sizeof(mi);
	mi.position = -2000020000;
	mi.flags = CMIF_TCHAR;

	HICON icons[MAX_REGS(actionIcons)];
	for(int i = 0; i < MAX_REGS(actionIcons); ++i)
		icons[i] = IcoLib_LoadIcon(actionIcons[i]);

	CreateServiceFunction(MS_VOICESERVICE_CM_CALL, Service_Call);
	mi.ptszName = actionNames[ACTION_CALL];
	mi.hIcon = icons[ACTION_CALL];
	mi.pszService = MS_VOICESERVICE_CM_CALL;
	hCMCall = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	CreateServiceFunction(MS_VOICESERVICE_CM_ANSWER, CMAnswer);
	mi.position++;
	mi.ptszName = actionNames[ACTION_ANSWER];
	mi.hIcon = icons[ACTION_ANSWER];
	mi.pszService = MS_VOICESERVICE_CM_ANSWER;
	hCMAnswer = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	CreateServiceFunction(MS_VOICESERVICE_CM_HOLD, CMHold);
	mi.position++;
	mi.position++;
	mi.ptszName = actionNames[ACTION_HOLD];
	mi.hIcon = icons[ACTION_HOLD];
	mi.pszService = MS_VOICESERVICE_CM_HOLD;
	hCMHold = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	CreateServiceFunction(MS_VOICESERVICE_CM_DROP, CMDrop);
	mi.position++;
	mi.ptszName = actionNames[ACTION_DROP];
	mi.hIcon = icons[ACTION_DROP];
	mi.pszService = MS_VOICESERVICE_CM_DROP;
	hCMDrop = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

	for(i = 0; i < MAX_REGS(actionIcons); ++i)
		IcoLib_ReleaseIcon(icons[i]);

	hHooks.push_back( HookEvent(ME_CLIST_PREBUILDCONTACTMENU, PreBuildContactMenu) );

	// Util services
	CreateServiceFunction(MS_VOICESERVICE_CALL, Service_Call);
	CreateServiceFunction(MS_VOICESERVICE_CAN_CALL, Service_CanCall);

	// Sounds
	SKINSOUNDDESCEX ssd = {0};
	ssd.cbSize = sizeof(ssd);
	ssd.pszSection = "Voice Calls";

	{
		for(int i = 0; i < MAX_REGS(sounds); ++i)
		{
			ssd.pszName = sounds[i].name;
			ssd.pszDescription = sounds[i].description;
			CallService(MS_SKIN_ADDNEWSOUND, 0, (LPARAM)&ssd);
		}
	}

	SetTimer(NULL, 0, 1000, ClearOldVoiceCalls);

	// Accounts
	int numAccounts;
	PROTOACCOUNT **accounts;
	ProtoEnumAccounts(&numAccounts, &accounts);
	for(i = 0; i < numAccounts; ++i)
		AddAccount(accounts[i]);

	hHooks.push_back( HookEvent(ME_PROTO_ACCLISTCHANGED, AccListChanged) );

	return 0;
}


static INT_PTR AccListChanged(WPARAM wParam, LPARAM lParam)
{
	PROTOACCOUNT *acc = (PROTOACCOUNT *) lParam;
	if (acc == NULL)
		return 0;

	VoiceProvider *provider = FindModule(acc->szModuleName);

	switch(wParam)
	{
		case PRAC_ADDED:
		{
			AddAccount(acc);
			break;
		}
		case PRAC_CHANGED:
		{
			if (provider != NULL)
				lstrcpyn(provider->description, acc->tszAccountName, MAX_REGS(provider->description));
			break;
		}
		case PRAC_CHECKED:
		{
			BOOL enabled = IsAccountEnabled(acc);

			if (!enabled)
			{
				if (provider != NULL)
					VoiceUnregister((WPARAM) acc->szModuleName, 0);
			}
			else 
			{
				if (provider == NULL)
					AddAccount(acc);
			}
			break;
		}
		case PRAC_REMOVED:
		{
			if (provider != NULL)
				VoiceUnregister((WPARAM) acc->szModuleName, 0);
			break;
		}
	}

	return 0;
}


static INT_PTR PreShutdown(WPARAM wParam, LPARAM lParam)
{
	DeInitFrames();
	DeInitOptions();

	for(unsigned int i = 0; i < hHooks.size(); ++i)
		UnhookEvent(hHooks[i]);
	hHooks.clear();

	return 0;
}


static INT_PTR ProtoAck(WPARAM wParam, LPARAM lParam)
{
	ACKDATA *ack = (ACKDATA*)lParam;

	if (ack->type == ACKTYPE_STATUS) 
	{
		if (hwnd_frame != NULL)
			PostMessage(hwnd_frame, WMU_REFRESH, 0, 0);
	}

	return 0;
}


HANDLE ConvertMetacontact(HANDLE hContact)
{
	if (ServiceExists(MS_MC_GETMOSTONLINECONTACT))
	{
		HANDLE hTmp = (HANDLE) CallService(MS_MC_GETMOSTONLINECONTACT, (WPARAM) hContact, NULL);
		if (hTmp != NULL)
			return hTmp;
	}
	return hContact;
}


VoiceProvider * FindModule(const char *szModule) 
{
	for(int i = 0; i < modules.getCount(); i++)
		if (strcmp(modules[i].name, szModule) == 0)
			return &modules[i];

	return NULL;
}


static bool IsCall(VoiceCall *call, const char *szModule, const char *id)
{
	return strcmp(call->module->name, szModule) == 0 
			&& call->id != NULL && strcmp(call->id, id) == 0;
}


VoiceCall * FindVoiceCall(const char *szModule, const char *id, bool add)
{
	for(int i = 0; i < calls.getCount(); i++)
	{
		if (IsCall(&calls[i], szModule, id))
		{
			return &calls[i];
		}
	}

	if (add)
	{
		VoiceProvider *module = FindModule(szModule);
		if (module == NULL)
			return NULL;

		VoiceCall *tmp = new VoiceCall(module, id);
		calls.insert(tmp);
		return tmp;
	}

	return NULL;
}


VoiceCall * FindVoiceCall(HANDLE hContact)
{
	for(int i = 0; i < calls.getCount(); i++)
	{
		if (calls[i].state != VOICE_STATE_ENDED && calls[i].hContact == hContact)
		{
			return &calls[i];
		}
	}

	return NULL;
}


static void RemoveVoiceCall(const char *szModule, const char *id)
{
	for(int i = calls.getCount() - 1; i >= 0; --i)
	{
		if (IsCall(&calls[i], szModule, id))
			calls.remove(i);
	}
}


static VOID CALLBACK ClearOldVoiceCalls(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	DWORD now = GetTickCount();
	BOOL refresh = FALSE;
	for(int i = calls.getCount() - 1; i >= 0; --i)
	{
		VoiceCall *call = &calls[i];

		if (call->state == VOICE_STATE_ENDED && call->end_time + TIME_TO_SHOW_ENDED_CALL < now)
		{
			calls.remove(i);
			refresh = TRUE;
		}
	}

	if (refresh && hwnd_frame != NULL)
		PostMessage(hwnd_frame, WMU_REFRESH, 0, 0);
}



static bool IsProtocol(const char *module)
{
	PROTOACCOUNT **protos;
	int count;

	BOOL hasAccounts = ServiceExists(MS_PROTO_ENUMACCOUNTS);

	if (hasAccounts)
		CallService(MS_PROTO_ENUMACCOUNTS, (WPARAM)&count, (LPARAM)&protos);
	else
		CallService(MS_PROTO_ENUMPROTOCOLS, (WPARAM)&count, (LPARAM)&protos);
	
	for (int i = 0; i < count; i++)
	{
		if (protos[i]->type != PROTOTYPE_PROTOCOL)
			continue;

		if (protos[i]->szModuleName == NULL || protos[i]->szModuleName[0] == '\0')
			continue;

		if (strcmp(module, protos[i]->szModuleName) == 0)
			return true;
	}

	return false;
}


static INT_PTR VoiceRegister(WPARAM wParam, LPARAM lParam)
{
	VOICE_MODULE *in = (VOICE_MODULE *) wParam;
	if (in == NULL || in->cbSize < sizeof(VOICE_MODULE) || in->name == NULL || in->description == NULL)
		return -1;

	if (FindModule(in->name) != NULL)
		return -2;

	if (!ProtoServiceExists(in->name, PS_VOICE_CALL)
			|| !ProtoServiceExists(in->name, PS_VOICE_ANSWERCALL)
			|| !ProtoServiceExists(in->name, PS_VOICE_DROPCALL))
		return -3;

	modules.insert(new VoiceProvider(in->name, in->description, in->flags, in->icon));

	if (hwnd_frame != NULL)
		PostMessage(hwnd_frame, WMU_REFRESH, 0, 0);

	return 0;
}


static INT_PTR VoiceUnregister(WPARAM wParam, LPARAM lParam)
{
	char *moduleName = (char *) wParam;
	if (moduleName == NULL || moduleName[0] == 0)
		return -1;

	VoiceProvider *module = FindModule(moduleName);
	if (module == NULL)
		return -2;

	for(int i = calls.getCount() - 1; i >= 0; --i)
	{
		VoiceCall *call = &calls[i];

		if (call->module == module)
		{
			call->Drop();
			call->SetState(VOICE_STATE_ENDED);

			calls.remove(i);
		}
	}

	modules.remove(module);

	if (hwnd_frame != NULL)
		PostMessage(hwnd_frame, WMU_REFRESH, 0, 0);

	return 0;
}


bool CanCall(HANDLE hContact, BOOL now)
{
	for(int i = 0; i < modules.getCount(); i++)
	{
		if (modules[i].CanCall(hContact, now))
			return true;
	}

	return false;
}


bool CanCall(const TCHAR *number)
{
	for(int i = 0; i < modules.getCount(); i++)
	{
		if (modules[i].CanCall(number))
			return true;
	}

	return false;
}


bool CanCallNumber()
{
	for(int i = 0; i < modules.getCount(); i++)
	{
		if (modules[i].flags & VOICE_CAPS_CALL_STRING)
			return true;
	}

	return false;
}


bool IsFinalState(int state)
{
	return state == VOICE_STATE_ENDED || state == VOICE_STATE_BUSY;
}


VoiceCall * GetTalkingCall()
{
	for(int i = 0; i < calls.getCount(); ++i)
	{
		VoiceCall *call = &calls[i];

		if (call->state == VOICE_STATE_TALKING)
			return call;
	}

	return NULL;
}


static void HoldOtherCalls(VoiceCall *call)
{
	for(int i = 0; i < calls.getCount(); ++i)
	{
		VoiceCall *other = &calls[i];

		if (other == call || other->state != VOICE_STATE_TALKING)
			continue;

		if (other->CanHold())
			other->Hold();
		else
			other->Drop();
	}
}


void Answer(VoiceCall *call)
{
	if (!call->CanAnswer())
		return;

	HoldOtherCalls(call);

	// Now annswer it
	call->Answer();
}


static INT_PTR VoiceState(WPARAM wParam, LPARAM lParam)
{
	VOICE_CALL *in = (VOICE_CALL *) wParam;
	if (in == NULL || in->cbSize < sizeof(VOICE_CALL) || in->moduleName == NULL || in->id == NULL)
		return 0;

	// Check if the call is aready in list
	VoiceCall *call = FindVoiceCall(in->moduleName, in->id, !IsFinalState(in->state));
	if (call == NULL)
		return 0;

	call->AppendCallerID(in->hContact, 
		(in->flags & VOICE_UNICODE) ? WcharToTchar(in->pwszName).get() : CharToTchar(in->pszName).get(),
		(in->flags & VOICE_UNICODE) ? WcharToTchar(in->pwszNumber).get() : CharToTchar(in->pszNumber).get());
	call->secure = (in->flags & VOICE_SECURE) != 0;

	if (in->state == VOICE_STATE_RINGING && call->hContact != NULL)
	{
		int aut = DBGetContactSettingWord(call->hContact, MODULE_NAME, "AutoAccept", AUTO_NOTHING);
		if (aut == AUTO_ACCEPT || aut == AUTO_DROP)
		{
			call->state = VOICE_STATE_RINGING;
			call->Notify(true, false, false, false);

			if (aut == AUTO_ACCEPT)
				Answer(call);
			else
				call->Drop();

			return 0;
		}
	}

	if (in->state == VOICE_STATE_TALKING)
		HoldOtherCalls(call);

	call->SetState(in->state);

	return 0;
}


static INT_PTR IconsChanged(WPARAM wParam, LPARAM lParam)
{
	if (hwnd_frame != NULL)
		PostMessage(hwnd_frame, WMU_REFRESH, 0, 0);

	return 0;
}


static INT_PTR ReloadFont(WPARAM wParam, LPARAM lParam) 
{
	FontIDT fi = {0};
	fi.cbSize = sizeof(fi);
	lstrcpyn(fi.group, TranslateT("Voice Calls"), MAX_REGS(fi.group));

	font_max_height = 0;
	for (int i = 0; i < NUM_FONTS; i++)
	{
		if (fonts[i] != 0) DeleteObject(fonts[i]);

		lstrcpyn(fi.name, stateNames[i], MAX_REGS(fi.name));

		LOGFONT log_font = {0};
		font_colors[i] = CallService(MS_FONT_GETT, (WPARAM) &fi, (LPARAM) &log_font);
		fonts[i] = CreateFontIndirect(&log_font);

		font_max_height = max(font_max_height, log_font.lfHeight);
	}
	
	if (hwnd_frame != NULL)
		PostMessage(hwnd_frame, WMU_REFRESH, 0, 0);

	return 0;
}


static INT_PTR ReloadColor(WPARAM wParam, LPARAM lParam) 
{
	ColourIDT ci = {0};
	ci.cbSize = sizeof(ci);
	lstrcpyn(ci.group, TranslateT("Voice Calls"), MAX_REGS(ci.group));
	lstrcpyn(ci.name, TranslateT("Background"), MAX_REGS(ci.name));

	bkg_color = CallService(MS_COLOUR_GETT, (WPARAM) &ci, 0);

	if (bk_brush != NULL)
		DeleteObject(bk_brush);
	bk_brush = CreateSolidBrush(bkg_color);

	if (hwnd_frame != NULL)
		InvalidateRect(hwnd_frame, NULL, TRUE);
	
	return 0;
}


static INT_PTR CListDblClick(WPARAM wParam, LPARAM lParam) 
{
	CLISTEVENT *ce = (CLISTEVENT *) lParam;
	
	VoiceCall *call = (VoiceCall *) ce->lParam;

	HWND hwnd = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_NEW_CALL), NULL, DlgProcNewCall, (LPARAM) call);

 	ShowWindow(hwnd, SW_SHOWNORMAL);

	call->SetNewCallHWND(hwnd);

	return 0;
}


static INT_PTR Service_CanCall(WPARAM wParam, LPARAM lParam) 
{
	HANDLE hContact = (HANDLE) wParam;
	if (hContact == NULL)
		return -1;

	hContact = ConvertMetacontact(hContact);

	return CanCall(hContact) ? 1 : 0;
}


static INT_PTR Service_Call(WPARAM wParam, LPARAM lParam) 
{
	HANDLE hContact = (HANDLE) wParam;
	if (hContact == NULL)
		return -1;

	hContact = ConvertMetacontact(hContact);

	OBJLIST<CallingMethod> methods(10, sttCompareCallingMethods);
	BuildCallingMethodsList(&methods, hContact);

	if (methods.getCount() < 1)
		return -2;

	CallingMethod *method = &methods[0];
	if (!IsEmpty(method->number))
		return -2;

	method->Call();

	return 0;
}


static INT_PTR Service_CallItem(WPARAM wParam, LPARAM lParam, LPARAM param)
{
	HANDLE hContact = (HANDLE) wParam;
	int index = (int) param;

	if (hContact == NULL)
		return -1;

	hContact = ConvertMetacontact(hContact);

	OBJLIST<CallingMethod> methods(10, sttCompareCallingMethods);
	BuildCallingMethodsList(&methods, hContact);

	if (index < 0 || index >= methods.getCount())
		return -2;

	methods[index].Call();

	return 0;
}


static INT_PTR CMAnswer(WPARAM wParam, LPARAM lParam) 
{
	HANDLE hContact = (HANDLE) wParam;
	if (hContact == NULL)
		return -1;

	hContact = ConvertMetacontact(hContact);

	VoiceCall *call = FindVoiceCall(hContact);
	if (call != NULL)
		Answer(call);
	
	return 0;
}


static INT_PTR CMHold(WPARAM wParam, LPARAM lParam) 
{
	HANDLE hContact = (HANDLE) wParam;
	if (hContact == NULL)
		return -1;

	hContact = ConvertMetacontact(hContact);

	VoiceCall *call = FindVoiceCall(hContact);
	if (call != NULL)
		call->Hold();

	return 0;
}


static INT_PTR CMDrop(WPARAM wParam, LPARAM lParam) 
{
	HANDLE hContact = (HANDLE) wParam;
	if (hContact == NULL)
		return -1;

	hContact = ConvertMetacontact(hContact);

	VoiceCall *call = FindVoiceCall(hContact);
	if (call != NULL)
		call->Drop();

	return 0;
}


static void HideMenuItem(HANDLE hMenu)
{
	CLISTMENUITEM mi = {0};
	mi.cbSize = sizeof(mi);
	mi.flags = CMIM_FLAGS | CMIF_HIDDEN | CMIF_TCHAR;

	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenu, (LPARAM) &mi);
}


static void ShowMenuItem(HANDLE hMenu)
{
	CLISTMENUITEM mi = {0};
	mi.cbSize = sizeof(mi);
	mi.flags = CMIM_FLAGS | CMIF_TCHAR;

	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenu, (LPARAM) &mi);
}


static INT_PTR PreBuildContactMenu(WPARAM wParam, LPARAM lParam) 
{
	HideMenuItem(hCMCall);
	HideMenuItem(hCMAnswer);
	HideMenuItem(hCMHold);
	HideMenuItem(hCMDrop);
	for(unsigned int i = 0; i < hCMCalls.size(); ++i)
		HideMenuItem(hCMCalls[i]);

	HANDLE hContact = (HANDLE) wParam;
	if (hContact == NULL)
		return -1;

	hContact = ConvertMetacontact(hContact);

	// There is a current call already?
	VoiceCall *call = FindVoiceCall(hContact);
	if (call == NULL)
	{
		OBJLIST<CallingMethod> methods(10, sttCompareCallingMethods);
		BuildCallingMethodsList(&methods, hContact);

		if (methods.getCount() == 1)
		{
			CallingMethod *method = &methods[0];

			TCHAR name[128];
			if (!IsEmpty(method->number))
				mir_sntprintf(name, MAX_REGS(name), TranslateT("Call %s with %s"), 
													method->number, method->provider->description);
			else
				mir_sntprintf(name, MAX_REGS(name), TranslateT("Call with %s"), 
													method->provider->description);

			CLISTMENUITEM mi = {0};
			mi.cbSize = sizeof(mi);
			mi.flags = CMIM_FLAGS | CMIF_TCHAR | CMIM_NAME;
			mi.ptszName = name;
			CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hCMCall, (LPARAM) &mi);
		}
		else if (methods.getCount() > 1)
		{
			CLISTMENUITEM mi = {0};
			mi.cbSize = sizeof(mi);
			mi.flags = CMIM_FLAGS | CMIF_TCHAR | CMIM_NAME;
			mi.ptszName = _T("Call");
			CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hCMCall, (LPARAM) &mi);

			for(int i = 0; i < methods.getCount(); ++i)
			{
				CallingMethod *method = &methods[i];

				HICON hIcon = method->provider->GetIcon();

				TCHAR name[128];
				if (!IsEmpty(method->number))
					mir_sntprintf(name, MAX_REGS(name), TranslateT("%s with %s"), 
														method->number, method->provider->description);
				else
					mir_sntprintf(name, MAX_REGS(name), TranslateT("with %s"), 
														method->provider->description);

				char service[128];
				mir_snprintf(service, MAX_REGS(service), "VoiceService/ContactMenu/Call_%d", i);

				CLISTMENUITEM mi = {0};
				mi.cbSize = sizeof(mi);
				mi.position = i;
				mi.flags = CMIF_TCHAR | CMIF_ROOTHANDLE;
				mi.ptszName = name;
				mi.hIcon = hIcon;
				mi.pszService = service;
				mi.hParentMenu = (HGENMENU) hCMCall;

				if (i == hCMCalls.size())
				{
					CreateServiceFunctionParam(service, Service_CallItem, i);

					HANDLE hMenu = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) &mi);

					hCMCalls.push_back(hMenu);
				}
				else
				{
					HANDLE hMenu = hCMCalls[i];

					mi.flags = mi.flags | CMIM_ALL;

					CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMenu, (LPARAM) &mi);
				}

				method->provider->ReleaseIcon(hIcon);
			}
		}
	}
	else
	{		
		switch (call->state)
		{
			case VOICE_STATE_CALLING:
			{
				ShowMenuItem(hCMDrop);
				break;
			}
			case VOICE_STATE_TALKING:
			{
				if (call->module->CanHold())
					ShowMenuItem(hCMHold);
				ShowMenuItem(hCMDrop);
				break;
			}
			case VOICE_STATE_RINGING:
			case VOICE_STATE_ON_HOLD:
			{
				ShowMenuItem(hCMAnswer);
				ShowMenuItem(hCMDrop);
				break;
			}
		}
	}

	return 0;
}


static BOOL CALLBACK DlgProcNewCall(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			VoiceCall *call = (VoiceCall *) lParam;

			TranslateDialogDefault(hwndDlg);

			TCHAR text[1024];

			VoiceCall *currentCall = GetTalkingCall();
			if (currentCall == NULL)
			{
				mir_sntprintf(text, MAX_REGS(text), TranslateT("%s wants to start a voice call with you. What you want to do?"),
					call->displayName);
			}
			else if (currentCall->CanHold())
			{
				mir_sntprintf(text, MAX_REGS(text), TranslateT("%s wants to start a voice call with you. What you want to do?\n\nIf you answer the call, the current call will be put on hold."),
					call->displayName);
			}
			else
			{
				mir_sntprintf(text, MAX_REGS(text), TranslateT("%s wants to start a voice call with you. What you want to do?\n\nIf you answer the call, the current call will be dropped."),
					call->displayName);
			}

			SendMessage(GetDlgItem(hwndDlg, IDC_TEXT), WM_SETTEXT, 0, (LPARAM) text);

			HICON hIcon = IcoLib_LoadIcon(stateIcons[VOICE_STATE_RINGING]);
			SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM) hIcon);
			IcoLib_ReleaseIcon(hIcon);

			if (call->hContact == NULL)
				ShowWindow(GetDlgItem(hwndDlg, IDC_AUTO), SW_HIDE);

			SetWindowLong(hwndDlg, GWL_USERDATA, (LONG) call);

			return TRUE;
		}

		case WM_COMMAND:
		{
			switch(wParam)
			{
				case ID_ANSWER:
				{
					VoiceCall *call = (VoiceCall *) GetWindowLong(hwndDlg, GWL_USERDATA);

					if (call->hContact != NULL && IsDlgButtonChecked(hwndDlg, IDC_AUTO))
						DBWriteContactSettingWord(call->hContact, MODULE_NAME, "AutoAccept", AUTO_ACCEPT);

					Answer(call);

					DestroyWindow(hwndDlg);
					break;
				}
				case ID_DROP:
				{
					VoiceCall *call = (VoiceCall *) GetWindowLong(hwndDlg, GWL_USERDATA);

					if (call->hContact != NULL && IsDlgButtonChecked(hwndDlg, IDC_AUTO))
						DBWriteContactSettingWord(call->hContact, MODULE_NAME, "AutoAccept", AUTO_DROP);

					call->Drop();

					DestroyWindow(hwndDlg);
					break;
				}
			}
			break;
		}

		case WM_CLOSE:
		{
			VoiceCall *call = (VoiceCall *) GetWindowLong(hwndDlg, GWL_USERDATA);
			call->Notify(false, false, false, true);

			DestroyWindow(hwndDlg);
			break;
		}

		case WM_DESTROY:
		{
			VoiceCall *call = (VoiceCall *) GetWindowLong(hwndDlg, GWL_USERDATA);
			call->SetNewCallHWND(NULL);
			break;
		}
	}
	
	return FALSE;
}


/////////////////////////////////////////////////////////////////////////////////////////


VoiceProvider::VoiceProvider(const char *name, const TCHAR *description, int flags, const char *icon)
{
	strncpy(this->name, name, MAX_REGS(this->name));
	this->name[MAX_REGS(this->name)-1] = 0;

	lstrcpyn(this->description, description, MAX_REGS(this->description));

	if (icon == NULL)
		this->icon[0] = 0;
	else
		lstrcpynA(this->icon, icon, MAX_REGS(this->icon));

	this->flags = flags;
	is_protocol = IsProtocol(name);
	canHold = (ProtoServiceExists(name, PS_VOICE_HOLDCALL) != 0);

	char str[MAXMODULELABELLENGTH];
	mir_snprintf(str, MAX_REGS(str), "%s%s", name, PE_VOICE_CALL_STATE);
	state_hook = HookEvent(str, VoiceState);
}


VoiceProvider::~VoiceProvider()
{
	UnhookEvent(state_hook);
	state_hook = NULL;
}


bool VoiceProvider::CanCall(HANDLE hContact, BOOL now)
{
	if (hContact == NULL)
		return false;

	if ((flags & VOICE_CAPS_CALL_CONTACT) == 0)
		return false;

	if (ProtoServiceExists(name, PS_VOICE_CALL_CONTACT_VALID))
		return CallProtoService(name, PS_VOICE_CALL_CONTACT_VALID, (WPARAM) hContact, now) != FALSE;

	if (is_protocol)
	{
		if (now && CallProtoService(name, PS_GETSTATUS, 0, 0) <= ID_STATUS_OFFLINE)
			return false;

		if (!CallService(MS_PROTO_ISPROTOONCONTACT, (WPARAM) hContact, (LPARAM) name))
			return false;

		return DBGetContactSettingWord(hContact, name, "Status", ID_STATUS_OFFLINE) > ID_STATUS_OFFLINE;
	}

	return true;
}

bool VoiceProvider::CanCall(const TCHAR *number)
{
	if (number == NULL || number[0] == 0)
		return false;

	if ((flags & VOICE_CAPS_CALL_STRING) == 0)
		return false;

	if (ProtoServiceExists(name, PS_VOICE_CALL_STRING_VALID))
		return CallProtoService(name, PS_VOICE_CALL_STRING_VALID, (WPARAM) number, 0) != FALSE;

	if (is_protocol)
	{
		return CallProtoService(name, PS_GETSTATUS, 0, 0) > ID_STATUS_OFFLINE;
	}

	return true;
}

bool VoiceProvider::CanHold()
{
	return canHold;
}

bool VoiceProvider::CanSendDTMF()
{
	return ProtoServiceExists(name, PS_VOICE_SEND_DTMF) != FALSE;
}

void VoiceProvider::Call(HANDLE hContact, const TCHAR *number)
{
	CallProtoService(name, PS_VOICE_CALL, (WPARAM) hContact, (LPARAM) number);
}

HICON VoiceProvider::GetIcon()
{
	if (!IsEmptyA(icon))
	{
		return IcoLib_LoadIcon(icon);
	}
	else if (is_protocol)
	{
		return LoadSkinnedProtoIcon(name, ID_STATUS_ONLINE);
	}
	else
	{
		return NULL;
	}
}

void VoiceProvider::ReleaseIcon(HICON hIcon)
{
	if (hIcon == NULL)
		return;

	if (!IsEmptyA(icon))
		IcoLib_ReleaseIcon(hIcon);
}




VoiceCall::VoiceCall(VoiceProvider *module, const char *id)
	: module(module), id(mir_strdup(id))
{
	hContact = NULL;
	name[0] = 0;
	number[0] = 0;
	displayName[0] = 0;
	state = -1;
	end_time = 0;
	clistBlinking = false;
	incoming = false;
	secure = false;
	hwnd = NULL;
	CreateDisplayName();
}

VoiceCall::~VoiceCall()
{
	RemoveNotifications();
	mir_free(id);
	id = NULL;
}

void VoiceCall::AppendCallerID(HANDLE aHContact, const TCHAR *aName, const TCHAR *aNumber)
{
	bool changed = false;

	if (aHContact != NULL)
	{
		hContact = aHContact;
		changed = true;
	}

	if (!IsEmpty(aName))
	{
		lstrcpyn(name, aName, MAX_REGS(name));
		changed = true;
	}

	if (!IsEmpty(aNumber))
	{
		lstrcpyn(number, aNumber, MAX_REGS(number));
		changed = true;
	}

	if (changed)
		CreateDisplayName();
}

void VoiceCall::CreateDisplayName()
{
	TCHAR *contact = NULL;
	if (hContact != NULL)
		contact = (TCHAR *) CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM) hContact, GCDNF_TCHAR);

	TCHAR *nameTmp = NULL;
	if (lstrcmp(name, number) != 0)
		nameTmp = name;

	if (contact != NULL)
	{
		if (!IsEmpty(number))
			mir_sntprintf(displayName, MAX_REGS(displayName), _T("%s <%s>"), contact, number);
		else 
			lstrcpyn(displayName, contact, MAX_REGS(displayName));
	}
	else if (!IsEmpty(nameTmp) && !IsEmpty(number))
	{
		mir_sntprintf(displayName, MAX_REGS(displayName), _T("%s <%s>"), name, number);
	}
	else if (!IsEmpty(nameTmp))
	{
		lstrcpyn(displayName, name, MAX_REGS(displayName));
	}
	else if (!IsEmpty(number))
	{
		lstrcpyn(displayName, number, MAX_REGS(displayName));
	}
	else
	{
		lstrcpyn(displayName, TranslateT("Unknown number"), MAX_REGS(displayName));
	}
}

void VoiceCall::RemoveNotifications()
{
	if (hwnd != NULL)
	{
		DestroyWindow(hwnd);
		hwnd = NULL;
	}

	if (clistBlinking)
	{
		CallService(MS_CLIST_REMOVEEVENT, (WPARAM) hContact, (LPARAM) this);
		clistBlinking = false;
	}
}

void VoiceCall::SetState(int aState)
{
	if (state == aState)
		return;

	if (aState == VOICE_STATE_RINGING)
		incoming = true;
	else if (aState == VOICE_STATE_CALLING)
		incoming = false;

	RemoveNotifications();

	state = aState;

	if (IsFinished())
	{
		if (end_time == 0)
			end_time = GetTickCount();

		// Remove id because providers can re-use them
		mir_free(id);
		id = NULL;
	}

	Notify();
}


void VoiceCall::Notify(bool history, bool popup, bool sound, bool clist)
{
	if (history)
	{
		TCHAR *variables[] = {
			_T("number"), displayName
		};
		HistoryEvents_AddToHistoryVars(hContact, EVENTTYPE_VOICE_CALL, state, variables, MAX_REGS(variables),
			DBEF_READ | (incoming ? 0 : DBEF_SENT));
	}

	if (popup)
	{
		TCHAR text[512];
		mir_sntprintf(text, MAX_REGS(text), TranslateTS(stateTexts[state]), displayName);

		ShowPopup(NULL, TranslateTS(popupTitles[state]), text);
	}

	if (sound)
		SkinPlaySound(sounds[state].name);

	if (clist && state == VOICE_STATE_RINGING)
	{
		HICON hIcon = IcoLib_LoadIcon(stateIcons[VOICE_STATE_RINGING]);

		CLISTEVENT ce = {0};
		ce.cbSize = sizeof(ce);
		ce.hContact = hContact;
		ce.hIcon = hIcon;
		ce.hDbEvent = (HANDLE) this;
		ce.pszService = MS_VOICESERVICE_CLIST_DBLCLK;
		ce.lParam = (LPARAM) this;
		CallService(MS_CLIST_ADDEVENT, 0, (LPARAM) &ce);

		IcoLib_ReleaseIcon(hIcon);

		clistBlinking = true;
	}
	
	if (hwnd_frame != NULL)
		PostMessage(hwnd_frame, WMU_REFRESH, 0, 0);
}


bool VoiceCall::IsFinished()
{
	return IsFinalState(state);
}


bool VoiceCall::CanDrop()
{
	return !IsFinished();
}

void VoiceCall::Drop()
{
	if (!CanDrop())
		return;

	RemoveNotifications();

	CallProtoService(module->name, PS_VOICE_DROPCALL, (WPARAM) id, 0);
}


bool VoiceCall::CanAnswer()
{
	return state == -1 || state == VOICE_STATE_RINGING || state == VOICE_STATE_ON_HOLD;
}

void VoiceCall::Answer()
{
	if (!CanAnswer())
		return;

	RemoveNotifications();

	CallProtoService(module->name, PS_VOICE_ANSWERCALL, (WPARAM) id, 0);
}


bool VoiceCall::CanHold()
{
	return module->CanHold() && (state == -1 || state == VOICE_STATE_TALKING);
}

void VoiceCall::Hold()
{
	if (!CanHold())
		return;

	RemoveNotifications();

	CallProtoService(module->name, PS_VOICE_HOLDCALL, (WPARAM) id, 0);
}

bool VoiceCall::CanSendDTMF()
{
	return module->CanSendDTMF() && state == VOICE_STATE_TALKING;
}

void VoiceCall::SendDTMF(TCHAR c)
{
	if (!CanSendDTMF())
		return;

	CallProtoService(module->name, PS_VOICE_SEND_DTMF, (WPARAM) id, (LPARAM) c);
}



void VoiceCall::SetNewCallHWND(HWND hwnd)
{
	if (hwnd != NULL)
		RemoveNotifications();

	this->hwnd = hwnd;
}