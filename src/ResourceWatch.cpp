// Copyright © 2017 CCP ehf.

#include "StdAfx.h"
#include "ResourceWatch.h"

ResourceWatch::ResourceWatch( IRoot* )
{
}

ResourceWatch::~ResourceWatch()
{
	if( m_resource )
	{
		m_resource->RemoveNotifyTarget( this );
	}
}

void ResourceWatch::Watch( BlueAsyncRes* resource )
{
	if( m_resource )
	{
		m_resource->RemoveNotifyTarget( this );
	}
	m_resource = resource;
	if( resource )
	{
		resource->AddNotifyTarget( this );
	}
}

void ResourceWatch::ReleaseCachedData( BlueAsyncRes* resource )
{
	if( resource == m_resource && m_onUnloaded.IsValid() )
	{
		m_onUnloaded.CallVoid( resource );
	}
}

void ResourceWatch::RebuildCachedData( BlueAsyncRes* resource )
{
	if( resource == m_resource && m_onLoaded.IsValid() )
	{
		m_onLoaded.CallVoid( resource );
	}
}
