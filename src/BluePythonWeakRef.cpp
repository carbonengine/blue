// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "BluePythonWeakRef.h"

BluePythonWeakRef::BluePythonWeakRef( IRoot* lockobj /*= NULL */ ) :
	m_object( NULL ),
	m_callback( Py_None )
{
	Py_INCREF( m_callback );
}

BluePythonWeakRef::~BluePythonWeakRef()
{
	if( m_object )
	{
		m_object->WeakRefUnregister( this );
	}

	Py_DECREF( m_callback );
}

void BluePythonWeakRef::WeakRefNotify( IWeakObject* ptr )
{
	// Set the object to NULL before calling into Python. We don't
	// want the callback to resurrect the object by getting a strong
	// reference to it.
	m_object = NULL;

	if( PyCallable_Check( m_callback ) )
	{
		PyObject* args = PyTuple_New( 0 );
		PyObject* result = PyObject_CallObject( m_callback, args );
		Py_DECREF( args );
		if( result )
		{
			Py_DECREF( result );
		}
	}
}

BlueStdResult BluePythonWeakRef::PyInit( IRoot* obj )
{
	if( !obj )
	{
		return BLUE_STD_RESULT_OK;
	}

	if( !obj->QueryInterface( GetIWeakObjectIID(), (void**)&m_object, BEQI_SILENT ) )
	{
		m_object = NULL;
		return BlueStdResult( BLUE_STD_RESULT_TYPE_ERROR, "BluePythonWeakRef expects IWeakObject support" );
	}

	m_object->WeakRefRegister( this );
	obj->Unlock();
	return BLUE_STD_RESULT_OK;
}

IRoot* BluePythonWeakRef::GetObject()
{
	return m_object;
}

void BluePythonWeakRef::SetObject( IWeakObject* obj )
{
	if( m_object )
	{
		m_object->WeakRefUnregister( this );
	}
	m_object = obj;
	m_object->WeakRefRegister( this );
}

#endif
