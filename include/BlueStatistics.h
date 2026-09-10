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
	bool m_trackLocks{false};
};

TYPEDEF_BLUECLASS( BlueStatisticsTelemetryConfig );


BLUE_DECLARE( BlueTelemetryColor );
BLUE_CLASS( BlueTelemetryColor ) : public IRoot
{
public:
	EXPOSE_TO_BLUE();

	// Returns the shared instance for the given color. Colors are interned so that the same color
	// always yields the same object, no matter whether it comes from one of the constants on the
	// exposed type or from a registered Telemetry category, which is what lets them be compared
	// with each other.
	static BlueTelemetryColor* Get( CcpColor color );

#if BLUE_WITH_PYTHON
	// Adds the colors of the CcpColor enum as constants on the exposed type, so they can be used
	// as blue.BlueTelemetryColor.SteelBlue. Has to be called once the blue module has been
	// created, as that is where the Python type object is finalized.
	static bool RegisterConstants();
#endif

	void SetColor( CcpColor color );
	CcpColor GetColor() const;

	// The color as a 0xRRGGBB value, and the name it is known by, such as "SteelBlue"
	uint32_t GetValue() const;
	std::string GetName() const;

protected:
	CcpColor m_color{CcpColor::White};
};

TYPEDEF_BLUECLASS( BlueTelemetryColor );


BLUE_DECLARE( BlueTelemetryCategory );
BLUE_CLASS( BlueTelemetryCategory ) : public IRoot
{
public:
	EXPOSE_TO_BLUE();

#if BLUE_WITH_PYTHON
	// Gives the exposed type an equality and a hash of its own, so that categories compare by the
	// name they were registered with rather than by the identity of the wrapper that was handed out
	// for them. Has to be called before the blue module is created, as the Python type object picks
	// the two up while it is being finalized as part of that.
	static bool RegisterComparison();
#endif

	// Attaches this wrapper to a category owned by the Telemetry category registry. Registered
	// categories live for the lifetime of the process, so the pointer stays valid once attached.
	void AttachCategory( const CcpTelemetryCategory* category );

	// The category this wrapper is attached to, or nullptr if it was never attached
	const CcpTelemetryCategory* GetCategory() const;

	const std::string& GetName() const;
	BlueTelemetryColor* GetColor() const;

protected:
	const CcpTelemetryCategory* m_category{nullptr};
};

TYPEDEF_BLUECLASS( BlueTelemetryCategory );


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
