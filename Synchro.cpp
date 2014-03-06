#include "StdAfx.h"

#if CCP_STACKLESS

#include "Synchro.h"
#include "include/IBlueOS.h"
#include "include/IBluePython.h"
#include "include/ITaskletTimer.h"

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "Synchro" );

CCP_STATS_DECLARE( statSleepers,	"Blue/Synchro/sleepers", false,	CST_COUNTER_LOW, "Number of tasklets sleeping" );
CCP_STATS_DECLARE( statYielders,	"Blue/Synchro/yielders", false,	CST_COUNTER_LOW, "Number of tasklets that yielded the frame" );
CCP_STATS_DECLARE( statRunnable,	"Blue/Synchro/runnable", false,	CST_COUNTER_LOW, "Number of runnable tasklets" );


struct TimerString
{
	const char* mName;
	PyObject* mContext;
};

static TimerString TIMERS[] =
{
	{"Tick::Tick", NULL},
	{"Tick::Resume yielders", NULL},
	{"Tick::Resume sleepers", NULL},
	{"Tick::Tick timers", NULL},
	{"Tick::Sleep", NULL},
	{"Tick::SetTimer", NULL},
	{"Tick::KillTimer", NULL},
};

// Timer strings
enum
{
	TIMER_TICK,
	TIMER_YIELDERS,
	TIMER_SLEEPERS,
	TIMER_TIMERS,
	TIMER_SLEEP,
	TIMER_SETTIMER,
	TIMER_KILLTIMER,
};


//--------------------------------------------------------------------
Synchro::Synchro()
{
	mTickers = PyList_New(0);
	mTimeSyncs = PyList_New(0);
	mYielders = PyChannel_New(NULL);
	PyChannel_SetPreference(mYielders, 0); //slow wakeup, just make it runnable.
	mShutdown = false;
	
	for (int i = 0; i < sizeof TIMERS / sizeof *TIMERS; i++)
		TIMERS[i].mContext = PyString_InternFromString(TIMERS[i].mName);
}


//--------------------------------------------------------------------
Synchro::~Synchro()
{
	Shutdown();
	Py_XDECREF(mTickers);
	Py_XDECREF(mTimeSyncs);
	Py_XDECREF(mYielders);

	for (int i = 0; i < sizeof TIMERS / sizeof *TIMERS; i++)
		Py_XDECREF(TIMERS[i].mContext);
}


//--------------------------------------------------------------------
void Synchro::InitializeType(PyTypeObject* type)
{
	type->tp_str = _Str;
	type->tp_doc = "Synchro - synchronizing stuff since 2003";
}


//--------------------------------------------------------------------
//shutdow: wake up all sleepers.
void Synchro::Shutdown()
{
	mShutdown = true;
	Py_CLEAR(mTickers);
	Py_CLEAR(mTimeSyncs);
	
	PyChannelObject *yielders = mYielders;
	mYielders = 0;

	Heap<Sleeper> wallSleepers;
	wallSleepers.swap(mWallclockSleepers);

	Heap<Sleeper> simSleepers;
	simSleepers.swap(mSimSleepers);

	//ok, now wake them all
	PyObject *value = 0;
	if (yielders) {
		for(int i = PyChannel_GetBalance(yielders);  i<0; i++) {
			if (!value)
				value = PyString_FromString("Synchro is shutting down");
			int fail = PyChannel_SendException(yielders, PyExc_TaskletExit, value);
			if (fail)
				PyOS->PyError();
		}
		Py_DECREF(yielders);
	}

	for(SleeperIt it = wallSleepers.begin(); it!= wallSleepers.end(); ++it) {
		if (PyChannel_GetBalance(it->channel)) {
			if (!value)
				value = PyString_FromString("Synchro is shutting down");
			int fail = PyChannel_SendException(it->channel, PyExc_TaskletExit, value);
			if (fail)
				PyOS->PyError();
		}
		Py_DECREF(it->channel);
	}

	for(SleeperIt it = mSimSleepers.begin(); it!= mSimSleepers.end(); ++it) {
		if (PyChannel_GetBalance(it->channel)) {
			if (!value)
				value = PyString_FromString("Synchro is shutting down");
			int fail = PyChannel_SendException(it->channel, PyExc_TaskletExit, value);
			if (fail)
				PyOS->PyError();
		}
		Py_DECREF(it->channel);
	}
	
	Py_XDECREF(value);
}


//--------------------------------------------------------------------
void Synchro::ScheduleTick()
{
	if (mWallclockSleepers.empty() && mSimSleepers.empty())
		return;

	Be::Time wallSleepersDue;
	if (!mWallclockSleepers.empty()) {
		wallSleepersDue = mWallclockSleepers.begin()->due - BeOS->GetInfo()->mRealTime;
	} else {
		wallSleepersDue = LONG_MAX; // A suitably large number
	}
	Be::Time simSleepersDue;
	if (!mSimSleepers.empty()) {
		simSleepersDue = mSimSleepers.begin()->due - BeOS->GetInfo()->mSimTime;
		// Bring the sim due time back to real time
		simSleepersDue = Be::Time(simSleepersDue / BeOS->GetInfo()->mSimDilation);
	} else {
		simSleepersDue = LONG_MAX; // A suitably large number
	}

	int ms = int(std::min(wallSleepersDue, simSleepersDue)) / 10000;
	BeOS->NextScheduledEvent(ms);
}


//--------------------------------------------------------------------
bool Synchro::Tick()
{
	AutoTasklet _at(PyOS->GetTaskletTimer(), TIMERS[TIMER_TICK].mContext);
	
	Be::Time now = BeOS->GetInfo()->mRealTime + 1;

	//Add stats
	AddStat();

	//wake up sleepers (they get to go first, they asked for a certain time)
	{
		AutoTasklet _at(PyOS->GetTaskletTimer(), TIMERS[TIMER_SLEEPERS].mContext);
		if (mWallclockSleepers.size()) {
			//gather sleepers for wakeup
			std::vector<Sleeper> sleepers;
			while (mWallclockSleepers.size()) {
				if (mWallclockSleepers.front().due > now)
					break;
				sleepers.push_back(mWallclockSleepers.Shift());
			}
			if (sleepers.size()) {
				Be::Time nnow = BeOS->GetActualTime();
				for(unsigned int i = 0; i<sleepers.size(); i++) {
					Sleeper &s = sleepers[i];
					if (PyChannel_GetBalance(s.channel)) {
						if (nnow-s.due > 50*10*1000*1000) { //50s
							PyObject *head = PyChannel_GetQueue(s.channel);
							if (head) {
								BluePy str(PyObject_Str(head));
								if (str)
									CCP_LOGWARN_CH( s_ch, "tasklet %s woken up %ds late (%I64d %I64d)", PyString_AsString(str), 
										(now-s.due)/(10*1000*1000), BeOS->GetActualTime(), BeOS->GetInfo()->mRealTime);
								Py_DECREF(head);
							}
						}
						int res = PyChannel_Send(s.channel, Py_None);
						if (res)
							PyOS->PyError();
					}
					Py_DECREF(s.channel);
				}
			}
		}
		if (mSimSleepers.size()) {
			Be::Time simNow = BeOS->GetInfo()->mSimTime + 1;
			//gather sleepers for wakeup
			std::vector<Sleeper> sleepers;
			while (mSimSleepers.size()) {
				if (mSimSleepers.front().due > simNow)
					break;
				sleepers.push_back(mSimSleepers.Shift());
			}
			if (sleepers.size()) {
				for(unsigned int i = 0; i<sleepers.size(); i++) {
					Sleeper &s = sleepers[i];
					if (PyChannel_GetBalance(s.channel)) {
						int res = PyChannel_Send(s.channel, Py_None);
						if (res)
							PyOS->PyError();
					}
					Py_DECREF(s.channel);
				}
			}
		}
	}
	ScheduleTick(); //add a tick for the current head of the sleepers queue

	// Tick tickers
	{
		PyObject* noargs = PyTuple_New(0);

		if (noargs) {
			for (int i = 0; i < PyList_GET_SIZE(mTickers); i++)
			{
				PyObject* tuple = PyList_GET_ITEM(mTickers, i);
				PyObject* ticker = PyTuple_GET_ITEM(tuple, 0);
				PyObject* ctx = PyTuple_GET_ITEM(tuple, 1);
				AutoTasklet _at(PyOS->GetTaskletTimer(), ctx);
				PyObject* ret = PyObject_Call(ticker, noargs, NULL);
				if (ret == NULL)
					PyOS->PyError();
				else
					Py_DECREF(ret);
			}
			Py_DECREF(noargs);
		}
	}

	// Resume yielders  Gather those due for wakeup now, don't do it on the
	// fly to avoid reentrancy.
	int nYielders = -mYielders->balance; //wake up only those that are here already.
	if (nYielders>0) {
		AutoTasklet _at(PyOS->GetTaskletTimer(), TIMERS[TIMER_YIELDERS].mContext);
		while (nYielders--)
		{
			int res = PyChannel_Send(mYielders, Py_None);
			if (res)
				PyOS->PyError();
		}
	}
	return true;
}

//--------------------------------------------------------------------
PyObject* Synchro::Str()
{
	return PyString_FromFormat(
		"Synchro with %d wallclock-sleepers, %d sim-sleepers and %d tickers", 
		mWallclockSleepers.size(), mSimSleepers.size(), PyList_GET_SIZE(mTickers)
		);
}


//--------------------------------------------------------------------
PyObject* Synchro::ResetClock(PyObject* newTime)
{
	if (!PyLong_Check(newTime))
	{
		PyErr_SetString(PyExc_TypeError, "'newTime' must be a long value.");
		return NULL;
	}

	Be::Time nt = PyLong_AsLongLong(newTime);
	if (nt == -1 && PyErr_Occurred())
		return 0;
	Be::Time diff = nt - BeOS->GetInfo()->mRealTime;
	PyObject* oldTime = PyLong_FromLongLong(BeOS->GetInfo()->mRealTime);

	// Adjust the wallclock sleepers since we're jerking the wallclock about
	for (SleeperIt si = mWallclockSleepers.begin(); si != mWallclockSleepers.end(); ++si)
		(*si).due += diff;

	extern void SetBlueTime(Be::Time time);
	SetBlueTime(nt);

	// Notify interested parties
	PyObject* args = Py_BuildValue("(OO)", oldTime, newTime);
	for (int i = 0; i < PyList_GET_SIZE(mTimeSyncs); i++)
	{
		PyObject* ret = PyObject_Call(PyList_GET_ITEM(mTimeSyncs, i), args, NULL);
		if (ret == NULL)
			PyOS->PyError();
		else
			Py_DECREF(ret);
	}

	Py_DECREF(oldTime);
	Py_DECREF(args);

	Py_INCREF(Py_None);
	return Py_None;
}

//--------------------------------------------------------------------
void Synchro::RebaseSimClock(Be::Time oldTime, Be::Time newTime)
{
	Be::Time diff = newTime - oldTime;

	CCP_LOG_CH( s_ch, "Shifting sim sleepers by %I64d", diff);
	for (SleeperIt si = mSimSleepers.begin(); si != mSimSleepers.end(); ++si)
		(*si).due += diff;
}

//--------------------------------------------------------------------
// Catch the main thread and raise an exception.  Main thread cannot
// go to sleep here, because only the main thread wakes things in synchro up.
bool Synchro::CatchMain()
{
	PyObject* tasklet = PyStackless_GetCurrent();
	bool result = false;
	if (PyTasklet_IsMain((PyTaskletObject*)tasklet)) {
		PyErr_SetString(PyExc_RuntimeError, "Main tasklet cannot block in Synchro");
		result = true;
	}
	
	Py_DECREF(tasklet);
	return result;
}


//--------------------------------------------------------------------
PyObject* Synchro::SleepWallclock(PyObject* millisec)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if (CatchMain())
		return 0;
	if (mShutdown)
		Py_RETURN_NONE;

	AutoTasklet _at(PyOS->GetTaskletTimer(), TIMERS[TIMER_SLEEP].mContext);
	Be::Time now = BeOS->GetInfo()->mRealTime;
		
	// Get millisec value
	double ms = PyFloat_AsDouble(millisec);
	if (ms == -1 && PyErr_Occurred())
		return 0;
	if (ms<0.0)
		ms = 0.0;

	__int64 due = now + (__int64)(ms*10000.0);
	return SleepWallclock(int(ms), due);
}


//--------------------------------------------------------------------
PyObject* Synchro::SleepSim(PyObject* millisec)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if (CatchMain())
		return 0;
	if (mShutdown)
		Py_RETURN_NONE;

	AutoTasklet _at(PyOS->GetTaskletTimer(), TIMERS[TIMER_SLEEP].mContext);
	Be::Time now = BeOS->GetInfo()->mSimTime;
		
	// Get millisec value
	double ms = PyFloat_AsDouble(millisec);
	if (ms == -1 && PyErr_Occurred())
		return 0;
	if (ms<0.0)
		ms = 0.0;

	__int64 due = now + (__int64)(ms*10000.0);
	return SleepSim(int(ms), due);
}


PyObject* Synchro::SleepUntilWallclock(PyObject* dueO)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if (CatchMain())
		return 0;
	if (mShutdown)
		Py_RETURN_NONE;

	AutoTasklet _at(PyOS->GetTaskletTimer(), TIMERS[TIMER_SLEEP].mContext);
		
	// Get millisec value
	__int64 due = PyLong_AsLongLong(dueO);
	if (due == -1 && PyErr_Occurred())
		return 0;

	Be::Time now = BeOS->GetInfo()->mRealTime;
	if (due < now)
		due = now;
	return SleepWallclock((int)((due-now)/10000), due);
}


PyObject* Synchro::SleepUntilSim(PyObject* dueO)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if (CatchMain())
		return 0;
	if (mShutdown)
		Py_RETURN_NONE;

	AutoTasklet _at(PyOS->GetTaskletTimer(), TIMERS[TIMER_SLEEP].mContext);
		
	// Get millisec value
	__int64 due = PyLong_AsLongLong(dueO);
	if (due == -1 && PyErr_Occurred())
		return 0;

	Be::Time now = BeOS->GetInfo()->mSimTime;
	if (due < now)
		due = now;
	return SleepSim((int)((due-now)/10000), due);
}


PyObject* Synchro::SleepWallclock(int ms, const __int64 &due)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Check millisec constraints
	if (ms < 0 || ms > 86400000)
	{
		PyErr_Format(PyExc_RuntimeError, "Cannot sleep for more than 24 hours, 'delta' is %d ms",ms);
		return NULL;
	}

	Sleeper sl;
	sl.channel = PyChannel_New(NULL);
	if (!sl.channel)
		return 0;
	PyChannel_SetPreference(sl.channel, 0); //just make runnable on wakeup
	sl.due = due;

	mWallclockSleepers.Insert(sl);
	BeOS->NextScheduledEvent(ms);
	
	// Go to sleep and wake up! *(the sender releases the channel)
	PyObject *ret = PyChannel_Receive(sl.channel);

	if (!ret) {
		//we were killed, so lets try and find us in the queue, to release resources.
		RemoveSleeper( mWallclockSleepers, sl );
	}
	return ret;
}


PyObject* Synchro::SleepSim(int ms, const __int64 &due)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Check millisec constraints
	if (ms < 0 || ms > 86400000)
	{
		PyErr_Format(PyExc_RuntimeError, "Cannot sleep for more than 24 hours, 'delta' is %d ms",ms);
		return NULL;
	}

	Sleeper sl;
	sl.channel = PyChannel_New(NULL);
	if (!sl.channel)
		return 0;
	PyChannel_SetPreference(sl.channel, 0); //just make runnable on wakeup
	sl.due = due;

	mSimSleepers.Insert(sl);
	BeOS->NextScheduledEvent(int(ms / BeOS->GetInfo()->mSimDilation));
	
	// Go to sleep and wake up! *(the sender releases the channel)
	PyObject *ret = PyChannel_Receive(sl.channel);

	if (!ret) {
		//we were killed, so lets try and find us in the queue, to release resources.
		RemoveSleeper( mSimSleepers, sl );
	}
	return ret;
}

//--------------------------------------------------------------------

bool Synchro::FindTasklet(PyObject *tasklet, Heap<Sleeper> &sleeperHeap, SleeperIt &outSleeper)
{
	for(outSleeper = sleeperHeap.begin(); outSleeper != sleeperHeap.end(); outSleeper++) {
		PyObject *queue = PyChannel_GetQueue(outSleeper->channel);
		bool eq = queue == (PyObject*)tasklet;
		Py_XDECREF(queue);
		if (eq)
			break;
	}

	if (outSleeper == sleeperHeap.end()) {
		return false;
	}
	return true;
}

//--------------------------------------------------------------------

PyObject* Synchro::Wakeup(PyObject *args)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	PyTaskletObject *tasklet;
	PyObject *arg = Py_None;
	Sleeper s;
	SleeperIt it;

	if (!PyArg_ParseTuple(args, "O!|O", &PyTasklet_Type, &tasklet, &arg))
		return 0;

	if (!FindTasklet((PyObject *)tasklet, mWallclockSleepers, it)) {
		if (!FindTasklet((PyObject *)tasklet, mSimSleepers, it)) {
			PyErr_SetString(PyExc_ValueError, "tasklet not found in sleepers");
			return NULL;
		} else {
			s = mSimSleepers.Remove(it);
		}
	} else {
		s = mWallclockSleepers.Remove(it);
	}

	if (PyChannel_GetBalance(s.channel)) {
		int res = PyChannel_Send(s.channel, Py_None);
		if (res)
			PyOS->PyError();
	}
	Py_DECREF(s.channel);

	Py_RETURN_NONE;
}

//--------------------------------------------------------------------
PyObject *Synchro::WakeupAtWallclock(PyObject *args)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	PyTaskletObject *tasklet;
	Be::Time newdue = 0;
	PyObject *arg = Py_None;
	if (!PyArg_ParseTuple(args, "O!|LO:Reschedule", &PyTasklet_Type, &tasklet, &newdue, &arg))
		return 0;

	//find the tasklet.We must be careful not to let iterators survive past channel sends
	//due to iterator debugging (they live on the stack)
	Sleeper s;
	SleeperIt it;
	s.channel = 0;
	{
		if (!FindTasklet((PyObject *) tasklet, mWallclockSleepers, it)) {
			PyErr_SetString(PyExc_ValueError, "tasklet not found in wallclock sleepers");
			return NULL;
		}

		Be::Time now = BeOS->GetInfo()->mRealTime;
		if (newdue > now) {
			//reschedule it.
			it->due = newdue;
			mWallclockSleepers.Reorder(it);
		} else 
			//pop it and wake up
			s = mWallclockSleepers.Remove(it);
	}
	if (s.channel) {
		if (PyChannel_GetBalance(s.channel)) {
			int res = PyChannel_Send(s.channel, arg);
			if (res)
				PyOS->PyError();
		}
		Py_DECREF(s.channel);
	}
	Py_INCREF(Py_None);
	return Py_None;
}

//--------------------------------------------------------------------
PyObject *Synchro::WakeupAtSim(PyObject *args)
{
	CCP_STATS_ZONE( __FUNCTION__ );

	PyTaskletObject *tasklet;
	Be::Time newdue = 0;
	PyObject *arg = Py_None;
	if (!PyArg_ParseTuple(args, "O!L|O:Reschedule", &PyTasklet_Type, &tasklet, &newdue, &arg))
		return 0;

	//find the tasklet.We must be careful not to let iterators survive past channel sends
	//due to iterator debugging (they live on the stack)
	Sleeper s;
	SleeperIt it;
	s.channel = 0;
	{
		if (!FindTasklet((PyObject *) tasklet, mSimSleepers, it)) {
			PyErr_SetString(PyExc_ValueError, "tasklet not found in sim sleepers");
			return NULL;
		}

		Be::Time now = BeOS->GetInfo()->mSimTime;
		if (newdue > now) {
			//reschedule it.
			it->due = newdue;
			mSimSleepers.Reorder(it);
		} else 
			//pop it and wake up
			s = mSimSleepers.Remove(it);
	}
	if (s.channel) {
		if (PyChannel_GetBalance(s.channel)) {
			int res = PyChannel_Send(s.channel, arg);
			if (res)
				PyOS->PyError();
		}
		Py_DECREF(s.channel);
	}
	Py_INCREF(Py_None);
	return Py_None;
}


//--------------------------------------------------------------------
PyObject* Synchro::Yield()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if (CatchMain())
		return 0;
	if (mShutdown)
		Py_RETURN_NONE;
	PyObject *result = PyChannel_Receive(mYielders);
	return result;
}


//--------------------------------------------------------------------
PyObject* Synchro::Get_sleepers()
{
	PyObject* sleepers = PyList_New(mWallclockSleepers.size());
	if (sleepers == NULL)
		return NULL;

	Py_ssize_t ix = 0;
	for (SleeperIt i = mWallclockSleepers.begin(); i != mWallclockSleepers.end(); ++i, ++ix)
	{
		Sleeper sl = *i;
		PyObject* sleeper = Py_BuildValue("OL", sl.channel, sl.due);
		PyList_SET_ITEM(sleepers, ix, sleeper);		
	}

	return sleepers;
}


void Synchro::AddStat()
{
	CCP_STATS_SET( statSleepers, mWallclockSleepers.size() + mSimSleepers.size() );
	CCP_STATS_SET( statYielders, -mYielders->balance );
	CCP_STATS_SET( statRunnable, PyStackless_GetRunCount() - 1 );

	Stat stat;
	stat.mTime = BeOS->GetActualTime();
	stat.mRunnable = PyStackless_GetRunCount()-1; //don't count the running tasklet.
	stat.mYielders = -mYielders->balance;
	stat.mSleepers = mWallclockSleepers.size() + mSimSleepers.size();

	//shuffle the vectors around
	if (!mStats.size() || mStats[mStats.size()-1].size()>= 5000)
		mStats.push_back(std::vector<Stat>());
	if (mStats.size()>2)
		mStats.erase(mStats.begin());
	mStats[mStats.size()-1].push_back(stat);
}

PyObject *Synchro::Get_stats()
{
	BluePy list(PyList_New(0));
	for(size_t i=0; i<mStats.size(); i++)
		for(size_t j = 0; j<mStats[i].size(); j++) {
			Stat &s = mStats[i][j];
			BluePy t(Py_BuildValue("Liin", s.mTime, s.mRunnable, s.mYielders, s.mSleepers));
			PyList_Append(list, t);
		}
		
	return list.Detach();
}

void Synchro::RemoveSleeper( Heap<Sleeper> &sleepers, Sleeper &sl )
{
	SleeperIt it;
	for( it = sleepers.begin(); it != sleepers.end(); ++it )
	{
		if( it->channel == sl.channel )
		{
			break;
		}
	}

	if( it != sleepers.end() )
	{
		//Ok found us.  Let's delete us.
		sleepers.Remove( it );
		Py_DECREF( sl.channel );
	}
}

#endif
