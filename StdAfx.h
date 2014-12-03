// StdAfx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently
#ifndef Blue_StdAfx_H
#define Blue_StdAfx_H

#define BLUEBUILD /* dll linkage of blue functions */

#ifdef _WIN32
#define STRICT
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _WIN32_WINNT 0x0501		// We support Windows XP/Server 2003 and beyond (XP SP1, SP2, Vista, Win7 etc) 
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

// Stuffer is enabled by default on Win32, disabled on other platforms
#ifndef STUFFER_ENABLED
#	if defined(_WIN32) && BLUE_WITH_PYTHON
#		define STUFFER_ENABLED 1
#	else
#		define STUFFER_ENABLED 0
#	endif
#endif

// include all python stuff
#if BLUE_WITH_PYTHON
#include <Python.h>

#if CCP_STACKLESS
#include <stackless_api.h>
#endif

#include <eval.h>  //for PyEval_EvalCode()

// this for pylogobject
#include <structmember.h>
#include <compile.h>
#include <frameobject.h>
#endif

//disable warning about evil use of placement new in class constructors
#pragma warning( disable : 4291 )

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "BlueExposure/include/BlueExposure.h"
#include "include/BlueStatistics.h"

#endif