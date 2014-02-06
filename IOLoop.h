/* 
	*************************************************************************

	IOLoop.h

	Author:    Kristjan Valur Jonsson
	Created:   Mai. 2011
	OS:        Win32
	Project:   Blue

	Description:   

		A mechanism to sleep and handle python IO without starting a new
		frame in BeOS.  This is a high speed loop that sleeps for a certain
		amount of time, but handles python events as it sees fit during
		this time.

	Dependencies:

		Blue

	(c) CCP 2011

	*************************************************************************
*/
#pragma once
#ifndef IOLOOP_H
#define IOLOOP_H

#include "PyScheduler.h"


// This is how you use this, typically, to sleep for a certain time, yet
// service IO during this time:

// SleepWithIo sleeper;
// bool retry = false;
// do {
//    sleeper.Sleep(sleeptime, retry)
// } while (retry);

// Each time retry is true on exit, Pyton tasklets were run as a result of IO
// happening.
// the sleeper instanance keeps track of the time spent in total so the caller
// needs not update "sleeptime".  This remains the total sleeptime for this
// batch of invokations.  But it can be reduced.  For example, some of the io code
// may decide that we shouldn't sleep this long!
class SleepWithIo
{
public:
	SleepWithIo();
	void Sleep( float sleeptime, bool &retry );
	void Sleep( int sleeptime_ms , bool &retry )
	{
		Sleep( (float)sleeptime_ms * 0.001f , retry );
	}

private:
	bool SleepAndRun( bool &retry, float relativeSleepTime );
	bool WaitForIo( DWORD &result, float sleeptime );
	static bool CheckStacklessIo( bool &result );
	static bool CheckCarbonIo( bool &result );
	float RelativeSleepTime();
	static double TimeScale();

private:
	float m_totalSleepTime;
	PyScheduler m_scheduler;
	LARGE_INTEGER m_startTime;
	HANDLE m_handles[3];
	DWORD m_nHandles;
	bool m_firstRun;
	static double m_tScale;
	static PyObject *m_ctxtSleep;
	static PyObject *m_ctxtRun;
};

#endif /* IOLOOP_H */