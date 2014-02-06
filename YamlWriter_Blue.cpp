#include "StdAfx.h"
#include "YamlWriter.h"
#include "include/IBluePersist.h"
#include "include/IBlueOS.h"

BLUE_DEFINE( YamlWriter );

const Be::ClassInfo* YamlWriter::ExposeToBlue()
{
	EXPOSURE_BEGIN( YamlWriter, "YamlWriter writes blue objects to red files." )
		MAP_INTERFACE( YamlWriter )

		MAP_ATTRIBUTE
		(
			"skipDefaults",
			m_skipDefaults,
			"If set (default) then attributes with default values are not written out.",
			Be::READWRITE
		)
		
		MAP_METHOD_AND_WRAP
		(
			"WriteObjectToString",
			WriteObjectToString,
			"Returns a string representation of an object in yaml format.\n"
			"\n"
			"Arguments:\n"
			"  obj - The object to write\n"
			"Returns:\n"
			"A string in yaml format."
		)

		MAP_METHOD_AND_WRAP
		(
			"WriteObjectToStream",
			WriteObjectToStream,
			"Writes a string representation of an object in yaml format to the given stream.\n"
			"\n"
			"Arguments:\n"
			"  obj - The object to write\n"
			"  stream - An IBlueStream object\n"
		)

		MAP_METHOD_AND_WRAP
		(
			"WriteObjectToFile",
			WriteObjectToFile,
			"Writes a string representation of an object in yaml format to the given file.\n"
			"\n"
			"Arguments:\n"
			"  obj - The object to write\n"
			"  filename - The name of the file\n"

		)
	EXPOSURE_END()
}