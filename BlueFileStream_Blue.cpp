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
			"Reads the entire contents of the given file. The file operations are atomic,\n"
			"meaning that other processes will not change the contents of the file while\n"
			"is being read."
		)

		MAP_METHOD_AND_WRAP
		(
			"ReadEntireFileWithYield",
			ReadEntireFileWithYield,
			"Reads the entire contents of the given file. The file operations are\n"
			"done on a background thread and the calling tasklet yields until it\n"
			"is done. The file operations are atomic,\n"
			"meaning that other processes will not change the contents of the file while\n"
			"is being read."
		)
	EXPOSURE_END()
}