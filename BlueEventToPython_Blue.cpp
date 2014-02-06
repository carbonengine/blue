#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "BlueEventToPython.h"
#include "include/Blue.h"
#include "include/IBlueOS.h"

BLUE_DEFINE( BlueEventToPython );

const Be::ClassInfo* BlueEventToPython::ExposeToBlue()
{
    EXPOSURE_BEGIN( BlueEventToPython, "" )
        MAP_INTERFACE( BlueEventToPython )
		MAP_INTERFACE( IBlueEventListener)

		MAP_ATTRIBUTE
		(
			"handler",
			m_handler,
			"",
			Be::READWRITE
		)

    EXPOSURE_END()
}

#endif
