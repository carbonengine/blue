// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#include "BlueRemoteStream.h"

#include "IBlueOS.h"

BLUE_DEFINE( BlueRemoteStream );

const Be::ClassInfo* BlueRemoteStream::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueRemoteStream, "" )
		MAP_INTERFACE( BlueRemoteStream )
		MAP_INTERFACE( IBlueStream )
	EXPOSURE_END()
}
