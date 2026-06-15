// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "IBlueOS.h"
#include "YamlReader.h"
#include "BlueMemStream.h"
#include "IBluePaths.h"

BLUE_DEFINE( YamlReader );

const Be::ClassInfo* YamlReader::ExposeToBlue()
{
	EXPOSURE_BEGIN( YamlReader, "YamlReader constructs blue objects from red files." )
		MAP_INTERFACE( YamlReader )
		MAP_INTERFACE( IRootReader )
		MAP_INTERFACE( IBlueObjectBuilder )
		MAP_INTERFACE( ICacheable )
		MAP_ICACHEABLE_METHODS()

		MAP_ATTRIBUTE
		(
			"isStrict",
			m_isStrict,
			"If set, then unrecognized classes and attributes are treated as errors\n"
			"and an exception is thrown. Otherwise a warning is written to the log.",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		(
			"doInitialize",
			m_doInitialize,
			"If set (the default), then objects are initialized in CreateObject.",
			Be::READWRITE
		)

		MAP_METHOD_AND_WRAP
		(
			"CreateObjectFromString",
			CreateObjectFromString,
			"Creates an object from the given yaml representation.\n"
			":param yamlString: string containing YAML\n"
			":returns: The object corresponding to the yaml representation."
		)
		MAP_METHOD_AND_WRAP
		(
			"CreateObjectFromStream",
			CreateObjectFromStream,
			"Creates an object from a yaml representation read from the given stream.\n"
			":param stream: blue stream\n"
			":returns:  The object corresponding to the yaml representation."
		)
		MAP_METHOD_AND_WRAP
		(
			"CreateObjectFromFile",
			CreateObjectFromFile,
			"Creates an object from a yaml representation read from the given file.\n"
			":param filename: res path\n"
			":returns: The object corresponding to the yaml representation."
		)
	EXPOSURE_END()
}
