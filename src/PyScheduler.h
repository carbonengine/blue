/* 
	*************************************************************************

	PyScheduler.h

	Author:    Kristjan Valur Jonsson
	Created:   Okt. 2008
	OS:        Win32
	Project:   Blue

	Description:   

		A Scheduler to run python tasklets for a certain amount of time.
		Uses internal statistics gathering to estimate the tick count
		required.


	Dependencies:

		Blue

	(c) CCP 2008

	*************************************************************************
*/

#ifndef PYSCHEDULER_H
#define PYSCHEDULER_H

class PyScheduler
{
public:
	PyScheduler(double maxTime = 0.0);

	//Schedule pytthon taskets for "t" seconds.  May overshoot.
	bool RunTime( double t );

	//Same as above, but use the maxTime used in the constructor.
	bool Run()
	{
		return RunTime(mMaxTime);
	}

	void GetStats(int &inQueue1, int &inQueue2, float &lastTime) {
		inQueue1 = mInQueue1;
		inQueue2 = mInQueue2;
		lastTime = mLastDuration;
	}

	void SetMaxTime(float maxTime) {
		mMaxTime = maxTime;
	}
	float GetMaxTime() const {
		return mMaxTime;
	}


private:
	float mMaxTime; //max time we want to take (seconds).
	long mOvershoot; //overshoot in time (seconds), estimate.
	static double mPt; //duration of a performance 'tick'
	int mInQueue1;		//runnables in queue before run
	int mInQueue2;		//runnables in queue after run
	float mLastDuration;//duration of last run
};

#endif //define PYSCHEDULER_H