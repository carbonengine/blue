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

// DANGER WILL ROBINSON!
// This is required for GetPythonTickCount - there may be better options available now than to rely on the internal API
#define Py_BUILD_CORE
#include <internal/pycore_pystate.h>
#undef Py_BUILD_CORE

#include "PyScheduler.h"
#include "SchedulerCAPI.h"

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
double PyScheduler::mTPS = 0.0;

PyScheduler::PyScheduler(double maxTime)
{
	mMaxTime = (float)maxTime;
	mOvershoot = mMinTicks = 0;
	mLastDuration = 0.0f;
	mInQueue1 = mInQueue2 = 0;
	if ( !mPt )
	{
		mPt = 1.0/(double)CcpGetTimestampFrequency();
	}
}


// This functionality will not be required when we move fully to scheduler, hence why it is commented out 
// and just returns 0
int GetPythonTickCount() {
	//PyThreadState *tstate = PyThreadState_Get();
	//return tstate->st.tick_counter * tstate->interp->check_interval;
	return 0;
}

bool PyScheduler::RunTicks(long ticks)
{
	LOGIT("Running %d ticks", ticks);
	// Run tasklets for ticks time, measuring the time required and the ticks used.
	uint64_t time1, time2;
	long tick1, tick2;

	tick1 = GetPythonTickCount();
	mInQueue1 = SchedulerAPI()->PyScheduler_GetRunCount()-1;
    time1 = CcpGetTimestamp();
	PyObject *r = SchedulerAPI()->PyScheduler_RunWatchdogEx( ticks,
		PY_WATCHDOG_SOFT | PY_WATCHDOG_IGNORE_NESTING | PY_WATCHDOG_TOTALTIMEOUT );
    time2 = CcpGetTimestamp();
	tick2 = GetPythonTickCount();
	mInQueue2 = SchedulerAPI()->PyScheduler_GetRunCount()-1;
	
	if(!r)
	{
		return false;
	}

	if( PyObject_TypeCheck( r, SchedulerAPI()->PyTaskletType ) )
	{
		CCP_LOGWARN("RunWatchdog returned a tasklet - rescheduling it");
		SchedulerAPI()->PyTasklet_Insert((PyTaskletObject*)r);
	}
	else if( r != Py_None )
	{
		CCP_LOGWARN("RunWatchdog returned an unexpected value");
	}

	Py_DECREF(r);

	//update ticks per second estimate if we have enough ticks or if we are here
	//for the first time.
	long dtick = tick2-tick1;
	double dt = ( time2 - time1 ) * mPt;

	//check for positive, since QueryPerformanceCounter may be broken on some
	//machines, giving us negative results.  The average of the positive ones ought
	//to be in the ballpark, though.  Also catches rollover.
	if ( dt < 0.0 )
	{
		return true; //in case of rollover, do nothing
	}
	mLastDuration = (float)dt;
	
	// Estimate bias from the smallest tick count seen.  Ticks are updated
	// in "CheckInterval" units, and not reported with that bias.
	if ( dtick > 0 && ( mMinTicks == 0 || dtick < mMinTicks ) )
		mMinTicks = dtick;
	// Add the bias to the ticks reported
	dtick += mMinTicks / 2;
		
	LOGIT("Ran %d ticks in %fs, tps=%f, runqueue=%d", dtick, dt, mTPS, mInQueue2);
	if ( dtick > 500 || ( !mTPS && dtick>0 ) )
	{
		double tps = (double)dtick / dt;
		if ( mTPS )
		{
			// Exponential smoothing
			// We could employ a smoothing factor that lends more weight to _longe_ periods...
			// something like 
			// a = 1 - exp(-dt/k) where k is some scaling factor...
			double a = 0.01; //smothing factor.  Tps is quite stable.
			mTPS = a * tps + ( 1.0 - a ) * mTPS;
		}
		else
		{
			mTPS = tps;
		}
	}

	//If we interrupted we can now learn on our interrupt behaviour.
	//Test for positive to catch integer rollovers.
	if ( dtick > ticks )
	{
		//update overshoot estimate.
		//We will overshoot our request because switch will not occur right away.
		//The overshoot will vary, but we try to find a good average.
		///We start estimating from 0, so that we slowly zoom into a something safe 
		long over = dtick - ticks;
		//smooth the overshoot
		double s = 0.01;
		mOvershoot = (long)( s * over + ( 1.0 - s ) * mOvershoot );
	}
	return true;
}

bool PyScheduler::RunTime( double t )
{
	long ticks;
	
	if (mTPS)
	{
#ifdef USE_OVERSHOOT
		//overshoot disabled, it causes too much instability in the estimation under load
		long overshoot = mOvershoot;
#else
		long overshoot = 0;
#endif
		ticks = (long)( t * mTPS - overshoot );
	}
	else
		ticks = 1000;  //try to get some reasonable estimates

	//maintain some sanity, also catches mTPS==0
	ticks = std::min( 10000000L, std::max( 100L, ticks ) ); //between 100 and 10 million
	return RunTicks( ticks );
}

#endif
