// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "BlueThreadMonitor.h"

BLUE_DEFINE( BlueThreadMonitor );

const Be::ClassInfo* BlueThreadMonitor::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueThreadMonitor, "Thread monitor" )
		MAP_METHOD_AND_WRAP
		(
			"GetStatus",
			GetStatus,
			"Returns the status of all known threads as a list of tuples containing\n"
			"thread ID and status.\n\n"
			"The status value is one of:\n"
			" 0 - idle\n"
			" 1 - working\n"
			" 2 - loading from disk\n"
			" 3 - downloading from CDN\n"
		)
	EXPOSURE_END()
}