// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "BlackReader.h"
#include "IBlueOS.h"
#include "IBluePaths.h"

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
			":param stream: blue stream\n"
			":returns: the object corresponding to the binary representation\n"
			":raises RuntimeError: on error"
		)
		MAP_METHOD_AND_WRAP
		(
			"CreateObjectFromFile",
			CreateObjectFromFile,
			"Creates an object from a binary representation read from the given file.\n"
			":param filename: res path to binary file\n"
			":returns: the object corresponding to the binary representation\n"
			":raises RuntimeError: on error"
		)

		MAP_METHOD_AND_WRAP
		(
			"GetVersionsSupported",
			GetVersionsSupported,
			"A list of black file versions supported."
		)
		
	EXPOSURE_END()
}
