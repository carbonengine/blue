////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		December 2013
// Copyright:	CCP 2013
//

#pragma once
#ifndef LogToPython_h
#define LogToPython_h

#if BLUE_WITH_PYTHON

void SetLogEchoFunction( CCP::LogType threshold, PyObject* callbackFunc );

#endif

#endif // LogToPython_h