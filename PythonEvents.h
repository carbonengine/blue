#pragma once

#ifndef PythonEvents_h
#define PythonEvents_h

#if BLUE_WITH_PYTHON

#include "Include/IBluePython.h"

//////////////////////////////////////////////////////////////////////
//
// PythonEvents class
//
//////////////////////////////////////////////////////////////////////
class PythonEvents :
	public IRoot
{
public:
	EXPOSE_TO_BLUE();

	PythonEvents() : mPort(PYSTDOUT), mSoftspace(0) { Py_INCREF(Py_None); mEncoding = Py_None; }

	PYPORT mPort;
	int mSoftspace; //used by python's print handler
	PyObject* mEncoding;

	PyObject* Pywrite ( PyObject* args );
	PyObject* Pyflush ( PyObject* args );
	PyObject* Pyencoding() { return mEncoding; };
	PyObject* Pyisatty ( PyObject* args );

};

TYPEDEF_BLUECLASS(PythonEvents);

#endif

#endif // PythonEvents_h
