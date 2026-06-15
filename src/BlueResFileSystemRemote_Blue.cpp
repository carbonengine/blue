// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "BlueResFileSystemRemote.h"

BLUE_DEFINE_NONEXPOSED( BlueResFileSystemRemote );

const Be::ClassInfo* BlueResFileSystemRemote::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueResFileSystemRemote, "" )
		MAP_INTERFACE( IBlueResFileSystem )
	EXPOSURE_END()
}