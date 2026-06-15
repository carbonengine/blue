// Copyright © 2013 CCP ehf.

#ifndef PrettyPrint_h
#define PrettyPrint_h

#pragma once

#if BLUE_WITH_PYTHON

PyObject* PrettyPrint(
	PyObject* _func,
	const char* caller,
	int callerline,
	const char* overrideName = NULL
	);

#endif

#endif // PrettyPrint_h
