////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		December 2012
// Copyright:	CCP 2012
//

#include "StdAfx.h"

#include "BlueObjectRecycler.h"
#include "include/BlueFileUtil.h"
#include "include/IBlueResMan.h"
#include "include/IBlueOS.h"

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

bool BlueObjectRecycler::RecycleOrLoad( const wchar_t* resPath, IRoot** obj )
{
	CCP_STATS_ZONE( __FUNCTION__ );
	CCP_STATS_INC( recyclerRequestCount );
	CCP_STATS_INC( recyclerTotalRequestCount );

	std::wstring normalizedPath;
	NormalizeResPath( resPath, normalizedPath );
	auto foundIt = m_objectInfoByName.find( normalizedPath );

	if( foundIt == m_objectInfoByName.end() )
	{
		// We haven't seen this path before - load the object and create
		// an entry for it. 
		IRoot* result = BeResMan->LoadObjectW( normalizedPath.c_str() );
		if( !result )
		{
			*obj = nullptr;
			return false;
		}

		foundIt = m_objectInfoByName.find( normalizedPath );
		if( foundIt != m_objectInfoByName.end() )
		{
			// Another tasklet added this name while we were loading. We take the
			// easy way out and don't try to recycle this particular instance.
			*obj = result;
			return true;
		}

		// We need a notification when the instance we just created is about to
		// die. We may decide to keep it alive for recycling.
		IWeakObjectPtr weakResult( BlueCastPtr( result ) );
		if( !weakResult )
		{
			CCP_LOGWARN_CH( s_ch, "%s: Can't recycle %S", __FUNCTION__, resPath );

			// Object doesn't support weak refs - can't recycle it.
			*obj = result;
			return true;
		}

		weakResult->WeakRefRegister( this );

		ObjectInfo* entry = CCP_NEW( "ObjectRecycler/entry" ) ObjectInfo;
		entry->resPath = normalizedPath;
		entry->totalRequests = 1;
		entry->liveCount = 1;
		entry->maxLiveCount = 1;
		entry->recycledCount = 0;
		entry->timeOfLastRequest = BeOS->GetCurrentFrameTime();

		m_objectInfoByName[normalizedPath] = entry;
		m_objectInfoByObject[weakResult] = entry;
		*obj = result;
		return true;
	}

	IRoot* result = nullptr;

	ObjectInfo* entry = foundIt->second;
	entry->totalRequests += 1;
	entry->timeOfLastRequest = BeOS->GetCurrentFrameTime();

	if( !entry->instances.empty() )
	{
		CCP_STATS_INC( recyclerRecycledCount );
		CCP_STATS_INC( recyclerTotalRecycledCount );

		CCP_LOG_CH( s_ch, "Recycling %S", resPath );

		// We have an instance we can recycle
		result = entry->instances.back();
		entry->instances.pop_back();
		entry->recycledCount += 1;
	}
	else
	{
		// Need to load a new one
		result = BeResMan->LoadObjectW( normalizedPath.c_str() );
		if( !result )
		{
			*obj = nullptr;
			return false;
		}
	}

	entry->liveCount += 1;
	if( entry->liveCount > entry->maxLiveCount )
	{
		entry->maxLiveCount = entry->liveCount;
	}

	IWeakObjectPtr weakResult( BlueCastPtr( result ) );
	weakResult->WeakRefRegister( this );

	m_objectInfoByObject[weakResult] = entry;

	*obj = result;
	return true;
}


bool BlueObjectRecycler::RecycleOrCopy( const wchar_t* key, IRoot* srcObj, IRoot** obj )
{
	CCP_STATS_ZONE( __FUNCTION__ );
	CCP_STATS_INC( recyclerRequestCount );
	CCP_STATS_INC( recyclerTotalRequestCount );

	auto foundIt = m_objectInfoByName.find( key );

	if( foundIt == m_objectInfoByName.end() )
	{
		// We haven't seen this key before - create an entry for it and copy the source object
		if( !BeClasses->CopyTo( srcObj, obj ) )
		{
			*obj = nullptr;
			return false;
		}

		// We need a notification when the instance we just created is about to
		// die. We may decide to keep it alive for recycling.
		IWeakObjectPtr weakResult( BlueCastPtr( *obj ) );
		if( !weakResult )
		{
			CCP_LOGWARN_CH( s_ch, "%s: Can't recycle %S", __FUNCTION__, key );

			// Object doesn't support weak refs - can't recycle it.
			return true;
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
		return true;
	}

	ObjectInfo* entry = foundIt->second;
	entry->totalRequests += 1;
	entry->timeOfLastRequest = BeOS->GetCurrentFrameTime();

	if( !entry->instances.empty() )
	{
		CCP_STATS_INC( recyclerRecycledCount );
		CCP_STATS_INC( recyclerTotalRecycledCount );

		CCP_LOG_CH( s_ch, "Recycling %S", key );

		// We have an instance we can recycle
		*obj = entry->instances.back();
		entry->instances.pop_back();
		entry->recycledCount += 1;
	}
	else
	{
		// Need to load a new one
		if( !BeClasses->CopyTo( srcObj, obj ) )
		{
			*obj = nullptr;
			return false;
		}
	}

	entry->liveCount += 1;
	if( entry->liveCount > entry->maxLiveCount )
	{
		entry->maxLiveCount = entry->liveCount;
	}

	IWeakObjectPtr weakResult( BlueCastPtr( *obj ) );
	weakResult->WeakRefRegister( this );

	m_objectInfoByObject[weakResult] = entry;

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
