// Copyright © 2013 CCP ehf.

#pragma once
#ifndef LogToPython_h
#define LogToPython_h

#if BLUE_WITH_PYTHON

void SetLogEchoFunction( CCP::LogType threshold, PyObject* callbackFunc );

#endif

#endif // LogToPython_h