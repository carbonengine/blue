////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		December 2013
// Copyright:	CCP 2013
//

#pragma once

#ifndef BlueStatistics_h
#define BlueStatistics_h

#include "ICcpStatisticsAccumulator.h"
#include "IBlueOS.h"
extern BLUEIMPORT IBlueOS* BeOS;

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

BLUE_DECLARE( BlueStatistics );
BLUE_CLASS( BlueStatistics ) : public IRoot
{
public:
	EXPOSE_TO_BLUE();

	BlueStatistics(IRoot* lockobj = NULL);

	void Update();

	// Telemetry buffer defaults to 8MB. But can be changed between samples using this.
	void SetTelemetryBufferSize( int bufferSize );

	// Typically used from the client.
	void StartTelemetry( const std::string& server );

	// Following functions are typically used from ESP for server profiling.
	void StartTimedTelemetry( const std::string& server, float samplePeriod );
	void StartTelemetryDump( const std::string& dumpFolder, float samplePeriod );

	void PauseTelemetry();
	void ResumeTelemetry();
	void PrimeTelemetry();
	void StopTelemetry();
	void UpdateTelemetry();
	void SetTimelineSectionName( const char* name );
	bool IsTelemetryConnectionRequested();
	float TelemetrySamplingTimeLeft();
	bool IsTelemetryConnected();
	bool IsTelemetryPaused();
	void SetCppCaptureEnabled( bool b );
	bool IsCppCaptureEnabled();

	void SetAccumulator( const std::string& name, ICcpStatisticsAccumulator* lg );
	ICcpStatisticsAccumulator* GetAccumulator( const std::string& name );

#if BLUE_WITH_PYTHON
	static PyObject* PyGetDescriptions( PyObject* self, PyObject* args );
	static PyObject* PyGetStats( PyObject* self, PyObject* args );
	static PyObject* PyGetValues( PyObject* self, PyObject* args );
	static PyObject* PyGetSingleStat( PyObject* self, PyObject* args );
#endif

protected:
	struct AccumulatorEntry
	{
		ICcpStatisticsAccumulatorPtr accumulator;
		CcpStaticStatisticsEntry* stat;
	};
	TrackableStdHashMap<std::string, AccumulatorEntry> m_accumulators;
};

TYPEDEF_BLUECLASS( BlueStatistics );

extern BlueStatistics* g_statistics;

#if CCP_TELEMETRY_ENABLED

class BLUEIMPORT tmTaskletZone
{
public:
	tmTaskletZone( uint32_t ctx, const char* name );
	~tmTaskletZone();

private:
	uint32_t m_telemetryContext;
};

void tmTaskletEnter( uint32_t ctx, const char* name );
void tmTaskletLeave( uint32_t ctx );
void tmTaskletAppendText( uint32_t ctx, const char* appendText );

#define CCP_STATS_SCOPED_TIME( identifier ) \
	tmTaskletZone zone_##_COUNTER_( TMCM_CPP, g_ccpStatistics_##identifier.GetName().c_str() );\
	CcpStatisticsStopwatch ccpStatsStopwatch_##identifier( g_ccpStatistics_##identifier )

#undef CCP_STATS_ZONE
#define CCP_STATS_ZONE( name ) \
	tmTaskletZone zone_##_COUNTER_( TMCM_CPP, name )
#else

#define CCP_STATS_SCOPED_TIME( identifier )
#undef CCP_STATS_ZONE
#define CCP_STATS_ZONE( name )

#endif

#endif // BlueStatistics_h