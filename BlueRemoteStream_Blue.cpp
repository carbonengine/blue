////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		August 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#include "BlueRemoteStream.h"

#include "include/IBlueOS.h"

BLUE_DEFINE( BlueRemoteStream );

const Be::ClassInfo* BlueRemoteStream::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueRemoteStream, "" )
		MAP_INTERFACE( BlueRemoteStream )
		MAP_INTERFACE( IBlueStream )
	EXPOSURE_END()
}
