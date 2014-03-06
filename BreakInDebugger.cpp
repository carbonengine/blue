////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		December 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"

#if BLUE_WITH_PYTHON && defined( _WIN32 )

static PyObject* PyBreakInDebugger( PyObject* module, PyObject* args )
{
	if( PyTuple_GET_SIZE(args) == 1 )
	{
		PyObject* o = PyTuple_GetItem( args, 0 );
		if( PyString_Check( o ) )
		{
			char* context = PyString_AsString( o );
			OutputDebugString( "Python Triggered Breakpoint: " );
			OutputDebugString( context );
			OutputDebugString( "\n" );
		}
	}	

	__try 
	{
		// This breakpoint exception is used by several D3D return value checking functions
		// If you get, here, go up the stack and see what D3D function failed
		DebugBreak();
	}
	__except(GetExceptionCode() == EXCEPTION_BREAKPOINT ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) 
	{
	}
	Py_RETURN_NONE;
}

MAP_FUNCTION( "BreakInDebugger", PyBreakInDebugger, "BreakInDebugger( contextString )\nBreaks in the debugger, if one is attached, allowing you to look at the program state at a point determined from Python." );

#endif