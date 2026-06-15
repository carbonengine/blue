// Copyright © 2014 CCP ehf.

/* 
	*************************************************************************

	TaskletTimer.h

	Author:    Kristj�n Valur J�nsson
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

#ifndef _TASKLETTIMER_H_
#define _TASKLETTIMER_H_

#include "ITaskletTimer.h"

#if CCP_STACKLESS

#include "IBlueOS.h" //for python decleration macros
#include "IBluePython.h" //for python decleration macros
#include "BlueTime.h"

#include <vector>
#include <unordered_set>
#include <unordered_map>


//A class to capture time stats from the system.  A high res real time thing, and
//low frequency kernel statistics.
class Times
{
public:
	Times() {}
	Times(int n) {Clear();}
	Times(HiResTime &hires, bool full = true) {Capture(hires, full);}
	int64_t mHires; //regular time
	int64_t mUser;
	int64_t mKernel;
	void Capture(HiResTime &hires, bool full = true) {
		mHires = hires.Get();
		if (full) {
            CcpGetThreadTimes( mKernel, mUser );
		} else
			mKernel = mUser = 0;
	}
	void Clear() {mHires = mUser = mKernel = 0;}
	Times &operator += (const Times &o) {
		mHires += o.mHires;
		mUser += o.mUser;
		mKernel += o.mKernel;
		return *this;
	}
	Times &operator -= (const Times &o) {
		mHires -= o.mHires;
		mUser -= o.mUser;
		mKernel -= o.mKernel;
		return *this;
	}
	Times operator + (const Times &o) {
		Times r(*this);
		r += o;
		return r;
	}
	Times operator - (const Times &o) {
		Times r(*this);
		r -= o;
		return r;
	}
	bool operator !() const {return !mHires;}
	operator Be::Time () const {return mHires;} //this is what we usually want
};


//This represents a frame in a unique activation chaing
class Frame
{
	friend class FrameTreeSet;

	Frame(Frame *p, PyObject *key) :
		mParent(p), mKey(key)
	{}
	void Init(int id) {
		Py_INCREF(mKey);
		mId = id;
		mTimes.Clear();
		mRealTime = 0;
		mNCalls = mNSwitches = 0;
	}
	void Fini() {
		Py_DECREF(mKey);
	}
	
public:
	PyObject *ToPython(double iFreq) const;
	Frame *Parent() const {return mParent;}
	PyObject *Key() const {return mKey;}
	int Id() const {return mId;}
	//ordering operator for map.
	const bool operator < (const Frame &other) const {
		return mParent < other.mParent ||
				(mParent == other.mParent && mKey < other.mKey);
	}
	const size_t Hash() const {
		//hash is the xor of the ponters, shifted right to remove ptr. redundancy.
		return ((size_t)mParent ^ (size_t)mKey) >> 3;
	}
	const bool operator==( const Frame& other ) const
	{
		return mParent == other.mParent && mKey == other.mKey;
	}

private:
	Frame * const mParent;
	PyObject * const mKey;
	int			mId;

public:
	//statistics
	Times		mTimes;
	Be::Time	mRealTime;  //real time spent non recursively in this context.  (includes tasklet switches)
	int			mNCalls;
	int			mNSwitches;
};


//A set of unique activation stacks.
class FrameTreeSet
{
public:
	FrameTreeSet() : mSet( "FrameTreeSet/mSet" )
	{
		mId = 0;
	}
	~FrameTreeSet()
	{
		Clear();
	}
	
	Frame *Insert(Frame *p, PyObject *key)
	{
		CCP_ASSERT(key);
		std::pair<FrameSet_t::iterator, bool> r = mSet.insert(Frame(p, key));

		// This is slightly evil. The insert gives us back a const iterator
		// to the Frame either found or just added. This used to be a
		// regular iterator, but with VS2010 it is a const iterator.
		// It would be cleaner to change this to a map - separate the
		// key out from the value. We know that the ordering operator
		// used on the Frame will not change so this does work - no
		// operations we can do on the frame will invalidate the set.
		// Note that the std::set documentation clearly states that
		// values should not change after they are added to the set.
		FrameSet_t::iterator& fsIt = r.first;
		Frame& frame = const_cast<Frame&>( *fsIt );
		if( r.second )
		{
			frame.Init(++mId);//new dude
		}
		return &frame;
	}

	//return a vector of all leaves in all the trees
	bool Leaves(std::vector<Frame*> &res, PyObject *filter = 0) {
		std::unordered_set<Frame*> set;
		//collect all nodes
		FrameSet_t::iterator i;
		for (i = mSet.begin(); i != mSet.end(); ++i)
		{
			Frame* fp = const_cast<Frame*>( & *i );
			set.insert( fp );
			set.erase( fp->Parent() ); //but don't keep any parents already inserted
		}
		//remove all nodes that are parents of someone
		for (i = mSet.begin(); i != mSet.end(); ++i)
			set.erase(i->Parent());

		//what remains in the set are the leaves.
		for(auto i2= set.begin(); i2 != set.end(); ++i2) {
			if (filter) {
				for (Frame* f = *i2; f; ++f ) {
                                    if (1 == PyObject_RichCompareBool(filter, f->Key(), Py_EQ))
                                        res.push_back(*i2);
//					int cmp;
//					if (PyObject_Cmp(filter, f->Key(), &cmp))
//						return false;
//					if (cmp == 0)
//						found
//						res.push_back(*i2);
				}
			} else
				res.push_back(*i2);
		}
		return true;
	}

	void Clear() {
		for( FrameSet_t::iterator i = mSet.begin(); i != mSet.end(); ++i )
		{
			// See comment in Insert above
			Frame& frame = const_cast<Frame&>( *i );
			frame.Fini();
		}
		mSet.clear();
	}

private:
	//comparison class to put Frames in a hash_set.
	class FrameCompare
	{
	public:
		size_t operator () (const Frame &f) const {
			return f.Hash();
		}
		bool operator () (const Frame &a, const Frame &b) const {
			return a<b;
		}
	};

	typedef TrackableStdHashSet<Frame, FrameCompare> FrameSet_t;
	FrameSet_t mSet;
	int mId;
};


//This class represents a single tasklet, or stack.
class Stack {
public:
	//create empty, and copy empty.
	Stack(intptr_t Id) :
		mCurrent(0), 
		mId(Id),
		mEntryTimes("Stack/mEntryTimes")
	{
	}
	
public:
	intptr_t Id() const {return mId;}
	Be::Time ElapsedInFrame(Be::Time now) const {return now - mEntryTimes.back();}

	Frame *CurrentFrame() const {return mCurrent;}
	Be::Time EntryTime() const {return mEntryTimes.back();}
	size_t Size() const {return mEntryTimes.size();}

	Frame *Push(FrameTreeSet &as, PyObject *key, Be::Time now) {
		mCurrent = as.Insert(mCurrent, key);
		mEntryTimes.push_back(now);
		return mCurrent;
	}
	void Pop() {
		if (mCurrent) {
			mCurrent = mCurrent->Parent();
			mEntryTimes.pop_back();
		}
	}
	
private:
	const intptr_t mId;	   //ID of stack
	Frame *mCurrent;
	TrackableStdVector<Be::Time> mEntryTimes; //real time of entry for each frame.
};


//a class to intern our tasklet keys.  Ensures that they are always unique things.
class Internator
{
public:
	Internator() {
		mDict = PyDict_New();
	}
	~Internator() {
		Py_XDECREF(mDict);
	}
	bool InternInPlace(PyObject ** obj) {
		CCP_ASSERT(obj && *obj);
		PyObject *found = PyDict_GetItem(mDict, *obj);
		if (found){
			*obj = found;
			return true;
		}
		return PyDict_SetItem(mDict, *obj, *obj) == 0;
	}
	void Clear() {
		if (mDict)
			PyDict_Clear(mDict);
	}
private:
	PyObject *mDict;
};


class TaskletTimer :
	public ITaskletTimer, public INotify
{
public:
	EXPOSE_TO_BLUE();

	TaskletTimer();
	~TaskletTimer();

	///////////////////////////////////////////////////////////
	// ITaskletTimer interface.  See interface file for info
	///////////////////////////////////////////////////////////
	PyObject * GetCurrent() override;
	float GetElapsed() override;
	PyObject * EnterTasklet(PyObject *newContext) override;
	PyObject * EnterTaskletEx(PyObject *newContext, TASKLETFLAGS flags) override;
	PyObject * EnterTaskletStr(const char *newContext, TASKLETFLAGS flags) override;
	bool ReturnFromTasklet(PyObject *prevContext) override;
	PyObject * SwitchStack(intptr_t stackID) override;
	void TimesliceReset() override;
	bool Reset() override;
	
	// INotify interface
	bool OnModified(Be::Var* value) override;

    // Python objects cannot be initialized before the interpreter runs
    bool InitPythonObjects();
		
private:

	//mark an event.  Gives the time since the last event.
	Times Mark(Times &now) {
		Times elapsed = mLastTime.mHires ? now - mLastTime : Times(0);
		mLastTime = now;
		return elapsed;
	}
	
	bool MarkCurrent();
	
	//warn of slice overrun
	void WarnSlice(Be::Time now, Stack *stack, PyObject *newctxt, bool switching);

	int TimeInMs( Be::Time time );

	bool IsValidStack( Stack * stack );

	bool UpdateMaxAndCount( PyObject* key, int rms );

	void SimpleWarnSlice(Be::Time now, const char *what, PyObject *newctxt, PyObject *oldctxt);


	//convenience functions
	Frame *CurrentFrame(int i=0) const {
		Stack *s = CurrentStack();
		return s?s->CurrentFrame():0;
	}

	Stack *CurrentStack() const {
		return mCurrentStack;
	}

	//Warning functions
	void MarkSwitch(Be::Time now) {mLastSwitch = mLastWarn = now;}
	void MarkWarn(Be::Time now) {mLastWarn = now;}
	void ClearWarn() {mLastWarn = mLastSwitch;}
	Be::Time ElapsedInTasklet(Be::Time now) const {return mLastSwitch? now - mLastSwitch : 0;}
	Be::Time ElapsedSinceWarn(Be::Time now) const {return mLastWarn? now - mLastWarn : 0;}
	
	
private:
	PyObject* PyGetCurrent ( PyObject* args );
	PyObject* PyGetElapsed ( PyObject* args );
	PyObject* PyEnterTasklet ( PyObject* args );
	PyObject* PyReturnFromTasklet ( PyObject* args );
	PyObject* PyReset ( PyObject* args );
	PyObject* PyGetTasklets ( PyObject* args );
	PyObject* PyGetThreadTimes ( PyObject* args );  //Thread times since last reset
	PyObject* PyGetProcessTimes ( PyObject* args );  //Process times since last reset
	PyObject* PyGetOverhead ( PyObject* args );  //overhead in tasklettimer
	
public:
	int mSliceWarning;  //warn of sliches larger than this (ms)

private:
	HiResTime mTime;	//the timer used
	const double mIFreq;//inverse frequency = time in milliseconds of each timer tick
	Times mLastTime;
	Be::Time mLastSwitch;	//When was this tasklet entered?
	Be::Time mLastWarn;	//When was the last warning issued?
	Times mOverhead;	//overhead in this thing.
	BluePyDict	mMaxWarn; //store info on warnings
	BluePy mSimpleCtxt; //used when timers are off
	long m_BlueOSPumpCountAtStart; // Store the BlueOS Pump counter value at the start
	BluePy mCanonicalizationDict;

	typedef TrackableStdHashMap<intptr_t, Stack> stackmap_t;  //maps stack ids to stacks
	typedef stackmap_t::iterator stackmap_i;
	stackmap_t mStackMap;
	Stack *mCurrentStack;

	FrameTreeSet mFrameTrees;
	Internator mInternator;
	
	bool mActive;  //do we do anything?
	bool mDoTelemetry;   // Should we do Telemetry zones as well?
	Be::Time mResetTime, mResetUTime, mResetKTime, mResetUTimeP, mResetKTimeP;  //times at last reset
	TASKLETFLAGS mFlags;
};

TYPEDEF_BLUECLASS(TaskletTimer);

#endif

#endif // _TASKLETTIMER_H_
