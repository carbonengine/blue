////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		December 2013
// Copyright:	CCP 2013
//

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
		return PyString_FromFormat("Unbound tasklet from %s(%d)", caller, callerline);
	}
	else if (PyCFunction_Check(_func))
	{
		PyCFunctionObject* cfunc = (PyCFunctionObject*)_func;
		const char* tpname;
		if (cfunc->m_self)
			tpname = cfunc->m_self->ob_type->tp_name;
		else
			tpname = "no";

		return PyString_FromFormat(
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
	else if (PyInstance_Check(_func))
	{
		PyObject* func2 = PyObject_GetAttrString(_func, "__func__");
		if (func2) {
			PyObject* ret = PrettyPrint(func2, caller, callerline);
			Py_DECREF(func2);
			return ret;
		}
		PyErr_Clear();

		const char* clname;
		PyInstanceObject* inst= (PyInstanceObject*)_func;

		if (PyString_Check(inst->in_class->cl_name))
			clname = PyString_AS_STRING(inst->in_class->cl_name);
		else
			clname = "?";

		return PyString_FromFormat(
			"<b>?.%s()</b> from %s(%d)",
			clname,
			caller, callerline
			);
	}
	else
	{
		return PyString_FromFormat(
			"<b>0x%.8x</b> of %s type from %s(%d)",
			uint32_t( uintptr_t( _func ) ), _func->ob_type->tp_name, // TODO: should format be really %p?
			caller, callerline
			);
	}

	const char* name = PyString_AS_STRING(func->func_name);
	PyCodeObject* code = (PyCodeObject*)func->func_code;
	const char* filename = PyString_AS_STRING(code->co_filename);
	int line = code->co_firstlineno;

	if (overrideName)
		name = overrideName;

	return PyString_FromFormat(
		"<b>%s()</b> in %s(%d) from %s(%d)",
		name, filename, line,
		caller, callerline
		);
}

#endif
