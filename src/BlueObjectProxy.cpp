// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#include "BlueObjectProxy.h"
#include <BlueStatistics.h>
#include "IBlueOS.h"
#include "IUnloadable.h"

BlueObjectProxy::BlueObjectProxy() : 
	m_objectMarker( 0 ),
	m_lastTimeUsed( 0 ),
	m_temporary( false ),
	m_isUnloaded( false )
{
}

BlueObjectProxy::~BlueObjectProxy()
{
	// note: no need to call OnObjectInvalidated() to notify the builder;
	// if it was hanging on to the proxy, we wouldn't get here in the first place (refcount > 0)
}

void BlueObjectProxy::OnObjectInvalidated()
{
	if( m_object && m_builder)
	{
		m_builder->OnObjectDestroyed( m_objectMarker, this );
	}
}

void BlueObjectProxy::SetBuilder( IBlueObjectBuilder* builder, unsigned int objectMarker )
{
	OnObjectInvalidated();	// make sure any old builders let go

	m_builder = builder;
	m_objectMarker = objectMarker;
}

IRoot* BlueObjectProxy::GetObject( )
{
	if( m_isUnloaded && m_object )
	{
		CCP_STATS_ZONE( CCP_STRINGIZE( __FUNCTION__ ) "ReloadWhenReferenced" );
		IUnloadablePtr objectAsUnloadable = BlueCastPtr( m_object );
		// Shouldn't need a nullptr check here - m_isUnloaded only becomes true if this
		// interface is supported.
		objectAsUnloadable->ReloadWhenReferenced();
		m_isUnloaded = false;
	}

	if( !m_object && m_builder )
	{
		CCP_STATS_ZONE( CCP_STRINGIZE( __FUNCTION__ ) "CreateObject" );
		m_object.Attach( m_builder->CreateObject( m_objectMarker, this ) );
	}

	m_lastTimeUsed = BeOS->GetInfo()->mRealTime;

	return m_object;
}

void BlueObjectProxy::ClearObject()
{
	OnObjectInvalidated();
	m_object = NULL;
}

bool BlueObjectProxy::Update( Be::Time time, Be::Time timeout )
{
	if( m_isUnloaded )
	{
		return false;
	}

	// If there is a time out set and we have a builder to reconstruct
	// the object we null out the object if it hasn't been used for
	// the given time out period.
	if( m_object && timeout && m_builder )
	{
		// Don't operate on the time we're given, it's sim time and we're operating in real time here.
		Be::Time delta = BeOS->GetInfo()->mRealTime - m_lastTimeUsed;
		if( delta > timeout )
		{
			OnObjectInvalidated();


			IUnloadablePtr objectAsUnloadable = BlueCastPtr( m_object );
			if( objectAsUnloadable )
			{
				CCP_STATS_ZONE( CCP_STRINGIZE( __FUNCTION__ ) "UnloadWhenUnreferenced" );
				objectAsUnloadable->UnloadWhenUnreferenced();
				m_isUnloaded = true;
			}
			else
			{
				m_object = NULL;
			}

			return true;
		}
	}

	return false;
}

bool BlueObjectProxy::IsResident() const
{
	return m_object != NULL;
}

void BlueObjectProxy::SetTemporary( bool isTemporary )
{
	m_temporary = isTemporary;
}

bool BlueObjectProxy::IsTemporary() const
{
	return m_temporary;
}

void BlueObjectProxy::OnSelected()
{
	if( m_builder)
	{
		m_builder->OnSelected( m_objectMarker, this );
	}
}

void BlueObjectProxy::SetObject( IRoot* obj )
{
	OnObjectInvalidated();
	m_object = obj;	
    m_builder.Unlock();
}

void BlueObjectProxy::SetObjectFromBuilder( IRoot * obj )
{
	m_object = obj;
}

void BlueObjectProxy::Freeze()
{
	//TODO document that this can't work well with async builders? since it's a contradiction..
	// .. or somehow support the freezing in the builder?
	if( !m_object && m_builder )
	{
		m_object.Attach( m_builder->CreateObject( m_objectMarker, this ) );
	}
	m_builder.Unlock();
}
