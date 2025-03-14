////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		December 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"
#include "IBlueOS.h"
#include <Scheduler.h>

static CBlueStatistics s_statisticsInstance;
BlueStatistics* g_statistics = &s_statisticsInstance;

BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "statistics", g_statistics );

enum ProfilerState {
	Stopped,
	StartRequested,
	Started,
	Paused,
	StopRequested,
};

ProfilerState s_profilerState{ProfilerState::Stopped};

static bool s_isTelemetryCppCaptureEnabled = true;
static bool s_isTelemetryTaskletCaptureEnabled = true;
static bool s_isTelemetryPythonCaptureEnabled = false;
static float s_telemetrySamplePeriod = 0.0f; // In seconds

#if CCP_TELEMETRY_ENABLED
static int s_telemetryConnectionType = 0;
static std::string s_telemetryServerOrFileSystemDumpPath;
static Be::Time s_telemetryStartTime;
static Be::Time s_telemetryLastCheckTime;

#if BLUE_WITH_PYTHON
// Turn a python string into an "immortal" string.  It is kept alive as long as the
// python interpreter is alive, until all interned strings are deleted.
// This allows the string to be used as a "static strint" for the purposes of Telemetry.
// returns NULL on failure.
const char *Immortalize( PyObject *s )
{
    if ( s == NULL || s == Py_None )
    {
        return "";
    }
    if ( !PyUnicode_Check( s ) )
    {
        PyErr_SetString( PyExc_TypeError, "string expected" );
        return NULL;
    }
    Py_INCREF( s ); //must own the reference we intern
    PyUnicode_InternInPlace( &s );
    const char *result = PyUnicode_AsUTF8( s );
    Py_DECREF( s );
    return result;
}
#endif  // BLUE_WITH_PYTHON

#if CCP_STACKLESS

namespace
{

struct TaskletInfo
{
	const char* name;
	const char* filename;
	int line;
};

const TaskletInfo s_fallbackInfo = { "Tasklet?", "", 0 };
TaskletInfo s_lastTasklet = s_fallbackInfo;  // need to remember last activated tasklet for tmEnd

std::unordered_map<PyTypeObject*, freefunc> s_taskletFree; // original tp_free functions for tasklet types
uint32_t s_taskletTrackID = 0;  // telemetry track ID for tasklet time spans

thread_local PyTaskletObject* g_activeFiber{nullptr};
typedef std::unordered_map<PyTaskletObject *, std::string> FiberNameStore;
FiberNameStore g_fiberNameStore;

typedef std::unordered_map<void*, TracyZone> TasketZoneStore;
TasketZoneStore g_taskletZoneStore;

// Overriden tp_free function for tasklets: notify telemetry and call original tp_free
void OnTaskletFree( void* tasklet )
{
	g_fiberNameStore.erase( (PyTaskletObject*) tasklet );
	auto found = s_taskletFree.find( Py_TYPE( tasklet ) );
	if( found != end( s_taskletFree ) )
	{
		( *found->second )( tasklet );
	}
}

void StoreFree( PyTaskletObject* tasklet )
{
	if( !tasklet )
	{
		return;
	}
	auto type = Py_TYPE( tasklet );
	auto free = type->tp_free;
	if( free && free != OnTaskletFree )
	{
		s_taskletFree[type] = free;
		type->tp_free = OnTaskletFree;
	}
}

// Try to get a readable description of a tasklet object for telemetry time span.
// Use infomation set by bluepy.TaskletExt.
TaskletInfo GetTaskletInfo( PyTaskletObject* tasklet )
{
	TaskletInfo info = s_fallbackInfo;

	if( auto methodName = PyObject_GetAttrString( (PyObject*)tasklet, "method_name" ) )
	{
		if( PyUnicode_CheckExact( methodName ) )
		{
			info.name = PyUnicode_AsUTF8( methodName );
		}
		Py_XDECREF( methodName );
	}
	else
	{
		PyErr_Clear();
	}

	if( auto fileName = PyObject_GetAttrString( (PyObject*)tasklet, "file_name" ) )
	{
		if( PyUnicode_CheckExact( fileName ) )
		{
			info.filename = PyUnicode_AsUTF8( fileName );
		}
		Py_XDECREF( fileName );
	}
	else
	{
		PyErr_Clear();
	}

	if( auto lineNumber = PyObject_GetAttrString( (PyObject*)tasklet, "line_number" ) )
	{
		if( PyLong_Check( lineNumber ) )
		{
			info.line = int( PyLong_AsLong( lineNumber ) );
		}
		Py_XDECREF( lineNumber );
	}
	else
	{
		PyErr_Clear();
	}
	return info;
}

int PythonProfiler( PyObject* obj, PyFrameObject* frame, int what, PyObject* arg )
{
	switch( what )
	{
	case PyTrace_CALL:
	{
		auto codeObj = PyFrame_GetCode( frame );  // Returns a strong reference
		auto zoneName = Immortalize( codeObj->co_name );
		auto fileName = Immortalize( codeObj->co_filename );
		if( zoneName && fileName )
			TracyEnterZone( frame, zoneName, fileName, static_cast<uint32_t>( PyFrame_GetLineNumber( frame ) ) );
		Py_XDECREF( codeObj );  // Release the reference to the frame code
	}
	break;
	case PyTrace_EXCEPTION:
	case PyTrace_RETURN:
		TracyLeaveZone( frame );
		break;
	default:
		break;
	}
	return 0;
}

}

#endif  // CCP_STACKLESS

#endif  // CCP_TELEMETRY_ENABLED


BlueStatistics::BlueStatistics(IRoot* lockobj) :
	m_accumulators( "m_accumulators" ),
	m_capture( "BlueStatistics::m_capture" ),
	m_telemetryMaxThreadCount( 512 ),
	m_isCapturing( false )
{
}

CcpStaticStatisticsEntry* BlueStatistics::CreateDynamicEntry( const char* name, bool reset, CcpStatisticsType_t type, const char* desc )
{
	return CCP_NEW( "CcpStaticStatisticsEntry" ) CcpStaticStatisticsEntry( name, reset, type, desc );
}

// Buffer size and sampling period hard coded in here as this
// is typically called from the client UI for client profiling.
// Use StartTimedTelemetry or StartTelemetryDump otherwise.
void BlueStatistics::StartTelemetry( const std::string& server )
{
	StartTimedTelemetry( server, 0 );
}

void BlueStatistics::StartTimedTelemetry( const std::string& server, float samplePeriod )
{
#if CCP_TELEMETRY_ENABLED
	if (s_profilerState != ProfilerState::Stopped )
	{
		return;
	}

	s_telemetryServerOrFileSystemDumpPath = server;
	s_telemetrySamplePeriod = (float)samplePeriod;

	s_profilerState = ProfilerState::StartRequested;
#else
#endif
}

void BlueStatistics::StartTelemetryDump( const std::string& dumpFolder, float samplePeriod )
{
#if CCP_TELEMETRY_ENABLED
	if (s_profilerState != ProfilerState::Stopped )
	{
		return;
	}
	s_telemetryServerOrFileSystemDumpPath = dumpFolder;
	s_telemetrySamplePeriod = (float)samplePeriod;

	s_profilerState = ProfilerState::StartRequested;
#else
#endif
}

void BlueStatistics::PauseTelemetry()
{
#if CCP_TELEMETRY_ENABLED
	if ( s_profilerState != ProfilerState::Started ) {
		return;
	}

	s_profilerState = ProfilerState::Paused;
#endif
}

void BlueStatistics::ResumeTelemetry()
{
#if CCP_TELEMETRY_ENABLED
	if ( s_profilerState != ProfilerState::Paused ) {
		return;
	}
	s_profilerState = ProfilerState::Started;
#endif
}

void BlueStatistics::StopTelemetry()
{
#if CCP_TELEMETRY_ENABLED
	if ( s_profilerState != ProfilerState::Started && s_profilerState != ProfilerState::Paused )
	{
		return;
	}

	if( s_isTelemetryPythonCaptureEnabled )
	{
		PyEval_SetProfile( nullptr, nullptr );
	}

#if CCP_STACKLESS
	for( auto& free : s_taskletFree )
	{
		free.first->tp_free = free.second;
	}
	s_taskletFree.clear();

	s_lastTasklet = s_fallbackInfo;
#endif
	// Wrap up instrumentation data
	g_taskletZoneStore.clear();
	if ( g_activeFiber )
	{
		TracyFiberLeave;
		g_activeFiber = nullptr;
	}

	// Ensure it gets flushed
	CcpTelemetryTick();

	// Ensure we stop profiling
	s_profilerState = ProfilerState::StopRequested;

	// Finally, ask for it to be shutdown
	tracy::GetProfiler().RequestShutdown();

	CcpTelemetryTick();
#endif
}

bool BlueStatistics::IsTelemetryConnected()
{
	return s_profilerState == ProfilerState::Started || s_profilerState == ProfilerState::Paused;
}

bool BlueStatistics::IsTelemetryConnectionRequested()
{
	return s_profilerState == ProfilerState::StartRequested;
}

float BlueStatistics::TelemetrySamplingTimeLeft()
{
	return s_telemetrySamplePeriod;
}

bool BlueStatistics::IsTelemetryPaused()
{
	return s_profilerState == ProfilerState::Paused;
}

void BlueStatistics::UpdateTelemetry()
{
#if CCP_TELEMETRY_ENABLED
	switch ( s_profilerState )
	{
		case ProfilerState::StartRequested:
		{
			if (tracy::IsProfilerStarted())
			{
				if (tracy::GetProfiler().IsConnected())
				{
					TracySetProgramName( s_telemetryServerOrFileSystemDumpPath.c_str() );
					if ( s_isTelemetryPythonCaptureEnabled ) {
						PyEval_SetProfile( &PythonProfiler, nullptr );
					}
					s_profilerState = ProfilerState::Started;
					s_telemetryLastCheckTime = s_telemetryStartTime = BeOS->GetActualTime();
				}
			}
			else if (!CcpStartTelemetry( s_telemetryServerOrFileSystemDumpPath.c_str(), s_telemetryConnectionType, m_telemetryMaxThreadCount ))
			{
				CCP_LOGERR( "Failed to start Telemetry" );
				s_profilerState = ProfilerState::Stopped;
			}
			break;
		}
		case ProfilerState::Started:
		{
			CcpTelemetryTick();
			if(s_telemetrySamplePeriod > 0.0f ) // Check if we have passed our timed sample time
			{
				Be::Time newTime = BeOS->GetActualTime();
				Be::Time delta = newTime - s_telemetryLastCheckTime;
				s_telemetryLastCheckTime = newTime;
				s_telemetrySamplePeriod -= ((float)delta / Be::Time(1e7));

				if(s_telemetrySamplePeriod < 0.0f)
				{
					CCP_LOG( "Finalising timed Telemetry run." );
					StopTelemetry();
				}
			}
			break;
		}
		case ProfilerState::StopRequested:
		{
			if ( tracy::GetProfiler().HasShutdownFinished() )
			{
				CcpStopTelemetry();
				s_profilerState = ProfilerState::Stopped;
			}
			break;
		}
		case ProfilerState::Paused:
		case ProfilerState::Stopped:
			// Nothing to do
			break;
		default:
			CCP_LOGERR( "BlueStatistics::UpdateTelemetry - unhandled profiler state %d", s_profilerState );
			break;
	}
#endif
}

void SwitchToFiber( PyTaskletObject* to )
{
	if( !to || SchedulerAPI()->PyTasklet_IsMain( to ) )
	{
		if ( g_activeFiber )
		{
			TracyFiberLeave;
		}
		g_activeFiber = nullptr;
	}
	else
	{
		auto existing = g_fiberNameStore.find( to );
		if( existing == g_fiberNameStore.cend() )
		{
			auto toInfo = GetTaskletInfo( to );
			// Construct the tracy fiber name as `function_name (address_of_tasklet)`
			// This should deal with the case of `function_name` running in two different tasklets
			g_fiberNameStore[to] = std::string( toInfo.name ) + " (" + std::to_string( reinterpret_cast<uint64_t>( to ) ) + ")";
			existing = g_fiberNameStore.find( to );
		}
		TracyFiberEnter( existing->second.c_str() );
		g_activeFiber = to;
	}
}

#if CCP_STACKLESS
void BlueStatistics::OnTaskletSwitch( PyObject* _from, PyObject* _to )
{
#if CCP_TELEMETRY_ENABLED
	PyTaskletObject* from = (PyTaskletObject*)_from;
	PyTaskletObject* to = (PyTaskletObject*)_to;

	if( s_profilerState == ProfilerState::Started )
	{
		StoreFree( from );
		StoreFree( to );

		if( s_isTelemetryTaskletCaptureEnabled )
		{
			SwitchToFiber( to );
		}
	}
#endif
}
#endif

void BlueStatistics::BeginCapture()
{
	m_isCapturing = true;
	m_capture.clear();
}

std::map<std::string, std::vector<double>> BlueStatistics::EndCapture()
{
	m_isCapturing = false;
	std::map<std::string, std::vector<double>> result;
	for( auto it = m_capture.begin(); it != m_capture.end(); ++it )
	{
		result.insert( *it );
	}
	return result;
}

void BlueStatistics::Update()
{
	CcpStatistics::Update();

	for( auto it = m_accumulators.begin(); it != m_accumulators.end(); ++it )
	{
		auto entry = it->second;
		if( entry.accumulator && entry.stat )
		{
			entry.accumulator->Add( entry.stat->GetValue() );
		}
	}

	UpdateTelemetry();

	if( m_isCapturing )
	{
		auto& entries = CcpStatistics::GetEntryArray();
		for( auto it = entries.begin(); it != entries.end(); ++it )
		{
			m_capture[( *it )->GetName()].push_back( ( *it )->GetValue() );
		}
	}
}


void BlueStatistics::SetAccumulator( const std::string& name, ICcpStatisticsAccumulator* lg )
{
	if( !lg )
	{
		m_accumulators.erase( name );
		return;
	}

	AccumulatorEntry accumulatorEntry;
	accumulatorEntry.accumulator = lg;
	accumulatorEntry.stat = nullptr;

	CcpStatistics::EntryArray& a = CcpStatistics::GetEntryArray();
	for( auto it = a.begin(); it != a.end(); ++it )
	{
		if( name == (*it)->GetName() )
		{
			accumulatorEntry.stat = *it;
			break;
		}
	}

	if( !accumulatorEntry.stat )
	{
		auto b = CcpStatistics::GetDerivedEntryArray();
		for( auto it = b.begin(); it != b.end(); ++it )
		{
			if( name == (*it)->GetName() )
			{
				accumulatorEntry.stat = *it;
				break;
			}
		}
	}

	if( accumulatorEntry.stat )
	{
		m_accumulators[name] = accumulatorEntry;
	}
}

ICcpStatisticsAccumulator* BlueStatistics::GetAccumulator( const std::string& name )
{
	auto accumulatorEntryIt = m_accumulators.find( name );
	if( accumulatorEntryIt == m_accumulators.end() )
	{
		return nullptr;
	}

	return accumulatorEntryIt->second.accumulator;
}

void BlueStatistics::SetTimelineSectionName( const char* name )
{
}

void BlueStatistics::SetCppCaptureEnabled( bool b )
{
	s_isTelemetryCppCaptureEnabled = b;
}

bool BlueStatistics::IsCppCaptureEnabled()
{
	return s_isTelemetryCppCaptureEnabled;
}

void BlueStatistics::SetTaskletCaptureEnabled( bool b )
{
	s_isTelemetryTaskletCaptureEnabled = b;
}

bool BlueStatistics::IsTaskletCaptureEnabled() const
{
	return s_isTelemetryTaskletCaptureEnabled;
}

void BlueStatistics::SetPythonCaptureEnabled( bool b )
{
	s_isTelemetryPythonCaptureEnabled = b;
}

bool BlueStatistics::IsPythonCaptureEnabled() const
{
	return s_isTelemetryPythonCaptureEnabled;
}

uint32_t BlueStatistics::GetTelemetryMaxThreadCount() const
{
	return m_telemetryMaxThreadCount;
}

void BlueStatistics::SetTelemetryMaxThreadCount( uint32_t maxThreadCount )
{
	m_telemetryMaxThreadCount = maxThreadCount;
}

CcpStatisticsEntry::CcpStatisticsEntry( IRoot* lockobj ) :
	m_statsEntry( nullptr ),
	m_resetPerFrame( false ),
	m_type( CST_COUNTER_LOW )
{
}

CcpStatisticsEntry::~CcpStatisticsEntry()
{
}

void CcpStatisticsEntry::AttachStat( CcpStaticStatisticsEntry* stat )
{
	m_statsEntry = stat;
}

CcpStaticStatisticsEntry* CcpStatisticsEntry::GetAttachedStat()
{
	if( !m_statsEntry )
	{
		m_statsEntry = CCP_NEW( "m_statsEntry" ) CcpStaticStatisticsEntry(
			m_name.c_str(),
			m_resetPerFrame,
			m_type,
			m_description.c_str() );
	}
	return m_statsEntry;
}

void CcpStatisticsEntry::Inc()
{
	if( m_statsEntry )
	{
		m_statsEntry->Inc();
	}
}

void CcpStatisticsEntry::Dec()
{
	if( m_statsEntry )
	{
		m_statsEntry->Dec();
	}
}

void CcpStatisticsEntry::Add( double d )
{
	if( m_statsEntry )
	{
		m_statsEntry->Add( d );
	}
}

void CcpStatisticsEntry::Set( double d )
{
	if( m_statsEntry )
	{
		m_statsEntry->Set( d );
	}
}

void CcpStatisticsEntry::Capture()
{
	if( m_statsEntry )
	{
		m_statsEntry->Capture();
	}
}

void CcpStatisticsEntry::ResetPeak()
{
	if( m_statsEntry )
	{
		m_statsEntry->ResetPeak();
	}
}

double CcpStatisticsEntry::GetValue()
{
	if( m_statsEntry )
	{
		return m_statsEntry->GetValue();
	}
	else
	{
		return 0;
	}
}

double CcpStatisticsEntry::GetPeak()
{
	if( m_statsEntry )
	{
		return m_statsEntry->GetPeak();
	}
	else
	{
		return 0;
	}
}

const std::string& CcpStatisticsEntry::GetDescription() const
{
	if( m_statsEntry )
	{
		return m_statsEntry->GetDescription();
	}
	else
	{
		return m_description;
	}
}

void CcpStatisticsEntry::SetDescription( const std::string& val )
{
	if( m_statsEntry )
	{
		m_statsEntry->SetDescription( val );
	}
}

const std::string& CcpStatisticsEntry::GetName() const
{
	if( m_statsEntry )
	{
		return m_statsEntry->GetName();
	}
	else
	{
		return m_name;
	}
}

void CcpStatisticsEntry::SetName( const std::string& val )
{
	m_name = val;
	if( m_statsEntry )
	{
		m_statsEntry->SetName( val );
	}
}

void CcpStatisticsEntry::SetType( CcpStatisticsType_t type )
{
	m_type = type;
	if( m_statsEntry )
	{
		m_statsEntry->SetType( type );
	}
}

CcpStatisticsType_t CcpStatisticsEntry::GetType()
{
	if( m_statsEntry )
	{
		return m_statsEntry->GetType();
	}
	else
	{
		return m_type;
	}
}

bool CcpStatisticsEntry::GetResetPerFrame() const
{
	if( m_statsEntry )
	{
		return m_statsEntry->GetResetPerFrame();
	}
	else
	{
		return m_resetPerFrame;
	}
}

void CcpStatisticsEntry::SetResetPerFrame( bool val )
{
	m_resetPerFrame = val;
	if( m_statsEntry )
	{
		m_statsEntry->SetResetPerFrame( val );
	}
}

#if CCP_TELEMETRY_ENABLED

void TracyEnterZone( void* key, const char* name, const char* filename, uint32_t lineno )
{
	if( s_profilerState == ProfilerState::Started )
	{
		if( g_taskletZoneStore.find( key ) == g_taskletZoneStore.end() )
		{
			g_taskletZoneStore.insert( { key, TracyZone( TMCM_CPP, name, filename, lineno, tracy::Color::Yellow ) } );
		}
	}
}

void TracyLeaveZone( void* key )
{
	if( s_profilerState == ProfilerState::Started )
	{
		if( g_taskletZoneStore.find( key ) != g_taskletZoneStore.end() )
		{
			g_taskletZoneStore.erase( key );
		}
	}
}

void TracyZoneAddText( void* key, const char* text )
{
	if( s_profilerState == ProfilerState::Started )
	{
		auto zone = g_taskletZoneStore.find( key );
		if( zone != g_taskletZoneStore.end() )
		{
			zone->second.text( text );
		}
	}
}

TracyZone::TracyZone( uint32_t ctx, const char* name, const char* filename, uint32_t lineno, uint32_t color ) : m_fiber( g_activeFiber )
{
	if( s_profilerState == ProfilerState::Started )
	{
		CCP_ASSERT( filename != nullptr );
		CCP_ASSERT( name != nullptr );
		auto data = ___tracy_alloc_srcloc( lineno, filename, strlen( filename ), name, strlen( name ), color );
		m_telemetryContext.emplace( ___tracy_emit_zone_begin_alloc( data, ctx & TMCM_CPP ) );
	}
}

TracyZone::TracyZone( TracyZone&& other ) noexcept
{
	m_fiber = other.m_fiber;
	m_telemetryContext = other.m_telemetryContext;
	// mark this instance's zone as inactive in case the destructor runs
	other.m_telemetryContext.reset();
}

TracyZone::~TracyZone()
{
	if( s_profilerState == ProfilerState::Started && m_telemetryContext )
	{
		// Zones need to end on the same fiber they were started from, so do a little song and dance to ensure that
		auto previous = g_activeFiber;
		SwitchToFiber( (PyTaskletObject*) m_fiber );
		TracyCZoneEnd( m_telemetryContext.value() );
		SwitchToFiber( previous );
	}
}

void TracyZone::text( const char* text ) const
{
	if( s_profilerState == ProfilerState::Started && m_telemetryContext )
	{
		CCP_ASSERT( text != nullptr );
		TracyCZoneText( m_telemetryContext.value(), text, strlen( text ) );
	}
}

#endif  // CCP_TELEMETRY_ENABLED
