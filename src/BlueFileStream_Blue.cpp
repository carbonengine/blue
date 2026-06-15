// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#include "BlueFileStream.h"

#include "IBlueOS.h"

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
			"is being read.\n"
			":param filename: path to the file\n"
			":raises RuntimeError: on error"
		)

		MAP_METHOD_AND_WRAP
		(
			"ReadEntireFileWithYield",
			ReadEntireFileWithYield,
			"Reads the entire contents of the given file. The file operations are\n"
			"done on a background thread and the calling tasklet yields until it\n"
			"is done. The file operations are atomic,\n"
			"meaning that other processes will not change the contents of the file while\n"
			"is being read.\n"
			":param filename: path to the file\n"
			":raises RuntimeError: on error"
		)

		MAP_METHOD_AND_WRAP
		(
			"close",
			Close,
			"Close the file stream."
		)
	EXPOSURE_END()
}
