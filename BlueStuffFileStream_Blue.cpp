////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		August 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#if STUFFER_ENABLED

#include "BlueStuffFileStream.h"

#include "include/IBlueOS.h"

BLUE_DEFINE( BlueStuffFileStream );

const Be::ClassInfo* BlueStuffFileStream::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueStuffFileStream, "" )
		MAP_INTERFACE( BlueStuffFileStream )
		MAP_INTERFACE( IBlueStream )
	EXPOSURE_END()
}

#endif
