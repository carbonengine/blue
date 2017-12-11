#pragma once

#include "Include/BlueAsyncRes.h"

BLUE_CLASS( ResourceWatch ): public IRoot, IBlueAsyncResNotifyTarget
{
public:
	EXPOSE_TO_BLUE();

	ResourceWatch( IRoot* lockobj = nullptr );
	~ResourceWatch();

	void Watch( BlueAsyncRes* resource );
protected:
	virtual void ReleaseCachedData( BlueAsyncRes* resource );
	virtual void RebuildCachedData( BlueAsyncRes* resource );
private:
	BlueAsyncResPtr m_resource;
	BlueScriptCallback m_onLoaded;
	BlueScriptCallback m_onUnloaded;
};

TYPEDEF_BLUECLASS( ResourceWatch );