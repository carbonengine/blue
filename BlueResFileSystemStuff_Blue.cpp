////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		November 2014
// Copyright:	CCP 2014
//

#include "StdAfx.h"
#include "BlueResFileSystemStuff.h"

BLUE_DEFINE_NONEXPOSED( BlueResFileSystemStuff );

const Be::ClassInfo* BlueResFileSystemStuff::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueResFileSystemStuff, "" )
		MAP_INTERFACE( IBlueResFileSystem )
	EXPOSURE_END()
}