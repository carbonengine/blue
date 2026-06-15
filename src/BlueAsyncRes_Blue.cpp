// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "BlueAsyncRes.h"

BLUE_DEFINE_ABSTRACT( BlueAsyncRes );

BLUEIMPORT const Be::ClassInfo* BlueAsyncRes::ExposeToBlue()
{
    EXPOSURE_BEGIN(BlueAsyncRes, "" )
        MAP_INTERFACE( IBlueResource )

		MAP_ATTRIBUTE( "path", m_path, "", Be::READ )
		MAP_ATTRIBUTE( "ext", m_ext, "", Be::READ )
		MAP_PROPERTY_READONLY
		( 
			"isGood", 
			IsGood, 
			"True if the resource was loaded and prepared successfully. If false, it is either\n"
			"not ready (still loading or preparing), or it failed. See 'isLoading' and 'isPrepared'\n"
			"for more details."

		)
		MAP_PROPERTY_READONLY
		( 
			"isPrepared", 
			IsPrepared, 
			"True if the resource has finished preparing, whether it was successful or not."
		)
		MAP_PROPERTY_READONLY
		( 
			"isLoading", 
			IsLoading, 
			"True if the resource is loading."
		)

		MAP_METHOD_AND_WRAP( "IsLoading", IsLoading, "" )
		MAP_METHOD_AND_WRAP( "IsPrepared", IsPrepared, "" )
		MAP_METHOD_AND_WRAP( "IsGood", IsGood, "" )
		MAP_METHOD_AND_WRAP( "Reload", Reload, "" )
	EXPOSURE_END()
}
