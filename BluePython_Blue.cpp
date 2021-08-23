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

		MAP_ATTRIBUTE( "interpreterMode", mInterpreterMode, "True if running in Python interpreter mode.", Be::READ )

		MAP_METHOD_AS_METHOD
		(
			"AddExitProc",
			PyAddExitProc, 
			"Add exit procedure\n" 
			":param proc: function that is called before the process terminates\n"
			":type proc: ()->None\n"
			":rtype: None"
		)
		MAP_METHOD_AS_METHOD
		(
			"GetArg",
			PyGetArg, 
			"Returns list of command line arguments.\n" 
			":rtype: list[unicode]"
		)
		MAP_METHOD_AS_METHOD
		(
			"GetEnv",
			PyGetEnv, 
			"Returns dictionary of environment variables.\n" 
			":param name: optional environment variable name; if passed the function returns its value, otherwise returns all variables\n"
			":type name: basestring\n"
			":rtype: unicode | dict[basestring, basestring]"
		)
		MAP_METHOD_AS_METHOD
		(
			"DumpState",
			PyDumpState, 
			"Dumps state, or something.\n" 
			":rtype: None"
		)
		MAP_METHOD_AS_METHOD
		(
			"write",
			Pywrite, 
			"For file io redirection\n"
			":param s:\n"
			":type s: str\n"
			":rtype: None"
		)
		MAP_METHOD_AS_METHOD
		(
			"CreateTasklet",
			PyCreateTasklet,	
			"The good ol' uthread.new\n" 
			":param func: function\n"
			":type func: callable\n"
			":param args: function arguments\n"
			":type args: tuple\n"
			":param kwargs: function keyword arguments\n"
			":type kwargs: dict\n"
			":rtype: stackless.tasklet"
		)
		MAP_METHOD_AS_METHOD
		(
			"NextScheduledEvent",
			PyNextScheduledEvent,	
			"NextScheduledEvent\n" 
			":param ms: milliseconds\n"
			":type ms: int\n"
			":rtype: None"
		)

		MAP_METHOD_AS_METHOD
		(
			"SetClipboardData",
			PySetClipboardData,	
			"SetClipboardData\n" 
			":jessica-deprecated: use blue.clipboard\n"
			":param data:\n"
			":type data: basestring\n"
			":rtype: None"
		)
#if CCP_STACKLESS
		MAP_METHOD_AS_METHOD
		(
			"GetTimeSinceSwitch",
			PyGetTimeSinceSwitch, 
			"time since last tasklet switch\n"
			":rtype: float"
		)
#endif
		MAP_METHOD_AS_METHOD
		(
			"BeNice",
			PyBeNice,		
			"BeNice\n" 
			":param timeslice:\n"
			":type timeslice: Optional[float]\n"
			":rtype: None"
		)
		MAP_METHOD_AS_METHOD
		(
			"XUtil_Index",
			PyXUtil_Index,			
			"dbutil.Index substitue\n" 
			":param rows:\n"
			":type rows: list[DBRow]\n"
			":param keyName:\n"
			":type keyName: str\n"
			":param result:\n"
			":type result: dict[Any, DBRow]\n"
			":rtype: dict[Any, DBRow]"
		)
		MAP_METHOD_AS_METHOD
		(
			"XUtil_Filter",
			PyXUtil_Filter,		
			"Filtering of DBRows\n" 
			":param rows:\n"
			":type rows: set[DBRow]\n"
			":param indices:\n"
			":type indices: list[None | int]\n"
			":param condvalues:\n"
			":type condvalues: list[None | int]\n"
			":param retset:\n"
			":type retset: set[DBRow]\n"
			":rtype: set[DBRow]"
		)
		MAP_METHOD_AS_METHOD
		(
			"GetMaxRunTime",
			PyGetMaxRunTime, 
			"Get maximum watchdog runtime\n" 
			":rtype: float"
		)
		MAP_METHOD_AS_METHOD
		(
			"SetMaxRunTime",
			PySetMaxRunTime, 
			"Set maximum watchdog runtime\n" 
			":param time:\n"
			":type time: float\n"
			":rtype: None"
		)

		MAP_METHOD
		( 
			"SetLogEchoFunction", 
			PySetLogEchoFunction, 
			"Sets the function to echo log messages to.\n\n"
			"The function passed in must take two arguments (int,string)\n"
			":param threshold: minimal severity level for the message to be logged\n"
			":type threshold: int\n"
			":param callback: function that is called for each log message\n"
			":type callback: (int, str)->None"
		)
		MAP_METHOD
		(
			"GetLogEchoFunction",
			PyGetLogEchoFunction,
			"Gets the log echo threshold and function currently used to echo log messages as a tuple.\n"
			":rtype: (int, (int, str)->None)"
		)
		
	EXPOSURE_END()
}

#endif
