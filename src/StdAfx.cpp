// Copyright © 2014 CCP ehf.

// StdAfx.cpp : source file that includes just the standard includes
//	Io.pch will be the pre-compiled header
//	StdAfx.obj will contain the pre-compiled type information

#include "StdAfx.h"

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file


// throw -- terminate on thrown exception REPLACEABLE
#if _WIN32 && !_DLL && _MSC_VER < 1400 //.net2003 and earlier when linking with static CRT
#include <exception>
_STD_BEGIN

_CRTIMP2 _Prhand _Raise_handler = 0;	// define raise handler pointer as null

_CRTIMP2 void __cdecl _Throw(const exception&)
	{	// report error and die
		// BOOHOO !!! 
	}
_STD_END
#endif
