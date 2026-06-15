// Copyright © 2013 CCP ehf.

#include "StdAfx.h"
#include "WatchdogThread.h"


WatchdogThread::WatchdogThread( const char* name ) :
	m_timeoutHandler( nullptr ),
	m_name( name ),
	m_counter( 0 ),
	m_isRunning( 0 ),
	m_thread( 0 )
{
}

void WatchdogThread::Start( uint32_t timeoutInMs, ITimeoutHandler* timeoutHandler )
{
	if( m_isRunning )
	{
		Stop();
	}

	m_counter = 0;
	m_isRunning = 1;
	m_timeout = timeoutInMs;
	m_timeoutHandler = timeoutHandler;

	if( m_timeout )
	{
		m_thread = CcpCreateThread( ThreadFunc, this, CCP_THREAD_PRIORITY_HIGHEST );
	}
}

void WatchdogThread::Stop()
{
	if( m_isRunning )
	{
		m_isRunning = false;

		CcpKillThread( m_thread );
		m_thread = 0;
	}
}

void WatchdogThread::Tick()
{
	if( m_isRunning )
	{
		m_counter++;
	}
}

uint32_t WatchdogThread::ThreadFunc( void* context )
{
	WatchdogThread* pThis = static_cast<WatchdogThread*>( context );

	while( pThis->m_isRunning )
	{
		uint32_t lastValue = pThis->m_counter;
		CcpThreadSleep( pThis->m_timeout );
		if( pThis->m_isRunning && (pThis->m_counter == lastValue) )
		{
			// Watch dog has timed out
			CCP_LOGWARN( "Watch dog %s timed out", pThis->m_name.c_str() );

			if( pThis->m_timeoutHandler )
			{
				pThis->m_timeoutHandler->NotifyOfTimeout();
			}
		}
	}

	return 0;
}
