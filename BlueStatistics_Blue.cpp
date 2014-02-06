#include "StdAfx.h"
#include "include/Blue.h"
#include "include/IBlueOS.h"

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
			"Add the given value to the statistic"
		)
		MAP_METHOD_AND_WRAP
		(
			"Set",
			Set,
			"Set the statistic to the given value"
		)
		MAP_METHOD_AND_WRAP
		(
			"ResetPeak",
			ResetPeak,
			"Resets the statistic peak value"
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

		PyTuple_SetItem( pyEntry, 0, PyString_FromString( entry->GetDescription().c_str() ) );
		PyTuple_SetItem( pyEntry, 1, PyString_FromString( typeNames[entry->GetType()] ) );

		PyDict_SetItemString( statsDict, entry->GetName().c_str(), pyEntry );
	}

	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(2);

		auto entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyString_FromString( entry->GetDescription().c_str() ) );
		PyTuple_SetItem( pyEntry, 1, PyString_FromString( typeNames[entry->GetType()] ) );

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

		PyTuple_SetItem( pyEntry, 0, PyString_FromString( entry->GetName().c_str() ) );
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

		PyTuple_SetItem( pyEntry, 0, PyString_FromString( entry->GetName().c_str() ) );
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

#if CCP_TELEMETRY_ENABLED

PyObject* PyEnterZone( PyObject* self, PyObject* args )
{
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

	tmTaskletEnter( g_telemetryContext, zone );

	Py_RETURN_NONE;
}

PyObject* PyLeaveZone( PyObject* self, PyObject* args )
{
	tmTaskletLeave( g_telemetryContext );

	Py_RETURN_NONE;
}

PyObject* PyAppendToZone( PyObject* self, PyObject* args )
{
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

	tmTaskletAppendText( g_telemetryContext, appendText );

	Py_RETURN_NONE;
}

static TmU64 s_timespanId = 0xf00000000;

PyObject* PyBeginTimeSpan( PyObject* self, PyObject* args )
{
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
	tmBeginTimeSpan( g_telemetryContext, s_timespanId, TMTSF_NONE, label );

	return PyLong_FromLongLong( s_timespanId );
}

PyObject* PyEndTimeSpan( PyObject* self, PyObject* args )
{
	TmU64 id = 0;
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

	tmEndTimeSpan( g_telemetryContext, id, TMTSF_NONE, label );

	Py_RETURN_NONE;
}

#endif


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

#if CCP_TELEMETRY_ENABLED

PyObject* PyStartTelemetry( PyObject* self, PyObject* args )
{
	BlueStatistics* pThis = BluePythonCast<BlueStatistics*>( self );
	const char *server = "localhost";
	int bufferSize= 8;

	if( !PyArg_ParseTuple( args, "s|i", &server, &bufferSize ) )
	{
		return NULL;
	}

	// we want this in bytes
	bufferSize = bufferSize*1024*1024;
	
	pThis->StartTelemetry( server, bufferSize );

	Py_RETURN_NONE;
}

PyObject* PyStartTelemetryDump( PyObject* self, PyObject* args )
{
	BlueStatistics* pThis = BluePythonCast<BlueStatistics*>( self );
	
	pThis->StartTelemetryDump();

	Py_RETURN_NONE;
}

#endif

} // anonymous namespace

#endif

const Be::ClassInfo* BlueStatistics::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueStatistics, "Trinity statistics gathering" )
		MAP_METHOD
		(
			"Register",
			PyRegister,
			"Register the given statistic"
		)

		MAP_METHOD
		(
			"Unregister",
			PyUnregister,
			"Unregister the given statistic"
		)
		
		MAP_METHOD
		(
			"RegisterDerived",
			PyRegisterDerived,
			"Register the given derived statistic"
		)

		MAP_METHOD
		(
			"UnregisterDerived",
			PyUnregisterDerived,
			"Unregister the given derived statistic"
		)
		
		MAP_METHOD
		(
			"Find",
			PyFind,
			"Find the CcpStatisticsEntry for the given name"
		)

		MAP_METHOD
		(
			"GetDescriptions", 
			PyGetDescriptions, 
			"Get description and type of stats"
		)

		MAP_METHOD
		( 
			"GetStats", 
			PyGetStats, 
			"Get current stats"
		)

		MAP_METHOD
		( 
			"GetValues", 
			PyGetValues, 
			"Get current stats values. Returns a dict with tuples (value,peak)"
		)

		MAP_METHOD
		( 
			"ResetPeaks", 
			PyResetPeaks, 
			"Resets all stats peak values." 
		)

		MAP_METHOD
		( 
			"ResetDerived", 
			PyResetDerived, 
			"Resets all derived stats (including peak values)." 
		)

		MAP_METHOD
		( 
			"GetSingleStat", 
			PyGetSingleStat, 
					
			"Get the current value of a single stat." 
			"\n"
			"\nArguments:"
			"\nstat - a string, the name of the statistic."
		)

		MAP_METHOD_AND_WRAP
		( 
			"SetAccumulator", 
			SetAccumulator, 
			"Sets an accumulator (such as a line graph) for the given statistic"
		)

		MAP_METHOD_AND_WRAP
		( 
			"GetAccumulator", 
			GetAccumulator, 
			"Gets an accumulator (such as a line graph) for the given statistic"
		)

#if CCP_TELEMETRY_ENABLED

		MAP_METHOD
		(
			"StartTelemetry", 
			PyStartTelemetry, 
			"Connects to a Telemetry server and starts gathering data."
			"\n"
			"\nArguments:"
			"\nserver - a string, the network address of the server to connect to."
			"\n  Use 'localhost' to connect to a Visualizer on the local machine."
			"\nbufferSize - Optional. The size of the memory buffer allocated for telemetry data, in megabytes."
		)

		MAP_METHOD
		(
			"StartTelemetryDump", 
			PyStartTelemetryDump, 
			"Works just like StartTelemetry, except that instead of talking to the server it dumps data to disk"
			"\n in the current users Documents directory."
			"\nWill overwrite preexisting files."
			"\nCannot run alongside a regular TCP based Telemetry session."
			"\nStopped using StopTelemetry."
		)

		MAP_METHOD_AND_WRAP
		(
			"PauseTelemetry",
			PauseTelemetry,
			"Pauses Telemetry capture. Ticking and frame boundary information are"
			"\nstill sent over, but high frequency data such as memory events, mutex"
			"\nstates, and zones are discarded. An application can use this function"
			"\nto keep Telemetry live but with very low overhead until a specific"
			"\nproblem area is encountered."
		)

		MAP_METHOD_AND_WRAP
		(
			"ResumeTelemetry",
			ResumeTelemetry,
			"Resumes Telemetry captures."
		)

		MAP_METHOD_AND_WRAP
		(
			"StopTelemetry",
			StopTelemetry,
			"Disconnect from a Telemetry server."
		)

		MAP_PROPERTY_READONLY
		(
			"isTelemetryConnected",
			IsTelemetryConnected,
			"Is Telemetry connected?"
		)

		MAP_PROPERTY_READONLY
		(
			"isTelemetryPaused",
			IsTelemetryPaused,
			"Is Telemetry paused?"
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

		MAP_METHOD_AND_WRAP
		(
			"SetTimelineSectionName",
			SetTimelineSectionName,
			"Changes the name of the global state. This helps identifying regions in the Telemetry timeline view."
		)

		MAP_METHOD
		(
			"EnterZone", 
			PyEnterZone, 
			"Enter a Telemetry zone. There must be a corresponding call to LeaveZone"
			"\n"
			"\nArguments:"
			"\nname - must be static string, such as the name of a function."
		)
		
		MAP_METHOD
		(
			"LeaveZone", 
			PyLeaveZone, 
			"Leave a Telemetry zone. This must match an EnterZone call."
			"\n"
			"\nArguments:"
			"\nname - must be static string, such as the name of a function."
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
			"\n"
			"Arguments:\n"
			"  label - The label to give the time span\n"
			"\n"
			"Returns:\n"
			"  id - an id to use with EndTimeSpan"
		)

		MAP_METHOD
		(
			"EndTimeSpan",
			PyEndTimeSpan,
			"Ends a time span started with BeginTimeSpan.\n"
			"\n"
			"Arguments:\n"
			"  id - the id returned from BeginTimeSpan\n"
			"  label - The closing label for the time span - can be used to\n"
			"          indicate success or failure, for example"
			"\n"
		)
#endif

	EXPOSURE_END()
}

