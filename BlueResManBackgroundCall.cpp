#include "StdAfx.h"

#if CCP_STACKLESS

#include "BlueResManBackgroundCall.h"
#include "stackless_api.h"
#include "BlueResFile.h"
#include "Include/IBlueResMan.h"
#include "Include/IBluePython.h"
#include "Include/BlueStatistics.h"


static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "ResMan" );



BlueResManBackgroundCall::BlueResManBackgroundCall( uint32_t flags /*= 0 */ ) : m_flags( flags )
{
	m_channel = PyChannel_New( NULL );
}

BlueResManBackgroundCall::~BlueResManBackgroundCall()
{
	Py_XDECREF( m_channel );
	m_channel = nullptr;
}

void BlueResManBackgroundCall::AddToQueue()
{
	BeResMan->AddToQueue( 
		BRMQ_BACKGROUND, 
		BlueResManBackgroundCall::ForwardMarkAsDone, 
		this, 
		IBlueCallbackMan::BCBF_FENCE | m_flags, 
		NULL );
}

bool BlueResManBackgroundCall::Wait()
{
	if( !PyOS->CanYield() )
	{
		return true;
	}

	// Go to sleep and wake up! *(the sender releases the channel)
	PyObject *ret = PyChannel_Receive( m_channel );

	if( !ret )
	{
		// Tasklet was killed
		Py_DECREF( m_channel );
		m_channel = nullptr;
		return false;
	}

	Py_DECREF( ret );
	return true;
}

void BlueResManBackgroundCall::ForwardMarkAsDone( void* pContext )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	BlueResManBackgroundCall* args = static_cast<BlueResManBackgroundCall*>( pContext );
	BeResMan->AddToQueue( BRMQ_MAIN, MarkAsDone, pContext, args->m_flags, NULL );
}

void BlueResManBackgroundCall::MarkAsDone( void* pContext )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	BlueResManBackgroundCall* args = static_cast<BlueResManBackgroundCall*>( pContext );

	// Tasklet may have been killed while we were waiting
	if( args->m_channel )
	{
		BeOS->NextScheduledEvent(0);
		PyChannel_Send( args->m_channel, Py_None );
	}
	else
	{
		// Tasklet was killed, so the args object won't be deleted by Wait.
		CCP_DELETE args;
	}
}

void BlueResManReadStreamArgs::ReadStream( const std::wstring& filename, IBlueStream* stream )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	m_filename = filename;
	m_destination = stream;

	if( BeResMan->IsOnMainThread() && PyOS->CanYield() )
	{
		AddToQueue();
	}
	else
	{
		CopyStreamImpl();
	}
}

void BlueResManReadStreamArgs::AddToQueue()
{
	BeResMan->AddToQueue( 
		BRMQ_BACKGROUND, 
		ForwardCopyStream, 
		this, 
		0, 
		NULL );
}

void BlueResManReadStreamArgs::ForwardCopyStream( void* context )
{
	BlueResManReadStreamArgs* pThis = reinterpret_cast<BlueResManReadStreamArgs*>( context );
	pThis->CopyStreamImpl();
	BeResMan->AddToQueue( BRMQ_MAIN, MarkAsDone, context, 0, NULL );
}

void BlueResManReadStreamArgs::CopyStreamImpl()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	ResFilePtr resFile;
	if( !resFile.CreateInstance() )
	{
		return;
	}

	if (!resFile->OpenW( m_filename.c_str(), true ) )
	{
		CCP_LOGERR_CH( s_ch, "'%S': no matching file in resource directory.", m_filename.c_str() );
		return;
	}

	void* data;
	size_t dataSize = resFile->GetSize();
	if( !resFile->LockData( &data, dataSize ) )
	{
		return;
	}

	m_destination->Write( data, dataSize );
	m_destination->Seek( 0, BS_BEGIN );
}

#endif
