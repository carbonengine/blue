// Copyright © 2012 CCP ehf.

#include "StdAfx.h"

#if CCP_STACKLESS

#include "TaskletTimer.h"

BLUE_DEFINE( TaskletTimer );

const Be::ClassInfo* TaskletTimer::ExposeToBlue()
{
	EXPOSURE_BEGIN( TaskletTimer, "" )
		MAP_INTERFACE(ITaskletTimer)
		MAP_INTERFACE(INotify)

		MAP_ATTRIBUTE( "timesliceWarning", mSliceWarning, "Max. acceptable timeslice in ms. for tasklet (0=disable).", Be::READWRITE )
		MAP_ATTRIBUTE( "active", mActive, "is the timer on or off", Be::READWRITE | Be::NOTIFY )
		MAP_ATTRIBUTE( "telemetryOn", mDoTelemetry, "are we reporting to Telemetry", Be::READWRITE )
		MAP_ATTRIBUTE
		( 
			"taskletWarnings", 
			mMaxWarn.o, 
			"Dictionary of tasklets that have run over the timeslice warning period.\n"
			"The key is the context of the tasklet - the value is a tuple of the\n"
			"tasklet id and the number of times the tasklet has run over.", 
			Be::READWRITE
		)

		MAP_METHOD_AS_METHOD
		(
			"GetCurrent",
			PyGetCurrent, 
			"GetCurrent" 
		)
		
		MAP_METHOD_AS_METHOD
		(
			"GetElapsed",
			PyGetElapsed, 
			"GetElapsed" 
		)
		
		MAP_METHOD_AS_METHOD
		(
			"EnterTasklet",
			PyEnterTasklet, 
			"EnterTasklet" 
		)
		
		MAP_METHOD_AS_METHOD
		(
			"ReturnFromTasklet",
			PyReturnFromTasklet, 
			"ReturnFromTasklet" 
		)
		
		MAP_METHOD_AS_METHOD
		(
			"Reset",
			PyReset, 
			"Reset" 
		)
		
		MAP_METHOD_AS_METHOD
		(
			"GetTasklets",
			PyGetTasklets, 
			"GetTasklets"
		)
		
		MAP_METHOD_AS_METHOD
		(
			"GetThreadTimes",
			PyGetThreadTimes, 
			"GetThreadTimes"
		)
		
		MAP_METHOD_AS_METHOD
		(
			"GetProcessTimes",
			PyGetProcessTimes, 
			"GetProcessTimes"
		)
		
		MAP_METHOD_AS_METHOD
		(
			"GetOverhead",
			PyGetOverhead, 
			"GetOverhead"
		)
	EXPOSURE_END()
}

#endif
