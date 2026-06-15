// Copyright © 2016 CCP ehf.

#include "StdAfx.h"
#include "LogControl.h"
#include <CCPLog.h>

#if BLUE_WITH_PYTHON

static CLogControl logControl;
static LogControl *pLogControl = &logControl;
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "LogControl", pLogControl );

BLUE_DEFINE(LogControl);

const Be::ClassInfo* LogControl::ExposeToBlue()
{
	EXPOSURE_BEGIN( LogControl, "" )
		MAP_PROPERTY(
			"LogtypeInfoIsPrivilegedOnly",
			Get_LogtypeInfoIsPrivilegedOnly, Set_LogtypeInfoIsPrivilegedOnly,
			""
		)

		MAP_PROPERTY(
			"LogtypeNoticeIsPrivilegedOnly",
			Get_LogtypeNoticeIsPrivilegedOnly, Set_LogtypeNoticeIsPrivilegedOnly,
			""
		)

		MAP_PROPERTY(
			"LogtypeWarnIsPrivilegedOnly",
			Get_LogtypeWarnIsPrivilegedOnly, Set_LogtypeWarnIsPrivilegedOnly,
			""
		)

		MAP_PROPERTY(
			"LogtypeErrIsPrivilegedOnly",
			Get_LogtypeErrIsPrivilegedOnly, Set_LogtypeErrIsPrivilegedOnly,
			""
		)
	EXPOSURE_END()
}

PyObject *LogControl::Get_LogtypeInfoIsPrivilegedOnly()
{
	auto old = CCP::SetLogtypeInfoIsPrivileged(false);
	CCP::SetLogtypeInfoIsPrivileged(old);

	if (old)
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

bool LogControl::Set_LogtypeInfoIsPrivilegedOnly(PyObject *v)
{
	if (!PyBool_Check(v))
		return PyErr_SetString(PyExc_ValueError, "bool required"), false;

	CCP::SetLogtypeInfoIsPrivileged( v == Py_True );

	return true;
}

PyObject *LogControl::Get_LogtypeNoticeIsPrivilegedOnly()
{
	auto old = CCP::SetLogtypeNoticeIsPrivileged(false);
	CCP::SetLogtypeNoticeIsPrivileged(old);

	if (old)
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

bool LogControl::Set_LogtypeNoticeIsPrivilegedOnly(PyObject *v)
{
	if (!PyBool_Check(v))
		return PyErr_SetString(PyExc_ValueError, "bool required"), false;

	CCP::SetLogtypeNoticeIsPrivileged( v == Py_True );

	return true;
}

PyObject *LogControl::Get_LogtypeWarnIsPrivilegedOnly()
{
	auto old = CCP::SetLogtypeWarnIsPrivileged(false);
	CCP::SetLogtypeWarnIsPrivileged(old);

	if (old)
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

bool LogControl::Set_LogtypeWarnIsPrivilegedOnly(PyObject *v)
{
	if (!PyBool_Check(v))
		return PyErr_SetString(PyExc_ValueError, "bool required"), false;

	CCP::SetLogtypeWarnIsPrivileged( v == Py_True );

	return true;
}

PyObject *LogControl::Get_LogtypeErrIsPrivilegedOnly()
{
	auto old = CCP::SetLogtypeErrIsPrivileged(false);
	CCP::SetLogtypeErrIsPrivileged(old);

	if (old)
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

bool LogControl::Set_LogtypeErrIsPrivilegedOnly(PyObject *v)
{
	if (!PyBool_Check(v))
		return PyErr_SetString(PyExc_ValueError, "bool required"), false;

	CCP::SetLogtypeErrIsPrivileged( v == Py_True );

	return true;
}

#endif
