// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#include "IBlueOS.h"
#include "BlueMemoryTracker.h"

BLUE_DEFINE( MemoryTracker );

// Create an instance of the memory tracker
static CMemoryTracker s_memoryTrackerInstance;

// This pointer is used to register the memory tracker as an object under the blue
// module in Python (see in BluePython.cpp).
MemoryTracker* BeMemoryTracker = &s_memoryTrackerInstance;
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "memoryTracker", BeMemoryTracker );


const Be::ClassInfo* MemoryTracker::ExposeToBlue()
{
	EXPOSURE_BEGIN( MemoryTracker, "" )
		MAP_INTERFACE( MemoryTracker )

		MAP_ATTRIBUTE
		(
			"loggingThreshold",
			m_loggingThreshold,
			"Threshold for memory logging - whenever any memory stat changes more\n"
			"than this threshold since last logging, all memory stats are logged out.",
			Be::READWRITE
		)

		MAP_METHOD_AND_WRAP
		(
			"SummaryReport",
			SummaryReport,
			"Generates a summary report and outputs it to the given filename\n"
			":param filename: path to the file"
		)

		MAP_METHOD_AND_WRAP( 
			"DumpReportAsText", 
			DumpReportAsText, 
			"Write text report into a file\n"
			":param filename: path to file" );
		MAP_METHOD_AND_WRAP( 
			"DumpReportAsBinary", 
			DumpReportAsBinary, 
			"Write binary report into a file\n"
			":param filename: path to file" );

		MAP_METHOD_AND_WRAP
		( 
			"GetCount", 
			GetCount,
			"Get the count of all tracked allocations, i.e. any allocations\n"
			"from the Blue heap with memory tracking enabled."
		)
		MAP_METHOD_AND_WRAP
		( 
			"GetSize", 
			GetSize,
			"Get the total size of all tracked allocations, i.e. any allocations\n"
			"from the Blue heap with memory tracking enabled."
		)
		MAP_METHOD_AND_WRAP
		( 
			"CallstackCaptureEnable", 
			CallstackCaptureEnable, 
			"Enable/disable callstack capture for new allocations\n"
			":param enable: True to enable callstack capture" 
			)

	EXPOSURE_END()
}
