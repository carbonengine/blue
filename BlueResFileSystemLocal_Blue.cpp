////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		November 2014
// Copyright:	CCP 2014
//

#include "StdAfx.h"
#include "BlueResFileSystemLocal.h"

BLUE_DEFINE_INTERFACE( IBlueResFileSystem );
BLUE_DEFINE_NONEXPOSED( BlueResFileSystemLocal );

const Be::ClassInfo* BlueResFileSystemLocal::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueResFileSystemLocal, "" )
		MAP_INTERFACE( IBlueResFileSystem )
	EXPOSURE_END()
}