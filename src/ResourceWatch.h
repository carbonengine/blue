// Copyright © 2017 CCP ehf.

#pragma once

#include "BlueAsyncRes.h"

BLUE_CLASS( ResourceWatch ): public IRoot, IBlueAsyncResNotifyTarget
{
public:
	EXPOSE_TO_BLUE();

	ResourceWatch( IRoot* lockobj = nullptr );
	~ResourceWatch();

	void Watch( BlueAsyncRes* resource );
protected:
	void ReleaseCachedData( BlueAsyncRes* resource ) override;
	void RebuildCachedData( BlueAsyncRes* resource ) override;
private:
	BlueAsyncResPtr m_resource;
	BlueScriptCallback m_onLoaded;
	BlueScriptCallback m_onUnloaded;
};

TYPEDEF_BLUECLASS( ResourceWatch );
