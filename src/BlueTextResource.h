// Copyright © 2015 CCP ehf.

#pragma once
#ifndef BlueTextResource_h
#define BlueTextResource_h

#include "BlueAsyncRes.h"
#include "ICacheable.h"

BLUE_CLASS( BlueTextResource ) :
public BlueAsyncRes,
	public ICacheable
{
public:
	EXPOSE_TO_BLUE();
	BlueTextResource( IRoot* lockobj = NULL );

	//////////////////////////////////////////////////////////////////////////
	// ICacheable
	bool IsMemoryUsageKnown() override;
	size_t GetMemoryUsage() override;

protected:
	LoadingResult DoLoad() override;
	bool DoPrepare() override;

	std::string m_text;
};

TYPEDEF_BLUECLASS_SHUTDOWN( BlueTextResource );

#endif // BlueTextResource_h
