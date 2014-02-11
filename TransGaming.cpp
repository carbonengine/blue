#include "StdAfx.h"

#ifdef _WIN32

#include "include/TransGaming.h"
#include <windows.h>

// --------------------------------------------------------------------------------
// Description:
//   This function indicates weather we're running with TransGaming.
// --------------------------------------------------------------------------------
bool IsTransgaming()
{
	static bool isKnown = false;
	static bool isTransgaming = false;
	if( !isKnown )
	{
		HMODULE hMod = GetModuleHandle("ntdll");
		typedef bool (*IsTransgaming) (void);
		IsTransgaming pFunc = (IsTransgaming)GetProcAddress (hMod, "IsTransgaming");

		isTransgaming = pFunc && pFunc();
		isKnown = true;
	}

	return isTransgaming;
}

// --------------------------------------------------------------------------------
// Description:
//   Gets a handle to the TGRegisterNotificationCallback from the user32 module
//   and registers an event notification callback function if successful.
// --------------------------------------------------------------------------------
BOOL TGRegisterForNotifications( TGNotifyCallback_Func callback, void* context )
{
	HMODULE hMod = GetModuleHandle("user32");
	if( !hMod )
	{
		return 0;
	}

	BOOL result = 0;

    TGRegisterNotificationCallback_Func TGRegisterNotificationCallback;
    TGRegisterNotificationCallback = (TGRegisterNotificationCallback_Func) GetProcAddress( hMod, "TGRegisterNotificationCallback" );
	if( !TGRegisterNotificationCallback )
	{
		return 0;
	}

	result = TGRegisterNotificationCallback( TGNOTIFY_ACTIVATION, callback, context );
	return result;
}

#endif
