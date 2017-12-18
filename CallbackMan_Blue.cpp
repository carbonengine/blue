#include "StdAfx.h"

#include "CallbackMan.h"
#include "Include/IBlueOS.h"

BLUE_DEFINE( BlueCallbackMan );

const Be::ClassInfo* BlueCallbackMan::ExposeToBlue()
{
	EXPOSURE_BEGIN(BlueCallbackMan, "" )

		MAP_INTERFACE( IBlueCallbackMan )

	EXPOSURE_END()
}
