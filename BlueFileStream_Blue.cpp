////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		August 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#include "BlueFileStream.h"

#include "include/IBlueOS.h"

BLUE_DEFINE( BlueFileStream );

const Be::ClassInfo* BlueFileStream::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueFileStream, "" )
		MAP_INTERFACE( BlueFileStream )
		MAP_INTERFACE( IBlueStream )
	EXPOSURE_END()
}
