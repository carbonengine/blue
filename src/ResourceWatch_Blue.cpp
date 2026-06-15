// Copyright © 2017 CCP ehf.

#include "StdAfx.h"
#include "ResourceWatch.h"

BLUE_DEFINE( ResourceWatch );

const Be::ClassInfo* ResourceWatch::ExposeToBlue()
{
	EXPOSURE_BEGIN( ResourceWatch, "" )
		MAP_INTERFACE( ResourceWatch )

		MAP_METHOD_AND_WRAP_OPTIONAL_ARGS(
			"__init__",
			Watch,
			1,
			"Watches provided resource and calls functions when resource is loaded or unloaded.\n"
			":param resource: blue resource to watch"
		)

		MAP_METHOD_AND_WRAP(
			"Watch",
			Watch,
			"Watches provided resource and calls functions when resource is loaded or unloaded.\n"
			":param resource: blue resource to watch"
		)

		MAP_ATTRIBUTE(
			"onLoaded",
			m_onLoaded,
			"Callback function that is called when a resource is loaded; the function is called\n"
			"with a single parameter - the resource object",
			Be::READWRITE
		)

		MAP_ATTRIBUTE(
			"onUnladed",
			m_onUnloaded,
			"Callback function that is called when a resource is unloaded; the function is called\n"
			"with a single parameter - the resource object",
			Be::READWRITE
		)
	EXPOSURE_END()
}