// Copyright © 2004 CCP ehf.

/* 
	*************************************************************************

	ITaskletTimer.h

	Project:   Blue

	Description:   

		TaskletTimer interface

	Dependencies:

		Blue

	*************************************************************************
*/

#ifndef _ITASKLETTIMER_H
#define _ITASKLETTIMER_H

#if BLUE_WITH_PYTHON

//This interface relies on python objects.  This simplifies a lot.  Everything is python, including
//exception state.
enum TASKLETFLAGS
{
	NONE = 0,
	IDLE = 1	//This context causes the thread to sleep. wallclock will be marked as idle time
};

BLUE_INTERFACE(ITaskletTimer) : public IRoot
{
	//Get the current tasklet id.  returns new reference.
	virtual PyObject * GetCurrent() = 0;

	//Get the elapsed time in the current context in seconds
	virtual float GetElapsed() = 0;

	//Enter a new context (as when calling a subroutine)
	//Returns the previous context as a new reference.
	virtual PyObject * EnterTasklet(PyObject *newContext) = 0;

	//Return from a tasklet.  Pass in the context we are returning to.
	virtual bool ReturnFromTasklet(PyObject *prevContext) = 0;

	//Context switch, when flipping between two stacks, e.g. on a channel send.
	//The context id is used internally to separate different callstacks when adding cumulative time.
	//It can be something like the python ID of the tasklet.
	virtual PyObject * SwitchStack(intptr_t contextId) = 0;

	//Resets all timers
	virtual bool Reset() = 0;

	//Clear the warning state.  Do this while in a context where you expect
	//long duration, e.g. a Sleep() call.
	virtual void TimesliceReset() = 0;

	//An enhanced version of EnterTasklet, which allows one to pass flags
	virtual PyObject * EnterTaskletEx(PyObject *newContext, TASKLETFLAGS) = 0;

	//A version which accepts C strings.  This may turn out to be faster in the future
	virtual PyObject * EnterTaskletStr(const char *newContext, TASKLETFLAGS) = 0;

};


//Okay, now define a class for automatic tasklet entry stuff.  This is purely inline so
//modules such as Jennifer can benefit from it.
//the active argument can be used to turn it off at runtime
class AutoTasklet
{
public:
	AutoTasklet(ITaskletTimer *timer, const char *context, bool active=true, TASKLETFLAGS flags=NONE) :
		mTimer(timer)
	{
#if CCP_STACKLESS
		if (active)
			mOld = timer->EnterTaskletStr(context, flags);
		else
#endif
			mOld = 0;
	}
	AutoTasklet(ITaskletTimer *timer, PyObject *context, bool active=true, TASKLETFLAGS flags=NONE) :
		mTimer(timer)
	{
#if CCP_STACKLESS
		if (active)
			mOld = timer->EnterTaskletEx(context, flags);
		else
#endif
			mOld = 0; 
	}
	~AutoTasklet()
	{
		if (mOld) {
			mTimer->ReturnFromTasklet(mOld);
			Py_DECREF(mOld);
		}
	}
	//Get borrowed references to old tasklet
	PyObject *GetOld() const {return mOld;}
private:
	AutoTasklet &operator=(const AutoTasklet &other); //not defined
	ITaskletTimer * const mTimer; //borrowed reference
	PyObject *mOld;
};

#endif

#endif // _ITASKLETTIMER_H
