/*
 * This code implements POP3 server checking for new mail and so on.
 * There's function SynchroPOP3 in this file- for checking and synchronising POP3 account
 * and DeleteMailsPOP3- for deleting mails from POP3 server
 *
 * Note this file acts as main file for internal plugin.
 *
 * (c) majvan 2002-2004
 * 18/08
 */


#pragma warning( disable : 4290 )
#include "../../main.h"
#include "../../yamn.h"
#include "pop3.h"
#include "pop3comm.h"		//all we need for POP3 account (POP3 account= YAMN account + some more POP3 specified members)

#define ERRORSTR_MAXLEN	1024	//in wide-chars

//- imported ---------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

//From pop3opt.cpp
extern int POP3OptInit(WPARAM wParam,LPARAM lParam);
//From netlib.cpp
extern HANDLE RegisterNLClient(const char *name);
//From ssl.cpp
extern int RegisterSSL();
//this is imported because of one bug, should not be imported normally (this POP3 is plugin of YAMN)
extern int FilterMailSvc(WPARAM,LPARAM);

extern char *ProtoName;
extern int YAMN_STATUS;
extern PLUGININFO pluginInfo;

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

HANDLE hNetLib=NULL;
PSCOUNTER CPOP3Account::AccountWriterSO;

//Creates new CPOP3Account structure
HACCOUNT WINAPI CreatePOP3Account(HYAMNPROTOPLUGIN Plugin,DWORD CAccountVersion);

//Deletes CPOP3Account structure
void WINAPI DeletePOP3Account(HACCOUNT Which);

//Sets stop flag to account
void WINAPI StopPOP3Account(HACCOUNT Which);

//Function registers standard functions for YAMN
int RegisterPOP3Plugin(WPARAM,LPARAM);

//This should be normally exporeted fcn UninstallEx from plugin DLL, but YAMN has already exported one, so this is the same called from YAMN UninstallEx
//This is used by PluginUninstall
int UninstallPOP3(PLUGINUNINSTALLPARAMS* ppup);

//Unloads all variables created on heap (delete[])
DWORD WINAPI UnLoadPOP3(void *);

//Function writes POP3 accounts using YAMN exported functions
DWORD WINAPI WritePOP3Accounts();

//Function stores plugin's data for account to file
DWORD WINAPI WritePOP3Options(HANDLE,HACCOUNT);

//Function reads plugin's data for account from file
DWORD WINAPI ReadPOP3Options(HACCOUNT,TCHAR **,TCHAR *);

//Creates new mail for an account
HYAMNMAIL WINAPI CreatePOP3Mail(HACCOUNT Account,DWORD CMimeMailVersion);

//Function does all needed work when connection failed or any error occured
//Creates structure containing error code, closes internet session, runs "bad connect" function
static void PostErrorProc(HPOP3ACCOUNT ActualAccount,void *ParamToBadConnect,DWORD POP3PluginParam,BOOL UseSSL);

//Checks POP3 account and stores all info to account. It deletes old mails=> synchro
// WhichTemp- pointer to strucure containing needed information
DWORD WINAPI SynchroPOP3(struct CheckParam *WhichTemp);

//Deletes mails from POP3 server
// WhichTemp- structure containing needed information (queued messages to delete)
//Function deletes from memory queue in WhichTemp structure
DWORD WINAPI DeleteMailsPOP3(struct DeleteParam *WhichTemp);

//Function makes readable message about error. It sends it back to YAMN, so YAMN then
//can show it to the message window
WCHAR* WINAPI GetErrorString(DWORD Code);

//Function deletes string allocated in GetErrorString
void WINAPI DeleteErrorString(LPVOID String);

//Extracts info from result of POP3's STAT command
// stream- source string
// len- length of source string
// mboxsize- adreess to integer, that receives size of mailbox
// mails- adreess to integer, that receives number of mails
void ExtractStat(char *stream,int len,int *mboxsize,int *mails);

//Extracts mail ID on mailbox
// stream- source string
// len- length of source string
// queue- address of first message, where first ID will be stored
void ExtractUIDL(char *stream,int len,HYAMNMAIL queue);

//Extracts mail size on mailbox
// stream- source string
// len- length of source string
// queue- address of first message, where size of message #1 will be stored
void ExtractList(char *stream,int len,HYAMNMAIL queue);

//  Loading Icon and checking for icolib 
void LoadIcons();

struct YAMNExportedFcns *pYAMNFcn;
struct MailExportedFcns *pYAMNMailFcn;

YAMN_PROTOIMPORTFCN POP3ProtocolFunctions=
{
	CreatePOP3Account,
	DeletePOP3Account,
	StopPOP3Account,
	WritePOP3Options,
	ReadPOP3Options,
	SynchroPOP3,
	SynchroPOP3,
	SynchroPOP3,
	DeleteMailsPOP3,
	GetErrorString,
	NULL,
	DeleteErrorString,
	WritePOP3Accounts,
	NULL,
	UnLoadPOP3,
};

YAMN_MAILIMPORTFCN POP3MailFunctions=
{
	CreatePOP3Mail,
	NULL,
	NULL,
	NULL,
};

PYAMN_VARIABLES pYAMNVar;
HYAMNPROTOPLUGIN POP3Plugin;

YAMN_PROTOREGISTRATION POP3ProtocolRegistration=
{
	"POP3 protocol (internal)",
	(char *)YAMN_VERSION,
	"� 2002-2004 majvan | 2005 tweety",
	"Mail notifier and browser for Miranda IM. Included POP3 protocol.",
	"francois.mean@skynet.be",
	"http://forums.miranda-im.org/showthread.php?t=3035",
};

WCHAR *FileName=NULL;

// External icon var for icolib support
extern HICON hYamnIcon;
extern HICON hNeutralIcon;
extern HICON hNewMailIcon;
extern HICON hConnectFailIcon;
extern HICON hTopToolBarUp;
extern HICON hTopToolBarDown;
extern YAMN_VARIABLES YAMNVar;
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

CPOP3Account::CPOP3Account()
{
//NOTE! This constructor constructs CAccount structure. If your plugin is not internal,
//you will need these constructors. All you need is in Account.cpp. Just copy to your source code
//constructor and destructor of CAccount.
	UseInternetFree=CreateEvent(NULL,FALSE,TRUE,NULL);
	InternetQueries=new SCOUNTER;
	AbilityFlags=YAMN_ACC_BROWSE | YAMN_ACC_POPUP;

	SetAccountStatus((HACCOUNT)this,Translate("Disconnected"));
}

CPOP3Account::~CPOP3Account()
{
	CloseHandle(UseInternetFree);
	if(InternetQueries!=NULL)
		delete InternetQueries;
}

HACCOUNT WINAPI CreatePOP3Account(HYAMNPROTOPLUGIN Plugin,DWORD CAccountVersion)
{
//First, we should check whether CAccountVersion matches.
//But this is internal plugin, so YAMN's CAccount structure and our CAccount structure are
//the same, so we do not need to test version. Otherwise, if CAccount version does not match
//in your plugin, you should return NULL, like this:
//	if(CAccountVersion!=YAMN_ACCOUNTVERSION) return NULL;

//Now it is needed to construct our POP3 account and return its handle
	return (HACCOUNT)new struct CPOP3Account();
//	return (HACCOUNT)new struct CPOP3Account();
}

void WINAPI DeletePOP3Account(HACCOUNT Which)
{
	delete (HPOP3ACCOUNT)Which;
}

void WINAPI StopPOP3Account(HACCOUNT Which)
{
	((HPOP3ACCOUNT)Which)->Client.Stopped=TRUE;
	if(((HPOP3ACCOUNT)Which)->Client.NetClient!=NULL)			//we should inform also network client. Usefull only when network client implements this feature
		((HPOP3ACCOUNT)Which)->Client.NetClient->Stopped=TRUE;
}


void LoadIcons()
{
	//Load icons

	if(ServiceExists(MS_SKIN2_ADDICON))
	{
		//MessageBox(NULL,"Icolib present","test",0);
		SKINICONDESC sid;
		char szFilename[MAX_PATH];
		strncpy(szFilename, "plugins\\YAMN.dll", MAX_PATH);

		sid.cbSize = sizeof(SKINICONDESC);
		sid.pszSection = "YAMN";
		sid.pszDefaultFile = szFilename;

		sid.pszName = "YAMN_Neutral";
        sid.pszDescription = Translate("Neutral");
        sid.iDefaultIndex = -IDI_ICONEUTRAL;
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
		
		sid.pszName = "YAMN_Yamn";
        sid.pszDescription = "YAMN";
        sid.iDefaultIndex = -IDI_ICOYAMN1;
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);

		sid.pszName = "YAMN_NewMail";
        sid.pszDescription = Translate("New Mail");
        sid.iDefaultIndex = -IDI_ICOYAMN2;
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);

		sid.pszName = "YAMN_ConnectFail";
        sid.pszDescription = Translate("Connect Fail");
        sid.iDefaultIndex = -IDI_ICOYAMN3;
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);

		sid.pszName = "YAMN_TopToolBarUp";
        sid.pszDescription = Translate("TopToolBar UP");
        sid.iDefaultIndex = -IDI_ICOTTBUP;
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);

		sid.pszName = "YAMN_TopToolBarDown";
        sid.pszDescription = Translate("TopToolBar Down");
        sid.iDefaultIndex = -IDI_ICOTTBDW;
		CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);

		hNeutralIcon = (HICON) CallService(MS_SKIN2_GETICON, 0, (LPARAM) "YAMN_Neutral");
		hYamnIcon = (HICON) CallService(MS_SKIN2_GETICON, 0, (LPARAM) "YAMN_Yamn");
		hNewMailIcon = (HICON) CallService(MS_SKIN2_GETICON, 0, (LPARAM) "YAMN_NewMail");
		hConnectFailIcon = (HICON) CallService(MS_SKIN2_GETICON, 0, (LPARAM) "YAMN_ConnectFail");
		hTopToolBarUp = (HICON) CallService(MS_SKIN2_GETICON, 0, (LPARAM) "YAMN_TopToolBarUp");
		hTopToolBarDown = (HICON) CallService(MS_SKIN2_GETICON, 0, (LPARAM) "YAMN_TopToolBarDown");

		//Icon to show in contact list
		DBVARIANT dbv;
		if(!DBGetContactSetting(NULL,"SkinIcons","YAMN_Neutral",&dbv)) 
		{
			DBWriteContactSettingString(NULL, "Icons", "YAMN40072", (char *)dbv.pszVal);			
			DBFreeVariant(&dbv);
		}
		else
			DBWriteContactSettingString(NULL,"Icons", "YAMN40072", "plugins\\YAMN.dll,-119");
	}
	else
	{
		hNeutralIcon = LoadIcon(YAMNVar.hInst,MAKEINTRESOURCE(IDI_ICONEUTRAL));
		hYamnIcon = LoadIcon(YAMNVar.hInst,MAKEINTRESOURCE(IDI_ICOYAMN1));
		hNewMailIcon = LoadIcon(YAMNVar.hInst,MAKEINTRESOURCE(IDI_ICOYAMN2));
		hConnectFailIcon = LoadIcon(YAMNVar.hInst,MAKEINTRESOURCE(IDI_ICOYAMN3));
		hTopToolBarUp = LoadIcon(YAMNVar.hInst,MAKEINTRESOURCE(IDI_ICOTTBUP));
		hTopToolBarDown = LoadIcon(YAMNVar.hInst,MAKEINTRESOURCE(IDI_ICOTTBDW));
		//Icon to show in contact list
		DBWriteContactSettingString(NULL, "Icons", "YAMN40072", "plugins\\YAMN.dll,-119");
	}

}

//This function is like main function for POP3 internal protocol
int RegisterPOP3Plugin(WPARAM,LPARAM)
{
	CLISTMENUITEM mi;

	//  Loading Icon and checking for icolib 
	LoadIcons();

	//Insert "Check mail (YAMN)" item to Miranda's menu
	ZeroMemory(&mi,sizeof(mi));
	mi.cbSize = sizeof(mi);
	mi.position = 0xb0000000;
	mi.flags = CMIM_ICON;
	mi.hIcon = hYamnIcon;
	mi.pszName = Translate("Check &mail (YAMN)");
	mi.pszService = MS_YAMN_FORCECHECK;
	CallService(MS_CLIST_ADDMAINMENUITEM,0,(LPARAM)&mi);

	mi.flags = mi.flags;
	mi.pszContactOwner = ProtoName;
	mi.pszService = MS_YAMN_CLISTCONTEXT;
	CallService(MS_CLIST_ADDCONTACTMENUITEM,0,(LPARAM)&mi);

	//Use for the Updater plugin
	if(ServiceExists(MS_UPDATE_REGISTER)) 
	{
		Update update = {0};
		char szVersion[16];
		char szUrl[250];

		update.szComponentName = pluginInfo.shortName;
		update.pbVersion = (BYTE *)CreateVersionStringPlugin(&pluginInfo, szVersion);
		update.cpbVersion = strlen((char *)update.pbVersion);

		#ifdef YAMN_9x
		update.szUpdateURL = "http://miranda-im.org/download/feed.php?dlfile=2166";
		update.szVersionURL = "http://www.miranda-im.org/download/details.php?action=viewfile&id=2166";
		update.pbVersionPrefix = (BYTE *)"<span class=\"fileNameHeader\">Updater ";
		#else
		update.szUpdateURL = "http://miranda-im.org/download/feed.php?dlfile=2165";
		update.szVersionURL = "http://www.miranda-im.org/download/details.php?action=viewfile&id=2165";
		update.pbVersionPrefix = (BYTE *)"<span class=\"fileNameHeader\">Updater ";
		#endif
		wsprintf(szUrl,"http://www.miranda-fr.net/tweety/yamn/%s.zip",YAMN_FILENAME);
	    update.szBetaUpdateURL = szUrl;
		update.szBetaVersionURL = "http://www.miranda-fr.net/tweety/yamn/yamn_beta.html";
		update.pbBetaVersionPrefix = (BYTE *)"YAMN version ";

		update.cpbVersionPrefix = strlen((char *)update.pbVersionPrefix);
		update.cpbBetaVersionPrefix = strlen((char *)update.pbBetaVersionPrefix);

		CallService(MS_UPDATE_REGISTER, 0, (WPARAM)&update);

	}
	

	//Get YAMN variables we can use
	if(NULL==(pYAMNVar=(PYAMN_VARIABLES)CallService(MS_YAMN_GETVARIABLES,(WPARAM)YAMN_VARIABLESVERSION,(LPARAM)0)))
		return 0;

	//We have to get pointers to YAMN exported functions: allocate structure and fill it
	if(NULL==(pYAMNFcn=new struct YAMNExportedFcns))
		return 0;

	//Register new pop3 user in netlib
	if(NULL==(hNetLib=RegisterNLClient("YAMN-POP3")))
	{
		delete pYAMNFcn;
		return 0;
	}
	RegisterSSL();

	pYAMNFcn->SetProtocolPluginFcnImportFcn=(YAMN_SETPROTOCOLPLUGINFCNIMPORTFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_SETPROTOCOLPLUGINFCNIMPORTID,(LPARAM)0);
	pYAMNFcn->WaitToWriteFcn=(YAMN_WAITTOWRITEFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_WAITTOWRITEID,(LPARAM)0);
	pYAMNFcn->WriteDoneFcn=(YAMN_WRITEDONEFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_WRITEDONEID,(LPARAM)0);
	pYAMNFcn->WaitToReadFcn=(YAMN_WAITTOREADFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_WAITTOREADID,(LPARAM)0);
	pYAMNFcn->ReadDoneFcn=(YAMN_READDONEFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_READDONEID,(LPARAM)0);
	pYAMNFcn->SCGetNumberFcn=(YAMN_SCMANAGEFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_SCGETNUMBERID,(LPARAM)0);
	pYAMNFcn->SCIncFcn=(YAMN_SCMANAGEFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_SCINCID,(LPARAM)0);
	pYAMNFcn->SCDecFcn=(YAMN_SCMANAGEFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_SCDECID,(LPARAM)0);
	pYAMNFcn->SetStatusFcn=(YAMN_SETSTATUSFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_SETSTATUSID,(LPARAM)0);
	pYAMNFcn->GetStatusFcn=(YAMN_GETSTATUSFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_GETSTATUSID,(LPARAM)0);

	if(NULL==(pYAMNMailFcn=new struct MailExportedFcns))
	{
		delete pYAMNFcn;
		return 0;
	}

	pYAMNMailFcn->SynchroMessagesFcn=(YAMN_SYNCHROMIMEMSGSFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_SYNCHROMIMEMSGSID,(LPARAM)0);
	pYAMNMailFcn->TranslateHeaderFcn=(YAMN_TRANSLATEHEADERFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_TRANSLATEHEADERID,(LPARAM)0);
	pYAMNMailFcn->AppendQueueFcn=(YAMN_APPENDQUEUEFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_APPENDQUEUEID,(LPARAM)0);
	pYAMNMailFcn->DeleteMessagesToEndFcn=(YAMN_DELETEMIMEQUEUEFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_DELETEMIMEQUEUEID,(LPARAM)0);
	pYAMNMailFcn->DeleteMessageFromQueueFcn=(YAMN_DELETEMIMEMESSAGEFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_DELETEMIMEMESSAGEID,(LPARAM)0);
	pYAMNMailFcn->FindMessageByIDFcn=(YAMN_FINDMIMEMESSAGEFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_FINDMIMEMESSAGEID,(LPARAM)0);
	pYAMNMailFcn->CreateNewDeleteQueueFcn=(YAMN_CREATENEWDELETEQUEUEFCN)CallService(MS_YAMN_GETFCNPTR,(WPARAM)YAMN_CREATENEWDELETEQUEUEID,(LPARAM)0);

	//set static variable
	if(CPOP3Account::AccountWriterSO==NULL)
		CPOP3Account::AccountWriterSO=new SCOUNTER;

	//First, we register this plugin
	//it is quite impossible this function returns zero (failure) as YAMN and internal plugin structre versions are the same
	if(NULL==(POP3Plugin=(HYAMNPROTOPLUGIN)CallService(MS_YAMN_REGISTERPROTOPLUGIN,(WPARAM)&POP3ProtocolRegistration,(LPARAM)YAMN_PROTOREGISTRATIONVERSION)))
		return 0;

	//Next we set our imported functions for YAMN
	if(!SetProtocolPluginFcnImport(POP3Plugin,&POP3ProtocolFunctions,YAMN_PROTOIMPORTFCNVERSION,&POP3MailFunctions,YAMN_MAILIMPORTFCNVERSION))
		return 0;

	//Then, we read all mails for accounts.
	//You must first register account, before using this function as YAMN must use CreatePOP3Account function to add new accounts
	//But if CreatePOP3Account is not implemented (equals to NULL), YAMN creates account as YAMN's standard HACCOUNT
	FileName=(WCHAR *)CallService(MS_YAMN_GETFILENAMEA,(WPARAM)"pop3",(LPARAM)0);

	switch(CallService(MS_YAMN_READACCOUNTSW,(WPARAM)POP3Plugin,(LPARAM)FileName))
	{
		case EACC_FILEVERSION:
			MessageBoxA(NULL,Translate("Found new version of account book, not compatible with this version of YAMN."),Translate(_T("YAMN (internal POP3) read error")),MB_OK);
			CallService(MS_YAMN_DELETEFILENAME,(WPARAM)FileName,(LPARAM)0);
			return 0;
		case EACC_FILECOMPATIBILITY:
			MessageBoxA(NULL,Translate("Error reading account file. Account file corrupted."),Translate("YAMN (internal POP3) read error"),MB_OK);
			CallService(MS_YAMN_DELETEFILENAME,(WPARAM)FileName,(LPARAM)0);
			return 0;
		case EACC_ALLOC:
			MessageBoxA(NULL,Translate("Memory allocation error while data reading"),Translate("YAMN (internal POP3) read error"),MB_OK);
			CallService(MS_YAMN_DELETEFILENAME,(WPARAM)FileName,(LPARAM)0);
			return 0;
		case EACC_SYSTEM:
			if(ERROR_FILE_NOT_FOUND!=GetLastError())
			{
				MessageBoxA(NULL,Translate("Reading file error. File already in use?"),Translate("YAMN (internal POP3) read error"),MB_OK);
				CallService(MS_YAMN_DELETEFILENAME,(WPARAM)FileName,(LPARAM)0);
				return 0;
			}
			break;
	}
	HookEvent(ME_OPT_INITIALISE,POP3OptInit);

	HACCOUNT Finder;
	HANDLE hContact;
	DBVARIANT dbv;
	char *szProto;

	for(Finder=POP3Plugin->FirstAccount;Finder!=NULL;Finder=Finder->Next)
	{
		Finder->hContact = NULL;
		hContact = (HANDLE)CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
		while(hContact) 
		{
			szProto = (char *)CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM)hContact, 0);
			if(szProto != NULL && strcmp(szProto, ProtoName)==0)
			{
				if(!DBGetContactSetting(hContact,ProtoName,"Id",&dbv)) 
				{
					
					if( strcmp((char*)dbv.pszVal, Finder->Name)==0)
					{
						Finder->hContact = hContact;
						DBWriteContactSettingWord(Finder->hContact, ProtoName, "Status", YAMN_STATUS);
						DBWriteContactSettingString(Finder->hContact, "CList", "StatusMsg", Translate("No new mail"));
						if((Finder->Flags & YAMN_ACC_ENA) && (Finder->NewMailN.Flags & YAMN_ACC_CONT))
						{
							DBDeleteContactSetting(Finder->hContact, "CList", "Hidden");
						}
						if(!(Finder->Flags & YAMN_ACC_ENA) || !(Finder->NewMailN.Flags & YAMN_ACC_CONT))
						{
							DBWriteContactSettingByte(Finder->hContact, "CList", "Hidden", 1);
						}
					}
					DBFreeVariant(&dbv);
				}

			}
			hContact = (HANDLE)CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM)hContact, 0);
		}
		if(Finder->hContact == NULL && (Finder->Flags & YAMN_ACC_ENA) && (Finder->NewMailN.Flags & YAMN_ACC_CONT))
		{
			//No account contact found, have to create one
			Finder->hContact =(HANDLE) CallService(MS_DB_CONTACT_ADD, 0, 0);
			CallService(MS_PROTO_ADDTOCONTACT,(WPARAM)Finder->hContact,(LPARAM)ProtoName);
			DBWriteContactSettingString(Finder->hContact,ProtoName,"Id",Finder->Name);
			DBWriteContactSettingString(Finder->hContact,ProtoName,"Nick",Finder->Name);
			DBWriteContactSettingString(Finder->hContact,"Protocol","p",ProtoName);
			DBWriteContactSettingWord(Finder->hContact, ProtoName, "Status", YAMN_STATUS);
		}

	}

	return 0;
}

int UninstallPOP3(PLUGINUNINSTALLPARAMS* ppup)		//Usually UninstallEx, but need different name, because it is registered yet in main.cpp
{
	if(ppup->bDoDeleteSettings)
	{
		char FileNameA[MAX_PATH+1];

		PUIRemoveDbModule("YAMN-POP3");

		if(FileName==NULL)
			MessageBox(NULL,"Cannot delete book file when YAMN is not loaded. Please do it manually.","YAMN-POP3 uninstalling",MB_OK|MB_ICONWARNING);
		else
		{
			if(!WideCharToMultiByte(CP_ACP,0,FileName,-1,FileNameA,MAX_PATH+1,NULL,NULL))
				MessageBox(NULL,"Cannot delete book file. Please do it manually.","YAMN-POP3 uninstalling",MB_OK|MB_ICONWARNING);
			else
				DeleteFile(FileNameA);
		}
	}
	return 0;
}

DWORD WINAPI UnLoadPOP3(void *)
{
	delete CPOP3Account::AccountWriterSO;	
	delete pYAMNMailFcn;
	delete pYAMNFcn;
	#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"UnLoadPOP3:done\n");
	#endif
	return 1;
}

DWORD WINAPI WritePOP3Accounts()
{
//	WCHAR *FileName=(WCHAR *)CallService(MS_YAMN_GETFILENAMEA,(WPARAM)"pop3",(LPARAM)0);
	DWORD ReturnValue=CallService(MS_YAMN_WRITEACCOUNTSW,(WPARAM)POP3Plugin,(LPARAM)FileName);
								
	switch(ReturnValue)
	{
		case EACC_SYSTEM:
			MessageBox(NULL,Translate("Error while copying data to disk occured. File in use?"),Translate("POP3 plugin- write file error"),MB_OK);
			break;
		case 0:
			break;
	}
//	CallService(MS_YAMN_DELETEFILENAME,(WPARAM)FileName,(LPARAM)0);
	return ReturnValue;
}

DWORD WINAPI WritePOP3Options(HANDLE File,HACCOUNT Which)
{
	DWORD WrittenBytes;
	DWORD Ver=POP3_FILEVERSION;

	if((!WriteFile(File,(char *)&Ver,sizeof(DWORD),&WrittenBytes,NULL)) ||
		(!WriteFile(File,(char *)&((HPOP3ACCOUNT)Which)->CP,sizeof(WORD),&WrittenBytes,NULL)))
		return EACC_SYSTEM;
	return 0;
}

DWORD WINAPI ReadPOP3Options(HACCOUNT Which,TCHAR **Parser,TCHAR *End)
{
	DWORD Ver;
#ifdef DEBUG_FILEREAD
	TCHAR Debug[256];
#endif
	Ver=*(DWORD *)(*Parser);
	(*Parser)+=sizeof(DWORD)/sizeof(TCHAR);
	if(*Parser>=End)
		return EACC_FILECOMPATIBILITY;
	if(Ver!=POP3_FILEVERSION)
		return EACC_FILECOMPATIBILITY;

	((HPOP3ACCOUNT)Which)->CP=*(WORD *)(*Parser);
	(*Parser)+=sizeof(WORD)/sizeof(TCHAR);
	if(*Parser>=End)
		return EACC_FILECOMPATIBILITY;
#ifdef DEBUG_FILEREAD
	_stprintf(Debug,_T("CodePage: %d, remaining %d chars"),((HPOP3ACCOUNT)Which)->CP,End-*Parser);
	MessageBox(NULL,Debug,_T("debug"),MB_OK);
#endif
	return 0;
}

HYAMNMAIL WINAPI CreatePOP3Mail(HACCOUNT Account,DWORD MailDataVersion)
{
	HYAMNMAIL NewMail;
//First, we should check whether MAILDATA matches.
//But this is internal plugin, so YAMN's MAILDATA structure and our MAILDATA structure are
//the same, so we do not need to test version. Otherwise, if MAILDATA version does not match
//in your plugin, you should return NULL, like this:
//	if(MailDataVersion!=YAMN_MAILDATAVERSION) return NULL;

//Now it is needed to construct our POP3 account and return its handle
	if(NULL==(NewMail=new YAMNMAIL))
		return NULL;
	
	if(NULL==(NewMail->MailData=new MAILDATA))
	{
		delete NewMail;
		return NULL;
	}
	NewMail->MailData->CP=((HPOP3ACCOUNT)Account)->CP;
	return (HYAMNMAIL)NewMail;
}

static void PostErrorProc(HPOP3ACCOUNT ActualAccount,void *ParamToBadConnection,DWORD POP3PluginParam,BOOL UseSSL)
{
	char *DataRX;
//We create new structure, that we pass to bad connection dialog procedure. This procedure next calls YAMN imported fuction
//from POP3 protocol to determine the description of error. We can describe error from our error code structure, because later,
//when YAMN calls our function, it passes us our error code. This is pointer to structure for POP3 protocol in fact. 
	PPOP3_ERRORCODE ErrorCode;

//We store status before we do Quit(), because quit can destroy our errorcode status
	if(NULL!=(ErrorCode=new POP3_ERRORCODE))
	{
		ErrorCode->SSL=UseSSL;
		ErrorCode->AppError=ActualAccount->SystemError;
		ErrorCode->POP3Error=ActualAccount->Client.POP3Error;
		ErrorCode->NetError=ActualAccount->Client.NetClient->NetworkError;
		ErrorCode->SystemError=ActualAccount->Client.NetClient->SystemError;
	}

	if(POP3PluginParam==(DWORD)NULL)		//if it was normal YAMN call (force check or so on)
	{
		try
		{
			DataRX=ActualAccount->Client.Quit();
			if(DataRX!=NULL)
				free(DataRX);
		}
		catch(...)
		{
		}
//We always close connection if error occured
		try
		{
			ActualAccount->Client.NetClient->Disconnect();
		}
		catch(...)
		{
		}

		SetAccountStatus(ActualAccount,Translate("Disconnected"));

//If we cannot allocate memory, do nothing
		if(ErrorCode==NULL)
		{
			SetEvent(ActualAccount->UseInternetFree);
			return;
		}
	}
	else						//else it was called from POP3 plugin, probably error when deleting old mail (POP3 synchro calls POP3 delete)
		if(ErrorCode==NULL)
			return;

	if((ActualAccount->BadConnectN.Flags & YAMN_ACC_MSG) || (ActualAccount->BadConnectN.Flags & YAMN_ACC_ICO) || (ActualAccount->BadConnectN.Flags & YAMN_ACC_POP))
	{
		YAMN_BADCONNECTIONPARAM cp={(HANDLE)0,ActualAccount,(DWORD)ErrorCode,ParamToBadConnection};

		CallService(MS_YAMN_BADCONNECTION,(WPARAM)&cp,(LPARAM)YAMN_BADCONNECTIONVERSION);
	}
	if(POP3PluginParam==(DWORD)NULL)		//if it was normal YAMN call
		SetEvent(ActualAccount->UseInternetFree);
}

//Checks POP3 account and synchronizes it
DWORD WINAPI SynchroPOP3(struct CheckParam * WhichTemp)
{
	HPOP3ACCOUNT ActualAccount;
	CPop3Client *MyClient;
	HYAMNMAIL NewMails=NULL,MsgQueuePtr=NULL;
	char* DataRX=NULL,*Temp;
	int mboxsize,msgs,i;
	SYSTEMTIME now;
	LPVOID YAMNParam;
	DWORD CheckFlags;
	BOOL UsingInternet=FALSE;
	struct {
		char *ServerName;
		DWORD ServerPort;
		char *ServerLogin;
		char *ServerPasswd;
		DWORD Flags;
		DWORD NFlags;
		DWORD NNFlags;
	} ActualCopied;

//First, we should compare our version of CheckParam structure, but here it is not needed, because YAMN and internal plugin
//have the same version. But your plugin should do that in this way:
//	if(((struct CheckParam *)WhichTemp)->Ver!=YAMN_CHECKVERSION)
//	{
//		SetEvent(((struct CheckParam *)WhichTemp)->ThreadRunningEV);				//don't forget to unblock YAMN
//		return (DWORD)-1;									//ok, but we should return value.
//			//When our plugin returns e.g. 0xFFFFFFFF (=-1, this is only our plugin value, YAMN does nothing with return value,
//			//but only tests if it is nonzero. If yes, it calls GetErrorStringFcn. We know problem occured in YAMN incompatibility
//			//and then we can in our GetErrorStringFcn e.g. return string "Uncompatible version of YAMN".
//	}

	ActualAccount=(HPOP3ACCOUNT)WhichTemp->AccountParam;			//copy address of structure from calling thread to stack of this thread
	YAMNParam=WhichTemp->BrowserParam;
	CheckFlags=WhichTemp->Flags;
#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"CheckPOP3:Incrementing \"using threads\" %x (account %x)\n",ActualAccount->UsingThreads,ActualAccount);
#endif
	SCInc(ActualAccount->UsingThreads);
//Unblock YAMN, signal that we have copied all parameters from YAMN thread stack
	if(INVALID_HANDLE_VALUE!=WhichTemp->ThreadRunningEV)
		SetEvent(WhichTemp->ThreadRunningEV);

#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"CheckPOP3:ActualAccountSO-read wait\n");
#endif
	if(WAIT_OBJECT_0!=WaitToRead(ActualAccount))
	{
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"CheckPOP3:ActualAccountSO-read wait failed\n");
#endif
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"CheckPOP3:Decrementing \"using threads\" %x (account %x)\n",ActualAccount->UsingThreads,ActualAccount);
#endif
		SCDec(ActualAccount->UsingThreads);
		return 0;
	}
#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"CheckPOP3:ActualAccountSO-read enter\n");
#endif
	MyClient=&ActualAccount->Client;
//Now, copy all needed information about account to local variables, so ActualAccount is not blocked in read mode during all connection process, which can last for several minutes.
	ActualCopied.ServerName=_strdup(ActualAccount->Server->Name);
	ActualCopied.ServerPort=ActualAccount->Server->Port;
	ActualCopied.Flags=ActualAccount->Flags;
	ActualCopied.ServerLogin=_strdup(ActualAccount->Server->Login);
	ActualCopied.ServerPasswd=_strdup(ActualAccount->Server->Passwd);
	ActualCopied.NFlags=ActualAccount->NewMailN.Flags;
	ActualCopied.NNFlags=ActualAccount->NoNewMailN.Flags;

#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"CheckPOP3:ActualAccountSO-read done\n");
#endif
	ReadDone(ActualAccount);

	SCInc(ActualAccount->InternetQueries);				//increment counter, that there is one more thread waiting for connection
#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"CheckPOP3:InternetFreeEV-wait\n");
#endif
	WaitForSingleObject(ActualAccount->UseInternetFree,INFINITE);	//wait until we can use connection
#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"CheckPOP3:InternetFreeEV-enter\n");
#endif
	SCDec(ActualAccount->InternetQueries);

//OK, we enter the "use internet" section. But after we start communication, we can test if we did not enter the "use internet" section only for the reason,
//that previous thread release the internet section because this account has stop signal (we stop account and there are 2 threads: one communicating,
//the second one waiting for network access- the first one ends because we want to stop account, this one is released, but should be stopped as well).
	if(!ActualAccount->AbleToWork)
	{
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"CheckPOP3:stop signal-InternetFreeEV-done\n");
#endif
		SetEvent(ActualAccount->UseInternetFree);
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"CheckPOP3:stop signal-Decrementing \"using threads\" %x (account %x)\n",ActualAccount->UsingThreads,ActualAccount);
#endif
		SCDec(ActualAccount->UsingThreads);
		return 0;
	}

	UsingInternet=TRUE;

	GetLocalTime(&now);
	ActualAccount->SystemError=0;					//now we can use internet for this socket. First, clear errorcode.
	try
	{
#ifdef DEBUG_COMM
		DebugLog(CommFile,"<--------Communication-------->\n");
#endif
//	if we are already connected, we have open session (another thread left us open session), so we don't need to login
//	note that connected state without logging cannot occur, because if we close session, we always close socket too (we must close socket is the right word :) )
		if((MyClient->NetClient==NULL) || !MyClient->NetClient->Connected())
		{
			SetAccountStatus(ActualAccount,Translate("Connecting to server"));

			DataRX=MyClient->Connect(ActualCopied.ServerName,ActualCopied.ServerPort,ActualCopied.Flags & YAMN_ACC_SSL23);
			char *timestamp=NULL;

			if(DataRX!=NULL)
			{
				if(ActualCopied.Flags & YAMN_ACC_APOP)
				{
					char *lpos=strchr(DataRX,'<');
					char *rpos=strchr(DataRX,'>');
					if(lpos && rpos && rpos>lpos) {
						int sz=(int)(rpos-lpos+2);
						timestamp=new char[sz];
						memcpy(timestamp, lpos, sz-1);
						timestamp[sz-1]='\0';
					}
				}
				free(DataRX);
				DataRX=NULL;
			}

			SetAccountStatus(ActualAccount,Translate("Entering POP3 account"));

			if(ActualCopied.Flags & YAMN_ACC_APOP)
			{
				DataRX=MyClient->APOP(ActualCopied.ServerLogin,ActualCopied.ServerPasswd,timestamp);
				if(DataRX!=NULL)
					free(DataRX);
				DataRX=NULL;
				delete[] timestamp;
			} else {
				DataRX=MyClient->User(ActualCopied.ServerLogin);
				if(DataRX!=NULL)
					free(DataRX);
				DataRX=NULL;
				DataRX=MyClient->Pass(ActualCopied.ServerPasswd);
				if(DataRX!=NULL)
					free(DataRX);
				DataRX=NULL;
			}
		}
		SetAccountStatus(ActualAccount,Translate("Searching for new mail"));

		DataRX=MyClient->Stat();

#ifdef DEBUG_DECODE
		DebugLog(DecodeFile,"<--------Account checking-------->\n");
		DebugLog(DecodeFile,"<Extracting stat>\n");
#endif
		ExtractStat(DataRX,MyClient->NetClient->Rcv,&mboxsize,&msgs);
#ifdef DEBUG_DECODE
		DebugLog(DecodeFile,"<MailBoxSize>%d</MailBoxSize>\n",mboxsize);
		DebugLog(DecodeFile,"<Msgs>%d</Msgs>\n",msgs);
		DebugLog(DecodeFile,"</Extracting stat>\n");
#endif
		if(DataRX!=NULL)
			free(DataRX);
		DataRX=NULL;
		for(i=0;i<msgs;i++)
		{
			if(!i)
				MsgQueuePtr=NewMails=(HYAMNMAIL)CallService(MS_YAMN_CREATEACCOUNTMAIL,(WPARAM)ActualAccount,(LPARAM)YAMN_MAILVERSION);
			else
			{
				MsgQueuePtr->Next=(HYAMNMAIL)CallService(MS_YAMN_CREATEACCOUNTMAIL,(WPARAM)ActualAccount,(LPARAM)YAMN_MAILVERSION);
				MsgQueuePtr=MsgQueuePtr->Next;
			}
			if(MsgQueuePtr==NULL)
			{
				ActualAccount->SystemError=EPOP3_QUEUEALLOC;
				throw (DWORD)ActualAccount->SystemError;
			}
		}

		if(msgs)
		{
			DataRX=MyClient->List();
			#ifdef DEBUG_DECODE
			DebugLog(DecodeFile,"<Extracting list>\n");
			#endif
			ExtractList(DataRX,MyClient->NetClient->Rcv,NewMails);
			#ifdef DEBUG_DECODE
			DebugLog(DecodeFile,"</Extracting list>\n");
			#endif
			if(DataRX!=NULL)
				free(DataRX);
			DataRX=NULL;

			#ifdef DEBUG_DECODE
			DebugLog(DecodeFile,"<Extracting UIDL>\n");
			#endif
			DataRX=MyClient->Uidl();
			ExtractUIDL(DataRX,MyClient->NetClient->Rcv,NewMails);
			#ifdef DEBUG_DECODE
			DebugLog(DecodeFile,"</Extracting UIDL>\n");
			#endif
			if(DataRX!=NULL)
				free(DataRX);
			DataRX=NULL;
		}
		#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write wait\n");
		#endif
		if(WAIT_OBJECT_0!=MsgsWaitToWrite(ActualAccount))
		{
			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write wait failed\n");
			#endif
			throw (DWORD)ActualAccount->SystemError=EACC_STOPPED;
		}
		#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write enter\n");
		#endif
		ActualAccount->LastChecked=now;
		SynchroMessages(ActualAccount,(HYAMNMAIL *)&ActualAccount->Mails,NULL,(HYAMNMAIL *)&NewMails,NULL);		//we get only new mails on server!
//		NewMails=NULL;

		#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write done\n");
		#endif
		MsgsWriteDone(ActualAccount);

		for(msgs=0,MsgQueuePtr=NewMails;MsgQueuePtr!=NULL;MsgQueuePtr=MsgQueuePtr->Next,msgs++);			//get number of new mails

		try
		{
			char accstatus[512];

			for(i=0,MsgQueuePtr=NewMails;MsgQueuePtr!=NULL;i++)
			{
				DataRX=MyClient->Top(MsgQueuePtr->Number);
				sprintf(accstatus,Translate("Reading new mails (%d%% done)"),100*i/msgs);
				SetAccountStatus(ActualAccount,accstatus);

				#ifdef DEBUG_DECODE
				DebugLog(DecodeFile,"<New mail>\n");
				DebugLog(DecodeFile,"<Header>%s</Header>\n",DataRX);
				#endif
				if(DataRX!=NULL)
				{
					Temp=DataRX;
					while((Temp<DataRX+MyClient->NetClient->Rcv) && (WS(Temp) || ENDLINE(Temp))) Temp++;
				
					if(OKLINE(DataRX))
						for(Temp=DataRX;(Temp<DataRX+MyClient->NetClient->Rcv) && (!ENDLINE(Temp));Temp++);
					while((Temp<DataRX+MyClient->NetClient->Rcv) && ENDLINE(Temp)) Temp++;
				}
				else
					continue;
					
				TranslateHeader(Temp,MyClient->NetClient->Rcv-(Temp-DataRX),&MsgQueuePtr->MailData->TranslatedHeader);

				#ifdef DEBUG_DECODE
				DebugLog(DecodeFile,"</New mail>\n");
				#endif
				MsgQueuePtr->Flags|=YAMN_MSG_NORMALNEW;

				//We are going to filter mail. Warning!- we must not be in read access neither write access to mails when calling this service
				//This is done, because the "NewMails" queue is not synchronised. It is because it is new queue. Only this thread uses new queue yet, it is not
				//connected to account mail queue.
				//				CallService(MS_YAMN_FILTERMAIL,(WPARAM)ActualAccount,(LPARAM)MsgQueuePtr);
				FilterMailSvc((WPARAM)ActualAccount,(LPARAM)MsgQueuePtr);

				MsgQueuePtr=MsgQueuePtr->Next;

				if(DataRX!=NULL)
					free(DataRX);
				DataRX=NULL;

			}
			#ifdef DEBUG_DECODE
			DebugLog(DecodeFile,"</--------Account checking-------->\n");
			#endif

			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write wait\n");
			#endif
			if(WAIT_OBJECT_0!=MsgsWaitToWrite(ActualAccount))
			{
				#ifdef DEBUG_SYNCHRO
				DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write wait failed\n");
				#endif
				throw (DWORD)ActualAccount->SystemError==EACC_STOPPED;
			}
			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write enter\n");
			#endif
			if(ActualAccount->Mails==NULL)
				ActualAccount->Mails=NewMails;
			else
			{
				ActualAccount->LastMail=ActualAccount->LastChecked;
				AppendQueue((HYAMNMAIL)ActualAccount->Mails,NewMails);
			}
			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write done\n");
			#endif
			MsgsWriteDone(ActualAccount);

			//	we are going to delete mails having SPAM flag level3 and 4 (see m_mails.h) set
			{
				struct DeleteParam ParamToDeleteMails={YAMN_DELETEVERSION,INVALID_HANDLE_VALUE,ActualAccount,YAMNParam,(void *)POP3_DELETEFROMCHECK};

				//	Delete mails from server. Here we should not be in write access for account's mails
				DeleteMailsPOP3(&ParamToDeleteMails);
			}

			//	if there is no waiting thread for internet connection close it
			//	else leave connection open
			if(0==SCGetNumber(ActualAccount->InternetQueries))
			{
				DataRX=MyClient->Quit();
				if(DataRX!=NULL)
					free(DataRX);
				DataRX=NULL;
				MyClient->NetClient->Disconnect();

				SetAccountStatus(ActualAccount,Translate("Disconnected"));
			}

			UsingInternet=FALSE;
			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"CheckPOP3:InternetFreeEV-done\n");
			#endif
			SetEvent(ActualAccount->UseInternetFree);

			ActualAccount->LastSChecked=ActualAccount->LastChecked;
			ActualAccount->LastSynchronised=ActualAccount->LastChecked;
		}
		catch(...)
		{
			throw;			//go to the main exception handling
		}

		{
			YAMN_MAILBROWSERPARAM Param={(HANDLE)0,ActualAccount,ActualCopied.NFlags,ActualCopied.NNFlags,YAMNParam};

			if(CheckFlags & YAMN_FORCECHECK)
				Param.nnflags=Param.nnflags | (YAMN_ACC_POP | YAMN_ACC_MSGP);				//if force check, show popup anyway and if mailbrowser was opened, do not close
			CallService(MS_YAMN_MAILBROWSER,(WPARAM)&Param,(LPARAM)YAMN_MAILBROWSERVERSION);
		}
	}
#ifdef DEBUG_COMM
	catch(DWORD ErrorCode)
#else
	catch(DWORD)
#endif
	{
		if(ActualAccount->Client.POP3Error==EPOP3_STOPPED)
	                ActualAccount->SystemError=EACC_STOPPED;
#ifdef DEBUG_COMM
		DebugLog(CommFile,"ERROR: %x\n",ErrorCode);
#endif
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write wait\n");
#endif
		if(WAIT_OBJECT_0==MsgsWaitToWrite(ActualAccount))
		{
#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write enter\n");
#endif
			ActualAccount->LastChecked=now;
#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write done\n");
#endif
			MsgsWriteDone(ActualAccount);
		}
#ifdef DEBUG_SYNCHRO
		else
			DebugLog(SynchroFile,"CheckPOP3:ActualAccountMsgsSO-write wait failed\n");
#endif

		DeleteMIMEQueue(ActualAccount,NewMails);

		if(DataRX!=NULL)
			free(DataRX);
		DataRX=NULL;
		switch(ActualAccount->SystemError)
		{
			case EACC_QUEUEALLOC:
			case EACC_STOPPED:
				ActualAccount->Client.NetClient->Disconnect();
				break;
			default:
				PostErrorProc(ActualAccount,YAMNParam,(DWORD)NULL,MyClient->SSL);	//it closes internet connection too
		}

		if(UsingInternet)	//if our thread still uses internet
		{
#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"CheckPOP3:InternetFreeEV-done\n");
#endif
			SetEvent(ActualAccount->UseInternetFree);
		}
	}

	free(ActualCopied.ServerName);
	free(ActualCopied.ServerLogin);
	free(ActualCopied.ServerPasswd);
#ifdef DEBUG_COMM
	DebugLog(CommFile,"</--------Communication-------->\n");
#endif
//	WriteAccounts();
#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"CheckPOP3:Decrementing \"using threads\" %x (account %x)\n",ActualAccount->UsingThreads,ActualAccount);
#endif
	SCDec(ActualAccount->UsingThreads);
	return 0;
}

DWORD WINAPI DeleteMailsPOP3(struct DeleteParam *WhichTemp)
{
	HPOP3ACCOUNT ActualAccount;
	LPVOID YAMNParam;
	DWORD POP3PluginParam;
	CPop3Client *MyClient;
	HYAMNMAIL DeleteMails,NewMails=NULL,MsgQueuePtr;
	char* DataRX=NULL;
	int mboxsize,msgs,i;
	BOOL UsingInternet=FALSE;
	struct {
		char *ServerName;
		DWORD ServerPort;
		char *ServerLogin;
		char *ServerPasswd;
		DWORD Flags;
		DWORD NFlags;
		DWORD NNFlags;
	} ActualCopied;

//First, we should compare our version of DeleteParam structure, but here it is not needed, because YAMN and internal plugin
//have the same version. But your plugin should do that in this way:
//	if(((struct DeleteParam *)WhichTemp)->Ver!=YAMN_DELETEVERSION)
//	{
//		SetEvent(((struct DeleteParam *)WhichTemp)->ThreadRunningEV);				//don't forget to unblock YAMN
//		return (DWORD)-1;									//ok, but we should return value.
//			//When our plugin returns e.g. 0xFFFFFFFF (this is only our plugin value, YAMN does nothing with return value,
//			//but only tests if it is nonzero. If yes, it calls GetErrorStringFcn), we know problem occured in YAMN incompatibility
//			//and then we can in our GetErrorStringFcn e.g. return string "Uncompatible version of YAMN".
//	}

	ActualAccount=(HPOP3ACCOUNT)((struct DeleteParam *)WhichTemp)->AccountParam;			//copy address of structure from calling thread to stack of this thread
	YAMNParam=((struct DeleteParam *)WhichTemp)->BrowserParam;
	POP3PluginParam=(DWORD)((struct DeleteParam *)WhichTemp)->CustomParam;
#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"DeleteMailsPOP3:Incrementing \"using threads\" %x (account %x)\n",ActualAccount->UsingThreads,ActualAccount);
#endif
	SCInc(ActualAccount->UsingThreads);
	if(INVALID_HANDLE_VALUE!=WhichTemp->ThreadRunningEV)
		SetEvent(WhichTemp->ThreadRunningEV);

#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"DeleteMailsPOP3:ActualAccountSO-read wait\n");
#endif
	if(WAIT_OBJECT_0!=WaitToRead(ActualAccount))
	{
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"DeleteMailsPOP3:ActualAccountSO-read wait failed\n");
#endif
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"DeleteMailsPOP3:Decrementing \"using threads\" %x (account %x)\n",ActualAccount->UsingThreads,ActualAccount);
#endif
		SCDec(ActualAccount->UsingThreads);
		return 0;
	}
#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"DeleteMailsPOP3:ActualAccountSO-read enter\n");
#endif
	if(NULL==(DeleteMails=(HYAMNMAIL)CreateNewDeleteQueue((HYAMNMAIL)ActualAccount->Mails)))	//if there's no mail for deleting, return
	{
		if(POP3_DELETEFROMCHECK!=POP3PluginParam)						//We do not wait for free internet when calling from SynchroPOP3. It is because UseInternetFree is blocked
		{
			YAMN_MAILBROWSERPARAM Param={(HANDLE)0,ActualAccount,YAMN_ACC_MSGP,YAMN_ACC_MSGP,YAMNParam};		//Just update the window

			CallService(MS_YAMN_MAILBROWSER,(WPARAM)&Param,(LPARAM)YAMN_MAILBROWSERVERSION);
		}
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"DeleteMailsPOP3:ActualAccountSO-read done\n");
#endif
		ReadDone(ActualAccount);
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"DeleteMailsPOP3:Decrementing \"using threads\" %x (account %x)\n",ActualAccount->UsingThreads,ActualAccount);
#endif
		SCDec(ActualAccount->UsingThreads);

		return NO_MAIL_FOR_DELETE;
	}
	MyClient=&(ActualAccount->Client);

//Now, copy all needed information about account to local variables, so ActualAccount is not blocked in read mode during all connection process, which can last for several minutes.
	ActualCopied.ServerName=_strdup(ActualAccount->Server->Name);
	ActualCopied.ServerPort=ActualAccount->Server->Port;
	ActualCopied.Flags=ActualAccount->Flags;
	ActualCopied.ServerLogin=_strdup(ActualAccount->Server->Login);
	ActualCopied.ServerPasswd=_strdup(ActualAccount->Server->Passwd);
	ActualCopied.NFlags=ActualAccount->NewMailN.Flags;
	ActualCopied.NNFlags=ActualAccount->NoNewMailN.Flags;

#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"DeleteMailsPOP3:ActualAccountSO-read done\n");
#endif
	ReadDone(ActualAccount);

	SCInc(ActualAccount->InternetQueries);	//This is POP3-internal SCOUNTER, we set another thread wait for this account to be connected to inet
	if(POP3_DELETEFROMCHECK!=POP3PluginParam)	//We do not wait for free internet when calling from SynchroPOP3. It is because UseInternetFree is blocked
	{
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"DeleteMailsPOP3:InternetFreeEV-wait\n");
#endif
		WaitForSingleObject(ActualAccount->UseInternetFree,INFINITE);
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"DeleteMailsPOP3:InternetFreeEV-enter\n");
#endif
	}
	SCDec(ActualAccount->InternetQueries);
	UsingInternet=TRUE;

	try
	{
#ifdef DEBUG_COMM
		DebugLog(CommFile,"<--------Communication-------->\n");
#endif
		if((MyClient->NetClient==NULL) || !MyClient->NetClient->Connected())
		{
			SetAccountStatus(ActualAccount,Translate("Connecting to server"));
			
			DataRX=MyClient->Connect(ActualCopied.ServerName,ActualCopied.ServerPort,ActualCopied.Flags & YAMN_ACC_SSL23);

			char *timestamp=NULL;
			if(DataRX!=NULL) {
				if(ActualAccount->Flags & YAMN_ACC_APOP) {
					char *lpos=strchr(DataRX,'<');
					char *rpos=strchr(DataRX,'>');
					if(lpos && rpos && rpos>lpos) {
						int sz=(int)(rpos-lpos+2);
						timestamp=new char[sz];
						memcpy(timestamp, lpos, sz-1);
						timestamp[sz-1]='\0';
					}
				}
				free(DataRX);
				DataRX=NULL;
			}
			SetAccountStatus(ActualAccount,Translate("Entering POP3 account"));

			if(ActualAccount->Flags & YAMN_ACC_APOP)
			{
				DataRX=MyClient->APOP(ActualCopied.ServerLogin,ActualCopied.ServerPasswd,timestamp);
				if(DataRX!=NULL)
					free(DataRX);
				DataRX=NULL;
				delete[] timestamp;
			} else {
				DataRX=MyClient->User(ActualCopied.ServerLogin);
				if(DataRX!=NULL)
					free(DataRX);
				DataRX=NULL;
				DataRX=MyClient->Pass(ActualCopied.ServerPasswd);
				if(DataRX!=NULL)
					free(DataRX);
				DataRX=NULL;
			}
		}

#ifdef DEBUG_DECODE
		DebugLog(DecodeFile,"<--------Deleting requested mails-------->\n");
#endif
		if(POP3_DELETEFROMCHECK!=POP3PluginParam)	//We do not need to get mails on server as we have already it from check function
		{
			SetAccountStatus(ActualAccount,Translate("Deleting requested mails"));

			DataRX=MyClient->Stat();

#ifdef DEBUG_DECODE
			DebugLog(DecodeFile,"<Extracting stat>\n");
#endif
			ExtractStat(DataRX,MyClient->NetClient->Rcv,&mboxsize,&msgs);
#ifdef DEBUG_DECODE
			DebugLog(DecodeFile,"<MailBoxSize>%d</MailBoxSize>\n",mboxsize);
			DebugLog(DecodeFile,"<Msgs>%d</Msgs>\n",msgs);
			DebugLog(DecodeFile,"</Extracting stat>\n");
#endif
			if(DataRX!=NULL)
				free(DataRX);
			DataRX=NULL;
			for(i=0;i<msgs;i++)
			{
				if(!i)
					MsgQueuePtr=NewMails=(HYAMNMAIL)CallService(MS_YAMN_CREATEACCOUNTMAIL,(WPARAM)ActualAccount,(LPARAM)YAMN_MAILVERSION);
				else
				{
					MsgQueuePtr->Next=(HYAMNMAIL)CallService(MS_YAMN_CREATEACCOUNTMAIL,(WPARAM)ActualAccount,(LPARAM)YAMN_MAILVERSION);
					MsgQueuePtr=MsgQueuePtr->Next;
				}
				if(MsgQueuePtr==NULL)
				{
					ActualAccount->SystemError=EPOP3_QUEUEALLOC;
					throw (DWORD)ActualAccount->SystemError;
				}
			}
	
			if(msgs)
			{
#ifdef DEBUG_DECODE
				DebugLog(DecodeFile,"<Extracting UIDL>\n");
#endif
				DataRX=MyClient->Uidl();
				ExtractUIDL(DataRX,MyClient->NetClient->Rcv,NewMails);
#ifdef DEBUG_DECODE
				DebugLog(DecodeFile,"</Extracting UIDL>\n");
#endif
				if(DataRX!=NULL)
					free(DataRX);
				DataRX=NULL;
//	we get "new mails" on server (NewMails will contain all mails on server not found in DeleteMails)
//	but also in DeleteMails we get only those, which are still on server with their responsable numbers
				SynchroMessages(ActualAccount,(HYAMNMAIL *)&DeleteMails,NULL,(HYAMNMAIL *)&NewMails,NULL);
			}
		}
		else
			SetAccountStatus(ActualAccount,Translate("Deleting spam"));
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"DeleteMailsPOP3:ActualAccountMsgsSO-write wait\n");
#endif
		if(WAIT_OBJECT_0!=MsgsWaitToWrite(ActualAccount))
		{
#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"DeleteMailsPOP3:ActualAccountMsgsSO-write wait failed\n");
#endif
			throw (DWORD)EACC_STOPPED;
		}
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"DeleteMailsPOP3:ActualAccountMsgsSO-write enter\n");
#endif
		if(msgs || POP3_DELETEFROMCHECK==POP3PluginParam)
		{
			try
			{
				HYAMNMAIL Temp;

				for(i=0,MsgQueuePtr=DeleteMails;MsgQueuePtr!=NULL;i++)
				{
					if(!(MsgQueuePtr->Flags & YAMN_MSG_VIRTUAL))	//of course we can only delete real mails, not virtual
					{
						DataRX=MyClient->Dele(MsgQueuePtr->Number);
						Temp=MsgQueuePtr->Next;
						if(POP3_FOK==MyClient->AckFlag)			//if server answers that mail was deleted
						{
							DeleteMIMEMessage((HYAMNMAIL *)&DeleteMails,MsgQueuePtr);
							HYAMNMAIL DeletedMail=FindMIMEMessageByID((HYAMNMAIL)ActualAccount->Mails,MsgQueuePtr->ID);
							if((MsgQueuePtr->Flags & YAMN_MSG_MEMDELETE))		//if mail should be deleted from memory (or disk)
							{
								DeleteMIMEMessage((HYAMNMAIL *)&ActualAccount->Mails,DeletedMail);	//remove from queue
								CallService(MS_YAMN_DELETEACCOUNTMAIL,(WPARAM)POP3Plugin,(LPARAM)DeletedMail);
							}
							else			//else mark it only as "deleted mail"
							{
								DeletedMail->Flags |= (YAMN_MSG_VIRTUAL | YAMN_MSG_DELETED);
								DeletedMail->Flags &= ~(YAMN_MSG_NEW | YAMN_MSG_USERDELETE | YAMN_MSG_AUTODELETE);	//clear "new mail"
							}
							delete MsgQueuePtr->MailData;
							delete MsgQueuePtr;
						}
						MsgQueuePtr=Temp;

						if(DataRX!=NULL)
							free(DataRX);
						DataRX=NULL;
					}
					else
						MsgQueuePtr=MsgQueuePtr->Next;
				}
			}
			catch(...)		//if any exception in the code where we have write-access to account occured, don't forget to leave write-access
			{
#ifdef DEBUG_SYNCHRO
				DebugLog(SynchroFile,"DeleteMailsPOP3:ActualAccountMsgsSO-write done\n");
#endif  	
				MsgsWriteDone(ActualAccount);
				throw;			//and go to the main exception handling
			}

			if(NewMails!=NULL)
//	in ActualAccount->Mails we have all mails stored before calling this function
//	in NewMails we have all mails not found in DeleteMails (in other words: we performed new ID checking and we
//	stored all mails found on server, then we deleted the ones we wanted to delete in this function
//	and NewMails queue now contains actual state of mails on server). But we will not use NewMails as actual state, because NewMails does not contain header data (subject, from...)
//	We perform deleting from ActualAccount->Mails: we remove from original queue (ActualAccount->Mails) all deleted mails
				SynchroMessages(ActualAccount,(HYAMNMAIL *)&ActualAccount->Mails,NULL,(HYAMNMAIL *)&NewMails,NULL);
//	Now ActualAccount->Mails contains all mails when calling this function except the ones, we wanted to delete (these are in DeleteMails)
//	And in NewMails we have new mails (if any)
			else if(POP3_DELETEFROMCHECK!=POP3PluginParam)
			{
				DeleteMIMEQueue(ActualAccount,(HYAMNMAIL)ActualAccount->Mails);
				ActualAccount->Mails=NULL;
			}
		}
		else
		{
			DeleteMIMEQueue(ActualAccount,(HYAMNMAIL)ActualAccount->Mails);
			ActualAccount->Mails=NULL;
		}
#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"DeleteMailsPOP3:ActualAccountMsgsSO-write done\n");
#endif
		MsgsWriteDone(ActualAccount);
#ifdef DEBUG_DECODE     	
		DebugLog(DecodeFile,"</--------Deleting requested mails-------->\n");
#endif

//	TODO: now, we have in NewMails new mails. If NewMails is not NULL, we found some new mails, so Checking for new mail should be performed
//	now, we do not call CheckPOP3

//	if there is no waiting thread for internet connection close it
//	else leave connection open
//	if this functin was called from SynchroPOP3, then do not try to disconnect 
		if(POP3_DELETEFROMCHECK!=POP3PluginParam)
		{
			YAMN_MAILBROWSERPARAM Param={(HANDLE)0,ActualAccount,ActualCopied.NFlags,YAMN_ACC_MSGP,YAMNParam};

			CallService(MS_YAMN_MAILBROWSER,(WPARAM)&Param,(LPARAM)YAMN_MAILBROWSERVERSION);

			if(0==SCGetNumber(ActualAccount->InternetQueries))
			{
				DataRX=MyClient->Quit();
				if(DataRX!=NULL)
					free(DataRX);
				DataRX=NULL;
				MyClient->NetClient->Disconnect();

				SetAccountStatus(ActualAccount,Translate("Disconnected"));
			}

			UsingInternet=FALSE;
#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"DeleteMailsPOP3:InternetFreeEV-done\n");
#endif
			SetEvent(ActualAccount->UseInternetFree);
		}
	}
#ifdef DEBUG_COMM
	catch(DWORD ErrorCode)
#else
	catch(DWORD)
#endif
	{
		if(ActualAccount->Client.POP3Error==EPOP3_STOPPED)
	                ActualAccount->SystemError=EACC_STOPPED;
#ifdef DEBUG_COMM
		DebugLog(CommFile,"ERROR %x\n",ErrorCode);
#endif
		if(DataRX!=NULL)
			free(DataRX);
		switch(ActualAccount->SystemError)
		{
			case EACC_QUEUEALLOC:
			case EACC_STOPPED:
				ActualAccount->Client.NetClient->Disconnect();
				break;
			default:
				PostErrorProc(ActualAccount,YAMNParam,POP3PluginParam,MyClient->SSL);	//it closes internet connection too
		}

		if(UsingInternet && (POP3_DELETEFROMCHECK!=POP3PluginParam))	//if our thread still uses internet and it is needed to release internet
		{
#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"CheckPOP3:InternetFreeEV-done\n");
#endif
			SetEvent(ActualAccount->UseInternetFree);
		}
	}

	free(ActualCopied.ServerName);
	free(ActualCopied.ServerLogin);
	free(ActualCopied.ServerPasswd);

	DeleteMIMEQueue(ActualAccount,NewMails);
	DeleteMIMEQueue(ActualAccount,DeleteMails);

#ifdef DEBUG_COMM
	DebugLog(CommFile,"</--------Communication-------->\n");
#endif
//	WriteAccounts();
#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"DeleteMailsPOP3:Decrementing \"using threads\" %x (account %x)\n",ActualAccount->UsingThreads,ActualAccount);
#endif
	SCDec(ActualAccount->UsingThreads);
	return 0;
}

void ExtractStat(char *stream,int len,int *mboxsize,int *mails)
{
	char *finder=stream;
	while(WS(finder) || ENDLINE(finder)) finder++;
	if(ACKLINE(finder))
	{
		while(!WS(finder)) finder++;
		while(WS(finder)) finder++;
	}
	if(1!=sscanf(finder,"%d",mails))
		throw (DWORD)EPOP3_STAT;
	while(!WS(finder)) finder++;
	while(WS(finder)) finder++;
	if(1!=sscanf(finder,"%d",mboxsize))
		throw (DWORD)EPOP3_STAT;
}

void ExtractUIDL(char *stream,int len,HYAMNMAIL queue)
{
	char *finder=stream;
	char *finderend;
	int msgnr,i;
	HYAMNMAIL queueptr=queue;

	while(WS(finder) || ENDLINE(finder)) finder++;
	while(!ACKLINE(finder)) finder++;
	while(!ENDLINE(finder)) finder++;			//now we at the end of first ack line
	while(finder<=(stream+len))
	{
		while(ENDLINE(finder)) finder++;		//go to the new line
		if(DOTLINE(finder+1))					//at the end of stream
			break;
#ifdef DEBUG_DECODE
		DebugLog(DecodeFile,"<Message>\n");
#endif
		while(WS(finder)) finder++;			//jump whitespace
		if(1!=sscanf(finder,"%d",&msgnr))
			throw (DWORD)EPOP3_UIDL;
#ifdef DEBUG_DECODE
		DebugLog(DecodeFile,"<Nr>%d</Nr>\n",msgnr);
#endif
//		for(i=1,queueptr=queue;(queueptr->Next!=NULL) && (i<msgnr);queueptr=queueptr->Next,i++);
//		if(i!=msgnr)
//			throw (DWORD)EPOP3_UIDL;
		while(!WS(finder)) finder++;			//jump characters
		while(WS(finder)) finder++;			//jump whitespace
		finderend=finder+1;
		while(!WS(finderend) && !ENDLINE(finderend)) finderend++;
		queueptr->ID=new char[finderend-finder+1];
		for(i=0;finder!=finderend;finder++,i++)
			queueptr->ID[i]=*finder;
		queueptr->ID[i]=0;				//ends string
		queueptr->Number=msgnr;
#ifdef DEBUG_DECODE
		DebugLog(DecodeFile,"<ID>%s</ID>\n",queueptr->ID);
		DebugLog(DecodeFile,"</Message>\n");
#endif
		queueptr=queueptr->Next;
		while(!ENDLINE(finder)) finder++;
	}
}

void ExtractList(char *stream,int len,HYAMNMAIL queue)
{
	char *finder=stream;
	char *finderend;
	int msgnr,i;
	HYAMNMAIL queueptr;

	while(WS(finder) || ENDLINE(finder)) finder++;
	while(!ACKLINE(finder)) finder++;
	while(!ENDLINE(finder)) finder++;			//now we at the end of first ack line
	while(finder<=(stream+len))
	{
		while(ENDLINE(finder)) finder++;		//go to the new line
		if(DOTLINE(finder+1))				//at the end of stream
			break;
#ifdef DEBUG_DECODE
		DebugLog(DecodeFile,"<Message>\n",NULL,0);
#endif
		while(WS(finder)) finder++;			//jump whitespace
		if(1!=sscanf(finder,"%d",&msgnr))		//message nr.
			throw (DWORD)EPOP3_LIST;
#ifdef DEBUG_DECODE
		DebugLog(DecodeFile,"<Nr>%d</Nr>\n",msgnr);
#endif

		for(i=1,queueptr=queue;(queueptr->Next!=NULL) && (i<msgnr);queueptr=queueptr->Next,i++);
		if(i!=msgnr)
			throw (DWORD)EPOP3_LIST;
		while(!WS(finder)) finder++;			//jump characters
		while(WS(finder)) finder++;			//jump whitespace
		finderend=finder+1;
		if(1!=sscanf(finder,"%d",&queueptr->MailData->Size))
			throw (DWORD)EPOP3_LIST;
#ifdef DEBUG_DECODE
		DebugLog(DecodeFile,"<Nr>%d</Nr>\n",queueptr->MailData->Size);
#endif
		while(!ENDLINE(finder)) finder++;
	}
}

WCHAR* WINAPI GetErrorString(DWORD Code)
{
	static char *POP3Errors[]=
	{
		"Memory allocation error.",		//memory allocation
		"Account is about to be stopped.",	//stop account
		"Cannot connect to POP3 server.",
		"Cannot allocate memory for received data.",
		"Cannot login to POP3 server.",
		"Bad user or password.",
		"Server does not support APOP authorization.",
		"Error while executing POP3 command.",
		"Error while executing POP3 command.",
		"Error while executing POP3 command.",
	};

	static char *NetlibErrors[]=
	{
		"Cannot connect to server with NetLib.",
		"Cannot send data.",
		"Cannot receive data.",
		"Cannot allocate memory for received data.",
	};

	static char *SSLErrors[]=
	{
		"OpenSSL not loaded.",
		"Windows socket 2.0 init failed.",
		"DNS lookup error.",
		"Error while creating base socket.",
		"Error connecting to server with socket.",
		"Error while creating SSL structure.",
		"Error connecting socket with SSL.",
		"Server rejected connection with SSL.",
		"Cannot write SSL data.",
		"Cannot read SSL data.",
		"Cannot allocate memory for received data.",
	};

	char *ErrorStringA=new char[ERRORSTR_MAXLEN];
	WCHAR *ErrorStringW=new WCHAR[ERRORSTR_MAXLEN];
	POP3_ERRORCODE *ErrorCode=(POP3_ERRORCODE *)Code;

	sprintf(ErrorStringA,Translate("Error %d-%d-%d-%d:"),ErrorCode->AppError,ErrorCode->POP3Error,ErrorCode->NetError,ErrorCode->SystemError);
	if(ErrorCode->POP3Error)
		sprintf(ErrorStringA,"%s\n%s",ErrorStringA,Translate(POP3Errors[ErrorCode->POP3Error-1]));
	if(ErrorCode->NetError)
		if(ErrorCode->SSL)
			sprintf(ErrorStringA,"%s\n%s",ErrorStringA,Translate(SSLErrors[ErrorCode->NetError-1]));
		else
			sprintf(ErrorStringA,"%s\n%s",ErrorStringA,Translate(NetlibErrors[ErrorCode->NetError-4]));
		
#ifdef DEBUG_COMM
	DebugLog(CommFile,"%s\n",ErrorStringA);
#endif
	MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,ErrorStringA,-1,ErrorStringW,strlen(ErrorStringA)+1);
	delete[] ErrorStringA;		//we delete ErrorStringA, used to get error string, because Translate() doesn't works in unicode
	delete ErrorCode;		//now we can delete ErrorCode, that will not be used anymore
	return ErrorStringW;
}

void WINAPI DeleteErrorString(LPVOID String)
{
	delete String;
}