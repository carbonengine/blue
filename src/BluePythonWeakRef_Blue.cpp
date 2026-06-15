// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "BluePythonWeakRef.h"

#include "IBlueOS.h"

BLUE_DEFINE( BluePythonWeakRef );

const Be::ClassInfo* BluePythonWeakRef::ExposeToBlue()
{
	EXPOSURE_BEGIN
	( 
		BluePythonWeakRef, 
		"BluePythonWeakRef(obj) -> creates a new BluePythonWeakRef object\n"
		"  holding a weak reference to 'obj'."
	)
		MAP_INTERFACE( BluePythonWeakRef )

		MAP_METHOD_AND_WRAP
		(
			"__init__",
			PyInit,
			"Construct a BluePythonWeakRef to the given object.\n"
			":param obj: object to create reference to"
		)
		
		MAP_PROPERTY_READONLY
		(
			"object",
			GetObject,
			"Gets the object referenced, or None if it has died."
		)

		MAP_ATTRIBUTE
		(
			"callback",
			m_callback,
			"Python callable that is called when the object dies",
			Be::READWRITE
		)
	EXPOSURE_END()
}

#endif
