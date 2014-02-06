/* 
	*************************************************************************

	Filer.h

	Author:    Matthias Gudmundsson
	Created:   April 2001
	OS:        Win32
	Project:   Yep

	Description:   

		Read/write for blue


	Dependencies:

		Blue

	(c) CCP 2000

	*************************************************************************
*/

#ifndef _FILER_H_
#define _FILER_H_

#include "include/IBluePersist.h"


class PythonPickler
{
public:

	class Marshal* mMarshal;
	const Be::Clsid* mClsid;

	PythonPickler(const Be::Clsid* clsid);
	~PythonPickler();

	bool PicklePython(PyObject* obj, PyObject** _pikl);
	bool UnpicklePython(PyObject* pikl, PyObject** obj);
	bool DeepCopyPython(PyObject* src, PyObject** dst);

private:

	bool Prepare();
};



#endif


