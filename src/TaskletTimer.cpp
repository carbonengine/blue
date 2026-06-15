// Copyright © 2014 CCP ehf.

/* 
	*************************************************************************

	TaskletTimer.cpp

	Author:    Kristján Valur Jónsson
	Created:   Sept 2004
	OS:        Win32
	Project:   Yep

	Description:   

		A class that encompasses the tasklet timing in Blue

	Dependencies:

		Blue

	(c) CCP 2004

	*************************************************************************
*/
#include "StdAfx.h"

#if CCP_STACKLESS

#include "BlueOS.h"
#include "TaskletTimer.h"
#include "SimpleJson.h"

#include <structmember.h>
#include <sstream>


static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "TaskletTimer" );
CCP_STATS_DECLARE( timesliceWarnings, "Blue/TimesliceWarnings", false, CST_COUNTER_LOW, "Count of timeslice warnings issued" );


PyObject *Frame::ToPython(double iFreq) const
{
	BluePyDict r(1);
	BluePy v;
	r.Set("hTime", BluePy(PyFloat_FromDouble(mTimes.mHires * iFreq))); //iFreq is already conv. to seconds
	r.Set("rTime", BluePy(PyFloat_FromDouble(mRealTime * iFreq)));
	r.Set("uTime", BluePy(PyFloat_FromDouble(mTimes.mUser * 1e-7))); //100ns to s
	r.Set("kTime", BluePy(PyFloat_FromDouble(mTimes.mKernel * 1e-7)));
	r.Set("nCalls", BluePy(PyLong_FromLong(mNCalls)));
	r.Set("nSwitches", BluePy(PyLong_FromLong(mNSwitches)));
	r.Set("Id", BluePy(PyLong_FromLong(mId)));
	r.Set("Key", mKey);
	return r.Detach();
}

static int nTimers = 0;

typedef TrackableStdMap<const char*, CcpStaticStatisticsEntry*> CcpStatisticsEntryMap_t;
typedef CcpStatisticsEntryMap_t::iterator CcpStatisticsEntryMap_i;
CcpStatisticsEntryMap_t s_statsMap( "TaskletTimer/s_statsMap" );

static void AddToStat( PyObject * newContext, Times elapsed, double frequency )
{
	if( PyUnicode_Check( newContext ) )
	{
		const char* name = PyUnicode_AsUTF8( newContext );

		CcpStatisticsEntryMap_i it = s_statsMap.find( name );
		if( it == s_statsMap.end() )
		{
			CcpStaticStatisticsEntry* newStat = CCP_NEW( "AddToState/newStat" ) CcpStaticStatisticsEntry( name, true, CST_TIME, "Tasklet timer" );
			std::pair<CcpStatisticsEntryMap_i, bool> res = 
				s_statsMap.insert( CcpStatisticsEntryMap_t::value_type( name, newStat ) );
			it = res.first;
		}

		CcpStaticStatisticsEntry* stat = it->second;
		stat->Add( elapsed * frequency );
	}
}

static void ClearStatsMap()
{
	for( CcpStatisticsEntryMap_i it = s_statsMap.begin(); it != s_statsMap.end(); ++it )
	{
		auto stat = it->second;
		CCP_DELETE stat;
	}
	s_statsMap.clear();
}

TaskletTimer::TaskletTimer() : 
	mIFreq(1000.0/mTime.GetUnitsPerSecond()),
	mLastTime(0), mOverhead(0), mLastSwitch(0), mLastWarn(0),
	mMaxWarn(1),
	mStackMap( "TaskletTimer/mStackMap" ),
	m_BlueOSPumpCountAtStart( 0 )
{
	mCurrentStack = 0;
	mActive = false;
	mDoTelemetry = false;
	mSliceWarning = 200;
	mFlags = NONE;
}


TaskletTimer::~TaskletTimer()
{
	;
}


bool TaskletTimer::InitPythonObjects()
{
    if (!mSimpleCtxt) {
        mSimpleCtxt = BluePy(Py_None, true);
    }
    if (!mCanonicalizationDict) {
        mCanonicalizationDict = BluePy(PyDict_New());
    }

    return mCanonicalizationDict && mSimpleCtxt;
}


PyObject *TaskletTimer::GetCurrent()
{
	PyObject *r;
	Frame *f = CurrentFrame();
	if (f) {
		r = f->Key();
		Py_INCREF(r);
	} else
		r = PyUnicode_FromString("<NO CONTEXT>"); //some python code expects strings always
	return r;
}


//return elapsed time in seconds in the current tasklet (without a switch).
//This is used for the benice timer
float TaskletTimer::GetElapsed()
{
	if (!mLastSwitch)
		return 0.0f;
	return (float)((mTime.Get()-mLastSwitch)*mIFreq*0.001); //mIFreq converts to milliseconds
}


PyObject *TaskletTimer::EnterTasklet(PyObject *newContext)
{
	return EnterTaskletEx(newContext, NONE);
}

PyObject *TaskletTimer::EnterTaskletStr(const char *context, TASKLETFLAGS flags)
{
	PyObject *obj = PyUnicode_FromString(context);
	if (!obj)
		return NULL;
	PyObject *result = EnterTaskletEx(obj, flags);
	Py_DECREF(obj);
	return result;
}

PyObject *TaskletTimer::EnterTaskletEx(PyObject *newContext, TASKLETFLAGS flags)
{
	mFlags = flags; //temporary hack to support the IDLE flag
	if (!mActive) {
		//timer is not enabled.
		Times now(mTime, false);
		SimpleWarnSlice(now, "entering", newContext, mSimpleCtxt);
		BluePy old = mSimpleCtxt ? mSimpleCtxt : BluePy(Py_None, true);
		mSimpleCtxt = BluePy(newContext, true);
		return old.Detach();
	}
	Times now(mTime);
	PyObject *res = 0;
	
	//intern context (get a standard pointer)
	if (!mInternator.InternInPlace(&newContext))
    {
        Times now2(mTime);
        mOverhead += now2-now; //accumulate overhead
        return res;
    }
	mSimpleCtxt = BluePy(newContext, true);
	
	Stack *stack = CurrentStack();
	if (!stack)
		//Create a stack with an unknown id.
		stack = mCurrentStack = & mStackMap.insert(stackmap_t::value_type(-1, Stack(-1))).first->second;	

	Frame *f, *of = stack->CurrentFrame();
	if (of) {
		//we only Mark if there is a previous frame.  When we enter a fresh stack, no frame
		//is generated.  The time between entering the stack and entering the tasklet will be
		//billed to the first frame.
		Times elapsed = Mark(now);
		of->mTimes += elapsed;

		AddToStat( newContext, elapsed, mIFreq * 0.001 );

	}

	if (mSliceWarning)
		WarnSlice(now, stack, mSimpleCtxt, false);
	
	f = stack->Push(mFrameTrees, newContext, now);
	f->mNCalls++;

	res = of?of->Key():Py_None;
	Py_INCREF(res);

    Times now2(mTime);
	mOverhead += now2-now; //accumulate overhead
	return res;
}
		
		
bool TaskletTimer::ReturnFromTasklet(PyObject *backContext)
{
	// Temporary hack to support IDLE
	if (mFlags) {
		TimesliceReset();
		mFlags = NONE;
	}
	if (!mActive) {
		//timer is not enabled.
		Times now(mTime, false);
		SimpleWarnSlice(now, "returning to", backContext, mSimpleCtxt);
		mSimpleCtxt = BluePy(backContext, true);
		return true;
	}
	Times now(mTime);
	mSimpleCtxt = BluePy(backContext, true);
	
	//We always "Mark" even if we can't add the elapsed time to anything.
	//This happens only during transient periods, so it is ok to lose a
	//little time and nothing gets overattributed.  the memory we set
	// to -1 if we have no other target for it.

	Times elapsed = Mark(now);
	Stack *stack = CurrentStack();
	if (!stack)
    {
        Times now2(mTime);
        mOverhead += now2-now; //accumulate overhead
        return true;
    }

	Frame *of = stack->CurrentFrame();
	if (!of)
    {
        Times now2(mTime);
        mOverhead += now2-now; //accumulate overhead
        return true;
    }
		
	of->mTimes += elapsed;
	Be::Time realElapsed = now.mHires - stack->EntryTime();
	of->mRealTime += realElapsed;

	AddToStat( backContext, elapsed, mIFreq * 0.001 );

	if (mSliceWarning)
		WarnSlice(now, stack, 0, false);
	
	stack->Pop();
	int id = stack->CurrentFrame() ? stack->CurrentFrame()->Id() : -1;
	
    Times now2(mTime);
	mOverhead += now2-now; //accumulate overhead
	return true;
}


PyObject * TaskletTimer::SwitchStack(intptr_t contextID)
{
	if (!mActive) {
		//timer is not enabled.
		Times now(mTime, false);
		ClearWarn();
		SimpleWarnSlice(now, "switching to", Py_None, mSimpleCtxt);
		MarkSwitch(now);
		BluePy old = mSimpleCtxt;
		//Now, we loose the simplectxt of the previous stack when we switch stacks.  Indicate this
		//case with Py_None.  If we want, we can later use the stackmap for this purpose.
		mSimpleCtxt = BluePy(Py_None, true); //mark this case with False
		return old.Detach();
	}
	Times now(mTime);
	PyObject *res = 0;

	//find old and new stacks
	Stack *oldStack = CurrentStack();
	Stack *newStack;
	stackmap_i i = mStackMap.find(contextID);
	if (i != mStackMap.end())
		newStack = &i->second;
	else
		//Entering a fresh context! make it ready.
		newStack = & mStackMap.insert(stackmap_t::value_type(contextID, Stack(contextID))).first->second;

	// TODO: Verify if still an issue outside of stackless
	//catch an edge case.  Sometimes, stackless switches to same tasklet.  This happens mainly when
	//the main tasklet is re-initialized, and gets the same ID!  We mustn't do anything here because
	//we would typically delete the old stack later, but that would be the same as deleting the current one.
	if (oldStack == newStack) {
		res = (oldStack && oldStack->CurrentFrame())?oldStack->CurrentFrame()->Key():Py_None;
		Py_INCREF(res);
		return res;
	}

	//the context in the new stack, or None if unknown
	mSimpleCtxt = BluePy(newStack->CurrentFrame()?newStack->CurrentFrame()->Key():Py_None, true);
	
	//Same logic for Mark, during transient phases (when we have just turned
	//timers on) it is ok to throw away time that we can't attribute to anything.
	Times elapsed = Mark(now);

	//switch stacks
	if (oldStack && oldStack->CurrentFrame())
		oldStack->CurrentFrame()->mTimes += elapsed;	
	mCurrentStack = newStack;

	ClearWarn();  //so we always warn when we switch
	if (mSliceWarning)
		WarnSlice(now, oldStack, mSimpleCtxt, true);
	MarkSwitch(now);

	res = (oldStack && oldStack->CurrentFrame())?oldStack->CurrentFrame()->Key():Py_None;
	Py_INCREF(res);

	//delete the old stack if it was the temporary one, or if it is empty
	if (oldStack && (oldStack->Id() == -1 || oldStack->Size() == 0))
		mStackMap.erase(oldStack->Id());

	Times now2(mTime);
	mOverhead += now2-now; //accumulate overhead
	return res;
}


//This function is usedif we want to sum up the time in the current timer at a certain
//point, for example when retuning the current timer set
bool TaskletTimer::MarkCurrent()
{
	if (!mActive)
		return true;
	Times now(mTime);
	Times elapsed = Mark(now);
	Stack *stack = CurrentStack();
	if (!stack)
		return true;
	Frame *of = stack->CurrentFrame();
	if (!of)
		return true;
	of->mTimes += elapsed;
	return true;
}


void TaskletTimer::WarnSlice(Be::Time now, Stack *stack, PyObject *newctxt, bool switching)
{
	if( !IsValidStack( stack ) )
	{
		return;
	}

	Be::Time realElapsed = ElapsedSinceWarn(now); //elapsed since stack entered or last warning
	int rms = TimeInMs( realElapsed );
	if( rms < mSliceWarning )
	{
		return;
	}

	Frame *frame = stack->CurrentFrame();
	
	realElapsed = ElapsedInTasklet( now ); //upgrade to full stack time
	rms = TimeInMs( realElapsed );

	//but we may suppress it from being output
	if( !UpdateMaxAndCount( frame->Key(), rms ) )
	{
		return;
	}
	MarkWarn( now );  //mark the time of warning

	//compute time spent in frame
	Be::Time elapsedInFrame = stack->ElapsedInFrame(now);
	int fms = TimeInMs( elapsedInFrame );

	float forder = (float)rms/mSliceWarning;
	forder = logf(forder)/logf(2.0f); // compute the base 2 logarithm.
	int order = (int)ceilf(forder);
	
	SimpleJson json;
	json.set( "time", rms );
	json.set( "order", order );
	json.set( "inframe", fms );

	if (newctxt)
	{
		BluePy nstr(PyObject_Repr(newctxt));
		if( !nstr )
		{
			PyErr_Clear();
		}
		json.set( "next", PyUnicode_AsUTF8( nstr ) );
	}

	//and now, the stack:
	std::vector<std::string> tb;
	for(; frame; frame = frame->Parent()) {
		BluePyStr ctxt = BluePy(PyObject_Repr(frame->Key()));
		if (ctxt)
			tb.push_back(ctxt.CStr());
		else
			tb.push_back("unknown");
	}
	while(tb.size()) {
		char buffer[32];
		sprintf_s( buffer, "stack_%lld", static_cast<long long>( tb.size() ) );
		json.set( buffer, tb.back() );

		tb.pop_back();
	}
	CCP_LOGWARN_CH( s_ch, "!! timeslice %s", json.str().c_str() );
	CCP_STATS_INC( timesliceWarnings );
}


void TaskletTimer::SimpleWarnSlice(Be::Time now, const char *what, PyObject *newctxt, PyObject *oldctxt)
{
	if (!mSliceWarning)
		return;

	Be::Time realElapsed = ElapsedSinceWarn(now); //elapsed since stack entered or last warning
	int rms = TimeInMs( realElapsed );
	if (rms < mSliceWarning)
		return;	
	realElapsed = ElapsedInTasklet(now); //upgrade to full stack time
	rms = TimeInMs( realElapsed );

	if( !UpdateMaxAndCount( oldctxt, rms ) )
	{
		return;
	}
	MarkWarn(now);

	float forder = (float)rms/mSliceWarning;
	forder = logf(forder)/logf(2.0f); // compute the base 2 logarithm.
	int order = (int)ceilf(forder);

	BluePyStr oc = oldctxt != Py_None ? BluePy(PyObject_Str(oldctxt)) : BluePyStr( "<None>" );
	if (!oc) {
		PyErr_Clear();
		oc = BluePyStr("<bad>");
	}
	BluePyStr nc = BluePy(PyObject_Str(newctxt));
	if (!nc) {
		PyErr_Clear();
		nc = BluePyStr("<bad>");
	}

	SimpleJson json;
	json.set( "time", rms );
	json.set( "order", order );
	json.set( "what", what );
	json.set( "new", nc.Str() );
	json.set( "old", oc.Str() );

	CCP_LOGWARN_CH( s_ch, "!! timeslice %s", json.str().c_str() );
	CCP_STATS_INC( timesliceWarnings );
}


bool TaskletTimer::Reset()
{
	Times now(mTime);

	mStackMap.clear();
		mCurrentStack  = 0;
	mFrameTrees.Clear();
	mInternator.Clear();
	mMaxWarn.Clear();
	
	//Get the thread times.
	CcpGetThreadTimes( mResetKTime, mResetUTime );
	CcpGetProcessTimes( mResetKTimeP, mResetUTimeP );
	mResetTime = TimeNow();

	//And the timestamp for benchmarking
	mLastTime = now;

	// Add the number of ticks that have happened in Blue,
	// together with the cumulative time, this allows us to work out the cumulative time per tick
	BeInfo* i = BeOS->GetInfo();
	if( i )
	{
		m_BlueOSPumpCountAtStart = i->m_pumpTicksTotal;
	} 
	else 
	{
		m_BlueOSPumpCountAtStart = 0;
	}
	
	MarkSwitch(now);
	mOverhead.Clear();

	ClearStatsMap();

	return true;
}


void TaskletTimer::TimesliceReset()
{
	Times now(mTime, false);
	MarkSwitch(now);
}


bool TaskletTimer::OnModified(Be::Var *value)
{
	if (value == (Be::Var*)&mActive)
		return Reset();
	return true;
}


///Python thunkers for those methods
PyObject *TaskletTimer::PyGetCurrent(PyObject* args)
{
	if (!PyArg_UnpackTuple(args, "GetCurrent", 0, 0))
		return 0;
	return GetCurrent();
}


PyObject *TaskletTimer::PyGetElapsed(PyObject* args)
{
	if (!PyArg_UnpackTuple(args, "GetCurrent", 0, 0))
		return 0;
	return PyFloat_FromDouble(GetElapsed());
}


PyObject *TaskletTimer::PyEnterTasklet(PyObject* args)
{
	PyObject *ctx;
	PyObject *flag = NULL;
	long flags = 0;
	if (!PyArg_UnpackTuple(args, "EnterTasklet", 1, 2, &ctx, &flag))
		return 0;
	if (flag) {
		flags = PyLong_AsLong(flag);
		if (flags == -1 && PyErr_Occurred())
			return NULL;
	}
	return EnterTaskletEx(ctx, (TASKLETFLAGS)flags);
}

PyObject *TaskletTimer::PyReturnFromTasklet(PyObject* args)
{
	PyObject *ctx;
	if (!PyArg_UnpackTuple(args, "ReturnFromTasklet", 1, 1, &ctx))
		return 0;
	if (!ReturnFromTasklet(ctx))
		return 0;
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *TaskletTimer::PyReset(PyObject* args)
{
	if (!PyArg_UnpackTuple(args, "Reset", 0,0))
		return 0;
	if (!Reset())
		return 0;
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *TaskletTimer::PyGetTasklets(PyObject* args)
{
	PyObject *filter = Py_None;
	if (!PyArg_ParseTuple(args, "|O:GetTasklets", &filter))
		return 0;
	std::vector<Frame*> leaves;

	if (!mFrameTrees.Leaves(leaves, filter != Py_None ? filter : 0))
		return 0;

	MarkCurrent(); //make current context's time up-to-date
	std::unordered_map<Frame *, BluePy> reuse;
	BluePyList res(0);
	for(std::vector<Frame*>::iterator i = leaves.begin(); i != leaves.end(); ++i) {
		BluePyList chain(0);
		Frame *f = *i;
		while(f) {
			std::unordered_map<Frame *, BluePy>::iterator where = reuse.find(f);
			if (where != reuse.end()) {
				chain.Append(where->second);
			} else {
				BluePy dict(f->ToPython(mIFreq*0.001)); //mIFreq alone converts to ms
				reuse[f] = dict;
				chain.Append(dict);
}
			f = f->Parent();
		}
		PyList_Reverse(chain.o);
		res.Append(chain);
	}
	return res.Detach();
}


PyObject *TaskletTimer::PyGetThreadTimes(PyObject* args)
{
	if (!PyArg_UnpackTuple(args, "GetThreadTimes", 0, 0))
		return 0;
	Be::Time kernel, user;
	CcpGetThreadTimes( kernel, user );
	Be::Time now = TimeNow();

	BluePyDict r(1);
	r.Set("reset", BluePy(PyLong_FromLongLong(mResetTime)));
	r.Set("wallclock", BluePy(PyLong_FromLongLong( now - mResetTime )));
	r.Set("kernel", BluePy(PyLong_FromLongLong(kernel-mResetKTime)));
	r.Set("user", BluePy(PyLong_FromLongLong(user-mResetUTime)));
	r.Set("cpu", BluePy(PyLong_FromLongLong(user-mResetUTime + kernel-mResetKTime)));
	r.Set("timerFreq", BluePy(PyLong_FromLongLong(mTime.GetUnitsPerSecond())));
	r.Set("BlueOSTickCountAtStart", BluePy(PyLong_FromLong(m_BlueOSPumpCountAtStart)));
	return r.Detach();
}


PyObject *TaskletTimer::PyGetProcessTimes(PyObject* args)
{
	if (!PyArg_UnpackTuple(args, "GetProcessTimes", 0, 0))
		return 0;
	Be::Time kernel, user;
	CcpGetProcessTimes( kernel, user );
	Be::Time now = TimeNow();

	BluePyDict r(1);
	r.Set("reset", BluePy(PyLong_FromLongLong(mResetTime)));
	r.Set("wallclock", BluePy(PyLong_FromLongLong(now-mResetTime)));
	r.Set("kernel", BluePy(PyLong_FromLongLong(kernel-mResetKTimeP)));
	r.Set("user", BluePy(PyLong_FromLongLong(user-mResetUTimeP)));
	r.Set("cpu", BluePy(PyLong_FromLongLong(user-mResetUTimeP + kernel-mResetKTimeP)));
	r.Set("timerFreq", BluePy(PyLong_FromLongLong(mTime.GetUnitsPerSecond())));
	return r.Detach();
}

PyObject *TaskletTimer::PyGetOverhead(PyObject *args)
{
	if(!PyArg_UnpackTuple(args, "GetOverhead", 0, 0))
		return 0;
	BluePyDict r(1);
	r.Set("hTime", BluePy(PyFloat_FromDouble(mOverhead.mHires*mIFreq*0.001)));
	r.Set("uTime", BluePy(PyFloat_FromDouble(mOverhead.mUser*1e-7)));
	r.Set("kTime", BluePy(PyFloat_FromDouble(mOverhead.mKernel*1e-7)));
	return r.Detach();
}

bool TaskletTimer::UpdateMaxAndCount( PyObject* key, int rms )
{
	PyObject *mw = PyDict_GetItem( mMaxWarn, key );
	long count = 1;
	if( mw )
	{
		long ms = PyLong_AsLong( PyTuple_GetItem( mw, 0 ) );
		if( ms == -1 && PyErr_Occurred() )
		{
			PyErr_Clear();
			ms = 0;
		}

		if( rms <= ms )
		{
			return false;
		}
		count = PyLong_AsLong( PyTuple_GetItem( mw, 1 ) );
		count++;
	}
	PyObject* tuple = PyTuple_New( 2 );
	PyTuple_SET_ITEM( tuple, 0, PyLong_FromLong( rms ) );
	PyTuple_SET_ITEM( tuple, 1, PyLong_FromLong( count ) );
	PyDict_SetItem( mMaxWarn, key, tuple );

	return true;
}

bool TaskletTimer::IsValidStack( Stack * stack )
{
	if( !stack )
	{
		return false;
	}

	Frame *frame = stack->CurrentFrame();
	if( !frame )
	{
		return false;
	}

	if( stack->Id() == -1 || //the default stack, used internally
		stack->Id() == 0 )   //the zombie tasklet, a pseudo tasklet entered when a tasklet dies
	{
		return false;
	}

	return true;
}

int TaskletTimer::TimeInMs( Be::Time time )
{
	return (int)((double)time * mIFreq);
}

#endif
