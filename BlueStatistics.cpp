////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		December 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"

static CBlueStatistics s_statisticsInstance;
BlueStatistics* g_statistics = &s_statisticsInstance;

BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "statistics", g_statistics );

static bool s_isTelemetryConnectionRequested = false;
static bool s_isTelemetryConnected = false;
static bool s_isTelemetryCppCaptureEnabled = true;
static bool s_isTelemetryPaused = false;
static float s_telemetrySamplePeriod = 0.0f; // In seconds

#if CCP_TELEMETRY_ENABLED
static bool s_isTelemetryShuttingDown = false;
static int s_telemetryConnectionType = 0;
static std::string s_telemetryServerOrFileSystemDumpPath;
static Be::Time s_telemetryStartTime;
static Be::Time s_telemetryLastCheckTime;
static CcpThreadId_t s_telemetryThread = NULL;
static uint32_t s_telemetryTaskletTlsIx = NULL;
static uint32_t s_telemetryTaskletStackTlsIx = NULL;

typedef TrackableStdVector<const char*> ZoneStack_t;
typedef TrackableStdMap<intptr_t, ZoneStack_t> TaskletZoneStackMap_t;

static TaskletZoneStackMap_t s_taskletZoneStackMap( "s_taskletZoneStackMap" );

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
    if ( !PyString_Check( s ) )
    {
        PyErr_SetString( PyExc_TypeError, "string expected" );
        return NULL;
    }
    Py_INCREF( s ); //must own the reference we intern
    PyString_InternImmortal( &s );
    const char *result = PyString_AS_STRING( s );
    Py_DECREF( s );
    return result;
}
#endif

#endif


#if CCP_STACKLESS && CCP_TELEMETRY_ENABLED

#include "CcpUtils/PyCpp.h"

static ZoneStack_t& GetStackForTasklet( intptr_t taskletId )
{
	TaskletZoneStackMap_t* taskletZoneStackMap = (TaskletZoneStackMap_t*)TlsGetValue( s_telemetryTaskletStackTlsIx );
	if( !taskletZoneStackMap )
	{
		taskletZoneStackMap = CCP_NEW( "CcpStatistics/taskletZoneStackMap" ) TaskletZoneStackMap_t( "CcpStatistics/taskletZoneStackMap" );
		TlsSetValue( s_telemetryTaskletStackTlsIx, taskletZoneStackMap );
	}

	auto foundIt = taskletZoneStackMap->find( taskletId );

	if( foundIt != taskletZoneStackMap->end() )
	{
		return foundIt->second;
	}


	std::pair<TaskletZoneStackMap_t::iterator, bool> res = taskletZoneStackMap->insert(TaskletZoneStackMap_t::value_type(taskletId, ZoneStack_t( "zoneStack" ) ) );
	ZoneStack_t& stack = res.first->second;
	if( res.second )
	{
		// The stack was added, find the context if possible
		const char* context = NULL;

		stack.reserve( 32 );

		if( Ccp::PyGilHave() )
		{
			PyObject* tasklet = PyStackless_GetCurrent();
			if( tasklet )
			{
				if( PyObject_HasAttrString( tasklet, "context" ) )
				{
					PyObject* ctx = PyObject_GetAttrString( tasklet, "context" );
					if( ctx && PyString_Check( ctx ) )
					{
						context = CCP_STRDUP( "Tasklet context", PyString_AS_STRING( ctx ) );

						// Note that we leak the memory for the context string
					}

					Py_XDECREF( ctx );
				}
				Py_DECREF( tasklet );
			}
		}

		if( context )
		{
			stack.push_back( context );
		}
	}
	return stack;
}

static void SwitchZoneContext( intptr_t from, intptr_t to )
{
	ZoneStack_t& stackFrom = GetStackForTasklet( from );
	for( ZoneStack_t::const_reverse_iterator it = stackFrom.rbegin(); it != stackFrom.rend(); ++it )
	{
		tmLeave( TMCM_GENERAL );
	}

	ZoneStack_t& stackTo = GetStackForTasklet( to );
	for( ZoneStack_t::const_iterator it = stackTo.begin(); it != stackTo.end(); ++it )
	{
		tmEnter( TMCM_GENERAL, TMZF_NONE, "%s", *it );
	}
}

#endif


BlueStatistics::BlueStatistics(IRoot* lockobj) :
	m_accumulators( "m_accumulators" ),
	m_capture( "BlueStatistics::m_capture" ),
	m_isCapturing( false )
{
#if CCP_TELEMETRY_ENABLED
	s_telemetryTaskletTlsIx = TlsAlloc();
	s_telemetryTaskletStackTlsIx = TlsAlloc();
#endif
}

CcpStaticStatisticsEntry* BlueStatistics::CreateDynamicEntry( const char* name, bool reset, CcpStatisticsType_t type, const char* desc )
{
	return CCP_NEW( "CcpStaticStatisticsEntry" ) CcpStaticStatisticsEntry( name, reset, type, desc );
}

void BlueStatistics::SetTelemetryBufferSize( int bufferSize )
{
	CCP_LOGWARN("SetTelemetryBufferSize is deprecated");
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
	if( s_isTelemetryConnected )
	{
		CCP_LOGERR( "Telemetry is already running!" );
		return;
	}

	s_telemetryServerOrFileSystemDumpPath = server;
	s_telemetryConnectionType = TMCT_TCP;
	s_telemetrySamplePeriod = (float)samplePeriod;
	s_isTelemetryConnectionRequested = true;
#else
#endif
}

void BlueStatistics::StartTelemetryDump( const std::string& dumpFolder, float samplePeriod )
{
#if CCP_TELEMETRY_ENABLED
	if( s_isTelemetryConnected )
	{
		CCP_LOGERR( "Telemetry is already running!" );
		return;
	}
	s_telemetryConnectionType = TMCT_FILE;
	s_telemetryServerOrFileSystemDumpPath = dumpFolder;
	s_telemetrySamplePeriod = (float)samplePeriod;
	s_isTelemetryConnectionRequested = true;
#else
#endif
}

void BlueStatistics::PauseTelemetry()
{
#if CCP_TELEMETRY_ENABLED
	tmPause( TMCM_GENERAL, 1 );
	s_isTelemetryPaused = true;
#endif
}

void BlueStatistics::ResumeTelemetry()
{
#if CCP_TELEMETRY_ENABLED
	tmPause( TMCM_GENERAL, 0 );
	s_isTelemetryPaused = false;
#endif
}

void BlueStatistics::StopTelemetry()
{
#if CCP_TELEMETRY_ENABLED
	if( s_isTelemetryShuttingDown )
	{
		return;
	}
	if( s_isTelemetryConnected )
	{
		s_isTelemetryConnected = false;
		s_isTelemetryShuttingDown = true;

		tmPause( TMCM_GENERAL, 1 );
	}
#endif
}

bool BlueStatistics::IsTelemetryConnected()
{
	return s_isTelemetryConnected;
}

bool BlueStatistics::IsTelemetryConnectionRequested()
{
	return s_isTelemetryConnectionRequested;
}

float BlueStatistics::TelemetrySamplingTimeLeft()
{
	return s_telemetrySamplePeriod;
}

bool BlueStatistics::IsTelemetryPaused()
{
	return s_isTelemetryPaused;
}

void BlueStatistics::UpdateTelemetry()
{
#if CCP_TELEMETRY_ENABLED
	if( s_isTelemetryConnectionRequested && !s_isTelemetryShuttingDown )
	{
		s_isTelemetryConnected = CcpStartTelemetry( s_telemetryServerOrFileSystemDumpPath.c_str(), s_telemetryConnectionType );
		s_isTelemetryConnectionRequested = false;
		s_telemetryLastCheckTime = s_telemetryStartTime = BeOS->GetActualTime();
		return;
	}

	CcpTelemetryTick();

	if( s_isTelemetryShuttingDown )
	{
		CcpStopTelemetry();

		s_isTelemetryShuttingDown = false;

		s_taskletZoneStackMap.clear();
	}
	else if(s_telemetrySamplePeriod > 0.0f ) // Check if we have passed our timed sample time
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
#endif
}

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
#if CCP_TELEMETRY_ENABLED
	tmSetTimelineSectionName( TMCM_GENERAL, name );
#endif
}

void BlueStatistics::SetCppCaptureEnabled( bool b )
{
#if CCP_TELEMETRY_ENABLED
	s_isTelemetryCppCaptureEnabled = b;
	tm_uint32 mask = TMCM_GENERAL;
	if (s_isTelemetryCppCaptureEnabled)
	{
		mask |= TMCM_CPP;
	}
	tmSetCaptureMask(mask);
#endif
}

bool BlueStatistics::IsCppCaptureEnabled()
{
	return s_isTelemetryCppCaptureEnabled;
}

void BlueStatistics::PrimeTelemetry()
{
	CcpPrimeTelemetry();
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

// Enter a zone in Python
void tmTaskletEnter( uint32_t ctx, const char* name )
{
#if CCP_STACKLESS
	intptr_t lastTasklet = (intptr_t)TlsGetValue( s_telemetryTaskletTlsIx );
	intptr_t curTasklet = PyStackless_GetCurrentId();

	if( curTasklet != lastTasklet )
	{
		SwitchZoneContext( lastTasklet, curTasklet );
		TlsSetValue( s_telemetryTaskletTlsIx, (void*)curTasklet );
	}

	ZoneStack_t& stack = GetStackForTasklet( curTasklet );
	stack.push_back( name );
#endif

	tmEnter( ctx, TMZF_NONE, "%s", name );
}

// Leave a zone in Python
void tmTaskletLeave( uint32_t ctx)
{
	tmLeave( ctx );

#if CCP_STACKLESS
	intptr_t lastTasklet = (intptr_t)TlsGetValue( s_telemetryTaskletTlsIx );
	intptr_t curTasklet = PyStackless_GetCurrentId();

	ZoneStack_t& stack = GetStackForTasklet( curTasklet );

	if( !stack.empty() )
	{
		stack.pop_back();
	}

	if( curTasklet != lastTasklet )
	{
		SwitchZoneContext( lastTasklet, curTasklet );
		TlsSetValue( s_telemetryTaskletTlsIx, (void*)curTasklet );
	}
#endif
}

void tmTaskletAppendText( uint32_t ctx, const char* appendText )
{
	tmMessage( ctx, TMMF_ZONE_SUBLABEL, "%s", tmDynamicString( TMCM_GENERAL, appendText ) );
}

tmTaskletZone::tmTaskletZone( uint32_t ctx, const char* name ) : m_telemetryContext( ctx )
{
	tmTaskletEnter( ctx, name );
}

tmTaskletZone::~tmTaskletZone()
{
	tmTaskletLeave( m_telemetryContext );
}

#endif


