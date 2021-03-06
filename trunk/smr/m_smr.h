/* 
Copyright (C) 2005 Ricardo Pescuma Domenecci

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


#ifndef __M_SMR_H__
# define __M_SMR_H__


#define MIID_STATUS_MESSAGE_RETRIEVER { 0xbdfc508a, 0x3c73, 0x40d4, { 0x9f, 0x15, 0xf0, 0xbe, 0xb5, 0xab, 0x72, 0x78 } }


/*
Return TRUE is smr is enabled for this protocol
If is enabled, status message is kept under CList\StatusMsg db key in user data

wParam: protocol name
lParam: ignored
*/
#define MS_SMR_ENABLED_FOR_PROTOCOL "SMR/MsgRetrievalEnabledForProtocol"


/*
Return TRUE is smr is enabled for this contact and its protocol (smr can be disabled per user,
if protocol is enabled)
If is enabled, status message is kept under CList\StatusMsg db key in user data

wParam: hContact
lParam: ignored
*/
#define MS_SMR_ENABLED_FOR_CONTACT "SMR/MsgRetrievalEnabledForUser"


/*
Enable status message retrieval for a contact

wParam: hContact
lParam: ignored
*/
#define MS_SMR_ENABLE_CONTACT "SMR/EnableContactMsgRetrieval"


/*
Disable status message retrieval for a contact

wParam: hContact
lParam: ignored
*/
#define MS_SMR_DISABLE_CONTACT			"SMR/DisableContactMsgRetrieval"





#endif // __M_SMR_H__
