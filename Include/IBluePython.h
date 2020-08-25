/*
	*************************************************************************

	IBluePython.h

	Author:    Matthias Gudmundsson
	Created:   Oct. 2000
	OS:        Win32
	Project:   Blue

	Description:

		IBluePyOS is the interface on a static object which handles most Python
		related activity in Blue, like the stackless thread support.  It also
		wraps Blue object instances into Python objects.  'PyOS' is a global
		variable which always points to the static instance of IBluePyOS.

		Additionally there are few convenience functions, like getting the PyType
		of the wrapper to use in PyArg_ParseTuple and such.

		Pending enhancements include support for sequence operators on IBlueList,
		i.e. make this possible: "scene.planets[2]".  Currently this notation has
		to be used: "scene.planets.Get(2)".  Is it worth it?  Only time can tell...


	Dependencies:

		Blue, Python

	(c) CCP 2000

	*************************************************************************
*/

#ifndef _IBLUEPYTHON_H_
#define _IBLUEPYTHON_H_


#if BLUE_WITH_PYTHON
#include <Python.h>

// forward decls
struct IPythonEvents;
struct BluePythonObject;
struct IList;
struct ITaskletTimer;

#undef Yield

//////////////////////////////////////////////////////////////////////
//
// IBluePyOS interface
//
//////////////////////////////////////////////////////////////////////
BLUE_INTERFACE(IBluePyOS) : public IRoot
{
	//--------------------------------------------------------------------
	// Data members.  How to initialize python
	//--------------------------------------------------------------------
	int mOptimizeFlag;  //the optimize level of python. -1 is default
	

	//--------------------------------------------------------------------
	// Magic blue to python marriage
	//--------------------------------------------------------------------

	// returns a pyobject representation of 'object'
	virtual BluePythonObject* WrapBlueObject(
		IRoot* object		
		) = 0;

	//--------------------------------------------------------------------
	// Python engine
	//--------------------------------------------------------------------

	// the startup
	virtual bool Startup(
		) = 0;

	// the shutdown
	virtual void Shutdown(
		int level
		) = 0;

	// the pumping
	virtual int PumpPython(
		bool quit
		) = 0;

	virtual void SetEventHandler(
		IPythonEvents* handler
		) = 0;

	//--------------------------------------------------------------------
	// Convenience functions
	//--------------------------------------------------------------------

	// sets python error from BeOS->GetLastError()
	// always returns 'NULL'
	virtual PyObject* PyError(
		PyObject* exception = NULL
		) = 0;

	// Do a stack trace to the logger
	virtual void DoStackTrace(
		PyObject *frame = 0
		) = 0;

	virtual PyObject* CreateTasklet(
		PyObject* meth,
		PyObject* args,
		PyObject* kw
		) = 0;

    // --------------------------------------------------------------------
	// Event dispatching
    // --------------------------------------------------------------------
	//
	// A blue object with decoration can send or post a message to a
	// callback method in the decoration code.
	// The callback method has the following format:
	//
	// def _OnSomething(self, ...)
	//
	// 'self' is the decorated object.
	// '...' are zero or more optional arguments which are defined by
	// the caller.
	//
	// The caller defines whether the call should return immediately or not.
	// Immediate callbacks MUST return as soon as possible and may not do
	// anything that might trigger a tasklet schedule (like doing DB or
	// network query).
    // --------------------------------------------------------------------



    // --------------------------------------------------------------------
	// SendEvent
    // --------------------------------------------------------------------
	// Calls a python callback function _synchronously_ and returns with
	// the result. If a task scheduling occurs while the callback is
	// executing, an error is raised.
	//
	// This function return true if the event was dispatched, false if
	// no event handler was found.
	//
	// 'caller' is the sender of the event, usually 'this'.
	//
	// The context string of the tasklet is 'context' and must be the same
	// for all calls on the same method.
	//
	// The callback method 'eventName' is called using arguments constructed
	// from 'format' and '...' (just like Py_BuildValue).
	// 'pRetval' is optional, and if used it will contain the return value
	// from the callback method (or NULL in case of error).
	//
	// This function return 'true' if successful, 'false' otherwise. In
	// case of failure, the caller must take care of reporting the error.
	// If the error can not propagate up to Blue system, then the caller
	// should call PyOS->PyError().
	//
	// Note! If 'pRetval' is used, remember to Py_DECREF it.
    // --------------------------------------------------------------------
	virtual bool SendEvent(
		IRoot* caller,
		const char* context,
		const char* eventName,
		PyObject** pRetval = NULL,
		const char* format = NULL,
		...
		) = 0;

    // --------------------------------------------------------------------
	// PostEvent
    // --------------------------------------------------------------------
	// Calls a python callback function _asynchronously_. It behaves
	// exactly like SendEvent except the event is dispatched asynchronously
	// and this function doesn't have any 'pRetval'.
    // --------------------------------------------------------------------
	virtual bool PostEvent(
		IRoot* caller,
		const char* context,
		const char* eventName,
		const char* format = NULL,
		...
		) = 0;

	// Get a string containing the stack trace
	virtual PyObject *GetStackTrace(
		PyObject *frame =0
		) = 0;

	//Turn a python exception into a BlueStr
	virtual void FormatException(char **result) = 0;

	//Get the taskletTimer object (borrowed ref)
	virtual ITaskletTimer *GetTaskletTimer() = 0;

	// Flushes out any error if there is one, with a proper "whence" message.  Use
	// This in preference to PyError, to give proper messages.
	// Used to turn Python exceptions into blue errors.
	virtual bool PyFlushError(
		const char *whence
		) = 0;

	//The hook into the tasklet switching stuff.
	virtual void OnTaskletSwitch(PyObject *from, PyObject *to) = 0;


	//Calling functions for blocktrapping
	virtual PyObject *CallMethodWithTrap(PyObject *target, const char *method, const char *ctxt,
										 const char *format, ...) = 0;

	virtual bool PythonEvent(const char *event, PyObject * arg) = 0;

	virtual bool IsPackaged() = 0;

	virtual bool IsInterpreterMode() = 0;
		
	// Returns true if the current tasklet can yield. The tasklet may be blocked
	// from yielding, and the main tasklet is not allowed to yield.
	virtual bool CanYield() = 0;

	// Yield execution to another tasklet. Returns false if the current tasklet has
	// been killed.
	virtual bool Yield() = 0;

	virtual void GetSchedulerStats(
		int &inQueue1,
		int &inQueue2,
		float &lastTime,
		float &maxTime
		) = 0;

	//Turns a Blue error into a python error.  There must be a pending blue error.
	//always returns 0.
	virtual PyObject * PyErr_BlueError() = 0;

	virtual void RebaseSimClock(Be::Time oldTime, Be::Time newTime) = 0;
};

extern BLUEIMPORT IBluePyOS* PyOS;

//////////////////////////////////////////////////////////////////////
//
// IPythonEvents interface
//
//////////////////////////////////////////////////////////////////////

enum PYPORT
{
	PYSTDOUT		= 0,
	PYSTDERR		= 1,
	PYLOGINFO		= 2,
	PYLOGWARN		= 3,
	PYLOGERR		= 4,
	PYLOGFATAL		= 5,
	_PYPORTLAST		= PYLOGFATAL
};

BLUE_INTERFACE(IPythonEvents) : public IRoot
{
	virtual void OnWrite(
		PYPORT port,
		const char* text
		) = 0;
};

#endif

#endif // _IBLUEPYTHON_H_
