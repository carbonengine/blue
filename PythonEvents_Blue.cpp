////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		October 2012
// Copyright:	CCP 2012
//

#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "PythonEvents.h"

BLUE_DEFINE( PythonEvents );

const Be::ClassInfo* PythonEvents::ExposeToBlue()
{
	EXPOSURE_BEGIN( PythonEvents, "" )
		MAP_ATTRIBUTE( "softspace", mSoftspace, "", Be::READWRITE )

		MAP_METHOD_AS_METHOD
		(
			"write",
			Pywrite, 
			"Write text to port" 
		)
		
		MAP_METHOD_AS_METHOD
		(
			"flush",
			Pyflush, 
			"Flush the buffer" 
		)


	EXPOSURE_END()
}

#endif
