// Copyright © 2024 CCP ehf.

#include "StdAfx.h"
#include "AllReferences.h"


BLUE_DEFINE( AllReferences );

const Be::ClassInfo* AllReferences::ExposeToBlue()
{
	EXPOSURE_BEGIN( AllReferences, "AllReferences maintans reference information (attributes, list indices) between objects." )
		MAP_INTERFACE( AllReferences )

		MAP_METHOD_AND_WRAP(
			"Update",
			Update,
			"Performs a timeslice update of reference information. When this method returns True, the object has collected a\n"
			"new generation of reference information that can be used in GetReferences or FindInterface methods.\n"
			"Users of this object need to call Update in a loop (in a tasklet).\n\n"
			":param time: maximum time in seconds that this method can run before returning\n"
			":returns: True if the new generation of reference data is available; False if the user needs to keep calling Update" )

		MAP_METHOD_AND_WRAP(
			"GetReferences",
			GetReferences,
			"Returns any objects (accesible from the root) that reference the given object. The result is returned as a list or tuples\n"
			"(parent, reference_type, index) where parent is an IRoot object referencing the given object, type is 0 for attribute,\n"
			"1 for list elements, 2 - for dictionary keys. The index is eitheer the attibute name, list index or dict key.\n "
			"The results are returned for data already gathered by Update method and may be out of date.\n\n"
			":param obj: child object" )

		MAP_METHOD_AND_WRAP(
			"FindInterface",
			FindInterface,
			"Similar to blue.FindInterface. The results are returned for data already gathered by Update method and may be out of date.\n\n"
			":param obj: root object for the search\n"
			":param iid: interface name" )

		MAP_METHOD_AND_WRAP(
			"SetRoot",
			SetRoot,
			"Sets the current root for all queries. This call will invalidate all data and the update cycle needs to be restarted.\n\n"
			":param root: new object root" )

		MAP_METHOD_AND_WRAP(
			"GetRoot",
			GetRoot,
			"Returns the current root object." )

	EXPOSURE_END()
}
