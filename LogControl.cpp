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
	if (CCP::g_logtypeInfoIsPrivilegedOnly)
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

bool LogControl::Set_LogtypeInfoIsPrivilegedOnly(PyObject *v)
{
	if (!PyBool_Check(v))
		return PyErr_SetString(PyExc_ValueError, "bool required"), false;

	CCP::g_logtypeInfoIsPrivilegedOnly = v == Py_True;

	return true;
}

PyObject *LogControl::Get_LogtypeNoticeIsPrivilegedOnly()
{
	if (CCP::g_logtypeNoticeIsPrivilegedOnly)
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

bool LogControl::Set_LogtypeNoticeIsPrivilegedOnly(PyObject *v)
{
	if (!PyBool_Check(v))
		return PyErr_SetString(PyExc_ValueError, "bool required"), false;

	CCP::g_logtypeNoticeIsPrivilegedOnly = v == Py_True;

	return true;
}

PyObject *LogControl::Get_LogtypeWarnIsPrivilegedOnly()
{
	if (CCP::g_logtypeWarnIsPrivilegedOnly)
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

bool LogControl::Set_LogtypeWarnIsPrivilegedOnly(PyObject *v)
{
	if (!PyBool_Check(v))
		return PyErr_SetString(PyExc_ValueError, "bool required"), false;

	CCP::g_logtypeWarnIsPrivilegedOnly = v == Py_True;

	return true;
}

PyObject *LogControl::Get_LogtypeErrIsPrivilegedOnly()
{
	if (CCP::g_logtypeErrIsPrivilegedOnly)
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

bool LogControl::Set_LogtypeErrIsPrivilegedOnly(PyObject *v)
{
	if (!PyBool_Check(v))
		return PyErr_SetString(PyExc_ValueError, "bool required"), false;

	CCP::g_logtypeErrIsPrivilegedOnly = v == Py_True;

	return true;
}

#endif
