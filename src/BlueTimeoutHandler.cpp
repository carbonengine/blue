// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "BlueTimeoutHandler.h"

BlueTimeoutHandler::BlueTimeoutHandler() :
	m_hasReportedTimeout( false )
{
}

void BlueTimeoutHandler::NotifyOfTimeout()
{
	if( !m_hasReportedTimeout )
	{
		if( BeCrashes )
		{
			// Ensure we only do this once
			m_hasReportedTimeout = true;

#if BLUE_WITH_PYTHON
			auto pythonDiagnosticThread = CcpCreateThread( PythonDiagnosticFunction, nullptr, CCP_THREAD_PRIORITY_NORMAL );
			uint32_t result;
			CcpJoinThreadWithTimeout( pythonDiagnosticThread, 10000, result );
#endif
		}
	}
}

void BlueTimeoutHandler::Reset()
{
	m_hasReportedTimeout = false;
}

#if BLUE_WITH_PYTHON
uint32_t BlueTimeoutHandler::PythonDiagnosticFunction( void* context )
{
	CCP_LOG( "Running Python diagnostics for freeze detection" );

	PyGILState_STATE state = PyGILState_Ensure();
	PyObject* module = PyImport_ImportModule( "pythonstatus" );
	if( module )
	{
		CCP_LOG( "Calling pythonstatus" );
		PyObject* result = PyObject_CallMethod( module, const_cast<char*>("pythonstatus"), const_cast<char*>("") );
		if( result )
		{
			CCP_LOG( "Calling pythonstatus - done" );
			Py_DECREF(result);
		}
		else
		{
			PyErr_WriteUnraisable( Py_None );
		}
		Py_DECREF( module );
	}
	else
	{
		CCP_LOGERR( "Failed to import pythonstatus module" );
		PyErr_WriteUnraisable( Py_None );
	}
	PyGILState_Release(state);

	return 0;
}
#endif
