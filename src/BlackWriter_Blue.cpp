// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "BlackWriter.h"
#include "BlueMemStream.h"
#include "IBlueOS.h"

BLUE_DEFINE( BlackWriter );

const Be::ClassInfo* BlackWriter::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlackWriter, "BlackWriter writes blue objects to black files." )
		MAP_INTERFACE( BlackWriter )

		MAP_METHOD_AND_WRAP
		(
			"WriteObjectToStream",
			WriteObjectToStream,
			"Writes a binary representation of an object to the given stream.\n"
			":param obj: The object to write\n"
			":param stream: An IBlueStream object\n"
		)
		MAP_METHOD_AND_WRAP
		(
			"WriteObjectToFile",
			WriteObjectToFile,
			"Writes a binary representation of an object to the given file.\n"
			":param obj: The object to write\n"
			":param filename: The name of the file\n"

		)

		MAP_PROPERTY_READONLY
		(
			"currentVersion",
			GetCurrentVersion,
			"The version number of black files written."
		)
		
		MAP_METHOD_AND_WRAP
		(
			"GetStrings",
			GetStrings,
			"Get the list of all strings stored."
		)

		MAP_METHOD_AND_WRAP
		(
			"GetWStrings",
			GetWStrings,
			"Get the list of all wide strings stored."
		)

		EXPOSURE_END()
}
