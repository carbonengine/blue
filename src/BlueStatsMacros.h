// Copyright © 2026 CCP ehf.

#pragma once
#ifndef BlueStatsMacros_h
#define BlueStatsMacros_h

#include <CcpMacros.h>
#include <CcpTelemetry.h>
#include "BlueTelemetryZones.h"

#if CCP_TELEMETRY_ENABLED

// Blue internal counterpart to exposed CCP_STATS_ZONE macro
#define BLUE_STATS_ZONE( zoneName ) \
	TelemetryZone CCP_ANONYMOUS_VARIABLE( blueStatsZone_ )( BlueTelemetryZoneCategory(), zoneName, __FILE__, __LINE__ )

// Blue internal counterpart to exposed CCP_STATS_SCOPED_TIME macro
#define BLUE_STATS_SCOPED_TIME( identifier ) \
	BLUE_STATS_ZONE( g_ccpStatistics_##identifier.GetName().c_str() ); \
	CcpStatisticsStopwatch CCP_ANONYMOUS_VARIABLE( blueStatsStopwatch_ )( g_ccpStatistics_##identifier )

#else  // CCP_TELEMETRY_ENABLED

#define BLUE_STATS_ZONE( zoneName )
#define BLUE_STATS_SCOPED_TIME( identifier ) \
	CcpStatisticsStopwatch CCP_ANONYMOUS_VARIABLE( blueStatsStopwatch_ )( g_ccpStatistics_##identifier )

#endif  // CCP_TELEMETRY_ENABLED

#endif // BlueStatsMacros_h

