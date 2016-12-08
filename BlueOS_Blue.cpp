#include "StdAfx.h"

#include "BlueOS.h"

#ifdef _WIN32
#include <Psapi.h>
#endif

#if BLUE_WITH_PYTHON

extern bool g_carbonIoFastWakeup;
static PyObject *PyCarbonIoFastWakeup( PyObject* self, PyObject* args)
{
	PyObject *arg;
	if ( !PyArg_ParseTuple( args, "O", &arg ) )
	{
		return NULL;
	}
	int v = PyObject_IsTrue( arg );
	if (v == -1)
	{
		return NULL;
	}
	PyObject *res = g_carbonIoFastWakeup ? Py_True : Py_False;
	g_carbonIoFastWakeup = v != 0;
	Py_INCREF( res );
	return res;
}

extern bool g_carbonIoManualWakeup;
static PyObject *PyCarbonIoManualWakeup( PyObject* self, PyObject* args)
{
	PyObject *arg;
	if ( !PyArg_ParseTuple( args, "O", &arg ) )
	{
		return NULL;
	}
	int v = PyObject_IsTrue( arg );
	if (v == -1)
	{
		return NULL;
	}
	PyObject *res = g_carbonIoManualWakeup ? Py_True : Py_False;
	g_carbonIoManualWakeup = v != 0;
	Py_INCREF( res );
	return res;
}

PyObject* PySetCrashKeyValues( PyObject* self, PyObject* args )
{
	if( !BeCrashes )
	{
		Py_RETURN_NONE;
	}

	PyUnicodeObject* k = NULL;
	PyUnicodeObject* v = NULL;

	if( PyArg_ParseTuple( args, "UU", &k, &v ) )
	{
		wchar_t key[128];
		wchar_t val[256];
		PyUnicode_AsWideChar(k,&key[0],127);
		key[127] = 0;
		PyUnicode_AsWideChar(v,&val[0],255);
		val[255] = 0;
		BeCrashes->SetCrashKeyValueW(key, val);

		Py_RETURN_NONE;
	}
	return NULL;
}

MAP_FUNCTION( "SetCrashKeyValues", PySetCrashKeyValues, "Sets arbitrary key values for Breakpad uploads" );

PyObject* PySetCrashSessionFileDescriptor( PyObject* self, PyObject* args )
{
	if( !BeCrashes )
	{
		Py_RETURN_NONE;
	}

	int fd;
	if( PyArg_ParseTuple( args, "i", &fd ) )
	{
		BeCrashes->SetSessionFileDescriptor(fd);

		Py_RETURN_NONE;
	}
	return NULL;
}

MAP_FUNCTION( "SetCrashSessionFileDescriptor", PySetCrashSessionFileDescriptor, "Sets a session file descriptor, for writing log info in case of a crash" );

PyObject* PySetCrashUserId( PyObject* self, PyObject* args )
{
	if( !BeCrashes )
	{
		Py_RETURN_NONE;
	}

	int id;
	if( PyArg_ParseTuple( args, "i", &id ) )
	{
		BeCrashes->SetUserId(id);

		Py_RETURN_NONE;
	}
	return NULL;
}

MAP_FUNCTION( "SetCrashUserId", PySetCrashUserId, "Sets a user id, for writing to session file in case of a crash" );

PyObject* PySetCrashSessionId( PyObject* self, PyObject* args )
{
	if( !BeCrashes )
	{
		Py_RETURN_NONE;
	}

	int64_t id;
	if( PyArg_ParseTuple( args, "L", &id ) )
	{
		BeCrashes->SetSessionId(id);

		Py_RETURN_NONE;
	}
	return NULL;
}

MAP_FUNCTION( "SetCrashSessionId", PySetCrashSessionId, "Sets a session id, for writing to session file in case of a crash" );

PyObject* PyEnableBreakpad( PyObject* self, PyObject* args )
{
	if( !BeCrashes )
	{
		Py_RETURN_NONE;
	}

	unsigned char enableBreakpad;

	if( PyArg_ParseTuple( args, "b", &enableBreakpad ) )
	{
		BeCrashes->EnableCrashReporting(enableBreakpad != 0);
		Py_RETURN_NONE;
	}

	return NULL;
}

MAP_FUNCTION( "EnableBreakpad", PyEnableBreakpad, "Enable or disable breakpad" );

PyObject* PyIsBreakpadEnabled( PyObject* self, PyObject* args )
{
	if( !BeCrashes )
	{
		Py_RETURN_FALSE;
	}

	if( BeCrashes->IsCrashReportingEnabled() )
	{
		Py_RETURN_TRUE;
	}
	else
	{
		Py_RETURN_FALSE;
	}
}

MAP_FUNCTION( "IsBreakpadEnabled", PyIsBreakpadEnabled, "Check if Breakpad upload is currently enabled" );

PyObject* PySetBuildNumber( PyObject* self, PyObject* args )
{
	if( !BeCrashes )
	{
		Py_RETURN_NONE;
	}

	int buildNo = 999999;
	
	if( PyArg_ParseTuple( args, "i", &buildNo ) )
	{
		BeCrashes->SetBuildNumber( buildNo );
		Py_RETURN_NONE;
	}
	else
	{
		// Error set by ParseTuple
		return NULL;
	}
}

MAP_FUNCTION( "SetBreakpadBuildNumber", PySetBuildNumber, "Set the build number for breakpad" );

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
		MAP_METHOD_AS_METHOD( "EnableSimDilation", PyEnableSimDilation, "Call this to make this node responsible for running its own simulation clock.  What is done cannot be undone.")
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
			"Given a single clientID, this function will register them for updates of our sim clock, assuming we are the master of it."
		)
		MAP_METHOD_AS_METHOD(
			"UnregisterClientIDForSimTimeUpdates",
			PyUnregisterClientIDForSimTimeUpdates,
			"Given a single clientID, this function will unregister them for updates of our sim clock."
		)

		// "slug" stuff
		MAP_ATTRIBUTE( "slugTimeMinMs", mSlugTimeMinMs, "Minimum slug time, in ms", Be::READWRITE )
		MAP_ATTRIBUTE( "slugTimeMaxMs", mSlugTimeMaxMs, "Maximum slug time, in ms", Be::READWRITE )

		// variable-rate ticking stuff
		MAP_ATTRIBUTE( "useSimpleCatchupLoop", mUseSimpleCatchupLoop,
			"Use the simple one-call-per-pump loop",
			Be::READWRITE )
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
			"When True, wakeup blue as fast as possible from CarbonIo"
		)

		MAP_METHOD
		(
			"CarbonIoManualWakeup",
			PyCarbonIoManualWakeup,
			"When True, cause a manual 'tick' of CarbonIO (for it to wake up tasklets) whenever Blue wakes up."
		)

		// other stuff
		MAP_ATTRIBUTE( "sleeptime",		mSleepTime,	"Sleep in ms. for pumping", Be::READWRITE | Be::PERSIST )
		MAP_ATTRIBUTE( "overridefg",		mOverrideFG, "Override foreground mode", Be::READWRITE | Be::PERSIST )
		MAP_ATTRIBUTE( "debuglevel",		mDebugLevel, "Level of debug checks", Be::READWRITE )

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

		MAP_METHOD_AS_METHOD
		(
			"GetTime",
			PyGetTime,
			"Returns world time" )  // TDTODO - Depricate me!
		MAP_METHOD_AS_METHOD
		(
			"GetWallclockTime",
			PyGetWallclockTime,
			"Returns real world time, as you would see on a clock on a wall, from the beginning of the frame" 
		)
		MAP_METHOD_AS_METHOD
		(
			"GetWallclockTimeNow",
			PyGetWallclockTimeNow,
			"Returns real world time, as you would see on a clock on a wall, right now" 
		)
		MAP_METHOD_AS_METHOD
		(
			"GetSimTime",
			PyGetSimTime,
			"Returns the current simulation time" 
		)
		MAP_METHOD_AS_METHOD
		(
			"GetCycles",
			PyGetCycles,
			"Returns a tuple of number of cpu cycles and cpu frequency." 
		)
		MAP_METHOD_AS_METHOD
		(
			"SetTime",
			PySetTime,
			"Sets world time" 
		)
		MAP_METHOD_AS_METHOD
		(
			"TimeDiffInMs",
			PyTimeDiffInMs,
			"Returns time diff in ms." 
		)
		MAP_METHOD_AS_METHOD
		(
			"TimeDiffInUs",
			PyTimeDiffInUs,
			"Returns time diff in microsecs." 
		)
#if CCP_STACKLESS
        MAP_ATTRIBUTE( "timeSyncAdjust", mTimeSyncAdjust, "Our current time sync nudge, used to maintain the sync over time",  Be::READWRITE )
        MAP_ATTRIBUTE( "timeSyncAdjustFactor", mTimeSyncAdjustFactor, "What factor of the frametime we're allowed to shift in order to maintain time sync",  Be::READWRITE )
#endif
		
		MAP_METHOD_AS_METHOD
		(
			"TimeFromDouble",
			PyTimeFromDouble,
			"Converts double time to UTC time." 
		)
		MAP_METHOD_AS_METHOD
		(
			"TimeAsDouble",
			PyTimeAsDouble,
			"Converts UTC time to double time." 
		)
		MAP_METHOD_AS_METHOD
		(
			"TimeAddSec",
			PyTimeAddSec,
			"Returns UTC time plus double secs." 
		)
		
		MAP_METHOD_AS_METHOD
		(
			"GetTimeParts",
			PyGetTimeParts,
			"Returns list of time parts" 
		)
		MAP_METHOD_AS_METHOD
		(
			"GetTimeFromParts",
			PyGetTimeFromParts,
			"Returns UTC time from parts" 
		)
		MAP_METHOD_AS_METHOD
		(
			"TimeDiffAsParts",
			PyTimeDiffAsParts,
			"Takes in two times and returns a list specifying the number of years, months, days, hours, minutes, seconds, ms" 
		)
		MAP_METHOD_AS_METHOD
		(
			"FormatUTC",
			PyFormatUTC,
			"Returns UTC time as string" 
		)

		MAP_METHOD_AS_METHOD
		(
			"GetCpuTime",
			PyGetCpuTime,
			"Returns CPU clock values" 
		)

#ifdef _WIN32
		MAP_METHOD_AS_METHOD
		(
			"HeapCompact",
			PyHeapCompact,
			"Attempt to compact the system heap." 
		)
		
		MAP_METHOD_AS_METHOD
		(
			"GlobalMemoryStatus",
			PyGlobalMemoryStatus,
			"Obtains information about the system's current usage of both"
			"physical and virtual memory." 
		)
#endif

		MAP_METHOD_AS_METHOD
		(
			"SetAppTitle",
			PySetAppTitle,
			"Set text in titlebar." 
		)
		MAP_METHOD_AS_METHOD
		(
			"GetAppTitle",
			PyGetAppTitle,
			"Get text in titlebar." 
		)

#ifdef _WIN32
		MAP_METHOD_AS_METHOD
		(
			"ApplyPatch",
			PyApplyPatch,
			"Apply patch file and kill current process"
		)
		MAP_METHOD_AS_METHOD
		(
			"ShellExecute",
			PyShellExecute,
			"Win32 shell execute (with constraints)"
		)
#endif

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
			"Terminates the process forcefully."
			"\n"
			"\nArguments:"
			"\nretCode - optional - integer value returned as the process return code. Default is 0."
		)

#if CCP_STACKLESS
		MAP_METHOD_AS_METHOD(
			"StacklessMain",
			PyStacklessMain,
			"" 
		)
#endif

#ifdef _WIN32
		MAP_METHOD
		(
			"GetExeFilePids",
			PyGetExeFilePids,
			"Returns a list of process ids for any ExeFile instances running"
		)
#endif

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
            "Arguments:\n"
            "title - message box title\n"
            "message - text message to show in the box"
        )


		MAP_PROPERTY_READONLY	
		(
			"isOnMainTasklet",
			IsOnMainTasklet,
			"Returns true if the code is run on the main tasklet, false otherwise"
		)

	EXPOSURE_END()

}
