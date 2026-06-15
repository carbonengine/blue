// Copyright © 2014 CCP ehf.

#pragma once
#ifndef BLUE_OBJECTBUILDER_PYTHON_H
#define BLUE_OBJECTBUILDER_PYTHON_H

#include "IBlueObjectBuilder.h"

BLUE_DECLARE( BlueObjectBuilderPython );

/// A builder for the IBlueObjectProxy system that creates its object by
/// calling a python function.  The object destroyed notification is also
/// forwarded out to python.
class BlueObjectBuilderPython : public IBlueObjectBuilder
{
public:
	EXPOSE_TO_BLUE();

	BlueObjectBuilderPython();
	~BlueObjectBuilderPython();

	IRoot* CreateObjectWithYield( unsigned int objectMarker, IRoot* callingProxy ) override;
	IRoot* CreateObject( unsigned int objectMarker, IRoot* callingProxy ) override;
	void OnObjectDestroyed( unsigned objectMarker, IRoot* callingProxy ) override;
	void OnSelected( unsigned objectMarker, IRoot* callingProxy ) override;
	void GetErrorMessage( std::string& msg ) override;

protected:
	PyObject* m_createMethod;
	PyObject* m_destroyHandler;
	PyObject* m_selectedHandler;

	// helper for PySet methods
	PyObject * PySetCallback( const char * signature, PyObject * &target, PyObject * self, PyObject * args );
	
	// 'method' as its supposed to return a newly built object for the proxy
	// see _Blue file for what the signature of the callback should be
	static PyObject * PySetCreateMethod( PyObject * self, PyObject * args );
	
	// 'handler' as its just a notification from the proxy to the builder
	// see _Blue file for what the signature of the callback should be
	static PyObject * PySetDestroyHandler( PyObject * self, PyObject * args );

	static PyObject * PySetSelectedHandler( PyObject * self, PyObject * args );

	// helper for calling out to python
	PyObject* DoCallback( PyObject* callback, unsigned objectMarker, IRoot* callingProxy );
};

TYPEDEF_BLUECLASS( BlueObjectBuilderPython );

#endif // BLUE_OBJECTBUILDER_PYTHON_H

