#include "StdAfx.h"
#include "BlackReader.h"
#include "include/IBlueOS.h"
#include "include/IBluePaths.h"

BLUE_DEFINE( BlackReader );

const Be::ClassInfo* BlackReader::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlackReader, "BlackReader constructs blue objects from black files." )
		MAP_INTERFACE( BlackReader )
		MAP_INTERFACE( IRootReader )
		MAP_INTERFACE( IBlueObjectBuilder )
		MAP_INTERFACE( ICacheable )

		MAP_METHOD_AND_WRAP
		(
			"CreateObjectFromStream",
			CreateObjectFromStream,
			"Creates an object from a binary representation read from the given stream.\n"
			"\n"
			"Arguments:\n"
			"  stream\n"
			"Returns:\n"
			"  The object corresponding to the binary representation."
		)
		MAP_METHOD_AND_WRAP
		(
			"CreateObjectFromFile",
			CreateObjectFromFile,
			"Creates an object from a binary representation read from the given file.\n"
			"\n"
			"Arguments:\n"
			"  filename\n"
			"Returns:\n"
			"  The object corresponding to the binary representation."
		)

		MAP_METHOD_AND_WRAP
		(
			"GetVersionsSupported",
			GetVersionsSupported,
			"A list of black file versions supported."
		)
		
	EXPOSURE_END()
}