#include "StdAfx.h"

#if BLUE_WITH_PYTHON
//TODO: Implement for non-Win32 platforms - Marshal needs work
#ifdef _WIN32

#include "Filer.h"
#include "Marshal.h"
#include "include/IBluePython.h"
#include "include/IBlueOS.h"


PythonPickler::PythonPickler(const Be::Clsid* clsid) :
	mClsid(clsid),
	mMarshal(NULL)
{
}

	
PythonPickler::~PythonPickler()
{
	Py_XDECREF(mMarshal);
}

bool PythonPickler::Prepare()
{
	if (!mMarshal)
	{
		mMarshal= Marshal::New();//new Marshal;
		if (!mMarshal)
			return false;
	}
	return true;
}



bool PythonPickler::PicklePython(PyObject* obj, PyObject** _pikl)
{
	*_pikl = NULL;

	if (!obj)
		return false;

	if (!Prepare())
		return false;

	PyObject* pikl = mMarshal->SaveObject(obj, NULL, 0);

	if (!pikl)
	{
		PyOS->PyError();
		BeOS->SetError(BEDEF, mClsid, "Couldn't pikle object.");
		return false;
	}

	CCP_ASSERT(pikl->ob_type->tp_as_buffer);

	*_pikl = pikl;

	return true;
}


bool PythonPickler::UnpicklePython(PyObject* pikl, PyObject** obj)
{
	if (!Prepare())
		return false;

	*obj = mMarshal->Load(pikl, 0);
	
	if (!*obj)
	{
		PyOS->PyError();
		BeOS->SetError(BEDEF, mClsid, "Couldn't unpickle object.");
		return false;
	}

	return true;
}


bool PythonPickler::DeepCopyPython(PyObject* src, PyObject** dst)
{
	Py_XDECREF(*dst);
				
	if (src)
	{
		if (!Prepare())
			return false;

		PyObject* pikl = mMarshal->SaveObject(src, NULL, 0);
		if (pikl == NULL)
		{
			PyOS->PyError();
			return false;
		}

		*dst = mMarshal->Load(pikl, 0);
		Py_DECREF(pikl);
		if (*dst == NULL)
		{
			PyOS->PyError();
			return false;
		}
	}
	else
	{
		*dst = NULL;
	}

	return true;
}


#endif
#endif
