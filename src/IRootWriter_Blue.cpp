// Copyright © 2013 CCP ehf.

#include "StdAfx.h"
#include "IRootWriter.h"

BLUE_DEFINE_ABSTRACT( IRootWriter );

const Be::ClassInfo* IRootWriter::ExposeToBlue()
{
	EXPOSURE_BEGIN( IRootWriter, "" )
		MAP_INTERFACE( IRootWriter )
	EXPOSURE_END()
}