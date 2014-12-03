////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		November 2014
// Copyright:	CCP 2014
//

#include "StdAfx.h"
#include "BlueResFileSystemRemote.h"

BLUE_DEFINE_NONEXPOSED( BlueResFileSystemRemote );

const Be::ClassInfo* BlueResFileSystemRemote::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueResFileSystemRemote, "" )
		MAP_INTERFACE( IBlueResFileSystem )
	EXPOSURE_END()
}