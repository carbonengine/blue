// Copyright © 2011 CCP ehf.

/* 
	*************************************************************************

	PyScheduler.h

	Project:   Blue

	Description:   

		A Scheduler to run python tasklets for a certain amount of time..


	Dependencies:

		Blue

	*************************************************************************
*/

#include "StdAfx.h"

#if CCP_STACKLESS

#include "PyScheduler.h"
#include <Scheduler.h>

#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

/* #define LOGG */
#ifdef LOGG
static char logbuf[256];
#define LOGIT(text, ...) do { \
	sprintf_s(logbuf, "PyScheduler::" text "\n", __VA_ARGS__); \
	OutputDebugString(logbuf);\
} while (0)

#else
#define LOGIT(text, ...) (void)0
#endif

using doubleMsType = std::chrono::duration<double, std::milli>;

PyScheduler::PyScheduler( double maxTime ) :
	mMaxTimeSec( (float)maxTime )
{

}

void PyScheduler::UpdatePreTickStats()
{
	mStats.numberOfTaskletsInQueuePreTick = SchedulerAPI()->PyScheduler_GetRunCount() - 1;
}

void PyScheduler::UpdatePostTickStats( std::chrono::duration<double> maxTimeSec, std::chrono::duration<double> lastDurationSec )
{
	mStats.numberOfActiveScheduleManagers = SchedulerAPI()->PyScheduler_GetNumberOfActiveScheduleManagers();
	mStats.numberOfActiveChannels = SchedulerAPI()->PyScheduler_GetNumberOfActiveChannels();
	mStats.numberOfActiveTaskets = SchedulerAPI()->PyScheduler_GetActiveTaskletCount();
	mStats.numberOfTaskletsInQueuePostTick = SchedulerAPI()->PyScheduler_GetRunCount() - 1;
	mStats.numberOfTaskletsCompletedLastTick = SchedulerAPI()->PyScheduler_GetTaskletsCompletedLastRunWithTimeout();
	mStats.numberOfTaskletsSwitchedLastTick = SchedulerAPI()->PyScheduler_GetTaskletsSwitchedLastRunWithTimeout();
	
	// All times given in milliseconds
	mStats.maxTimeMs = std::chrono::duration_cast<doubleMsType>( maxTimeSec ).count();
	mStats.lastDurationMs = std::chrono::duration_cast<doubleMsType>( lastDurationSec ).count();
	mStats.overshootMs = lastDurationSec > maxTimeSec ? std::chrono::duration_cast<doubleMsType>( lastDurationSec - maxTimeSec ).count() : 0;
}

bool PyScheduler::RunTime( double t )
{
	// Run tasklets for time, measuring the time taken.

	LOGIT( "Running %d seconds", t );

	auto maxTimeSec = std::chrono::duration<double>( t );

	
	UpdatePreTickStats();
	
	std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

	long long runTimeInNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>( maxTimeSec ).count();

	PyObject* r = SchedulerAPI()->PyScheduler_RunWithTimeout( runTimeInNanoseconds );

	std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();

	auto lastDurationSec = std::chrono::duration<double>( endTime - startTime );

	if( !r )
	{
		return false;
	}

	Py_DECREF( r );

	LOGIT( "Ran for %fs, runqueue=%d", dt, mStats.numberOfTaskletsInQueuePostTick );

	UpdatePostTickStats( maxTimeSec, lastDurationSec );

	return true;
}

#endif
