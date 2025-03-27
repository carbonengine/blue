// StdAfx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently
#ifndef Blue_StdAfx_H
#define Blue_StdAfx_H

#ifndef BLUEBUILD
#define BLUEBUILD /* dll linkage of blue functions */
#endif

#ifdef _WIN32
#ifndef STRICT
#define STRICT
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <optional>

// include all python stuff
#if BLUE_WITH_PYTHON
#include <Python.h>

#if CCP_STACKLESS
#include <stackless_api.h>
#else
#define CCP_NO_STACKLESS
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
#include <Psapi.h>
#endif

#include <BlueExposure.h>
#include "Blue.h"
#include "BlueStatistics.h"

#endif
