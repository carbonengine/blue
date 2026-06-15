// Copyright © 2013 CCP ehf.

#pragma once

#ifndef BlueStatistics_h
#define BlueStatistics_h

#include <optional>

#include <CcpTelemetry.h>

#include <ICcpStatisticsAccumulator.h>

BLUE_DECLARE( CcpStatisticsEntry );

BLUE_CLASS( CcpStatisticsEntry ) : public IRoot
{
public:
	EXPOSE_TO_BLUE();

	CcpStatisticsEntry( IRoot* lockobj = nullptr );

	virtual ~CcpStatisticsEntry();

	void AttachStat( CcpStaticStatisticsEntry* stat );
	CcpStaticStatisticsEntry* GetAttachedStat();

	void Inc();
	void Dec();
	void Add( double d );
	void Set( double d );
	void Capture();
	void ResetPeak();
	double GetValue();
	double GetPeak();

	const std::string& GetDescription() const;
	void SetDescription( const std::string& val );

	const std::string& GetName() const;
	void SetName( const std::string& val );

	CcpStatisticsType_t GetType();
	void SetType( CcpStatisticsType_t type );

	bool GetResetPerFrame() const;
	void SetResetPerFrame( bool val );

protected:
	CcpStaticStatisticsEntry* m_statsEntry;

	bool m_resetPerFrame;
	CcpStatisticsType_t m_type;
	std::string m_name;
	std::string m_description;
};

TYPEDEF_BLUECLASS( CcpStatisticsEntry );


BLUE_DECLARE( BlueStatisticsTelemetryConfig );
BLUE_CLASS( BlueStatisticsTelemetryConfig ) : public IRoot
{
public:
	EXPOSE_TO_BLUE();

	std::string m_applicationName;
	float m_captureDurationSec{0};  // In seconds
	bool m_trackMemoryAllocations{false};
};

TYPEDEF_BLUECLASS( BlueStatisticsTelemetryConfig );


BLUE_DECLARE( BlueStatistics );
BLUE_CLASS( BlueStatistics ) : public IRoot
{
public:
	EXPOSE_TO_BLUE();

	BlueStatistics(IRoot* lockobj = NULL);

	void Update();

	// Typically used from the client.
	void StartTelemetry( const std::string& server );
	void StartTelemetryFromConfig( BlueStatisticsTelemetryConfig* config );

	// Following functions are typically used from ESP for server profiling.
	void StartTimedTelemetry( const std::string& server, float samplePeriod );
	void StartTelemetryDump( const std::string& dumpFolder, float samplePeriod );

	void PauseTelemetry();
	void ResumeTelemetry();
	void StopTelemetry();
	void UpdateTelemetry();
	void SetTimelineSectionName( const char* name );
	bool IsTelemetryConnectionRequested();
	float TelemetrySamplingTimeLeft();
	bool IsTelemetryConnected();
	bool IsTelemetryPaused();
	bool IsTelemetryStarted();

	void SetCppCaptureEnabled( bool b );
	bool IsCppCaptureEnabled();

	void SetTaskletCaptureEnabled( bool b );
	bool IsTaskletCaptureEnabled() const;

	void SetPythonCaptureEnabled( bool b );
	bool IsPythonCaptureEnabled() const;

	uint32_t GetTelemetryMaxThreadCount() const;
	void SetTelemetryMaxThreadCount( uint32_t maxThreadCount );


	void BeginCapture();
	std::map<std::string, std::vector<double>> EndCapture();

	void SetAccumulator( const std::string& name, ICcpStatisticsAccumulator* lg );
	ICcpStatisticsAccumulator* GetAccumulator( const std::string& name );

	static BLUEIMPORT CcpStaticStatisticsEntry* CreateDynamicEntry( const char* name, bool reset, CcpStatisticsType_t type, const char* desc );

#if BLUE_WITH_PYTHON
	static PyObject* PyGetDescriptions( PyObject* self, PyObject* args );
	static PyObject* PyGetStats( PyObject* self, PyObject* args );
	static PyObject* PyGetValues( PyObject* self, PyObject* args );
	static PyObject* PyGetSingleStat( PyObject* self, PyObject* args );

#if CCP_STACKLESS
	// Called from BluePyOS
	static void OnTaskletSwitch( PyObject* from, PyObject* to );
#endif

#endif

protected:
	struct AccumulatorEntry
	{
		ICcpStatisticsAccumulatorPtr accumulator;
		CcpStaticStatisticsEntry* stat;
	};
	TrackableStdHashMap<std::string, AccumulatorEntry> m_accumulators;
	TrackableStdHashMap<std::string, std::vector<double>> m_capture;
	uint32_t m_telemetryMaxThreadCount;
	bool m_isCapturing;
};

TYPEDEF_BLUECLASS( BlueStatistics );

extern BlueStatistics* g_statistics;

#if CCP_TELEMETRY_ENABLED

#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>

class [[deprecated( "Use CcpTelemetryZone from CcpCore instead" )]] TracyZone
{
public:
	TracyZone() = delete;
	BLUEIMPORT TracyZone( uint32_t ctx, const char* name, const char* filename, uint32_t lineno, uint32_t color = tracy::Color::SteelBlue4 );
	BLUEIMPORT ~TracyZone();
	TracyZone( TracyZone&& other ) noexcept;
	TracyZone( const TracyZone& ) = delete;
	TracyZone& operator=( TracyZone&& ) = delete;
	TracyZone& operator=( const TracyZone& ) = delete;

	void text( const char* text ) const;

private:
	std::optional<TracyCZoneCtx> m_telemetryContext; // now un-used
	void* m_fiber{nullptr}; // NB: This is now a pointer to CcpTelemetryZone from core, but kept this way to keep ABI compatibility
};

[[deprecated( "Use CcpTelemetryEnterZone from CcpCore instead" )]] void BLUEIMPORT TracyEnterZone( void* key, const char* name, const char* filename, uint32_t lineno );
[[deprecated( "Use CcpTelemetryLeaveZone from CcpCore instead" )]] void BLUEIMPORT TracyLeaveZone( void* key );
[[deprecated( "Use CcpTelemetryZoneAddText from CcpCore instead" )]] void BLUEIMPORT TracyZoneAddText( void* key, const char* text );

#define CCP_STATS_SCOPED_TIME( identifier ) \
	TelemetryZone telemetry_zone_##__COUNTER__( TMCM_CPP, g_ccpStatistics_##identifier.GetName().c_str(), __FILE__, __LINE__ );\
	CcpStatisticsStopwatch ccpStatsStopwatch_##identifier( g_ccpStatistics_##identifier )

#undef CCP_STATS_ZONE
#define CCP_STATS_ZONE( name ) \
	TelemetryZone telemetry_zone_##__COUNTER__( TMCM_CPP, name, __FILE__, __LINE__ );

#else  // CCP_TELEMETRY_ENABLED

#define CCP_STATS_SCOPED_TIME( identifier ) CcpStatisticsStopwatch ccpStatsStopwatch_##identifier( g_ccpStatistics_##identifier )
#undef CCP_STATS_ZONE
#define CCP_STATS_ZONE( name )

#endif  // CCP_TELEMETRY_ENABLED

#endif // BlueStatistics_h
