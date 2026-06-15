// Copyright © 2013 CCP ehf.

#include "StdAfx.h"
#include <BlueStatistics.h>

#if BLUE_WITH_PYTHON

static PyObject* s_logCallback = NULL;
static CCP::LogType s_logCallbackThreshold = CCP::LOGTYPE_LOWEST;

static bool s_logCallbackErrorReported = false;

void LogToPython( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* message )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Echo to python if callback is installed
	if( s_logCallback && s_logCallback != Py_None && !s_logCallbackErrorReported)
	{
		PyGILState_STATE gilstate = PyGILState_Ensure();
		PyObject* result = PyObject_CallFunction( s_logCallback, const_cast<char*>( "isss" ), type, logObject.facility, logObject.object, message );
		if( result == NULL )
		{
			// There's an error with calling the current s_logCallback method.
			// We should report it.
			PyErr_Clear();
			if( !s_logCallbackErrorReported )
			{
				s_logCallbackErrorReported = true;  // Have to do this first, before calling
				CCP_LOGERR( "Error while calling Python logging callback function. Disabling logging in Python!" );
			}
		}
		else
		{
			Py_DECREF( result );
		}
		PyGILState_Release(gilstate);
	}
}

void SetLogEchoFunction( CCP::LogType threshold, PyObject* callbackFunc )
{
	if( s_logCallback != Py_None )
	{
		CCP::UnregisterLogEcho( LogToPython );
	}

	Py_XINCREF( callbackFunc ); // Add a reference to the new callback
	Py_CLEAR( s_logCallback ); // Remove ref to previous callback
	s_logCallback = callbackFunc; // Assign the new callback
	s_logCallbackThreshold = threshold;
	s_logCallbackErrorReported = false; // No errors reported (yet) on this new callback

	if( s_logCallback != Py_None )
	{
		CCP::RegisterLogEcho( LogToPython, threshold );
	}
}

PyObject* PySetLogEchoFunction( PyObject* self, PyObject* args )
{
	PyObject* callbackFunc = NULL;
	int threshold = 0;
	PyObject* ret = NULL;

	if( PyArg_ParseTuple( args, "iO", &threshold, &callbackFunc ) )
	{
		if( threshold < CCP::LOGTYPE_LOWEST )
		{
			threshold = CCP::LOGTYPE_LOWEST;
		}
		if( threshold > CCP::LOGTYPE_HIGHEST )
		{
			threshold = CCP::LOGTYPE_HIGHEST;
		}
		if( callbackFunc != Py_None && !PyCallable_Check( callbackFunc ) )
		{
			PyErr_SetString( PyExc_TypeError, "Argument must be None or callable" );
		}
		else
		{
			SetLogEchoFunction( (CCP::LogType)threshold, callbackFunc );

			Py_INCREF( Py_None );
			ret = Py_None;
		}
	}
	else
	{
		PyErr_SetString( PyExc_TypeError, "Function accepts two arguments (threshold, callable)" );
	}

	return ret;
}

PyObject* PyGetLogEchoFunction( PyObject* self, PyObject* args )
{
	PyObject* returnValue = PyTuple_New( 2 );

	PyTuple_SET_ITEM( returnValue, 0, PyLong_FromLong( s_logCallbackThreshold ) );
	if( !s_logCallback )
	{
		s_logCallback = Py_None;
		Py_INCREF( s_logCallback );
	}
	Py_INCREF( s_logCallback );
	PyTuple_SET_ITEM( returnValue, 1, s_logCallback );

	return returnValue;
}

#endif
