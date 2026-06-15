// Copyright © 2012 CCP ehf.

#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "BluePython.h"

BLUE_DEFINE_NONEXPOSED( BlueCpuUsage );

const Be::ClassInfo* BlueCpuUsage::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueCpuUsage, "" )
		MAP_ATTRIBUTE("timestamp", timestamp, "Point in time when the sample was taken", Be::READ)
		MAP_ATTRIBUTE("userProcessCpuUsage", userProcessCpuUsage, "user-time spent in process", Be::READ)
		MAP_ATTRIBUTE("userThreadCpuUsage", userThreadCpuUsage, "user-time spent in thread", Be::READ)
		MAP_ATTRIBUTE("kernelProcessCpuUsage", kernelProcessCpuUsage, "kernel time spent in process", Be::READ)
		MAP_ATTRIBUTE("kernelThreadCpuUsage", kernelThreadCpuUsage, "kernel time spent in thread", Be::READ)
		MAP_ATTRIBUTE("virtualMemory", pageFileUsage, "Memory used by the process according to the OS", Be::READ)
		MAP_ATTRIBUTE("pythonMemoryUsage", pythonMemoryUsage, "Amount of memory used by Python", Be::READ)
		MAP_ATTRIBUTE("workingSetSize", workingSetSize, "", Be::READ)
		MAP_ATTRIBUTE("pageFaultCount", pageFaultCount, "Number of page faults", Be::READ)
		MAP_ATTRIBUTE("fps", fps, "Frames per second", Be::READ)
		MAP_ATTRIBUTE("taskletsYielding", taskletsYielding, "Number of tasklets that yielded this frame", Be::READ)
		MAP_ATTRIBUTE("taskletsSleeping", taskletsSleeping, "Number of tasklets that slept this frame", Be::READ)
		MAP_ATTRIBUTE("taskletsSchedulerDuration", taskletsSchedulerDuration, "Time spent processing tasklets this frame in ms", Be::READ)
		MAP_ATTRIBUTE("taskletsSchedulerMaxDuration", taskletsSchedulerMaxDuration, "Desired max time for processing tasklets per frame in ms", Be::READ)
		MAP_ATTRIBUTE("taskletsSchedulerDurationOvershoot", taskletsSchedulerDurationOvershoot, "Overshoot of max time this frame in ms", Be::READ)
		MAP_ATTRIBUTE("taskletsQueued", taskletsQueued, "Number of tasklets queued for execution", Be::READ)
		MAP_ATTRIBUTE( "taskletsActive", taskletsActive, "Number of active Tasklets", Be::READ )
		MAP_ATTRIBUTE( "scheduleManagersActive", scheduleManagersActive, "Number of active ScheduleManagers", Be::READ )
		MAP_ATTRIBUTE( "channelsActive", channelsActive, "Number of active Channels", Be::READ )
		MAP_ATTRIBUTE( "taskletsProcessed", taskletsProcessed, "Number of Tasklets completed last tick", Be::READ )
		MAP_ATTRIBUTE( "taskletsSwitched", taskletsSwitched, "Number of Tasklets switched last tick", Be::READ )
	EXPOSURE_END()
}

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
			":rtype: scheduler.tasklet"
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
