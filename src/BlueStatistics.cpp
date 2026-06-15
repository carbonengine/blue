// Copyright © 2013 CCP ehf.

#include "StdAfx.h"
#include "BlueStatistics.h"
#include "IBlueOS.h"
#include <Scheduler.h>

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "Telemetry" );

static CBlueStatistics s_statisticsInstance;
BlueStatistics* g_statistics = &s_statisticsInstance;

BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "statistics", g_statistics );

static bool s_isTelemetryCppCaptureEnabled = true;
static bool s_isTelemetryTaskletCaptureEnabled = true;
static bool s_isTelemetryPythonCaptureEnabled = false;
static float s_telemetrySamplePeriod = 0.0f; // In seconds

#if CCP_TELEMETRY_ENABLED

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

std::unordered_map<PyTypeObject*, freefunc> s_taskletFree; // original tp_free functions for tasklet types

std::string GetFiberName( const PyTaskletObject* to )
{
	std::string taskletName;
	if( auto methodName = PyObject_GetAttrString( (PyObject*)to, "method_name" ) )
	{
		if( PyUnicode_CheckExact( methodName ) )
		{
			taskletName = PyUnicode_AsUTF8( methodName );
		}
		Py_XDECREF( methodName );
	}
	else
	{
		PyErr_Clear();
	}
	// Construct the tracy fiber name as `function_name (address_of_tasklet)`
	// This should deal with the case of `function_name` running in two different tasklets
	taskletName += " (";
	taskletName += std::to_string( reinterpret_cast<uint64_t>( to ) );
	taskletName += ")";
	return taskletName;
}

// Overriden tp_free function for tasklets: notify telemetry and call original tp_free
void OnTaskletFree( void* tasklet )
{
	CcpTelemetryRemoveFiber( GetFiberName( (PyTaskletObject*)tasklet ) );
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
			CcpTelemetryEnterZone( frame, zoneName, fileName, static_cast<uint32_t>( PyFrame_GetLineNumber( frame ) ) );
		Py_XDECREF( codeObj );  // Release the reference to the frame code
	}
	break;
	case PyTrace_EXCEPTION:
	case PyTrace_RETURN:
		CcpTelemetryLeaveZone( frame );
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

void TelemetryEventHandler( CcpTelemetryEvent event, void* userdata )
{
	if ( event == CCP_TELEMETRY_STARTED && s_isTelemetryPythonCaptureEnabled ) {
		PyEval_SetProfile( &PythonProfiler, nullptr );
	}
}

void BlueStatistics::StartTelemetryFromConfig( BlueStatisticsTelemetryConfig* config )
{
	if ( config != nullptr)
	{
		// Map BlueStatisticsTelemetryConfig to carbon-core CcpTelemetryConfig
		CcpTelemetryConfig coreTelemetryConfig{
			config->m_applicationName,
			std::chrono::milliseconds( static_cast<int>( config->m_captureDurationSec * 1000 ) ),
			config->m_trackMemoryAllocations
		};

		CcpStartTelemetry( coreTelemetryConfig );
		CcpRegisterTelemetryEventHandler( TelemetryEventHandler, nullptr );
	}
}

void BlueStatistics::StartTimedTelemetry( const std::string& server, float samplePeriod )
{
	using namespace std::chrono;
	CcpStartTelemetry( CcpTelemetryConfig{ server, duration_cast<milliseconds>( duration<float>(samplePeriod) ) } );
	CcpRegisterTelemetryEventHandler( TelemetryEventHandler, nullptr );
}

void BlueStatistics::StartTelemetryDump( const std::string& dumpFolder, float samplePeriod )
{
	// Deprecated
}

void BlueStatistics::PauseTelemetry()
{
	// Deprecated
}

void BlueStatistics::ResumeTelemetry()
{
	// Deprecated
}

void BlueStatistics::StopTelemetry()
{
	PyEval_SetProfile( nullptr, nullptr );
	CcpUnregisterTelemetryEventHandler( TelemetryEventHandler, nullptr );
	CcpStopTelemetry();
}

bool BlueStatistics::IsTelemetryConnected()
{
	return CcpTelemetryIsConnected();
}

bool BlueStatistics::IsTelemetryConnectionRequested()
{
	return CcpTelemetryIsConnectionRequested();
}

float BlueStatistics::TelemetrySamplingTimeLeft()
{
	return s_telemetrySamplePeriod;
}

bool BlueStatistics::IsTelemetryPaused()
{
	return false; // Deprecated
}

bool BlueStatistics::IsTelemetryStarted()
{
	return CcpTelemetryIsStarted();
}

void BlueStatistics::UpdateTelemetry()
{
#if CCP_TELEMETRY_ENABLED
	CcpTelemetryTick();

#endif
}

void SwitchToFiber( PyTaskletObject* to )
{
	if( !to || SchedulerAPI()->PyTasklet_IsMain( to ) )
	{
		CcpTelemetrySetActiveFiber( "" );
	}
	else
	{
		std::string taskletName = GetFiberName( to );
		CcpTelemetrySetActiveFiber( taskletName );
	}
}

#if CCP_STACKLESS
void BlueStatistics::OnTaskletSwitch( PyObject* _from, PyObject* _to )
{
#if CCP_TELEMETRY_ENABLED
	if ( ! CcpTelemetryIsStarted() )
	{
		return;
	}

	auto from = (PyTaskletObject*)_from;
	auto to = (PyTaskletObject*)_to;

	StoreFree( from );
	StoreFree( to );

	if( s_isTelemetryTaskletCaptureEnabled )
	{
		SwitchToFiber( to );
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
	CcpTelemetryEnterZone( key, name, filename, lineno );
}

void TracyLeaveZone( void* key )
{
	CcpTelemetryLeaveZone( key );
}

void TracyZoneAddText( void* key, const char* text )
{
	if (text != nullptr )
	{
		CcpTelemetryZoneAddText( key, text );
	}
}

TracyZone::TracyZone( uint32_t ctx, const char* name, const char* filename, uint32_t lineno, uint32_t color ) : m_fiber( new TelemetryZone( ctx, name, filename, lineno, color ) )
{

}

TracyZone::TracyZone( TracyZone&& other ) noexcept
{
	m_fiber = other.m_fiber;
	other.m_fiber = nullptr;
}

TracyZone::~TracyZone()
{
	if( m_fiber )
	{
		delete reinterpret_cast<TelemetryZone*>( m_fiber );
	}
}

void TracyZone::text( const char* text ) const
{
	if ( m_fiber )
	{
		reinterpret_cast<TelemetryZone*>( m_fiber )->text( text );
	}
}

#endif  // CCP_TELEMETRY_ENABLED
