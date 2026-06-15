// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#include "CallbackMan.h"
#include <CcpTime.h>
#include <BlueStatistics.h>
#include "IBlueThreadMonitor.h"

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "CallbackMan" );

#define CALLBACKMAN_DEBUGGING 0

#if CALLBACKMAN_DEBUGGING
	#define REPORT( msg, ... ) CCP_LOG_CH( s_ch, msg, ##__VA_ARGS__ )
	#define REPORT_TIME( msg, t ) { double d = t.GetSeconds(); char buffer[256]; sprintf_s( buffer, 256, msg, d ); CCP_LOG_CH( s_ch, buffer ); }
	#define REPORT_TIME1( msg, t, a1 ) { double d = t.GetSeconds(); char buffer[256]; sprintf_s( buffer, 256, msg, d, a1 ); CCP_LOG_CH( s_ch, buffer ); }
#else
	#define REPORT( x, ... )
	#define REPORT_TIME( msg, t )
	#define REPORT_TIME1( msg, t, a1 )
#endif

namespace
{
	// Cache results from QueryPerformanceFrequency for timing
	uint64_t s_qpFreq = 0;
}

BlueCallbackMan::ThreadData::ThreadData() :
	m_owner( nullptr ),
	m_currentId( 0 ),
	m_cbInProgressMutex( "BlueCallbackMan", "m_cbInProgressMutex" ),
	m_threadHandle( 0 ),
	m_threadId( 0 ),
	m_name( "BlueCallbackManThread" )
{
}

BlueCallbackMan::BlueCallbackMan( IRoot* lockobj )
	: m_size( 0 )
	, m_queue( "BlueCallbackMan/m_queue" )
	, m_urgentQueue( "BlueCallbackMan/m_urgentQueue" )
	, m_fenceQueue( "BlueCallbackMan/m_fenceQueue" )
	, m_nextId( 1 )
	, m_queueMutex( "BlueCallbackMan", "m_queueMutex" )
	, m_threadCount( 1 )
	, m_isRunningOwnThreads( false )
	, m_threads( "BlueCallbackMan/m_threads" )
	, m_threadPriority( 0 )
	, m_pauseCounter( 0 )
	, m_stop( false )
	, m_timeInQueueMax( 0 )
	, m_timeInQueueTotal( 0 )
	, m_timeInQueueAverage( 0 )
	, m_entriesProcessed( 0 )
{
	if( !s_qpFreq )
	{
		s_qpFreq = CcpGetTimestampFrequency();
	}
}

BlueCallbackMan::~BlueCallbackMan()
{
	if( m_isRunningOwnThreads )
	{
		Stop();
	}
	else
	{
		// If this callback manager was explicitly ticked, then a dummy
		// thread data structure was set up.
		for( unsigned int i = 0; i < m_threads.size(); ++i )
		{
			ThreadData* td = m_threads[i];
			CCP_DELETE( td );
		}

		m_threads.clear();
	}
}

void BlueCallbackMan::SetThreadCount( unsigned int threadCount )
{
	if( threadCount > 32 )
	{
		CCP_LOGWARN_CH( s_ch, "Maximum thread count is 32" );
		threadCount = 32;
	}
	m_threadCount = threadCount;
}

bool BlueCallbackMan::Add( CallbackFunc pCb, void* pContext, uint32_t flags, CcpAtomic<uint32_t>* pId )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	m_queueMutex.Acquire();

	REPORT( "Adding callback\n" );
	
	if( pId )
	{
		*pId = m_nextId;
	}

	CallbackEntry entry;
	entry.id = m_nextId;
	entry.pCb = pCb;
	entry.pContext = pContext;
	entry.fenceMask = 0;

	if( m_isRunningOwnThreads && (flags & BCBF_FENCE) )
	{
		REPORT( "Adding fenced entry" );

		for( unsigned int i = 0; i < m_threadCount; ++i )
		{
			entry.fenceMask |= 1 << i;
		}
	}

	entry.timeStamp = CcpGetTimestamp();
	if( flags & BCBF_URGENT )
	{
		m_urgentQueue.push_back( entry );
	}
	else
	{
		m_queue.push_back( entry );
	}

	++m_nextId;
	++m_size;

	m_queueMutex.Release();

	if( m_isRunningOwnThreads )
	{
		CCP_STATS_ZONE( CCP_STRINGIZE( __FUNCTION__ ) " signal");

		m_alarm.Signal();
	}

	return true;
}

void BlueCallbackMan::Cancel( uint32_t id )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	m_queueMutex.Acquire();

	for( unsigned int i = 0; i < m_threads.size(); ++i )
	{
		ThreadData* td = m_threads[i];

		if( id == td->m_currentId )
		{
			// Callback being canceled is already in progress - wait for it to finish
			td->m_currentId = 0;

			// Have to release the queue mutex here to prevent a deadlock
			m_queueMutex.Release();

			// Acquiring the callback-in-progress mutex waits for currently
			// executing callback to finish.
			REPORT( "Cancel waiting for callback to finish\n" );
			td->m_cbInProgressMutex.Acquire();
			td->m_cbInProgressMutex.Release();

			return;
		}
	}

	bool found = RemoveFromQueue( m_urgentQueue, id );
	if( !found )
	{
		found = RemoveFromQueue( m_queue, id );
	}
	if( !found )
	{
		RemoveFromQueue( m_fenceQueue, id );
	}

	m_queueMutex.Release();
}

void BlueCallbackMan::Run()
{
	m_isRunningOwnThreads = true;
	m_stop = false;

	m_threads.resize( m_threadCount );
	for( unsigned int i = 0; i < m_threads.size(); ++i )
	{
		ThreadData* td = CCP_NEW( "BlueCallbackMan/m_threads/item" ) BlueCallbackMan::ThreadData();
		m_threads[i] = td;

		td->m_owner = this;

		td->m_threadHandle = CcpCreateThread( StaticThreadProc, td, (CcpThreadPriority_t)m_threadPriority );
		td->m_threadIndex = i;
	}

	SetName( m_name.c_str() );
}

uint32_t BlueCallbackMan::StaticThreadProc( void *pContext )
{
	BlueCallbackMan::ThreadData* context = (BlueCallbackMan::ThreadData*)pContext;
	return context->m_owner->ThreadProc( context );
}

uint32_t BlueCallbackMan::ThreadProc( BlueCallbackMan::ThreadData* td )
{
	while( !m_stop )
	{
		BeTimer t;
		if( !UpdateThread( td ) )
		{
			// Queue was empty, sleep until the alarm goes off, indicating new entry (or request to stop)
			REPORT( "-- Queue is empty\n" );
			m_alarm.Wait();
			REPORT( "-+ Waking up\n" );

		}

        Throttle();

        uint32_t pauseCounter = m_pauseCounter;
        if( pauseCounter > 0 )
        {
            REPORT( "-- Queue is paused\n" );
            m_pauseSemaphore.Wait();
            REPORT( "-+ Resuming from pause\n" );
        }

        REPORT_TIME( "== Update done - %g seconds\n", t );
	}

	return 0;
}

void BlueCallbackMan::Stop()
{
	m_stop = true;

	// This should only be called on a callback manager that runs its own thread(s)
	CCP_ASSERT( m_isRunningOwnThreads );

	// Ensure all threads wake up and see the stop sign
	for( unsigned int i = 0; i < m_threads.size(); ++i )
	{
		m_alarm.Signal();
	}

	// Wait for each thread to exit
	for( unsigned int i = 0; i < m_threads.size(); ++i )
	{
		ThreadData* td = m_threads[i];
		uint32_t result;
		CcpJoinThread( td->m_threadHandle, result );

		CCP_DELETE( td );
	}

	m_threads.clear();

	m_isRunningOwnThreads = false;
}

void BlueCallbackMan::Pause()
{
	// This should only be called on a callback manager that runs its own thread(s)
	CCP_ASSERT( m_isRunningOwnThreads );

	m_pauseCounter++;
}

void BlueCallbackMan::Resume()
{
	// This should only be called on a callback manager that runs its own thread(s)
	CCP_ASSERT( m_isRunningOwnThreads );

	uint32_t pauseCounter = m_pauseCounter;
    if( pauseCounter > 0 )
    {
		m_pauseCounter--;
    }
    if( pauseCounter == 1 )
    {
        m_pauseSemaphore.Signal();
    }
}

bool BlueCallbackMan::IsPaused()
{
    uint32_t pauseCounter = m_pauseCounter;
    return pauseCounter > 0;
}

bool BlueCallbackMan::Update()
{
	// This should never be called on a callback manager that runs its own thread(s)
	CCP_ASSERT( !m_isRunningOwnThreads );

	if( m_threads.empty() )
	{
		m_threads.resize( 1 );
		ThreadData* td = CCP_NEW( "BlueCallbackMan/m_threads/item" ) BlueCallbackMan::ThreadData();
		m_threads[0] = td;
	}

	return UpdateThread( m_threads[0] );
}

bool BlueCallbackMan::UpdateThread( struct ThreadData* td )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	ScopedThreadStatus threadStatus( IBlueThreadMonitor::BTS_WORKING );

	BeTimer timeToGetQueue;

	m_queueMutex.Acquire();
	if( timeToGetQueue.GetSeconds() > 0.001 )
	{
		REPORT_TIME( "- Acquired queue in %g seconds\n", timeToGetQueue );
	}

	CallbackEntry entry;
	bool haveEntry = false;
	bool processedFenceEntry = false;

	if( !m_fenceQueue.empty() )
	{
		CallbackEntry& fencedEntry = m_fenceQueue.front();
		
		uint32_t maskBefore = fencedEntry.fenceMask;
		fencedEntry.fenceMask &= ~(1 << td->m_threadIndex);

		if( fencedEntry.fenceMask && m_urgentQueue.empty() && m_queue.empty() )
		{
			for( unsigned int i = 0; i < m_threadCount; ++i )
			{
				ThreadData* otherThreadData = m_threads[i];
				if( otherThreadData != td )
				{
					if( otherThreadData->m_currentId == 0 )
					{
						fencedEntry.fenceMask &= ~(1 << i);
					}
				}
			}
		}

		REPORT( "Front of fenced queue - %x - %x", maskBefore, fencedEntry.fenceMask );
        (void)maskBefore;

		if( fencedEntry.fenceMask == 0 )
		{
			entry = fencedEntry;
			m_fenceQueue.pop_front();
			haveEntry = true;
			processedFenceEntry = true;
			REPORT( "Processing fenced entry %d on thread %d", fencedEntry.id, td->m_threadIndex );
		}
	}

	// Note that size of queue is not decreased until after processing callback.
	if( !haveEntry )
	{
		haveEntry = ExtractFromQueue( m_urgentQueue, entry, td->m_threadIndex );
		if( haveEntry )
		{
			REPORT( "Processing urgent entry %d on thread %d", entry.id, td->m_threadIndex );
		}
	}
	if( !haveEntry )
	{
		haveEntry = ExtractFromQueue( m_queue, entry, td->m_threadIndex );
		if( haveEntry )
		{
			REPORT( "Processing regular entry %d on thread %d", entry.id, td->m_threadIndex );
		}
	}

	if( haveEntry )
	{
		td->m_currentId = entry.id;
	}

	m_queueMutex.Release();
	
	if( haveEntry )
	{
		BeTimer t;

		td->m_cbInProgressMutex.Acquire();

		// It is possible the callback was canceled. Cancel has to release
		// the queue mutex before acquiring the callback-in-progress mutex
		// to prevent a deadlock.
		if( td->m_currentId == entry.id )
		{
			uint64_t now;
			now = CcpGetTimestamp();
			if( now > entry.timeStamp )
			{
				uint64_t delta = now - entry.timeStamp;
				if( delta > m_timeInQueueMax )
				{
					m_timeInQueueMax = delta;
				}
				m_timeInQueueTotal += delta;
			}
			++m_entriesProcessed;
			m_timeInQueueAverage = (double)m_timeInQueueTotal / (double)m_entriesProcessed;
			entry.pCb( entry.pContext );
		}

		td->m_cbInProgressMutex.Release();

		m_queueMutex.Acquire();

		td->m_currentId = 0;
		--m_size;

		m_queueMutex.Release();


		REPORT_TIME( "<<Processing callback done - %g seconds\n", t );
	}

	if( processedFenceEntry )
	{
		m_alarm.Signal();
	}

	return haveEntry;
}

float BlueCallbackMan::GetTimeInQueueMax() const
{
	if( s_qpFreq )
	{
		return (float)((double)m_timeInQueueMax / (double)s_qpFreq);
	}
	return 0;
}

float BlueCallbackMan::GetTimeInQueueAverage() const
{
	if( s_qpFreq )
	{
		return (float)((double)m_timeInQueueAverage / (double)s_qpFreq);
	}
	return 0;
}

void BlueCallbackMan::ResetQueueStats()
{
	m_timeInQueueAverage = 0;
	m_timeInQueueMax = 0;
	m_timeInQueueTotal = 0;
	m_entriesProcessed = 0;
}

bool BlueCallbackMan::IsEmpty() const
{
	return m_size == 0;
}

void BlueCallbackMan::SetPriority( int prio )
{
	if( prio > CCP_THREAD_PRIORITY_HIGHEST )
	{
		prio = CCP_THREAD_PRIORITY_HIGHEST;
	}
	else if( prio < CCP_THREAD_PRIORITY_LOWEST )
	{
		prio = CCP_THREAD_PRIORITY_LOWEST;
	}
	m_threadPriority = prio;

	for( unsigned int i = 0; i < m_threads.size(); ++i )
	{
		CcpSetThreadPriority( m_threads[i]->m_threadHandle, CcpThreadPriority_t( m_threadPriority ) );
	}
}

void BlueCallbackMan::SetName( const char* name )
{
	m_name = name;
	m_queueMutex.SetOwner( name );

	for( unsigned int i = 0; i < m_threads.size(); ++i )
	{
		ThreadData* td = m_threads[i];

		td->m_name = name;
		if( m_threads.size() > 1 )
		{
			char buffer[4];
			sprintf_s( buffer, "_%d", i );
			td->m_name += buffer;
		}
		td->m_cbInProgressMutex.SetOwner( td->m_name.c_str() );

		CcpRegisterThread( CcpGetThreadId( td->m_threadHandle ), td->m_name.c_str() );
	}
}

unsigned int BlueCallbackMan::GetSize() const
{
	return m_size;
}

uint32_t BlueCallbackMan::GetNextId() const
{
	return m_nextId;
}

bool BlueCallbackMan::RemoveFromQueue( CallbackEntryList &queue, uint32_t id )
{
	bool found = false;

	for( CallbackEntryList::iterator it = queue.begin(); it != queue.end(); ++it )
	{
		if( it->id == id )
		{
			queue.erase( it );
			--m_size;
			found = true;
			break;
		}
	}
	return found;
}

// Queue mutex is assumed to be held by the calling thread
bool BlueCallbackMan::ExtractFromQueue( CallbackEntryList &queue, CallbackEntry &entry, int threadIndex )
{
	bool haveEntry = false;

	// Check for fenced entries
	while( !queue.empty() )
	{
		entry = queue.front();
		if( entry.fenceMask )
		{
			// Entry is fenced - add it to the fence queue.
			entry.fenceMask &= ~(1 << threadIndex);
			queue.pop_front();
			m_fenceQueue.push_back( entry );
			m_alarm.Signal();
		}
		else
		{
			break;
		}
	}

	if( !queue.empty() )
	{
		// If queue is not empty it means we broke out of the loop above with an
		// entry that does not have a fence mask. 'entry' has been set, we just
		// need to remove it from the queue.
		queue.pop_front();
		haveEntry = true;
	}
	
	return haveEntry;
}
