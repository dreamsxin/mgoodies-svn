#include "Compatibility.h"

static HANDLE hContactDeletedEvent,hContactAddedEvent;

bool RegisterCompatibilityServices()
{
	CreateServiceFunction(MS_DB_CONTACT_GETCOUNT,GetContactCount);
	CreateServiceFunction(MS_DB_CONTACT_FINDFIRST,FindFirstContact);
	CreateServiceFunction(MS_DB_CONTACT_FINDNEXT,FindNextContact);
	CreateServiceFunction(MS_DB_CONTACT_DELETE,DeleteContact);
	CreateServiceFunction(MS_DB_CONTACT_ADD,AddContact);
	CreateServiceFunction(MS_DB_CONTACT_IS,IsDbContact);
	hContactDeletedEvent=CreateHookableEvent(ME_DB_CONTACT_DELETED);
	hContactAddedEvent=CreateHookableEvent(ME_DB_CONTACT_ADDED);
	return true;
}
bool UnRegisterCompatibilityServices()
{
	DestroyHookableEvent(hContactDeletedEvent);
	DestroyHookableEvent(hContactAddedEvent);
	return true;
}
int AddContact(WPARAM wParam,LPARAM lParam)
{
	int ret;
	ret=DBEntryCreate(DBEntryGetRoot(0,0),0);
	if(ret == DB_INVALIDPARAM)
		return 1;
	NotifyEventHooks(hContactAddedEvent,(WPARAM)ret,0);
	return ret;
}
int DeleteContact(WPARAM hEntry,LPARAM lParam)
{
	int ret;
	ret=DBEntryDelete(hEntry,0); //what about settings and events?
	if(ret==DB_INVALIDPARAM)
		return 1;
	NotifyEventHooks(hContactDeletedEvent,hEntry,0);
	return ret;
}
int IsDbContact(WPARAM hEntry,LPARAM lParam)
{
	return NULL;
}
int GetContactCount(WPARAM wParam,LPARAM lParam)
{
	TDBEntryHandle hEntry=NULL;
	TDBEntryIterFilter IterFilter;
	IterFilter.fDontHasFlags=DB_EF_IsGroup|DB_EF_IsVirtual;
	TDBEntryIterationHandle hIter=DBEntryIterInit((WPARAM)&IterFilter,0);
	int nCount=0;
	while(hIter!=DB_INVALIDPARAM && hIter!=0)
	{
		hEntry=DBEntryIterNext(hIter,0);
		if(hEntry!=0 && hEntry!= DB_INVALIDPARAM)
			nCount++;
	}
	DBEntryIterClose(hIter,0);
	return nCount;
}
int FindFirstContact(WPARAM wParam,LPARAM lParam)
{
	TDBEntryHandle hEntry=NULL;
	TDBEntryIterFilter IterFilter;
	IterFilter.fDontHasFlags=DB_EF_IsGroup|DB_EF_IsVirtual;
	TDBEntryIterationHandle hIter=DBEntryIterInit((WPARAM)&IterFilter,0);
	if(hIter!=DB_INVALIDPARAM && hIter!=0)
		hEntry=DBEntryIterNext(hIter,0);
	DBEntryIterClose(hIter,0);
	if(hEntry==0 || hEntry== DB_INVALIDPARAM)
		return NULL;
	else
		return hEntry;
}
int FindNextContact(WPARAM hEntry,LPARAM lParam)
{
	TDBEntryHandle hEntry=NULL;
	return hEntry;
}