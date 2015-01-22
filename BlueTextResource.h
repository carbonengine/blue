////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		January 2015
// Copyright:	CCP 2015
//

#pragma once
#ifndef BlueTextResource_h
#define BlueTextResource_h

#include "Include/BlueAsyncRes.h"
#include "Include/ICacheable.h"

BLUE_CLASS( BlueTextResource ) :
public BlueAsyncRes,
	public ICacheable
{
public:
	EXPOSE_TO_BLUE();
	BlueTextResource( IRoot* lockobj = NULL );

	//////////////////////////////////////////////////////////////////////////
	// ICacheable
	bool IsMemoryUsageKnown();
	size_t GetMemoryUsage();

protected:
	LoadingResult DoLoad();
	bool DoPrepare();

	std::string m_text;
};

TYPEDEF_BLUECLASS( BlueTextResource );

#endif // BlueTextResource_h