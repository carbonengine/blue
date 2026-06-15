// Copyright © 2016 CCP ehf.

#pragma once
#ifndef LOG_CONTROL_H
#define LOG_CONTROL_H

#if BLUE_WITH_PYTHON

#include "Python.h"
#include <BlueExposureMacros.h>

BLUE_CLASS( LogControl )
	: public IRoot
{
public:
	EXPOSE_TO_BLUE();

	PyObject *Get_LogtypeInfoIsPrivilegedOnly();
	bool Set_LogtypeInfoIsPrivilegedOnly(PyObject *v);

	PyObject *Get_LogtypeNoticeIsPrivilegedOnly();
	bool Set_LogtypeNoticeIsPrivilegedOnly(PyObject *v);

	PyObject *Get_LogtypeWarnIsPrivilegedOnly();
	bool Set_LogtypeWarnIsPrivilegedOnly(PyObject *v);

	PyObject *Get_LogtypeErrIsPrivilegedOnly();
	bool Set_LogtypeErrIsPrivilegedOnly(PyObject *v);
};

TYPEDEF_BLUECLASS( LogControl )

#endif

#endif
