#include "StdAfx.h"

#include "include/BlueAsyncRes.h"
#include "include/IBlueResMan.h"

#define ASYNCLOADEDRESOURCE_DEBUGGING 0
#if ASYNCLOADEDRESOURCE_DEBUGGING
#define REPORT( x ) OutputDebugString( x )
#define REPORT_TIME( msg, t ) { double d = t.GetSeconds(); char buffer[256]; sprintf_s( buffer, 256, msg, d ); OutputDebugString( buffer ); }
#define REPORT_TIME1( msg, t, a1 ) { double d = t.GetSeconds(); char buffer[256]; sprintf_s( buffer, 256, msg, d, a1 ); OutputDebugString( buffer ); }
#else
#define REPORT( x )
#define REPORT_TIME( msg, t )
#define REPORT_TIME1( msg, t, a1 )
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

BlueAsyncRes::BlueAsyncRes() :
	m_isLoading( FALSE ),
	m_isPrepared( FALSE ),
	m_loadSucceeded( FALSE ),
	m_notify( TRUE ),
	m_isGood( FALSE ),
	m_loadCbId( 0 ),
	m_prepareCbId( 0 ),
	m_isUrgent( false ),
	m_isForcedSynchronous( false ),
	m_reloadNotifyTargets( NULL ),
	m_reloadNotifyTargetsCount( 0 )
{
}

void BlueAsyncRes::Shutdown()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_isLoading )
	{
		CancelPendingLoad();
	}
	
	// Give subclasses a chance to do something on shutdown
	OnShutdown();
}

void BlueAsyncRes::SetPrepared( bool b )
{
	m_isPrepared = b ? TRUE : FALSE;
}


void BlueAsyncRes::SetGood( bool b )
{
	m_isGood = b ? TRUE : FALSE;
}

void BlueAsyncRes::StaticLoadAsync(void *pContext)
{
	BlueAsyncRes* pThis = static_cast<BlueAsyncRes*>( pContext );
	pThis->LoadAsync();
}

// This normally gets called on the background loading thread
void BlueAsyncRes::LoadAsync()
{
	CCP_STATS_ZONE( "BlueAsyncRes/LoadAsync" );

	BeTimer t;

	bool open = DoOpenStream();
	LoadingResult load = LR_FAILED;
	if( open )
	{
		load = DoLoad();
	}
	bool loadSucceeded = load != LR_FAILED;

	REPORT_TIME1( "LOAD: %5.5f sec '%S'\n", t, GetPath() );

	m_notify = load != LR_SUCCESS_DONTNOTIFY;

	uint32_t flags = 0;
	if( m_isUrgent )
	{
		flags = IBlueCallbackMan::BCBF_URGENT;
	}

	if( loadSucceeded )
	{
		m_loadSucceeded = TRUE;
		if( BeResMan->IsOnMainThread() )
		{
			// Async loads are disabled - we're on the main thread so call the function directly
			PrepareAsync();
		}
		else
		{
			// Queue a request for the D3D create calls on the main thread
			BeResMan->AddToQueue( BRMQ_MAIN, StaticPrepareAsync, this, flags, &m_prepareCbId );
		}
	}
	else
	{
		CCP_LOGWARN( "Failed to load '%S'", GetPath() );

		BeResMan->AddToQueue( BRMQ_MAIN, StaticFailedLoadNotify, this, flags, &m_prepareCbId );
	}

	m_loadCbId = 0;
}

void BlueAsyncRes::StaticPrepareAsync(void *pContext)
{
	BlueAsyncRes* pThis = static_cast<BlueAsyncRes*>( pContext );
	pThis->PrepareAsync();
}

// This gets called on the main thread
void BlueAsyncRes::PrepareAsync()
{
	CCP_STATS_ZONE( "BlueAsyncRes/PrepareAsync" );

	BeTimer t;

	bool isGood = DoPrepare();

	REPORT_TIME1( "PREP: %5.5f sec '%S'\n", t, GetPath() );

	DoCloseStream();
	m_prepareCbId = 0;
	m_isPrepared = TRUE;

	// Loading is finished, regardless of success
	m_isLoading = FALSE;

	if( isGood )
	{
		m_isGood = TRUE;
	}

	if( m_notify )
	{
		NotifyRebuildCachedData();
	}
}

void BlueAsyncRes::StaticFailedLoadNotify( void* pContext )
{
	BlueAsyncRes* pThis = static_cast<BlueAsyncRes*>( pContext );
	if( !pThis )
    {
        CCP_LOGERR( "BlueAsyncRes::StaticFailedLoadNotify: pContext is NULL!  Debug this, its important" );
        return;
    }
	// Loading is finished, regardless of success
	pThis->m_isPrepared = TRUE;
	pThis->m_loadSucceeded = FALSE;
	pThis->m_isLoading = FALSE;
    pThis->DoCloseStream();
	
	pThis->NotifyRebuildCachedData();
}

void BlueAsyncRes::InitializeImpl( const wchar_t* name, const wchar_t* ext )
{
	CCP_STATS_ZONE( "BlueAsyncRes/Initialize" );

	m_path = name;

	CW2A asciiName( name );
	CCP_LOG( "BlueAsyncRes::Initialize: %s", (const char*)asciiName );
#if CCP_TELEMETRY_ENABLED
	tmAlloc( g_telemetryContext, this, 1111111, "%s", tmDynamicString( g_telemetryContext, (char*)asciiName ) );
#endif

	m_ext = ext;

	m_isPrepared = FALSE;
	m_isGood = FALSE;

	if( m_path.size() > 0 )
	{
    	m_isLoading = TRUE;
    	m_loadSucceeded = FALSE;
		if( m_isForcedSynchronous )
		{
			LoadAsync();
		}
		else
		{
			m_isUrgent = BeResMan->IsUrgentResourceLoads();
			uint32_t flags = 0;
			if( m_isUrgent )
			{
				flags = IBlueCallbackMan::BCBF_URGENT;
			}
			BeResMan->AddToQueue( BRMQ_BACKGROUND, StaticLoadAsync, this, flags, &m_loadCbId );	
		}
	}
}


void BlueAsyncRes::CancelPendingLoad()
{
	CCP_STATS_ZONE( "BlueAsyncRes/CancelPendingLoad" );

	uint32_t loadCbId = m_loadCbId;
	if( loadCbId )
	{
		BeResMan->CancelFromQueue( BRMQ_BACKGROUND, loadCbId );
		m_loadCbId = 0;
		m_isLoading = FALSE;
	}

	uint32_t prepareCbId = m_prepareCbId;
	if( prepareCbId )
	{
		BeResMan->CancelFromQueue( BRMQ_MAIN, prepareCbId );
		m_prepareCbId = 0;
		m_isLoading = FALSE;
	}

	DoCloseStream();
}

void BlueAsyncRes::AddNotifyTarget( IBlueAsyncResNotifyTarget *p )
{
	CCP_ASSERT( p );

	for( size_t i = 0; i < m_reloadNotifyTargetsCount; ++i )
	{
		if( m_reloadNotifyTargets[i] == p )
		{
			CCP_LOGERR( "%s - target already registered", __FUNCTION__ );
			return;
		}
	}

	if( m_isPrepared )
	{
		p->RebuildCachedData( this );
	}

	IBlueAsyncResNotifyTarget** newBlock = (IBlueAsyncResNotifyTarget**)CCP_MALLOC( "BlueAsyncRes/m_reloadNotifyTargets", (1 + m_reloadNotifyTargetsCount) * sizeof(IBlueAsyncResNotifyTarget **) );
	memcpy( newBlock, m_reloadNotifyTargets, m_reloadNotifyTargetsCount * sizeof(IBlueAsyncResNotifyTarget **) );
	newBlock[m_reloadNotifyTargetsCount] = p;
	CCP_FREE( m_reloadNotifyTargets );
	m_reloadNotifyTargets = newBlock;
	++m_reloadNotifyTargetsCount;
}

void BlueAsyncRes::RemoveNotifyTarget( IBlueAsyncResNotifyTarget *p )
{
	CCP_ASSERT( p );
	CCP_ASSERT( m_reloadNotifyTargetsCount > 0 );

	bool found = false;
	IBlueAsyncResNotifyTarget** newBlock = NULL;
	if( m_reloadNotifyTargetsCount > 0 )
	{
		for( size_t i = 0; i < m_reloadNotifyTargetsCount; ++i )
		{
			if( m_reloadNotifyTargets[i] == p )
			{
				found = true;
				break;
			}
		}

		if( found && m_reloadNotifyTargetsCount > 1)
		{
			newBlock = (IBlueAsyncResNotifyTarget**)CCP_MALLOC( "BlueAsyncRes/m_reloadNotifyTargets", (m_reloadNotifyTargetsCount - 1) * sizeof(IBlueAsyncResNotifyTarget **) );
			size_t copied = 0;
			for( size_t i = 0; i < m_reloadNotifyTargetsCount; ++i )
			{
				if( m_reloadNotifyTargets[i] != p )
				{
					newBlock[copied] = m_reloadNotifyTargets[i];
					++copied;
				}
			}
		}
	}

	if( found )
	{
		CCP_FREE( m_reloadNotifyTargets );
		m_reloadNotifyTargets = newBlock;
		--m_reloadNotifyTargetsCount;
	}
	else
	{
		CCP_LOGERR( "%s - target not registered", __FUNCTION__ );
	}
}

void BlueAsyncRes::ForceSynchronousLoad()
{
	m_isForcedSynchronous = true;
}

void BlueAsyncRes::NotifyRebuildCachedData()
{
	for( size_t i = 0; i < m_reloadNotifyTargetsCount; ++i )
	{
		m_reloadNotifyTargets[i]->RebuildCachedData( this );
	}
}

void BlueAsyncRes::NotifyReleaseCachedData()
{
	for( size_t i = 0; i < m_reloadNotifyTargetsCount; ++i )
	{
		m_reloadNotifyTargets[i]->ReleaseCachedData( this );
	}
}

void BlueAsyncRes::Reload()
{
	CancelPendingLoad();
	NotifyReleaseCachedData();
	Initialize( m_path.c_str(), m_ext.c_str() );
}
