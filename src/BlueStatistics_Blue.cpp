// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "Blue.h"
#include "IBlueOS.h"
#include "BlueStatistics.h"

#if BLUE_WITH_PYTHON
extern const char *Immortalize( PyObject *s );
#endif

BLUE_DEFINE( CcpStatisticsEntry );

const Be::ClassInfo* CcpStatisticsEntry::ExposeToBlue()
{
	EXPOSURE_BEGIN( CcpStatisticsEntry, "Statistic entry" )

		MAP_PROPERTY
		(
			"name",
			GetName, SetName,
			"Name of statistic"
		)

		MAP_PROPERTY
		(
			"description",
			GetDescription, SetDescription,
			"Description of statistic"
		)
		
		MAP_PROPERTY
		(
			"resetPerFrame",
			GetResetPerFrame, SetResetPerFrame,
			"If set, the statistic is reset every frame"
		)

		MAP_PROPERTY
		(
			"type",
			GetType, SetType,
			"Type of statistic (time, low counter, high counter, memory)"
		)
		
		MAP_PROPERTY_READONLY
		(
			"value",
			GetValue,
			"Value of statistic"
		)

		MAP_PROPERTY_READONLY
		(
			"peak",
			GetPeak,
			"Peak value of statistic"
		)

		MAP_METHOD_AND_WRAP
		(
			"Inc",
			Inc,
			"Increment counter by 1"
		)

		MAP_METHOD_AND_WRAP
		(
			"Dec",
			Dec,
			"Decrement counter by 1"
		)

		MAP_METHOD_AND_WRAP
		(
			"Add",
			Add,
			"Add the given value to the statistic\n"
			":param value: increment value"
		)
		MAP_METHOD_AND_WRAP
		(
			"Set",
			Set,
			"Set the statistic to the given value\n"
			":param value: new value"
		)
		MAP_METHOD_AND_WRAP
		(
			"ResetPeak",
			ResetPeak,
			"Resets the statistic peak value"
		)
	EXPOSURE_END()
}


BLUE_DEFINE( BlueStatisticsTelemetryConfig );

const Be::ClassInfo* BlueStatisticsTelemetryConfig::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueStatisticsTelemetryConfig, "Configuration for telemetry capture" )

	MAP_ATTRIBUTE
	(
		"applicationName",
		m_applicationName,
		"Name provided by profiled application, presented in connection list of a Profiler tool",
		Be::READWRITE
	)

	MAP_ATTRIBUTE
	(
		"captureDuration",
		m_captureDurationSec,
		"Capture duration for a telemetry session in seconds (0 = until manually stopped)",
		Be::READWRITE
	)

	MAP_ATTRIBUTE
	(
		"trackMemory",
		m_trackMemoryAllocations,
		"If set, memory allocations/deallocations are visible in Profiler tool",
		Be::READWRITE
	)

	EXPOSURE_END()
}


BLUE_DEFINE( BlueStatistics );

#if BLUE_WITH_PYTHON
PyObject* BlueStatistics::PyGetDescriptions( PyObject* self, PyObject* args )
{
	PyObject* statsDict = PyDict_New();

	const char* typeNames[CST_TYPE_COUNT] = {"counterHigh", "counterLow", "memory", "time"};

	CcpStatistics::EntryArray& a = CcpStatistics::GetEntryArray();
	for( CcpStatistics::EntryArray::iterator it = a.begin(); it != a.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(2);

		CcpStaticStatisticsEntry* entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyUnicode_FromString( entry->GetDescription().c_str() ) );
		PyTuple_SetItem( pyEntry, 1, PyUnicode_FromString( typeNames[entry->GetType()] ) );

		PyDict_SetItemString( statsDict, entry->GetName().c_str(), pyEntry );
	}

	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(2);

		auto entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyUnicode_FromString( entry->GetDescription().c_str() ) );
		PyTuple_SetItem( pyEntry, 1, PyUnicode_FromString( typeNames[entry->GetType()] ) );

		PyDict_SetItemString( statsDict, entry->GetName().c_str(), pyEntry );
	}

	return statsDict;
}


PyObject* BlueStatistics::PyGetStats( PyObject* self, PyObject* args )
{
	PyObject* statsList = PyList_New(0);

	CcpStatistics::EntryArray& a = CcpStatistics::GetEntryArray();
	for( CcpStatistics::EntryArray::iterator it = a.begin(); it != a.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(3);

		CcpStaticStatisticsEntry* entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyUnicode_FromString( entry->GetName().c_str() ) );
		PyTuple_SetItem( pyEntry, 1, PyFloat_FromDouble( entry->GetValue() ) );
		PyTuple_SetItem( pyEntry, 2, PyFloat_FromDouble( entry->GetPeak() ) );

		// OutputDebugString( (*it)->Describe() );

		PyList_Append( statsList, pyEntry ); 
	}

	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(3);

		auto entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyUnicode_FromString( entry->GetName().c_str() ) );
		PyTuple_SetItem( pyEntry, 1, PyFloat_FromDouble( entry->GetValue() ) );
		PyTuple_SetItem( pyEntry, 2, PyFloat_FromDouble( entry->GetPeak() ) );

		// OutputDebugString( (*it)->Describe() );

		PyList_Append( statsList, pyEntry ); 
	}

	return statsList;
}

PyObject* BlueStatistics::PyGetValues( PyObject* self, PyObject* args )
{
	PyObject* statsDict = PyDict_New();

	CcpStatistics::EntryArray& a = CcpStatistics::GetEntryArray();
	for( auto it = a.begin(); it != a.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(2);

		CcpStaticStatisticsEntry* entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyFloat_FromDouble( entry->GetValue() ) );
		PyTuple_SetItem( pyEntry, 1, PyFloat_FromDouble( entry->GetPeak() ) );

		PyDict_SetItemString( statsDict, entry->GetName().c_str(), pyEntry );
	}

	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(2);

		auto entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyFloat_FromDouble( entry->GetValue() ) );
		PyTuple_SetItem( pyEntry, 1, PyFloat_FromDouble( entry->GetPeak() ) );

		PyDict_SetItemString( statsDict, entry->GetName().c_str(), pyEntry );
	}

	return statsDict;
}

PyObject* PyResetPeaks( PyObject* self, PyObject* args )
{
	CcpStatistics::EntryArray& a = CcpStatistics::GetEntryArray();

	for( CcpStatistics::EntryArray::iterator it = a.begin(); it != a.end(); ++it )
	{
		auto entry = *it;

		entry->ResetPeak();
	}

	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		auto entry = *it;

		entry->ResetPeak();
	}

	Py_RETURN_NONE;
}

PyObject* PyResetDerived( PyObject* self, PyObject* args )
{
	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		CcpDerivedStatisticsEntry* entry = *it;

		entry->Reset();
		entry->ResetPeak();
	}

	Py_RETURN_NONE;
}

PyObject* BlueStatistics::PyGetSingleStat( PyObject* self, PyObject* args )
{
	const char *text;

	if( !PyArg_ParseTuple(args, "s", &text) )
	{
		return NULL;
	}

	if( !text || !text[0] )
	{
		return NULL;
	}

	const CcpStatistics::EntryArray& a = CcpStatistics::GetEntryArray();
	for( CcpStatistics::EntryArray::const_iterator it = a.begin(); it != a.end(); ++it )
	{
		if( (*it)->GetName() == text )
		{
			return PyFloat_FromDouble( (*it)->GetValue() );
		}		
	}

	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		if( (*it)->GetName() == text )
		{
			return PyFloat_FromDouble( (*it)->GetValue() );
		}		
	}

	Py_RETURN_NONE;
}

namespace
{

PyObject* PyEnterZone( PyObject* self, PyObject* args )
{
#if CCP_TELEMETRY_ENABLED
	PyObject* zoneO;

	if( !PyArg_ParseTuple( args, "O", &zoneO ) )
	{
		return nullptr;
	}
	const char* zone = Immortalize( zoneO );
	if( !zone )
	{
		return nullptr;
	}

	auto frame = PyEval_GetFrame();
	if ( !frame )
	{
		return nullptr;
	}
	auto codeObj = PyFrame_GetCode( frame );  // Returns a strong reference
	auto fileName = Immortalize( codeObj->co_filename );
	if (!fileName)
	{
		return nullptr;
	}
	CcpTelemetryEnterZone( frame, zone, fileName, static_cast<uint32_t>( PyFrame_GetLineNumber( frame ) ) );
	Py_XDECREF( codeObj );  // Release the reference to the frame code
#endif
	Py_RETURN_NONE;
}

PyObject* PyLeaveZone( PyObject* self, PyObject* args )
{
#if CCP_TELEMETRY_ENABLED
	CcpTelemetryLeaveZone( PyEval_GetFrame() );
#endif
	Py_RETURN_NONE;
}

PyObject* PyAppendToZone( PyObject* self, PyObject* args )
{
#if CCP_TELEMETRY_ENABLED
	PyObject* appendTextO;

	if( !PyArg_ParseTuple( args, "O", &appendTextO ) )
	{
		return nullptr;
	}

	const char* appendText = Immortalize( appendTextO );
	if( !appendText )
	{
		return nullptr;
	}

	CcpTelemetryZoneAddText( PyEval_GetFrame(), appendText );
#endif
	Py_RETURN_NONE;
}

#if CCP_TELEMETRY_ENABLED
static uint64_t s_timespanId = 0xf00000000;
#endif

PyObject* PyBeginTimeSpan( PyObject* self, PyObject* args )
{
#if CCP_TELEMETRY_ENABLED
	PyObject* labelO;

	if( !PyArg_ParseTuple( args, "O", &labelO ) )
	{
		return nullptr;
	}

	const char* label = Immortalize( labelO );
	if( !label )
	{
		return nullptr;
	}

	++s_timespanId;

	return PyLong_FromLongLong( s_timespanId );
#else
    return PyLong_FromLongLong( 0 );
#endif
}

PyObject* PyEndTimeSpan( PyObject* self, PyObject* args )
{
#if CCP_TELEMETRY_ENABLED
	uint64_t id = 0;
	PyObject* labelO;

	if( !PyArg_ParseTuple( args, "LO", &id, &labelO ) )
	{
		return nullptr;
	}

	const char* label = Immortalize( labelO );
	if( !label )
	{
		return nullptr;
	}
#endif
	Py_RETURN_NONE;
}


PyObject* PyRegister( PyObject* self, PyObject* args )
{
	PyObject* obj = NULL;
	if( !PyArg_ParseTuple( args, "O", &obj ) )
	{
		return NULL;
	}

	CcpStatisticsEntry* stat = BluePythonCast<CcpStatisticsEntry*>( obj );
	if( !stat )
	{
		PyErr_SetString( PyExc_TypeError, "Register expects a CcpStatisticsEntry" );
		return NULL;
	}

		stat->GetAttachedStat();

	Py_RETURN_NONE;
}

PyObject* PyUnregister( PyObject* self, PyObject* args )
{
	PyObject* obj = NULL;
	if( !PyArg_ParseTuple( args, "O", &obj ) )
	{
		return NULL;
	}

	CcpStaticStatisticsEntry* stat = BluePythonCast<CcpStaticStatisticsEntry*>( obj );
	if( !stat )
	{
		PyErr_SetString( PyExc_TypeError, "Unregister expects a CcpStatisticsEntry" );
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject* PyRegisterDerived( PyObject* self, PyObject* args )
{
	PyObject* obj = NULL;
	if( !PyArg_ParseTuple( args, "O", &obj ) )
	{
		return NULL;
	}

	CcpDerivedStatisticsEntry* stat = BluePythonCast<CcpDerivedStatisticsEntry*>( obj );
	if( !stat )
	{
		PyErr_SetString( PyExc_TypeError, "Register expects a CcpDerivedStatisticsEntry" );
		return NULL;
	}

	CcpStatistics::RegisterDerived( stat );

	Py_RETURN_NONE;
}

PyObject* PyUnregisterDerived( PyObject* self, PyObject* args )
{
	PyObject* obj = NULL;
	if( !PyArg_ParseTuple( args, "O", &obj ) )
	{
		return NULL;
	}

	CcpDerivedStatisticsEntry* stat = BluePythonCast<CcpDerivedStatisticsEntry*>( obj );
	if( !stat )
	{
		PyErr_SetString( PyExc_TypeError, "Unregister expects a CcpDerivedStatisticsEntry" );
		return NULL;
	}

	CcpStatistics::UnregisterDerived( stat );

	Py_RETURN_NONE;
}

PyObject* PyFind( PyObject* self, PyObject* args )
{
	CcpStatistics* pThis = BluePythonCast<CcpStatistics*>( self );

	char* name = NULL;
	if( !PyArg_ParseTuple( args, "s", &name ) )
	{
		return NULL;
	}

	const CcpStatistics::EntryArray& a = pThis->GetEntryArray();
	for( CcpStatistics::EntryArray::const_iterator it = a.begin(); it != a.end(); ++it )
	{
		if( strcmp( (*it)->GetName().c_str(), name ) == 0 )
		{
			CcpStaticStatisticsEntry* entry = *it;
			CcpStatisticsEntryPtr pyEntry;
			pyEntry.CreateInstance();
			pyEntry->AttachStat( entry );
			return BlueWrapObjectForPython( pyEntry );
		}		
	}

	auto b = pThis->GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		if( strcmp( (*it)->GetName().c_str(), name ) == 0 )
		{
			CcpStaticStatisticsEntry* entry = *it;
			CcpStatisticsEntryPtr pyEntry;
			pyEntry.CreateInstance();
			pyEntry->AttachStat( entry );
			return BlueWrapObjectForPython( pyEntry );
		}		
	}

	Py_RETURN_NONE;
}

} // anonymous namespace

#endif

const Be::ClassInfo* BlueStatistics::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueStatistics, "Trinity statistics gathering" )
		MAP_METHOD
		(
			"Register",
			PyRegister,
			"Register the given statistic\n"
			":param entry: statistic entry\n"
			":type entry: CcpStatisticsEntry\n"
			":rtype: None"
		)

		MAP_METHOD
		(
			"Unregister",
			PyUnregister,
			"Unregister the given statistic\n"
			":param entry: statistic entry\n"
			":type entry: CcpStatisticsEntry\n"
			":rtype: None"
		)
		
		MAP_METHOD
		(
			"RegisterDerived",
			PyRegisterDerived,
			"Register the given derived statistic\n"
			":param entry: derived statistic entry\n"
			":type entry: CcpDerivedStatisticsEntry\n"
			":rtype: None"
		)

		MAP_METHOD
		(
			"UnregisterDerived",
			PyUnregisterDerived,
			"Unregister the given derived statistic\n"
			":param entry: derived statistic entry\n"
			":type entry: CcpDerivedStatisticsEntry\n"
			":rtype: None"
		)
		
		MAP_METHOD
		(
			"Find",
			PyFind,
			"Find the CcpStatisticsEntry for the given name\n"
			":param name: statistic name\n"
			":type name: str\n"
			":rtype: CcpStatisticsEntry"
		)

		MAP_METHOD
		(
			"GetDescriptions", 
			PyGetDescriptions, 
			"Get description and type of stats\n"
			":rtype: dict[str, CcpStatisticsEntry]"
		)

		MAP_METHOD
		( 
			"GetStats", 
			PyGetStats, 
			"Get current stats\n"
			":rtype: list[(str, float, float)]"
		)

		MAP_METHOD
		( 
			"GetValues", 
			PyGetValues, 
			"Get current stats values. Returns a dict with tuples (value,peak)\n"
			":rtype: dict[str, (float, float)]"
		)

		MAP_METHOD
		( 
			"ResetPeaks", 
			PyResetPeaks, 
			"Resets all stats peak values.\n" 
			":rtype: None"
		)

		MAP_METHOD
		( 
			"ResetDerived", 
			PyResetDerived, 
			"Resets all derived stats (including peak values).\n" 
			":rtype: None"
		)

		MAP_METHOD
		( 
			"GetSingleStat", 
			PyGetSingleStat, 
					
			"Get the current value of a single stat.\n" 
			":param stat: the name of the statistic.\n"
			":type stat: str\n"
			":rtype: CcpStatisticsEntry | None"
		)

		MAP_METHOD_AND_WRAP
		( 
			"SetAccumulator", 
			SetAccumulator, 
			"Sets an accumulator (such as a line graph) for the given statistic\n"
			":param name: stat name\n"
			":param accumulator: new accumulator"
		)

		MAP_METHOD_AND_WRAP
		( 
			"GetAccumulator", 
			GetAccumulator, 
			"Gets an accumulator (such as a line graph) for the given statistic\n"
			":param name: stat name"
		)

#if CCP_TELEMETRY_ENABLED

		MAP_METHOD_AND_WRAP
		(
			"StartTelemetry", 
			StartTelemetry, 
			"Starts a profiling/telemetry session, visible in a Profiler tool.\n"
			":param server: the name of the application being profiled."
		)

		MAP_METHOD_AND_WRAP
		(
			"StartTelemetryFromConfig",
			StartTelemetryFromConfig,
			"Starts a profiling/telemetry session based on config, visible in a Profiler tool.\n"
			":param config: the telemetry configuration to use.\n"
			":type config: BlueStatisticsTelemetryConfig"
		)

		MAP_METHOD_AND_WRAP
		(
			"StartTimedTelemetry", 
			StartTimedTelemetry,
			"Starts a profiling/telemetry session for a set time, visible in a Profiler tool.\n"
			":param server: the name of the application being profiled.\n"
			":param samplePeriod: Time to sample for (in seconds) default of 0 means infinite sampling."
		)

		MAP_METHOD_AND_WRAP
		(
			"StartTelemetryDump", 
			StartTelemetryDump, 
			"Works just like StartTelemetry, except that instead of talking to the server it dumps data to disk"
			"\n in the current users Documents directory."
			"\nWill overwrite preexisting files."
			"\nCannot run alongside a regular TCP based Telemetry session."
			"\nStopped using StopTelemetry."
			"\n"
			":param dumpFolder: a string, the path to dump intermediate data to.\n"
			":param samplePeriod: Time to sample for (in seconds). 0 means infinite sampling."
		)

		MAP_METHOD_AND_WRAP
		(
			"PauseTelemetry",
			PauseTelemetry,
			"Pauses Telemetry capture. (deprecated)"
		)

		MAP_METHOD_AND_WRAP
		(
			"ResumeTelemetry",
			ResumeTelemetry,
			"Resumes Telemetry capture. (deprecated)"
		)

		MAP_METHOD_AND_WRAP
		(
			"StopTelemetry",
			StopTelemetry,
			"Disconnect from a Telemetry server."
		)

		MAP_PROPERTY_READONLY
		(
			"isTelemetryConnectionRequested",
			IsTelemetryConnectionRequested,
			"Is Telemetry profiler connection pending?"
		)

		MAP_PROPERTY_READONLY
		(
			"telemetrySamplingTimeLeft",
			TelemetrySamplingTimeLeft,
			"Seconds left to sample with Telemetry."
		)

		MAP_PROPERTY_READONLY
		(
			"isTelemetryConnected",
			IsTelemetryConnected,
			"Is Telemetry connected to a profiler?"
		)

		MAP_PROPERTY_READONLY
		(
			"isTelemetryPaused",
			IsTelemetryPaused,
			"Is Telemetry paused? (deprecated)"
		)

		MAP_PROPERTY_READONLY
		(
			"isTelemetryStarted",
			IsTelemetryStarted,
			"Is Telemetry instrumentation active?"
		)

		MAP_PROPERTY
		(
			"isCppCaptureEnabled",
			IsCppCaptureEnabled, SetCppCaptureEnabled,
			"If set (default), then both Python and C++ zones are captured. If not set, then\n"
			"only Python zones are captured.\n\n"
			"Setting this to False can reduce the size of Telemetry captures drastically,\n"
			"making it easier to grab longer sessions if you are focusing on Python code."
		)

		MAP_PROPERTY(
			"isTaskletCaptureEnabled",
			IsTaskletCaptureEnabled,
			SetTaskletCaptureEnabled,
			"If set (default), then we record a \"Tasklet\" tack in telemetry with all active tasklets" )

		MAP_PROPERTY(
			"isPythonCaptureEnabled",
			IsPythonCaptureEnabled,
			SetPythonCaptureEnabled,
			"If set, then all Python calls are captured" )

		MAP_ATTRIBUTE(
			"telemetryMaxThreadCount",
			m_telemetryMaxThreadCount,
			"Maximum number of threads/tasklets expected during telemetry capture. This attribute needs to be set before the first\n"
			"call to PrimeTelemetry/StartTelemetry. Changing it after starting telemetry capture will not have an effect.",
			Be::READWRITE )
#endif

		MAP_METHOD_AND_WRAP
		(
			"SetTimelineSectionName",
			SetTimelineSectionName,
			"Changes the name of the global state. This helps identifying regions in the Telemetry timeline view.\n"
			":param name: section name"
		)

		MAP_METHOD
		(
			"EnterZone", 
			PyEnterZone, 
			"Enter a Telemetry zone. There must be a corresponding call to LeaveZone\n"
			":param name: must be static string, such as the name of a function.\n"
			":type name: str\n"
			":rtype: None"
		)
		
		MAP_METHOD
		(
			"LeaveZone", 
			PyLeaveZone, 
			"Leave a Telemetry zone. This must match an EnterZone call.\n"
			":param name: must be static string, such as the name of a function.\n"
			":type name: str\n"
			":rtype: None"
		)
		
		MAP_METHOD
		(
			"AppendToZone", 
			PyAppendToZone, 
			"Appends a string to the latest EnterZone's name."
			"\n"
			"\nArguments:"
			"\ntext - The text to append to the zone name."
		)

		MAP_METHOD
		(
			"BeginTimeSpan",
			PyBeginTimeSpan,
			"Adds a time span to Telemetry.\n"
			":param label: The label to give the time span\n"
			":type label: long\n"
			":returns: an id to use with EndTimeSpan\n"
			":rtype: long"
		)

		MAP_METHOD
		(
			"EndTimeSpan",
			PyEndTimeSpan,
			"Ends a time span started with BeginTimeSpan.\n"
			":param id: the id returned from BeginTimeSpan\n"
			":type id: long\n"
			":param label: The closing label for the time span - can be used to\n"
			"          indicate success or failure, for example\n"
			":type label: str\n"
			":rtype: None"
		)

		MAP_METHOD_AND_WRAP
		(
			"BeginCapture",
			BeginCapture,
			"Begins capturing per-frame data for all statistics entries"
		)

		MAP_METHOD_AND_WRAP
		(
			"EndCapture",
			EndCapture,
			"Ends capturing per-frame data started with BeginCapture. Returns a dict with\n"
			"per-frame statistic entries values"
		)

	EXPOSURE_END()
}
