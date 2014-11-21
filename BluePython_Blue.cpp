////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		October 2012
// Copyright:	CCP 2012
//

#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "BluePython.h"

BLUE_DEFINE_NO_REGISTER( BluePyOS );
BLUE_REGISTER_CLASS_EX( BluePyOS, DynamicSingletonFactory<OBluePyOS>::Create, Be::ClassRegistration::DISABLE_PYTHON_CONSTRUCTION );

const Be::ClassInfo* BluePyOS::ExposeToBlue()
{
	EXPOSURE_BEGIN( BluePyOS, "" )
		MAP_INTERFACE(IBluePyOS)

		MAP_ATTRIBUTE("exceptionHandler", mExceptionHandler, "Global exception handler function", Be::READWRITE)
		MAP_ATTRIBUTE("cpuUsage", mCpuUsage, "CPU usage", Be::READ)
		MAP_ATTRIBUTE( "markupZonesInPython", mMarkupZonesInPython, "Mark up zones in Python code?", Be::READWRITE )

        MAP_ATTRIBUTE( "performanceUpdateFrequency", mPerformanceUpdateFrequency, "Frequency at which process peformance statistics are updated (100000000=default).", Be::READWRITE )
		MAP_ATTRIBUTE( "timesliceWarning", mSliceWarning, "Max. acceptable timeslice in ms. for tasklet (0=disable).", Be::READWRITE )
		MAP_ATTRIBUTE( "beNiceSlice", mBeNiceSlice, "Timeslice in ms. (for BeNice yielding.).", Be::READWRITE )
		MAP_ATTRIBUTE( "packaged", mPackaged, "Is this a packaged client?", Be::READ )

		MAP_ATTRIBUTE
		(
			"scatterEvent",
			m_scatterEvent,
			"Python callback for scattering events from C++",
			Be::READWRITE
		)
		MAP_ATTRIBUTE
		(
			"sendEvent",
			m_sendEvent,
			"Python callback for sending events from C++",
			Be::READWRITE
		)
		MAP_ATTRIBUTE
		(
			"chainEvent",
			m_chainEvent,
			"Python callback for chaining events from C++",
			Be::READWRITE
		)

#if CCP_STACKLESS
		MAP_ATTRIBUTE( "taskletTimer", mTTimer, "the timer for tasklet objects", Be::READ )
		////////////////////////////////////////////////////////////////////////////
		//               synchro
		MAP_ATTRIBUTE(
			"synchro", 
			mPySynchro, 
			"Synchronization stuff",
			Be::READ
			)
		
		MAP_ATTRIBUTE(
			"contextHooks",
			mContextHooks,
			"callables to be called on context switch",
			Be::READ
			)
#endif

		MAP_ATTRIBUTE( "softspace", mSoftspace, "", Be::READWRITE ) //for python write

		MAP_METHOD_AS_METHOD
		(
			"AddExitProc",
			PyAddExitProc, 
			"Add exit procedure" 
		)
		MAP_METHOD_AS_METHOD
		(
			"GetArg",
			PyGetArg, 
			"Returns list of command line arguments." 
		)
		MAP_METHOD_AS_METHOD
		(
			"GetEnv",
			PyGetEnv, 
			"Returns dictionary of environment variables." 
		)
		MAP_METHOD_AS_METHOD
		(
			"DumpState",
			PyDumpState, 
			"Dumps state, or something." 
		)
		MAP_METHOD_AS_METHOD
		(
			"_DumpLockCount",
			Py_DumpLockCount, 
			"Show lock count on class instances"
		)
		MAP_METHOD_AS_METHOD
		(
			"write",
			Pywrite, 
			"For file io redirection"
		)
		MAP_METHOD_AS_METHOD
		(
			"CreateTasklet",
			PyCreateTasklet,	
			"The good ol' uthread.new" 
		)
#ifdef _WIN32
		MAP_METHOD_AS_METHOD
		(
			"SpyDirectory",
			PySpyDirectory,	
			"SpyDirectory" 
		)
#endif
		MAP_METHOD_AS_METHOD
		(
			"NextScheduledEvent",
			PyNextScheduledEvent,	
			"NextScheduledEvent" 
		)

#ifdef _WIN32
		MAP_METHOD_AS_METHOD
		(
			"GetClipboardData",
			PyGetClipboardData,	
			"GetClipboardData" 
		)
		MAP_METHOD_AS_METHOD
		(
			"SetClipboardData",
			PySetClipboardData,	
			"SetClipboardData" 
		)
		MAP_METHOD_AS_METHOD
		(
			"GetThreadTimes",
			PyGetThreadTimes,	
			"GetThreadTimes" 
		)
		MAP_METHOD_AS_METHOD
		(
			"ProbeStuff",
			PyProbeStuff,	
			"ProbeStuff" 
		)
#if CCP_STACKLESS
		MAP_METHOD_AS_METHOD
		(
			"GetTimeSinceSwitch",
			PyGetTimeSinceSwitch, 
			"time since last tasklet switch"
		)
#endif
#endif
		MAP_METHOD_AS_METHOD
		(
			"BeNice",
			PyBeNice,		
			"BeNice" 
		)
		MAP_METHOD_AS_METHOD
		(
			"XUtil_Index",
			PyXUtil_Index,			
			"dbutil.Index substitue" 
		)
		MAP_METHOD_AS_METHOD
		(
			"XUtil_Filter",
			PyXUtil_Filter,		
			"Filtering of DBRows" 
		)
		MAP_METHOD_AS_METHOD
		(
			"XUtil_SwapLists",
			PyXUtil_SwapLists,	
			"Swap the contents of two lists" 
		)
		MAP_METHOD_AS_METHOD
		(
			"GetMaxRunTime",
			PyGetMaxRunTime, 
			"Get maximum watchdog runtime" 
		)
		MAP_METHOD_AS_METHOD
		(
			"SetMaxRunTime",
			PySetMaxRunTime, 
			"Set maximum watchdog runtime" 
		)

		MAP_METHOD
		( 
			"SetLogEchoFunction", 
			PySetLogEchoFunction, 
			"Sets the function to echo log messages to.\n\n"
			"The function passed in must take two arguments (int,string)"
		)
		MAP_METHOD
		(
			"GetLogEchoFunction",
			PyGetLogEchoFunction,
			"Gets the log echo threshold and function currently used to echo log messages as a tuple."
		)
		
	EXPOSURE_END()
}

#endif
