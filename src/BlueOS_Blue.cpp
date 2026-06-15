// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#include "BlueOS.h"

#if __APPLE__
#include <sys/sysctl.h>
#endif

#if BLUE_WITH_PYTHON

static PyObject *PyCarbonIoFastWakeup( PyObject* self, PyObject* args)
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "fast wakeup has no more effect", 1 );
	Py_RETURN_FALSE;
}

static PyObject *PyCarbonIoManualWakeup( PyObject* self, PyObject* args)
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "manual wakeup has no more effect", 1 );
	Py_RETURN_FALSE;
}

void SetCrashKeyValues( const std::string& k, const std::string& v )
{
	if( !BeCrashes )
	{
		return;
	}
	BeCrashes->SetCrashKeyValue( k.c_str(), v.c_str() );
}

MAP_FUNCTION_AND_WRAP( 
	"SetCrashKeyValues", 
	SetCrashKeyValues, 
	"Sets arbitrary key values for crash dump uploads\n"
	":param key: key string\n"
	":param value: value string" );

void EnableCrashReporting( bool enable )
{
	if( !BeCrashes )
	{
		return;
	}

	BeCrashes->EnableCrashReporting( enable );
}

MAP_FUNCTION_AND_WRAP( 
	"EnableCrashReporting", 
	EnableCrashReporting, 
	"Enable or disable crash reporting\n"
	":param enable: True to enable and False to disable" );

bool IsCrashReportingEnabled()
{
	if( !BeCrashes )
	{
		return false;
	}

	return BeCrashes->IsCrashReportingEnabled();
}

MAP_FUNCTION_AND_WRAP(
	"IsCrashReportingEnabled",
	IsCrashReportingEnabled,
	"Check if crash report uploading is currently enabled" );

#ifdef _WIN32
static PyObject *PyGetExeFilePids( PyObject* self, PyObject* args)
{
	if( !PyArg_ParseTuple( args, "" ) )
	{
		return nullptr;
	}

	DWORD processIds[1024];
	DWORD bytesReturned = 0;
	if( !EnumProcesses( processIds, sizeof( processIds ), &bytesReturned ) )
	{
		PyErr_SetString( PyExc_RuntimeError, "EnumProcesses failed" );
		return nullptr;
	}

	PyObject* list = PyList_New( 0 );

	int numProcesses = bytesReturned / sizeof( DWORD );
	for( int i = 0; i < numProcesses; ++i )
	{
		long pid = processIds[i];
		HANDLE ph = OpenProcess( PROCESS_QUERY_INFORMATION, FALSE, pid );

		if( ph )
		{
			char processFileName[MAX_PATH];
			DWORD bufferSize = MAX_PATH;
			DWORD length = GetProcessImageFileName( ph, processFileName, bufferSize );
			const int lengthOfExeFile = 11; // Length of 'exefile.exe'
			if( length > lengthOfExeFile )
			{
				char* p = processFileName + length - lengthOfExeFile;
				if( _stricmp( "exefile.exe", p ) == 0 )
				{
					PyList_Append( list, BluePy( PyLong_FromLong( pid ) ) );
				}
			}

			CloseHandle( ph );
		}
	}

	return list;
}
#elif __APPLE__

static PyObject *PyGetExeFilePids( PyObject* self, PyObject* args)
{
    if( !PyArg_ParseTuple( args, "" ) )
    {
        return nullptr;
    }
    std::vector<kinfo_proc> result;
    int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };

    while( true )
    {
        size_t length = 0;
        if( sysctl( name, ( sizeof( name ) / sizeof( *name ) ) - 1, nullptr, &length, nullptr, 0 ) )
        {
            return PyErr_SetFromErrno( PyExc_OSError );
        }

        result.resize( length / sizeof( kinfo_proc ) );

        auto err = sysctl( name, ( sizeof( name ) / sizeof( *name ) ) - 1, result.data(), &length, nullptr, 0 );
        if( err == 0 )
        {
            break;
        }
        if( err == -1 )
        {
            return PyErr_SetFromErrno( PyExc_OSError );
        }
        if( err == ENOMEM )
        {
            continue;
        }
    }
    
    PyObject* list = PyList_New( 0 );

    for( auto proc : result )
    {
        if( strcmp( proc.kp_proc.p_comm, "Exefile" ) == 0 )
        {
            PyList_Append( list, BluePy( PyLong_FromLong( proc.kp_proc.p_pid ) ) );
        }
    }

    return list;
}
#endif
#endif

const Be::ClassInfo* BlueOS::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueOS, "BlueOS" )

		MAP_INTERFACE( IBlueOS )

		// fps stuff
		MAP_ATTRIBUTE( "framesTotal", m_pumpTicksTotal, "Total frames rendered", Be::READ )
		MAP_ATTRIBUTE( "ioRunsTotal", m_ioRunsTotal, "Total python IO runs", Be::READ )
		MAP_ATTRIBUTE( "fps", mFps, "Frames per sec.", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "fpsRefreshRate", mFpsRefreshRate, "Time between recalc of fps.", Be::READWRITE )
		MAP_ATTRIBUTE( "lockFramerate", mLockFramerate, "FPS lock value.", Be::READWRITE )

		// Simulation clock management
		MAP_METHOD_AS_METHOD( 
			"EnableSimDilation", 
			PyEnableSimDilation, 
			"Call this to make this node responsible for running its own simulation clock.  What is done cannot be undone.\n"
			":param offset: sim clock offset\n"
			":type offset: long\n"
			":rtype: None"
			)
		MAP_ATTRIBUTE("minSimDilation", mMinSimDilation, "The minimum dilation factor allowed.  For instance, setting this to .25 will allow the sim clock to run at quarter-time and no slower.  Useless unless EnableSimDilation has been called.", Be::READWRITE )
		MAP_ATTRIBUTE("maxSimDilation", mMaxSimDilation, "The maximum dilation factor allowed.  For instance, setting this to 4 will allow the sim clock to run at quadruple time but no faster.  The default of 1 is almost surely what you want here.  Useless unless EnableSimDilation has been called.", Be::READWRITE )
		MAP_ATTRIBUTE("simDilation", mSimDilation, "The current sim time dilation factor.", Be::READ )
		MAP_ATTRIBUTE("desiredSimDilation", mDilationSyncFactor, "The current desired sim time dilation factor, for use in interface and the like.", Be::READ )
		MAP_ATTRIBUTE("dilationOverloadThreshold", mDilationOverloadThreshold, "", Be::READWRITE )
		MAP_ATTRIBUTE("dilationUnderloadThreshold", mDilationUnderloadThreshold, "", Be::READWRITE )
		MAP_ATTRIBUTE("dilationOverloadAdjustment", mDilationOverloadAdjustment, "", Be::READWRITE )
		MAP_ATTRIBUTE("dilationUnderloadAdjustment", mDilationUnderloadAdjustment, "", Be::READWRITE )
		
		MAP_METHOD_AS_METHOD
		(
			"RegisterClientIDForSimTimeUpdates",
			PyRegisterClientIDForSimTimeUpdates,
			"Given a single clientID, this function will register them for updates of our sim clock, assuming we are the master of it.\n"
			":param clientID: client ID\n"
			":type clientID: long\n"
			":rtype: None"
		)
		MAP_METHOD_AS_METHOD(
			"UnregisterClientIDForSimTimeUpdates",
			PyUnregisterClientIDForSimTimeUpdates,
			"Given a single clientID, this function will unregister them for updates of our sim clock.\n"
			":param clientID: client ID\n"
			":type clientID: long\n"
			":rtype: None"
		)

		// "slug" stuff
		MAP_ATTRIBUTE( "slugTimeMinMs", mSlugTimeMinMs, "Minimum slug time, in ms", Be::READWRITE )
		MAP_ATTRIBUTE( "slugTimeMaxMs", mSlugTimeMaxMs, "Maximum slug time, in ms", Be::READWRITE )

		// variable-rate ticking stuff
		MAP_ATTRIBUTE( "useNominalDeltaT", mUseNominalDeltaT,
			"Use a nominal deltaT rather than a measured one",
			Be::READWRITE )
		MAP_ATTRIBUTE( "useSmoothedDeltaT", mUseSmoothedDeltaT,
			"Use the smoothed deltaT rather than the 'raw' one",
			Be::READWRITE )
		MAP_ATTRIBUTE( "nominalDeltaT_sec", mNominalDeltaTSec,
			"Specifies the nominal deltaT value",
			Be::READWRITE )
		MAP_ATTRIBUTE( "timeScaler", mTimeScaler,
			"A purely diagnostic scaling value (usually 1.0f)",
			Be::READWRITE )

		MAP_METHOD
		(
			"CarbonIoFastWakeup",
			PyCarbonIoFastWakeup,
			"When True, wakeup blue as fast as possible from CarbonIo\n"
			":param wakeup: turn fast wake up on/off (evaluated as bool)\n"
			":type wakeup: bool\n"
			":returns: previous fast wake up state\n"
			":rtype: bool"
		)

		MAP_METHOD
		(
			"CarbonIoManualWakeup",
			PyCarbonIoManualWakeup,
			"When True, cause a manual 'tick' of CarbonIO (for it to wake up tasklets) whenever Blue wakes up.\n"
			":param manual: turn manual wake up on/off (evaluated as bool)\n"
			":type manual: bool\n"
			":returns: previous manual wake up state\n"
			":rtype: bool"
		)

		// other stuff
		MAP_PROPERTY
		(
			"sleeptime",
			GetSleepTime, SetSleepTime,
			"Deprecated (Sleep in ms. for pumping.)\n"
		)
		MAP_ATTRIBUTE( "overridefg",		mOverrideFG, "Override foreground mode", Be::READWRITE | Be::PERSIST )
		MAP_ATTRIBUTE( "debuglevel",		mDebugLevel, "Level of debug checks", Be::READWRITE )
		MAP_ATTRIBUTE(
			"languageID",
			mLanguageID,
			"Language ID for resource loading",
			Be::READWRITE
		)

		// BeInfo
		MAP_ATTRIBUTE( "miniDump", mMiniDump, "Have ExeFile generate minidumps", Be::READWRITE )
		MAP_ATTRIBUTE( "pid", mPID, "Process ID", Be::READ )

		MAP_ATTRIBUTE( "externalTime", mExternalTime, "time spent outside OS", Be::READ )
		MAP_ATTRIBUTE( "useRDTSC", mUseRDTSC, "use the rdtsc opcode for performance measurements", Be::READWRITE )
#if BLUE_WITH_PYTHON
		MAP_ATTRIBUTE( "frameClock", mFrameClock, "Clock that ticks on frame basis", Be::READWRITE )
#endif
		MAP_ATTRIBUTE
		( 
			"advanceTimeInPump", 
			mAdvanceTimeInPump, 
			"If true (the default) then Pump will advance the time. This can be set to false to disable\n"
			"time advancement - used by butter smooth rendering where fixed time steps are used and time\n"
			"is set explicitly with SetTime.",
			Be::READWRITE
		)

		MAP_PROPERTY
		(
			"frameTimeTimeout",
			GetFrameTimeTimeout, SetFrameTimeTimeout,
			"Time out value, in milliseconds, for frame time. If frame time exceeds the given\n"
			"time out value the process is assumed to be hanging and will be treated as a crash.\n"
			"Set this value to 0 to disable hang detection altogether."
		)

		MAP_PROPERTY
		(
			"desiredFrameTimeMilliseconds",
			GetDesiredFrameTimeMilliseconds, SetDesiredFrameTimeMilliseconds,
			"Set the value, in milliseconds, for the desired duration of one frame in milliseconds.\n"
			"Any time left over at the end of a frame will be used for sleep.\n"
			"If the value is set to 0 or 1, the process will yield each frame, but never sleep.\n"
		)

		MAP_METHOD_AS_METHOD
		(
			"GetTime",
			PyGetTime,
			"DEPRECATED!\nReturns world time\n" 
			":param timeType: zero for \"wallclock\" time and non-zero for \"wallclock\" time now\n"
			":type timeType: Optional[int]\n"
			":rtype: long"
			)  // TDTODO - Depricate me!
		MAP_METHOD_AS_METHOD
		(
			"GetWallclockTime",
			PyGetWallclockTime,
			"Returns real world time, as you would see on a clock on a wall, from the beginning of the frame\n" 
			":rtype: long"
		)
		MAP_METHOD_AS_METHOD
		(
			"GetWallclockTimeNow",
			PyGetWallclockTimeNow,
			"Returns real world time, as you would see on a clock on a wall, right now\n" 
			":rtype: long"
		)
		MAP_METHOD_AS_METHOD
		(
			"GetSimTime",
			PyGetSimTime,
			"Returns the current simulation time\n" 
			":rtype: long"
		)
		MAP_METHOD_AS_METHOD
		(
			"GetCycles",
			PyGetCycles,
			"Returns a tuple of number of cpu cycles and cpu frequency.\n" 
			":rtype: (long, long)"
		)
		MAP_METHOD_AS_METHOD
		(
			"SetTime",
			PySetTime,
			"Sets world time\n" 
			":param time: new time\n"
			":type time: long\n"
			":rtype: None"
		)
		MAP_METHOD_AS_METHOD
		(
			"TimeDiffInMs",
			PyTimeDiffInMs,
			"Returns time diff in ms.\n" 
			":param t1: first time\n"
			":type t1: long\n"
			":param t2: second time\n"
			":type t2: long\n"
			":rtype: int | long"
		)
		MAP_METHOD_AS_METHOD
		(
			"TimeDiffInUs",
			PyTimeDiffInUs,
			"Returns time diff in microsecs.\n" 
			":param t1: first time\n"
			":type t1: long\n"
			":param t2: second time\n"
			":type t2: long\n"
			":rtype: int | long"
		)
#if CCP_STACKLESS
        MAP_ATTRIBUTE( "timeSyncAdjust", mTimeSyncAdjust, "Our current time sync nudge, used to maintain the sync over time",  Be::READWRITE )
        MAP_ATTRIBUTE( "timeSyncAdjustFactor", mTimeSyncAdjustFactor, "What factor of the frametime we're allowed to shift in order to maintain time sync",  Be::READWRITE )
#endif
		
		MAP_METHOD_AS_METHOD
		(
			"TimeFromDouble",
			PyTimeFromDouble,
			"Converts double time to UTC time.\n" 
			":param time: double time\n"
			":type time: float\n"
			":rtype: long"
		)
		MAP_METHOD_AS_METHOD
		(
			"TimeAsDouble",
			PyTimeAsDouble,
			"Converts UTC time to double time.\n" 
			":param time: UTC time\n"
			":type time: long\n"
			":rtype: double"
		)
		MAP_METHOD_AS_METHOD
		(
			"TimeAddSec",
			PyTimeAddSec,
			"Returns UTC time plus double secs.\n" 
			":param time: UTC time\n"
			":type time: long\n"
			":param secs: seconds\n"
			":type secs: float\n"
			":rtype: long"
		)
		
		MAP_METHOD_AS_METHOD
		(
			"GetTimeParts",
			PyGetTimeParts,
			"Returns list of time parts\n" 
			":param time: UTC time\n"
			":type time: long\n"
			":rtype: (int, int, int, int, int, int, int)"
		)
		MAP_METHOD_AS_METHOD
		(
			"GetTimeFromParts",
			PyGetTimeFromParts,
			"Returns UTC time from parts\n" 
			":param year:\n"
			":type year: int\n"
			":param month:\n"
			":type month: int\n"
			":param day:\n"
			":type day: int\n"
			":param hour:\n"
			":type hour: int\n"
			":param minute:\n"
			":type minute: int\n"
			":param second:\n"
			":type second: int\n"
			":param milliseconds:\n"
			":type milliseconds: int\n"
			":rtype: long\n"
		)

		MAP_METHOD_AS_METHOD
		(
			"GetCpuTime",
			PyGetCpuTime,
			"DEPRECATED!\nReturns CPU clock values\n" 
			":raises RuntimeError: always!"
		)

		MAP_METHOD_AS_METHOD
		(
			"SetAppTitle",
			PySetAppTitle,
			"Set text in titlebar.\n" 
			":param title: app title\n"
			":type title: str\n"
			":rtype: None"
		)

		MAP_METHOD_AS_METHOD
		(
			"ShellExecute",
			PyShellExecute,
			"Win32 shell execute (with constraints)\n"
			":param path: res path to file or URL\n"
			":type path: basestring\n"
			":param parameter: program arguments\n"
			":type parameter: Optional[basestring]\n"
			":rtype: None"
		)

		MAP_METHOD_AND_WRAP
		(
			"Pump",
			PumpOS,
			"Pump up the volume"
		)

		MAP_METHOD_AS_METHOD
		( 
			"Terminate", 
			PyTerminate, 
			"Terminates the process forcefully.\n"
			":param retCode: integer value returned as the process return code. Default is 0."
			":type retCode: Optional[int]\n"
			":rtype: None"
		)

#if CCP_STACKLESS
		MAP_METHOD_AS_METHOD(
			"StacklessMain",
			PyStacklessMain,
			"" 
		)
#endif

		MAP_METHOD
		(
			"GetExeFilePids",
			PyGetExeFilePids,
			"Returns a list of process ids for any ExeFile instances running\n"
			":rtype: list[long]"
		)

		MAP_METHOD_AND_WRAP
		(
			"GetStartupArgs",
			GetStartupArgs,
			"Returns a list of startup arguments"
		)

		MAP_METHOD_AND_WRAP
		(
			"SetStartupArgs",
			SetStartupArgs,
			"Sets the startup arguments\n"
			":param args: list of startup arguments"
		)

		MAP_METHOD_AND_WRAP
		(
			"HasStartupArg",
			HasStartupArg,
			"Returns true if the given argument is present on the command line\n"
			":param arg: argument"
		)

		MAP_METHOD_AND_WRAP
		(
			"GetStartupArgValue",
			GetStartupArgValue,
			"Gets the value associated with the given argument, if present on the command line."
			"If the argument is not present, the return value is an empty string.\n"
			":param arg: argument key"
		)
    
        MAP_METHOD_AND_WRAP
        (
            "ShowErrorMessageBox",
            ShowErrorMessageBox,
            "Shows a modal message box indicating an error.\n"
			":param title: message box title\n"
			":param message: text message to show in the box"
        )


		MAP_PROPERTY_READONLY	
		(
			"isOnMainTasklet",
			IsOnMainTasklet,
			"Returns true if the code is run on the main tasklet, false otherwise"
		)

	EXPOSURE_END()

}
