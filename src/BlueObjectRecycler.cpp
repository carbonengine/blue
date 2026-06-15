// Copyright © 2012 CCP ehf.

#include "StdAfx.h"

#include "BlueObjectRecycler.h"
#include "BlueFileUtil.h"
#include <BlueStatistics.h>
#include "IBlueResMan.h"
#include "IBlueOS.h"

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "ObjectRecycler" );

CCP_STATS_DECLARE( recyclerRequestCount,			"Blue/Recycler/RequestCount",			true,	CST_COUNTER_LOW, "Number of requests for recyclable objects per frame" );
CCP_STATS_DECLARE( recyclerTotalRequestCount,		"Blue/Recycler/TotalRequestCount",		false,	CST_COUNTER_LOW, "Total number of requests for recyclable objects" );
CCP_STATS_DECLARE( recyclerRecycledCount,			"Blue/Recycler/RecycledCount",			true,	CST_COUNTER_LOW, "Number of requests fulfilled using recycled objects per frame" );
CCP_STATS_DECLARE( recyclerTotalRecycledCount,		"Blue/Recycler/TotalRecycledCount",		false,	CST_COUNTER_LOW, "Total number of requests fulfilled using recycled objects" );

BlueObjectRecycler::BlueObjectRecycler( IRoot* lockobj /*= nullptr */ ) :
	m_objectInfoByName( "BlueObjectRecycler/m_objectInfoByName" ),
	m_objectInfoByObject( "BlueObjectRecycler/m_objectInfoByObject"),
	m_objectInfosWithInstances( "BlueObjectRecycler/m_objectInfosWithInstances"),
	m_timeLimit( 60.0f )
{

}

BlueObjectRecycler::~BlueObjectRecycler()
{
	Clear();
}

void BlueObjectRecycler::Add( const std::wstring& key, IRoot* instance )
{
	auto foundIt = m_objectInfoByName.find( key );
	if( foundIt == m_objectInfoByName.end() )
	{
		IWeakObjectPtr weakResult( BlueCastPtr( instance ) );
		if( !weakResult )
		{
			CCP_LOGWARN_CH( s_ch, "%s: Can't recycle %S", __FUNCTION__, key.c_str() );
			return;
		}

		weakResult->WeakRefRegister( this );

		ObjectInfo* entry = CCP_NEW( "ObjectRecycler/entry" ) ObjectInfo;
		entry->resPath = key;
		entry->totalRequests = 1;
		entry->liveCount = 1;
		entry->maxLiveCount = 1;
		entry->recycledCount = 0;
		entry->timeOfLastRequest = BeOS->GetCurrentFrameTime();

		m_objectInfoByName[key] = entry;
		m_objectInfoByObject[weakResult] = entry;
	}
	else
	{
		ObjectInfo* entry = foundIt->second;
		entry->totalRequests += 1;
		entry->timeOfLastRequest = BeOS->GetCurrentFrameTime();
		entry->liveCount += 1;
		if( entry->liveCount > entry->maxLiveCount )
		{
			entry->maxLiveCount = entry->liveCount;
		}

		IWeakObjectPtr weakResult( BlueCastPtr( instance ) );
		weakResult->WeakRefRegister( this );

		m_objectInfoByObject[weakResult] = entry;
	}
}

IRoot* BlueObjectRecycler::Recycle( const std::wstring& key )
{
	CCP_STATS_INC( recyclerRequestCount );
	CCP_STATS_INC( recyclerTotalRequestCount );

	auto foundIt = m_objectInfoByName.find( key );
	if( foundIt == m_objectInfoByName.end() )
	{
		return nullptr;
	}

	ObjectInfo* entry = foundIt->second;

	if( entry->instances.empty() )
	{
		return nullptr;
	}

	// We have an instance we can recycle
	CCP_STATS_INC( recyclerRecycledCount );
	CCP_STATS_INC( recyclerTotalRecycledCount );

	CCP_LOG_CH( s_ch, "Recycling %S", key.c_str() );

	auto result = entry->instances.back();
	entry->instances.pop_back();
	entry->recycledCount += 1;
	return result;
}

bool BlueObjectRecycler::RecycleOrLoad( const wchar_t* resPath, IRoot** obj )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	std::wstring normalizedPath;
	NormalizeResPath( resPath, normalizedPath );

	auto result = Recycle( normalizedPath );
	if( !result )
	{
		result = BeResMan->LoadObject( normalizedPath.c_str() ).Detach();
		if( !result )
		{
			*obj = nullptr;
			return false;
		}
	}
	Add( normalizedPath, result );
	*obj = result;
	return true;
}


bool BlueObjectRecycler::RecycleOrCopy( const wchar_t* key, IRoot* srcObj, IRoot** obj )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	auto result = Recycle( key );
	if( !result )
	{
		if( !BeClasses->CopyTo( srcObj, &result ) )
		{
			*obj = nullptr;
			return false;
		}
	}
	Add( key, result );
	*obj = result;
	return true;
}


void BlueObjectRecycler::Clear()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	m_objectInfosWithInstances.clear();

	// Iterate over all objects created through us and unregister from weak-ref notifications
	for( auto it = m_objectInfoByObject.begin(); it != m_objectInfoByObject.end(); ++it )
	{
		it->first->WeakRefUnregister( this );
	}

	m_objectInfoByObject.clear();

	// Iterate over all object info entries, release objects kept for recycling and release the
	// info entries themselves.
	for( auto it = m_objectInfoByName.begin(); it != m_objectInfoByName.end(); ++it )
	{
		auto entry = it->second;
		for( auto instIt = entry->instances.begin(); instIt != entry->instances.end(); ++instIt )
		{
			auto instance = *instIt;
			instance->Unlock();
		}

		CCP_DELETE( entry );
	}

	m_objectInfoByName.clear();
}


void BlueObjectRecycler::Update( Be::Time time )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	std::vector<TrackableStdSet<ObjectInfo*>::iterator> toRemove;
	for( auto it = m_objectInfosWithInstances.begin(); it != m_objectInfosWithInstances.end(); ++it )
	{
		auto entry = *it;

		Be::Time delta = time - entry->timeOfLastRequest;
		if( !entry->instances.empty() && TimeAsFloat( delta ) > m_timeLimit )
		{
			// Object hasn't been requested for a while, time to prune instances
			size_t numToDelete;
			if( entry->instances.size() > 1 )
			{
				numToDelete = entry->instances.size() / 2;
			}
			else
			{
				numToDelete = 1;
			}
			for( size_t i = 0; i < numToDelete; ++i )
			{
				IRoot* obj = entry->instances.back();
				{
					IWeakObjectPtr weak( BlueCastPtr( obj ) );
					m_objectInfoByObject.erase( weak );
				}
				obj->Unlock();
				entry->instances.pop_back();
			}
			
			// Set time of last request to now, otherwise pruning will continue on the next update.
			entry->timeOfLastRequest = time;
		}

		if( entry->instances.empty() )
		{
			toRemove.push_back( it );
		}
	}

	for( auto it = toRemove.begin(); it != toRemove.end(); ++it )
	{
		m_objectInfosWithInstances.erase( *it );
	}
}


void BlueObjectRecycler::WeakRefNotify( IWeakObject *ptr )
{
	auto foundIt = m_objectInfoByObject.find( ptr );
	if( foundIt == m_objectInfoByObject.end() )
	{
		CCP_LOGERR_CH( s_ch, "ObjectRecycler got a notification for an object that is not on record" );
		return;
	}

	auto entry = foundIt->second;

	m_objectInfoByObject.erase( foundIt );

	if( entry->liveCount == 0 )
	{
		CCP_LOGERR_CH( s_ch, "ObjectRecycler live count is wrong" );
		entry->liveCount = 1;
	}
	entry->liveCount -= 1;

	Be::Time now = BeOS->GetCurrentFrameTime();

	Be::Time delta = now - entry->timeOfLastRequest;
	float deltaInSeconds = TimeAsFloat( delta );
	if( deltaInSeconds > m_timeLimit )
	{
		// Object hasn't been requested for a while, no reason to keep this instance
		return;
	}

	// Add a reference to the object, thus keeping it alive, and add it to the
	// instances list for later recycling.
	ptr->Lock();
	entry->instances.push_back( ptr );

	m_objectInfosWithInstances.insert( entry );
}

#if BLUE_WITH_PYTHON
PyObject* BlueObjectRecycler::GetInfo()
{
	PyObject* result = PyList_New( m_objectInfoByName.size() );
	unsigned int ix = 0;
	for( auto it = m_objectInfoByName.begin(); it != m_objectInfoByName.end(); ++it )
	{
		const std::wstring& path = it->first;
		auto entry = it->second;

		PyObject* tuple = PyTuple_New(6);
		PyTuple_SET_ITEM( tuple, 0, PyUnicode_FromWideChar( path.c_str(), path.size() ) );
		PyTuple_SET_ITEM( tuple, 1, PyLong_FromUnsignedLong( entry->totalRequests ) );
		PyTuple_SET_ITEM( tuple, 2, PyLong_FromUnsignedLong( entry->liveCount ) );
		PyTuple_SET_ITEM( tuple, 3, PyLong_FromUnsignedLong( entry->maxLiveCount ) );
		PyTuple_SET_ITEM( tuple, 4, PyLong_FromUnsignedLong( entry->recycledCount ) );
		PyTuple_SET_ITEM( tuple, 5, PyLong_FromUnsignedLong( (unsigned long)entry->instances.size() ) );

		PyList_SET_ITEM( result, ix, tuple );

		++ix;
	}

	return result;
}
#endif
