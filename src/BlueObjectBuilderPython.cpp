// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#include <BlueStatistics.h>

#if BLUE_WITH_PYTHON

#include "BlueObjectBuilderPython.h"

BlueObjectBuilderPython::BlueObjectBuilderPython()
: m_createMethod( NULL )
, m_destroyHandler( NULL )
, m_selectedHandler( NULL )
{
}

BlueObjectBuilderPython::~BlueObjectBuilderPython()
{
	Py_XDECREF( m_createMethod );
	Py_XDECREF( m_destroyHandler );
	Py_XDECREF( m_selectedHandler );
}

PyObject * BlueObjectBuilderPython::PySetCallback( const char * signature, PyObject * &target, PyObject * self, PyObject * args )    
{
    PyObject *newCallback;
	// http://python.org/doc/1.6/ext/callingPython.html
    if( PyArg_ParseTuple( args, signature, &newCallback ))
	{
        if ( !PyCallable_Check( newCallback ))
		{
            PyErr_SetString(PyExc_TypeError, "parameter must be callable");
            return NULL;
        }

        Py_XINCREF( newCallback );		/* Add a reference to new callback */
        Py_XDECREF( target );			/* Dispose of previous callback */
        target = newCallback;			/* Remember new callback */
        
		Py_RETURN_NONE;
    }

    return NULL;
}

PyObject * BlueObjectBuilderPython::PySetCreateMethod( PyObject * self, PyObject * args )    
{
	BlueObjectBuilderPython* pThis = BluePythonCast<BlueObjectBuilderPython*>( self );
	return pThis->PySetCallback( "O:SetCreateMethod", pThis->m_createMethod, self, args );
}

PyObject * BlueObjectBuilderPython::PySetDestroyHandler( PyObject * self, PyObject * args )    
{
	BlueObjectBuilderPython* pThis = BluePythonCast<BlueObjectBuilderPython*>( self );
	return pThis->PySetCallback( "O:SetDestroyHandler", pThis->m_destroyHandler, self, args );
}

PyObject * BlueObjectBuilderPython::PySetSelectedHandler( PyObject * self, PyObject * args )    
{
	BlueObjectBuilderPython* pThis = BluePythonCast<BlueObjectBuilderPython*>( self );
	return pThis->PySetCallback( "O:SetSelectedHandler", pThis->m_selectedHandler, self, args );
}

void BlueObjectBuilderPython::OnObjectDestroyed( unsigned objectMarker, IRoot* callingProxy )
{
	PyObject* result = DoCallback( m_destroyHandler, objectMarker, callingProxy );
	if( result )
	{
		Py_XDECREF( result );	// shouldn't return anything, but it's no problem if it does, just clean it up properly
	}
}

void BlueObjectBuilderPython::OnSelected( unsigned objectMarker, IRoot* callingProxy )
{
	PyObject* result = DoCallback( m_selectedHandler, objectMarker, callingProxy );
	if( result )
	{
		Py_XDECREF( result );
	}
}

IRoot* BlueObjectBuilderPython::CreateObjectWithYield( unsigned int objectMarker, IRoot* callingProxy )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	PyObject* result = DoCallback( m_createMethod, objectMarker, callingProxy );
	
	if( !result )
	{
		return NULL;
	}

	IRoot* root = BluePythonCast<IRoot*>( result );
	if( root )
	{
		root->Lock();
	}
	Py_XDECREF( result );	
	return root;
}

IRoot* BlueObjectBuilderPython::CreateObject( unsigned int objectMarker, IRoot* callingProxy )
{
	// TODO: Set block trap
	return CreateObjectWithYield( objectMarker, callingProxy );
}

PyObject* BlueObjectBuilderPython::DoCallback( PyObject * callback, unsigned objectMarker, IRoot* callingProxy )
{
	if( !callback || callback == Py_None )
		return NULL; // TODO: There is no error raised here, and yet NULL is returned.

	return PyObject_CallFunction( callback, (char*)"iN", objectMarker, BlueWrapObjectForPython( callingProxy ) );
}

void BlueObjectBuilderPython::GetErrorMessage( std::string& msg )
{
	msg = "";
}

#endif
