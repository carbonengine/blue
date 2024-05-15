/* 
	*************************************************************************

	PyScheduler.h

	Author:    Kristjan Valur Jonsson
	Created:   Mai. 2011
	OS:        Win32
	Project:   Blue

	Description:   

		A Scheduler to run python tasklets for a certain amount of time..


	Dependencies:

		Blue

	(c) CCP 2008

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

double PyScheduler::mPt = 0.0;

PyScheduler::PyScheduler(double maxTime)
{
	mMaxTime = (float)maxTime;
	mOvershoot = 0;
	mLastDuration = 0.0f;
	mInQueue1 = mInQueue2 = 0;
	if ( !mPt )
	{
		mPt = 1.0/(double)CcpGetTimestampFrequency();
	}
}

bool PyScheduler::RunTime( double t )
{
	LOGIT( "Running %d seconds", t );

	// Run tasklets for time, measuring the time required.
	uint64_t time1, time2;

	mInQueue1 = SchedulerAPI()->PyScheduler_GetRunCount() - 1;

	time1 = CcpGetTimestamp();

	long long runTimeInNanoseconds = t * 1000000000;

	PyObject* r = SchedulerAPI()->PyScheduler_RunWatchdogEx( runTimeInNanoseconds,
															 PY_WATCHDOG_SOFT | PY_WATCHDOG_IGNORE_NESTING | PY_WATCHDOG_TOTALTIMEOUT );

	time2 = CcpGetTimestamp();

	mInQueue2 = SchedulerAPI()->PyScheduler_GetRunCount() - 1;

	if( !r )
	{
		return false;
	}

	Py_DECREF( r );

	double dt = ( time2 - time1 ) * mPt;

	//check for positive, since QueryPerformanceCounter may be broken on some
	//machines, giving us negative results.  The average of the positive ones ought
	//to be in the ballpark, though.  Also catches rollover.
	if( dt < 0.0 )
	{
		return true; //in case of rollover, do nothing
	}
	mLastDuration = (float)dt;

	LOGIT( "Ran for %fs, runqueue=%d", dt, mInQueue2 );

	//If we interrupted we can now learn on our interrupt behaviour.
	//Test for positive to catch integer rollovers.
	
	//Overshoot is calculated but not currently used anywhere
	//However this is a useful metric to track and warn game code developers of
	if( dt > t )
	{
		//update overshoot estimate.
		//We will overshoot our request because switch will not occur right away.
		//The overshoot will vary, but we try to find a good average.
		///We start estimating from 0, so that we slowly zoom into a something safe
		long over = dt - t;
		//smooth the overshoot
		double s = 0.01;
		mOvershoot = (long)( s * over + ( 1.0 - s ) * mOvershoot );
	}
	return true;
}

#endif
