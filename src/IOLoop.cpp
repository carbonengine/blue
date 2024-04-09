/* 
	*************************************************************************

	IOLoop.cpp

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
#include "StdAfx.h"

#if CCP_STACKLESS && !PORTING_TO_LINUX

#include "IOLoop.h"

#include "IBlueOS.h"
#include "ITaskletTimer.h"

#include "PyScheduler.h"

#include <Scheduler.h>

#include <stacklessio_api.h>
#include <CarbonIO/dll_exports.h>

#include <BluePyCpp.h>

/* #define LOGG */
#ifdef LOGG
static char logbuf[256];
#define LOGIT(text, ...) do { \
	sprintf_s(logbuf, "SleepWithIo::" text "\n", __VA_ARGS__); \
	OutputDebugString(logbuf);\
} while (0)

#else
#define LOGIT(text, ...) (void)0
#endif

extern HANDLE gBreakSleep;
extern HANDLE gBreakCarbonIo;
extern bool g_carbonIoManualWakeup;

PyObject *SleepWithIo::m_ctxtSleep = 0;
PyObject *SleepWithIo::m_ctxtRun = 0;
SleepWithIo::SleepWithIo() :
	m_scheduler(0)
{
	if ( !m_ctxtSleep )
	{
		m_ctxtSleep = PyUnicode_FromString("SleepWithIo::Wait");
		m_ctxtRun = PyUnicode_FromString("SleepWithIo::Run");
	}
}

//Forward to inner function and handle errors.
void SleepWithIo::Sleep( float totalSleepTime, bool &retry )
{
	float relativeSleepTime;
	if ( !retry )
	{
		//The initial call
		QueryPerformanceCounter( &m_startTime );
		m_handles[0] = gBreakSleep;
		m_handles[1] = PyStacklessIoGetWakeupEventHandle();
		m_nHandles = 2;
		if ( gBreakCarbonIo != INVALID_HANDLE_VALUE )
		{
			m_handles[2] = gBreakCarbonIo;
			m_nHandles = 3;
		}
		m_totalSleepTime = totalSleepTime;
		relativeSleepTime = totalSleepTime;
		m_firstRun = true;
	}
	else
	{
		// We don't allow sleep time to grow over iterations. This is a sanity measure.
		if ( totalSleepTime < m_totalSleepTime )
		{
			LOGIT("time reduced from %f to %f", m_totalSleepTime, totalSleepTime);
			m_totalSleepTime = totalSleepTime;
		}
		relativeSleepTime = RelativeSleepTime();
		m_firstRun = false;
	}
	LOGIT("Sleep start retry %d, %f", (int)retry, relativeSleepTime);

	//Set retry to false.  It is only set to true when we finally succeed
	retry = false;
	if ( relativeSleepTime > 0.0f )
	{
		bool ok = SleepAndRun( retry, relativeSleepTime );
		if ( !ok )
		{
			BeOS->SetError( BEDEF, 0, "SleepWithIo::Sleep" );
			BeOS->SetError( BEFLUSH );
		}
	}
	LOGIT("Sleep stop  retry %d", (int)retry);
}

// Perform the sleep and subsequent scheduling of python code as an IO response.
// Return false if there is a Blue error
bool SleepWithIo::SleepAndRun( bool &retry, float relativeSleepTime )
{
	// If this is not the first call, there may be runnable tasklets left over
	// from the last scheduling run.  In this case, don't sleep, just schedule.
	bool shouldSleep = true;
	if ( !m_firstRun )
	{
		if ( SchedulerAPI()->PyScheduler_GetRunCount() - 1 > 0 )
		{
			LOGIT("nosleep due to runnable taskets");
			shouldSleep = false;
		}
	}
	if ( shouldSleep )
	{
		// We loop here, since we may need to sleep multiple times to clear
		// away redundant IO events.  This actually happens at most once.
		bool workToDo;
		do {
			DWORD result;
			if ( !WaitForIo( result, relativeSleepTime ) )
			{
				return false;
			}
			if ( result == WAIT_TIMEOUT ||
				 result == WAIT_OBJECT_0 || //gBreakSleep
				 result == WAIT_OBJECT_0 + m_nHandles ) //win msg
			{
				//stop the IO loop because of timeout, or non IO events.
				LOGIT( "Stoploop %d", result );
				return true;
			}
			_ASSERT( result >= WAIT_OBJECT_0 + 1 && result < WAIT_OBJECT_0 + m_nHandles );

			//How much time is remaining after the sleep?
			relativeSleepTime = RelativeSleepTime();
		
			if ( relativeSleepTime <= 0.0f)
			{
				return true; //no time to deal with IO here
			}
		
			if (result == WAIT_OBJECT_0 + 1)
			{
				// We were signaled by stacklessIO
				// Verify that there is python IO work to be done.
				if ( !CheckStacklessIo( workToDo ) )
				{
					return false; //error
				}
			}
			else
			{
				// We were signaled by CarbonIo
				// Verify that there is python IO work to be done.
				if ( !CheckCarbonIo( workToDo ) )
				{
					return false; //error
				}
			}

		} while ( !workToDo );
	}

	// There was IO and we should run the tasklets and continue sleeping
	// if appropriate.
	LOGIT( "Scheduling %f", relativeSleepTime );
	bool ok;
	{
		AutoTasklet _at( PyOS->GetTaskletTimer(), m_ctxtRun );
		ok = m_scheduler.RunTime( relativeSleepTime );
	}
	if ( !ok )
	{
		PyOS->PyFlushError( "SleepWithIo" );
		return false;
	}

	/* Since we have now run Python, it is possible that the sleeptime
	 * has changed, due to some synchro activity.  Therefore, we exit the loop
	 * but signal to the caller that he should call us again
	 */
	retry = true;
	return true;
}

//Wait for IO, and deal with errors
bool SleepWithIo::WaitForIo( DWORD &result, float sleepTime )
{
	DWORD ms = (DWORD)( sleepTime * 1000.0 );
	if ( ms == 0 )
	{
		result = WAIT_TIMEOUT;
		return true;
	}
	{
		AutoTasklet _at( PyOS->GetTaskletTimer(), m_ctxtSleep, true, IDLE);
		Ccp::PyAllowThreads _allow; //allow python threads during the sleep
		LOGIT("StartSleep %f", sleepTime);
		result = MsgWaitForMultipleObjectsEx(
			m_nHandles, m_handles,
			ms, QS_ALLEVENTS, MWMO_ALERTABLE );
#ifdef LOGG
		LOGIT("StopSleep A %f", RelativeSleepTime());
#endif
	}
#ifdef LOGG
	LOGIT("StopSleep B %f", RelativeSleepTime());
#endif
	if ( result == WAIT_TIMEOUT || ( result >= WAIT_OBJECT_0 && result <= WAIT_OBJECT_0 + m_nHandles ) )
	{
		return true;
	}
	if ( result == WAIT_FAILED )
	{
		 BeOS->SetError( BE32, NULL, "MsgWaitForMultitpleObjectsEx failed" );
	}
	else
	{
		BeOS->SetError( BEDEF, NULL, "MsgWaitForMultitpleObjectsEx failed: %d", result );
	}
	return false;
}

//After waking up by the stacklessIO flag, see if there is work to be done
bool SleepWithIo::CheckStacklessIo( bool &result )
{
	result = false;
	PyStacklessIoStatus_t s;
	s.struct_size = sizeof(s);
	PyStacklessIoGetStatus(&s);
	if (s.nNonRunnable) {
		//StacklessIO needs pumping
		int p = PyStacklessIoDispatchEvents("SleepWithIO");
		if (p < 0) {
			PyOS->PyFlushError("SleepWithIO");
			return false;
		}
		result = SchedulerAPI()->PyScheduler_GetRunCount() > 1;
	} else if (s.nRunnable)
		result = true;
	return true;
}

//After waking up by the CarbonIo flag, see if there is work to be done
bool SleepWithIo::CheckCarbonIo( bool &result )
{
	// Depending on this flag, now have carbonIO wake up its tasklets.  Perhaps it
	// didn't already do so (because it merely scheduled a pending call) so this forces
	// the issue.
	// This is the right thing to do, but it is switchable for testing purposes.
	if (g_carbonIoManualWakeup)
		CioWakeupTasklets();
	result = SchedulerAPI()->PyScheduler_GetRunCount() > 1;
	return true;
}

//Return the timescale used by QueryPerformanceCounter
double SleepWithIo::m_tScale = 0.0;
double SleepWithIo::TimeScale()
{
	if (m_tScale == 0.0)
	{
		LARGE_INTEGER f;
		QueryPerformanceFrequency(&f);
		m_tScale = 1.0 / (double)f.QuadPart;
	}
	return m_tScale;
}

float SleepWithIo::RelativeSleepTime()
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	double dt = (double)(now.QuadPart - m_startTime.QuadPart) * TimeScale();
	return m_totalSleepTime - (float)dt;
}

#endif
