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


#ifndef __OPTIONS_H__
# define __OPTIONS_H__


#include "commons.h"

#include <windows.h>

#ifdef __cplusplus
extern "C" 
{
#endif

#define OPT_CHECK_ONTIMER				"CheckOnTimer"
#define OPT_CHECK_ONSTATUSCHANGE		"CheckOnStatusChange"
#define OPT_CHECK_ONSTATUSCHANGETIMER	"CheckOnStatusChangeTimer"
#define OPT_CLEAR_ONSTATUSCHANGE		"ClearOnStatusChange"
#define OPT_CHECK_ONTIMER_TIMER			"CheckOnTimerTimer"
#define OPT_CHECK_ONSTATUSTIMER_TIMER	"CheckOnStatusTimer"
#define OPT_CONTACT_GETMSG				"MsgCheck"
#define OPT_PROTOCOL_GETMSG				"%sMsgCheck"


typedef struct 
{
	BOOL pool_check_on_timer;
	BOOL pool_check_on_status_change;
	BOOL pool_check_on_status_change_timer;
	BOOL pool_clear_on_status_change;
	WORD pool_timer_check;
	WORD pool_timer_status;
} Options;

extern Options opts;


// Initializations needed by options
void InitOptions();

// Deinitializations needed by options
void DeInitOptions();


// Loads the options from DB
// It don't need to be called, except in some rare cases
void LoadOptions();



#ifdef __cplusplus
}
#endif

#endif // __OPTIONS_H__