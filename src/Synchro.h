// Copyright © 2014 CCP ehf.

/* 
	*************************************************************************

	Synchro.h

	Author:    Matthias Gudmundsson
	Created:   Mar. 2003
	OS:        Win32
	Project:   Yet another synchro port

	Description:   

		Takes care of ticks, sleeps and yielders

	(c) CCP 2003

	*************************************************************************
*/

#if CCP_STACKLESS


#ifndef _SYNCHRO_H_
#define _SYNCHRO_H_

#include "IBlueOS.h"
#include "Heap.h"
#include "PyTemplates.h"
#include <set>
#include <vector>

// Forward declare types from the <Scheduler.h> header, include will be in .cpp file
struct PyTaskletObject;
struct PyChannelObject;

#undef Yield

//--------------------------------------------------------------------


//--------------------------------------------------------------------
class Synchro :
	public PyXObject<Synchro>
{
public:
	Synchro();
	~Synchro();
	
	bool Tick();

	struct Stat {
		Be::Time mTime;
		int mRunnable;
		int mYielders;
		size_t mSleepers;
	};
	void GetLastStat(Stat &s)
	{
		size_t i = mStats.size();
		if (i > 0) {
			size_t j = mStats[i-1].size();
			if (j > 0) {
				s = mStats[i-1][j-1];
				return;
			}
		}
		memset(&s, 0, sizeof(s));
	}

	void RebaseSimClock(Be::Time oldTime, Be::Time newTime);  // For Blue to be able to tell us the sim clock re-based

private:
	
	// data members
	PyObject* mTickers;		// tickers list
	PyObject* mTimeSyncs;	// time synchronization callback list
	
	
	// Sleeper stuff
	struct Sleeper
	{
		int64_t due;		// 64 bit objects first (alignment)
		PyChannelObject* channel;
		PyTaskletObject* tasklet;
		//copy constructor, to copy exactly our data
		Sleeper() {}
		Sleeper(const Sleeper &other) : due(other.due), channel(other.channel), tasklet(other.tasklet) {}
		Sleeper &operator= (const Sleeper &other) {
			due = other.due;
			channel = other.channel;
			tasklet = other.tasklet;
			return *this;
		}
	};

	friend bool operator<(const Sleeper& x, const Sleeper& y) {
		return x.due == y.due ? x.channel < y.channel : x.due < y.due;}

	Heap<Sleeper> mWallclockSleepers;
	Heap<Sleeper> mSimSleepers;
	typedef Heap<Sleeper>::iterator SleeperIt;
	
	// Yielders
	PyChannelObject* mYielders;

	// shutdown flag
	bool mShutdown;

	// Stats:
	
	std::vector<std::vector<Stat> > mStats;
	void AddStat();
	PyObject *Get_stats();

	//Blockable test and exception generator
	bool CatchMain();

	PyObject *SleepWallclock(int ms, const int64_t &due);
	PyObject *SleepSim(int ms, const int64_t &due);

	bool FindTasklet(PyObject *tasklet, Heap<Sleeper> &sleeperHeap, SleeperIt &outSleeper);
	
	// Python blurb	
public:
	PYTHON_CLASS("Synchro");
	static void InitializeType(PyTypeObject* type);
	PyObject* Str();

    PyObject* ResetClock(PyObject* newTime);
	PyObject* SleepWallclock(PyObject* millisec);

	void RemoveSleeper( Heap<Sleeper> &sleepers, Sleeper &sl );

	PyObject* SleepSim(PyObject* millisec);
	PyObject* SleepUntilWallclock(PyObject *due);
	PyObject* SleepUntilSim(PyObject *due);
	PyObject* Yield();
	PyObject* Wakeup(PyObject* args);
	PyObject* WakeupAtWallclock(PyObject *args);
	PyObject* WakeupAtSim(PyObject *args);
	PyObject* Get_sleepers();
	void Shutdown();


	PYTHON_METHODS_BEGIN()
		METHOD_O(
			ResetClock, 
			"ResetClock(newTime) -> None\n\n"
			"Updates blue's clock to 'newTime' and resynchronized all sleepers accordingly."
			)

		METHOD_O(
			SleepWallclock, 
			"SleepWallclock(ms) -> None or argument to WakeupAt\n\n"
			"Suspends the calling tasklet for 'ms' milliseconds."
			)
		{"Sleep", PyCFuncArgs<&Synchro::SleepWallclock>, METH_O, // TDTODO - Remove me
			"Sleep(ms) -> None or argument to WakeupAt\n\n"
			"Suspends the calling tasklet for 'ms' milliseconds wallclock time."},
			
		METHOD_O(
			SleepSim, 
			"SleepSim(ms) -> None or argument to WakeupAt\n\n"
			"Suspends the calling tasklet for 'ms' milliseconds simulation time."
			)

		METHOD_O(
			SleepUntilWallclock, 
			"SleepUntilWallclock(time) -> None or argument to WakeupAt\n\n"
			"Suspends the calling tasklet until the time specified (might be later)."
			)
		{"SleepUntil", PyCFuncArgs<&Synchro::SleepUntilWallclock>, METH_O, // TDTODO - Remove me
			"SleepUntil(time) -> None or argument to WakeupAt\n\n"
			"Suspends the calling tasklet until the wallclock time specified (might be later)."},

		METHOD_O(
			SleepUntilSim, 
			"SleepUntilSim(time) -> None or argument to WakeupAt\n\n"
			"Suspends the calling tasklet until the simulation time specified (might be later)."
			)

		METHOD_NOARGS(
			Yield, 
			"Yield() -> None\n\n"
			"Reschedules the calling tasklet to be run on the next frame (or application cycle)."
			)
			
		METHOD_VARARGS(
			Wakeup, 
			"Wakeup(tasklet,arg=None) -> None\n\n"
			"Wakes up the given sleeping tasklet."
			)

		METHOD_VARARGS(
			WakeupAtWallclock, // TDTODO - Make when non-optional
			"WakeupAtWallclock(tasklet,when=0,arg=None)\n"
			"wakes up the sleeping tasklet at \"when\""
			)
		{"WakeupAt", PyCFuncArgs<&Synchro::WakeupAtWallclock>, METH_VARARGS, // TDTODO - Remove me
			"Wakeup(tasklet,when=0,arg=None)\n"
			"wakes up the sleeping tasklet at \"when\" or immediately with the optional arg"
			},

		METHOD_VARARGS(
			WakeupAtSim,
			"WakeupAtSim(tasklet,when,arg=None)\n"
			"wakes up the sleeping tasklet at \"when\""
			)

	PYTHON_METHODS_END()

	PYTHON_GETSET_BEGIN()
		PYTHON_GET(sleepers, "sleepers")
		PYTHON_GET(stats, "")
	PYTHON_GETSET_END()

	// published members
	PYTHON_MEMBERS_BEGIN()
		PYTHON_MEMBER("tickers", T_OBJECT, mTickers, READONLY)
		PYTHON_MEMBER("timesyncs", T_OBJECT, mTimeSyncs, READONLY)
		PYTHON_MEMBER("yielders", T_OBJECT, mYielders, READONLY)
	PYTHON_MEMBERS_END()
};


#endif


#endif
