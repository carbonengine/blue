// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#include "IBlueOS.h"
#include "BlueResManBackgroundCall.h"
#include "BlueResFile.h"
#include "IBlueResMan.h"
#include "IBluePython.h"
#include "BlueStatistics.h"
#include <Scheduler.h>


static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "ResMan" );



BlueResManBackgroundCall::BlueResManBackgroundCall( IBlueResManBackgroundCall* theCall, uint32_t flags /*= 0 */ ) :
	m_id( 0 ),
	m_mainQueueId( 0 ),
	m_flags( flags ),
	m_isCanceling( 0 ),
	m_backgroundCall( theCall )
{
#if CCP_STACKLESS
	m_channel = SchedulerAPI()->PyChannel_New( NULL );
#endif
}

BlueResManBackgroundCall::~BlueResManBackgroundCall()
{
#if CCP_STACKLESS
	Py_XDECREF( m_channel );
	m_channel = nullptr;
#endif
}

void BlueResManBackgroundCall::AddToQueue()
{
	BeResMan->AddToQueue(
		BRMQ_BACKGROUND,
		BlueResManBackgroundCall::DoTheCall,
		this,
		m_flags,
		&m_id );
}

bool BlueResManBackgroundCall::Wait()
{
	CCP_STATS_ZONE( __FUNCTION__ );

#if CCP_STACKLESS
	if( !PyOS->CanYield() )
	{
		CCP_LOGERR_CH( s_ch, "Tasklet can't yield!" );
		return true;
	}

	PyObject *ret = SchedulerAPI()->PyChannel_Receive( m_channel );

	// We don't want to run in the context of the resman update. MarkAsDone below
	// sends on the channel, switching execution to this tasklet. We'd rather want
	// the update to finish as soon as possible.
	PyOS->Yield();

	if( !ret )
	{
		m_isCanceling = 1;

		BeResMan->CancelFromQueue( BRMQ_BACKGROUND, m_id );
		if( m_mainQueueId )
		{
			BeResMan->CancelFromQueue( BRMQ_MAIN, m_mainQueueId );
		}

		// Tasklet was killed
		Py_DECREF( m_channel );
		m_channel = nullptr;
		return false;
	}

	Py_DECREF( ret );

	return true;
#else
	CCP_ASSERT_M( false, "Wait shouldn't be called without Stackless" );
	return false;
#endif

}

void BlueResManBackgroundCall::DoTheCall( void* pContext )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	BlueResManBackgroundCall* args = static_cast<BlueResManBackgroundCall*>( pContext );

	if( args->m_backgroundCall )
	{
		args->m_backgroundCall->Perform();
	}

	if( !args->m_isCanceling )
	{
		BeResMan->AddToQueue( BRMQ_MAIN, MarkAsDone, pContext, args->m_flags, &args->m_mainQueueId );
	}
}

void BlueResManBackgroundCall::MarkAsDone( void* pContext )
{
	CCP_STATS_ZONE( __FUNCTION__ );

#if CCP_STACKLESS
	BlueResManBackgroundCall* args = static_cast<BlueResManBackgroundCall*>( pContext );
	
	SchedulerAPI()->PyChannel_Send( args->m_channel, Py_None );
#endif
}

bool BlueResManBackgroundCall::Issue( IBlueResManBackgroundCall* theCall, uint32_t flags /*= 0 */ )
{
#if CCP_STACKLESS
	if( !PyOS->CanYield() )
	{
		theCall->Perform();
		return true;
	}

	BlueResManBackgroundCall* cbArgs = CCP_NEW( "GetFileContents/cbArgs" )
		BlueResManBackgroundCall( theCall, flags );

	cbArgs->AddToQueue();
	bool result = cbArgs->Wait();

	CCP_DELETE cbArgs;
	return result;
#else
	theCall->Perform();
	return true;
#endif
}
