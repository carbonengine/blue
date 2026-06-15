// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "BlueEventToPython.h"
#include "IBluePython.h"

BlueEventToPython::BlueEventToPython( IRoot* lockobj ) :
	m_handler( NULL )
{
}

BlueEventToPython::~BlueEventToPython()
{
	Py_XDECREF( m_handler );
}

void BlueEventToPython::HandleEvent( const wchar_t* evtName )
{
	if( !m_handler )
	{
		return;
	}

	if( PyCallable_Check( m_handler ) )
	{
		PyObject* result = PyObject_CallFunction( m_handler, (char*)"u", evtName );
		if( result )
		{
			Py_DECREF( result );
		}
		else
		{
			std::string name = static_cast<const char*>( CW2A( evtName ) );
			std::string message = "BlueEventToPython: ";
			message += name;
			PyOS->PyFlushError( message.c_str() );
		}
	}
	else
	{
		CCP_LOGWARN( "BlueEventToPython::HandleEvent: Handler not callable" );
	}
}

#endif
