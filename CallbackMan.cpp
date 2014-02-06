#include "StdAfx.h"

#include "CallbackMan.h"
#include "CcpCore/include/CcpTime.h"

#define CALLBACKMAN_DEBUGGING 0

#if CALLBACKMAN_DEBUGGING
	#define REPORT( x ) OutputDebugString( x )
	#define REPORT_TIME( msg, t ) { double d = t.GetSeconds(); char buffer[256]; sprintf_s( buffer, 256, msg, d ); OutputDebugString( buffer ); }
	#define REPORT_TIME1( msg, t, a1 ) { double d = t.GetSeconds(); char buffer[256]; sprintf_s( buffer, 256, msg, d, a1 ); OutputDebugString( buffer ); }
#else
	#define REPORT( x )
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
	, m_nextId( 1 )
	, m_queueMutex( "BlueCallbackMan", "m_queueMutex" )
	, m_fenceMutex( "BlueCallbackMan", "m_fenceMutex" )
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
	entry.isFenced = (flags & BCBF_FENCE) != 0;
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
		m_alarm.Signal();
	}

	return true;
}

void BlueCallbackMan::Cancel( uint32_t id )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	m_queueMutex.Acquire();

	bool found = false;
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

			found = true;
		}
	}
	if( !found )
	{
		for( CallbackEntryList::iterator it = m_urgentQueue.begin(); it != m_urgentQueue.end(); ++it )
		{
			if( it->id == id )
			{
				m_urgentQueue.erase( it );
				--m_size;
				found = true;
				break;
			}
		}
		if( !found )
		{
			for( CallbackEntryList::iterator it = m_queue.begin(); it != m_queue.end(); ++it )
			{
				if( it->id == id )
				{
					m_queue.erase( it );
					--m_size;
					break;
				}
			}
		}
		m_queueMutex.Release();
	}
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
		CcpJoinThread( td->m_threadHandle, nullptr );

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

	bool wasEmpty = true;
	CallbackEntry entry;

	BeTimer timeToGetQueue;

	// Acquire fence mutex. It ensures that other threads can't pull of the queue until we've
	// determined whether the entry we're about to pull is fenced. The fence mutex is only
	// used by the worker threads so it won't stall the main thread.
	m_fenceMutex.Acquire();

	// Ensure exclusive access to queue. We already hold the fence mutex but that is not enough
	// as the main thread may want to add entries to the queue while we're here. We hold the queue
	// mutex only for queue maintenance itself so we don't stall the main thread.
	m_queueMutex.Acquire();
	if( timeToGetQueue.GetSeconds() > 0.001 )
	{
		REPORT_TIME( "- Acquired queue in %g seconds\n", timeToGetQueue );
	}

	// Note that size of queue is not decreased until after processing callback.
	if( !m_urgentQueue.empty() )
	{
		entry = m_urgentQueue.front();
		m_urgentQueue.pop_front();
		wasEmpty = false;
		td->m_currentId = entry.id;
	}
	else if( !m_queue.empty() )
	{
		entry = m_queue.front();
		m_queue.pop_front();
		wasEmpty = false;
		td->m_currentId = entry.id;
	}

	m_queueMutex.Release();
	
	if( !wasEmpty )
	{
		BeTimer t;

		td->m_cbInProgressMutex.Acquire();

		if( entry.isFenced )
		{
			// This callback requires all callbacks that were ahead of it in the
			// queue to be finished before it finishes. This is ensured by waiting
			// for other callbacks in progress to have finished before we let this
			// further here.
			// No other thread could have pulled anything from the queue yet as we
			// hold the fence mutex still, and we grab the in-progress mutex for all
			// the other threads so even after we release the fence mutex the thread
			// won't be able to issue the callback.
			for( ThreadDataVector::iterator it = m_threads.begin(); it != m_threads.end(); ++it )
			{
				if( *it != td )
				{
					(*it)->m_cbInProgressMutex.Acquire();
				}
			}
		}

		m_fenceMutex.Release();

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

		td->m_currentId = 0;
		td->m_cbInProgressMutex.Release();

		if( entry.isFenced )
		{
			// Let the other threads loose again
			for( ThreadDataVector::iterator it = m_threads.begin(); it != m_threads.end(); ++it )
			{
				if( *it != td )
				{
					(*it)->m_cbInProgressMutex.Release();
				}
			}
		}

		m_queueMutex.Acquire();

		--m_size;

		m_queueMutex.Release();


		REPORT_TIME( "<<Processing callback done - %g seconds\n", t );
	}
	else
	{
		// Queue was empty, no callback to issue, simply release the fence
		// mutex again.
		m_fenceMutex.Release();
	}

	return !wasEmpty;
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
	// TODO: Thread priorities on non-win32
#ifdef _WIN32
	if( prio > THREAD_PRIORITY_HIGHEST )
	{
		prio = THREAD_PRIORITY_HIGHEST;
	}
	else if( prio < THREAD_PRIORITY_LOWEST )
	{
		prio = THREAD_PRIORITY_LOWEST;
	}
	m_threadPriority = prio;

	for( unsigned int i = 0; i < m_threads.size(); ++i )
	{
		SetThreadPriority( m_threads[i]->m_threadHandle, m_threadPriority );
	}
#endif
}

void BlueCallbackMan::SetName( const char* name )
{
	m_name = name;
	m_queueMutex.SetOwner( name );

	CCP_ASSERT( m_threads.size() <= 8 );
	static const char* suffixes[] = {"_1", "_2", "_3", "_4", "_5", "_6", "_7", "_8"};

	for( unsigned int i = 0; i < m_threads.size(); ++i )
	{
		ThreadData* td = m_threads[i];

		td->m_name = name;
		if( m_threads.size() > 1 )
		{
			td->m_name += suffixes[i];
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
