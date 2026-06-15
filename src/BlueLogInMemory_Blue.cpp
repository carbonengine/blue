// Copyright © 2010 CCP ehf.

#include "StdAfx.h"
#include "blueloginmemory.h"
#include "IBlueOS.h"

BLUE_DEFINE_ABSTRACT( BlueLogInMemory );

const Be::ClassInfo* BlueLogInMemory::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueLogInMemory, "This class controls logging in memory" )
		MAP_METHOD_AND_WRAP
		(
			"Start",
			Start,
			"Starts capturing of log messages."
		)
		MAP_METHOD_AND_WRAP
		(
			"Stop",
			Stop,
			"Stops capturing of log messages."
		)
		MAP_PROPERTY_READONLY
		(
			"isActive",
			IsActive,
			"Returns True if capturing of log messages is enabled."
		)
#if BLUE_WITH_PYTHON
		MAP_METHOD_AS_METHOD
		(
			"GetEntries",
			GetEntries,
			"Gets a list of log entries captured.\n"
			":rtype: (str, str, int, long, str)"
		)
#endif
		MAP_METHOD_AND_WRAP
		(
			"Clear",
			Clear,
			"Clears any log messages captured."
		)
		MAP_PROPERTY
		(
			"capacity",
			GetCapacity,
			SetCapacity,
			"Controls how many log messages can be captured."
		)
		MAP_PROPERTY
		(
			"threshold",
			GetThreshold,
			SetThreshold,
			"Controls the severity threshold for capturing log messages."
		)
		MAP_PROPERTY
		(
			"saveOnQuit",
			IsSaveOnQuit,
			SetSaveOnQuit,
			"Flag that determines if the in memory logs should be saved out when the user quits."
		)
#if BLUE_WITH_PYTHON
		MAP_ATTRIBUTE
		(
			"saveLogCallback",
			m_saveLogCallback,
			"Python function that saves out the in memory logs",
			Be::READWRITE
		)
#endif
	EXPOSURE_END()
}
