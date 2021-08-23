/*************************************************************************

stats.h

Author:    Curt Hartung
Created:   Sep 2010
OS:        Win32
Project:   Carbon IO

Description: structure describing the statistics gathered from CarbonIO

(c) CCP 2010

***************************************************************************/
#include "StdAfx.h"
#include "stats.h"

#include <string.h>
#include <memory.h>

#include <CcpUtils/LinkHash.h>

//------------------------------------------------------------------------------
void CioStats::reset()
{
	// only reset the periodics, not the whole gamut
	GILAcquiredNum = 0;
	GILCoalesced = 0;
	GILAcquireTime = 0;
	longestGILAcquireTime = 0;
	zeroByteReadsRequired = 0;

	packetsReceived = 0;
	bytesReceived = 0;
	bytesReceivedDecompressed = 0;
	compressedAcceptedIn = 0;
	compressedRejectedIn = 0;
	compressedAccepted = 0;
	compressedRejected = 0;

	oorRecieved = 0;
	oorGaveUp = 0;
	noReadJobsScheduledGaveUp = 0;
	spunWaitingForReadJobsScheduled = 0;
	shutdownSpinWaitCount = 0;

	packetsSent = 0;
	bytesSent = 0;
	bytesSentCompressed = 0;

	accepts = 0;
	connects = 0;

	GILAcquiredNum = 0;
	GILCoalesced = 0;
	GILAcquireTime = 0;
	longestGILAcquireTime = 0;

	zeroByteReadsRequired = 0;
	partialSends = 0;

	packetsAllocated = 0;
	packetFreeListSize = 0;

	pendingCallQueueFailure = 0;
}

//------------------------------------------------------------------------------
CioEventTracker::CioEventTracker()
{
	InitializeCriticalSection( &m_listLock );
}

//------------------------------------------------------------------------------
CioEventTracker::~CioEventTracker()
{
	DeleteCriticalSection( &m_listLock );
}

//------------------------------------------------------------------------------
CioEventTracker::SEntry *CioEventTracker::get( const char* name )
{
	if ( !name )
	{
		return NULL;
	}

	unsigned int len = (unsigned int)strlen(name);
	if ( len > 126 )
	{
		return NULL;
	}
	
	unsigned int key = Ccp::Hash( name, len );

	Ccp::CriticalLockScoped lock( m_listLock );
	
	SEntry *entry = m_eventList.get( key );
	if ( !entry )
	{
		entry = m_eventList.add( key );
		memset( entry, 0, sizeof(SEntry) );
		strncpy_s( entry->name, name, len );
	}

	entry->count++;
	return entry;
}

//------------------------------------------------------------------------------
void CioEventTracker::inc( const char* name, long long time )
{
	SEntry *entry = get( name );
	if ( !entry )
	{
		return;
	}
	
	// The below is done without synchronization and so can be subject to race,
	// but it is non-critical stats information and so we don't care.
	entry->count ++;
	entry->t.time += time;

	// lifted directly from stacklessIO
	if ( entry->t.timeSmoothed )
	{
		entry->t.timeSmoothed = ( 127 * entry->t.timeSmoothed + time + 64 ) >> 7;  //exponential smoothing with f = 1/128
		long long var = ( time - entry->t.timeSmoothed ) * ( time  - entry->t.timeSmoothed );
		entry->t.varSmoothed = ( 127 * entry->t.varSmoothed + var + 64 ) >> 7;
		var *= ( time - entry->t.timeSmoothed );
		entry->t.thirdSmoothed = ( 127 * entry->t.thirdSmoothed + var + 64 ) >> 7;
	}
	else
	{
		entry->t.timeSmoothed = time;
		entry->t.varSmoothed = 0;
		entry->t.thirdSmoothed = 0;
	}
}

//------------------------------------------------------------------------------
void CioEventTracker::incval( const char* name, float value )
{
	SEntry *entry = get( name );
	if ( !entry )
	{
		return;
	}
	entry->type = 1;
	entry->f.val += value;
	entry->count ++;

	// lifted directly from stacklessIO
	if ( entry->f.valSmoothed )
	{
		const float f = 1.0f/128.0f;
		entry->f.valSmoothed = ( (1.0f - f) * entry->f.valSmoothed + f * value );
		float var = ( value - entry->f.valSmoothed ) * ( value - entry->f.valSmoothed );
		entry->f.varSmoothed = ( (1.0f - f) * entry->f.valSmoothed + f * var );
		var *= ( value - entry->f.valSmoothed );
		entry->f.thirdSmoothed = ( (1.0f - f) * entry->f.thirdSmoothed + f * var );
	}
	else
	{
		entry->f.valSmoothed = value;
		entry->f.varSmoothed = 0;
		entry->f.thirdSmoothed = 0;
	}
}

//------------------------------------------------------------------------------
void CioEventTracker::incevt( const char* name, int count /* =1 */ )
{
	SEntry *entry = get( name );
	if ( !entry )
	{
		return;
	}
	entry->count += count;
	entry->type = 2;
}

//------------------------------------------------------------------------------
void CioEventTracker::clear()
{
	// Because the methods above don't hold a lock while updating the entries
	// we must not delete the entries, even when we clear them.  Just
	// zero them.  Otherwise we might delete an entry that's being updated
	// by another thread.
	Ccp::CriticalLockScoped lock( m_listLock );
	Ccp::LinkHash<SEntry>::CIterator it( m_eventList );
	for( SEntry *e = it.getFirst(); e; e = it.getNext() )
	{
		memset(&e->t.time, 0, sizeof(*e) - offsetof(SEntry, t.time));
	}
}
