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
		MAP_INTERFACE( IBlueStream )

		MAP_METHOD_AND_WRAP
		(
			"ReadEntireFile",
			ReadEntireFile,
			"Reads the entire contents of the given file"
		)
	EXPOSURE_END()
}
