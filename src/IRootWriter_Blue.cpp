////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"
#include "IRootWriter.h"

BLUE_DEFINE_ABSTRACT( IRootWriter );

const Be::ClassInfo* IRootWriter::ExposeToBlue()
{
	EXPOSURE_BEGIN( IRootWriter, "" )
		MAP_INTERFACE( IRootWriter )
	EXPOSURE_END()
}