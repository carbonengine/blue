// Copyright © 2013 CCP ehf.

#include "StdAfx.h"
#include "PrettyPrint.h"

#if BLUE_WITH_PYTHON

PyObject* PrettyPrint(
	PyObject* _func,
	const char* caller,
	int callerline,
	const char* overrideName
	)
{
	PyFunctionObject* func;

	if (_func == NULL)
	{
		return PyUnicode_FromFormat("Unbound tasklet from %s(%d)", caller, callerline);
	}
	else if (PyCFunction_Check(_func))
	{
		PyCFunctionObject* cfunc = (PyCFunctionObject*)_func;
		const char* tpname;
		if (cfunc->m_self)
			tpname = cfunc->m_self->ob_type->tp_name;
		else
			tpname = "no";

		return PyUnicode_FromFormat(
			"<b>%s()</b> C/C++ function of %s type from %s(%d)",
			cfunc->m_ml->ml_name, tpname,
			caller, callerline
			);
	}
	else if (PyMethod_Check(_func))
	{
		func = (PyFunctionObject*)((PyMethodObject*)_func)->im_func;
	}
	else if (PyFunction_Check(_func))
	{
		func = (PyFunctionObject*)_func;
	}
	else
	{
		return PyUnicode_FromFormat(
			"<b>0x%.8x</b> of %s type from %s(%d)",
			uint32_t( uintptr_t( _func ) ), _func->ob_type->tp_name, // TODO: should format be really %p?
			caller, callerline
			);
	}

	const char* name = PyUnicode_AsUTF8(func->func_name);
	PyCodeObject* code = (PyCodeObject*)func->func_code;
	const char* filename = PyUnicode_AsUTF8(code->co_filename);
	int line = code->co_firstlineno;

	if (overrideName)
		name = overrideName;

	return PyUnicode_FromFormat(
		"<b>%s()</b> in %s(%d) from %s(%d)",
		name, filename, line,
		caller, callerline
		);
}

#endif
