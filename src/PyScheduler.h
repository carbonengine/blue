// Copyright © 2008 CCP ehf.

/* 
	*************************************************************************

	PyScheduler.h

	Project:   Blue

	Description:   

		A Scheduler to run python tasklets for a certain amount of time.

	Dependencies:

		Blue

	*************************************************************************
*/

#ifndef PYSCHEDULER_H
#define PYSCHEDULER_H

struct SchedulerStats
{
	SchedulerStats() :
		numberOfTaskletsInQueuePreTick( 0 ),
		numberOfTaskletsInQueuePostTick( 0 ),
		lastDurationMs( 0 ),
		numberOfActiveScheduleManagers( 0 ),
		numberOfActiveChannels( 0 ),
		numberOfActiveTaskets( 0 ),
		numberOfTaskletsCompletedLastTick( 0 ),
		numberOfTaskletsSwitchedLastTick( 0 ),
		maxTimeMs( 0 ),
		overshootMs( 0 )
	{}

	int numberOfTaskletsInQueuePreTick;
	int numberOfTaskletsInQueuePostTick;
	double lastDurationMs;
	int numberOfActiveScheduleManagers;
	int numberOfActiveChannels;
	int numberOfActiveTaskets;
	int numberOfTaskletsCompletedLastTick;
	int numberOfTaskletsSwitchedLastTick;
	double maxTimeMs;
	double overshootMs;
};

class PyScheduler
{
public:
	PyScheduler(double maxTime = 0.0);

	// Schedule pytthon taskets for "t" seconds.  May overshoot.
	bool RunTime( double t );

	// Same as above, but use the maxTime used in the constructor.
	bool Run()
	{
		return RunTime(mMaxTimeSec);
	}

	SchedulerStats& GetStats()
	{
		return mStats;
	}

	void SetMaxTime(float maxTime) {
		mMaxTimeSec = maxTime;
	}
	float GetMaxTime() const {
		return mMaxTimeSec;
	}

	void UpdatePreTickStats();

	void UpdatePostTickStats( std::chrono::duration<double> maxTimeSec, std::chrono::duration<double> lastDurationSec );

private:
	float mMaxTimeSec; //max time we want to take (seconds).
	SchedulerStats mStats; //Run statistics
};

#endif //define PYSCHEDULER_H