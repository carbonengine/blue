// Copyright © 2010 CCP ehf.

#pragma once
#ifndef BluePythonWeakRef_h
#define BluePythonWeakRef_h

#include "Blue.h"

BLUE_DECLARE( BluePythonWeakRef );

// BluePythonWeakRef is used to hold weak references from Python
// to Blue objects. Regular Python weak references can only hold
// references to the BlueWrapper, which may die before the Blue
// object itself dies. See http://carbon/wiki/BluePythonWeakRef
// for more details.
class BluePythonWeakRef : 
	public IRoot,
	public IWeakRef
{
public:
	EXPOSE_TO_BLUE();

	BluePythonWeakRef( IRoot* lockobj = NULL );
	~BluePythonWeakRef();

	IRoot* GetObject();
	void SetObject( IWeakObject* obj );

	//////////////////////////////////////////////////////////////////////////
	// IWeakRef
	void WeakRefNotify( IWeakObject* ptr ) override;

private:
	IWeakObject* m_object;
	PyObject* m_callback;

private:
	BlueStdResult PyInit( IRoot* obj );
};



TYPEDEF_BLUECLASS( BluePythonWeakRef );

#endif // BluePythonWeakRef_h
