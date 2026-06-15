// Copyright © 2015 CCP ehf.

#include "StdAfx.h"
#include "BlueTextResource.h"

BLUE_DEFINE( BlueTextResource );

const Be::ClassInfo* BlueTextResource::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueTextResource, "Simple resource class for plain text files" )
		MAP_ATTRIBUTE
		(
			"text",
			m_text,
			"Contents of the text file",
			Be::READ
		)
	EXPOSURE_CHAINTO( BlueAsyncRes )
}