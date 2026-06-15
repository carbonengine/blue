// Copyright © 2012 CCP ehf.

#include "StdAfx.h"
#include "BlueMemStream.h"

// Class registration info
BLUE_DEFINE( MemStream );

const Be::ClassInfo* MemStream::ExposeToBlue()
{
	EXPOSURE_BEGIN( MemStream, "A memory stream" )
		MAP_INTERFACE( MemStream )
		MAP_INTERFACE( IBlueMemStream )
		MAP_INTERFACE( IBlueStream )
		MAP_INTERFACE( ICacheable )

		MAP_ATTRIBUTE( "size", mSize, "size", Be::READ )
		MAP_ATTRIBUTE( "pos", mPosition, "position", Be::READ )
	EXPOSURE_END()
}