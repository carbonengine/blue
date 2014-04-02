////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#if USE_RESFILE_2

#include "BlueRemoteStream.h"
#include "include/IBlueOS.h"

#if CCP_STACKLESS
#include "CcpUtils/PyCpp.h"
#endif
#include "md5.h"

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "RemoteStream" );

BlueRemoteStream::BlueRemoteStream() :
	m_curl( nullptr ),
	m_data( nullptr ),
	m_readLocation( nullptr ),
	m_dataSize( 0 )
{
	InitializeCurl();
}

BlueRemoteStream::~BlueRemoteStream()
{
	if( m_data )
	{
		CCP_FREE( m_data );
	}
}

bool BlueRemoteStream::Open( const char* resUrl )
{
#if CCP_STACKLESS
	Ccp::PyAllowThreads allowThreads( true );
#endif
	CCP_LOG_CH( s_ch, "Opening %s", resUrl );

	m_curl = curl_easy_init();
	curl_easy_setopt( m_curl, CURLOPT_URL, resUrl );
	curl_easy_setopt( m_curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback );
	curl_easy_setopt( m_curl, CURLOPT_WRITEDATA, (void*)this );
	curl_easy_setopt( m_curl, CURLOPT_FAILONERROR, 1 );
	curl_easy_setopt( m_curl, CURLOPT_ACCEPT_ENCODING, "gzip" );

	CURLcode res = curl_easy_perform( m_curl );

	if( res != CURLE_OK )
	{
		CCP_LOGERR( "curl_easy_perform() failed: %s\n", curl_easy_strerror( res ) );
	}
	else
	{
		m_readLocation = m_data;
	}

	curl_easy_cleanup( m_curl );
	m_curl = nullptr;

	if( m_data )
	{
		if( strcmp( (const char*)m_data, "error" ) == 0 )
		{
			return false;
		}
	}

	return res == CURLE_OK;
}

ptrdiff_t BlueRemoteStream::Read( void* dest, ptrdiff_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );

#ifdef CCP_STACKLESS
	Ccp::PyAllowThreads allowThreads( true );
#endif

	if( count == -1 )
	{
		// Count of -1 is taken to mean the remainder of the file
		count = INT_MAX;
	}

	if( count < 0 )
	{
		return -1;
	}

	uint8_t* end = m_data + m_dataSize;

	if( m_readLocation >= end )
	{
		return 0;
	}

	ssize_t numLeft = end - m_readLocation;
	if( count > numLeft )
	{
		count = numLeft;
	}

	memcpy( dest, m_readLocation, count );
	m_readLocation += count;

	return count;
}

ptrdiff_t BlueRemoteStream::Write( const void* source, size_t count )
{
	return -1;
}

ptrdiff_t BlueRemoteStream::Seek( ptrdiff_t distance, SeekOrigin method )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( method == SO_BEGIN )
	{
		m_readLocation = m_data + distance;
	}
	else if( method == SO_CURRENT )
	{
		m_readLocation += distance;
	}
	else
	{
		m_readLocation = m_data + m_dataSize - distance;
	}

	if( m_readLocation > m_data + m_dataSize )
	{
		CCP_LOGERR( "Seeking past EOF" );
		m_readLocation = m_data + m_dataSize;
		return -1;
	}

	return true;
}

bool BlueRemoteStream::SetSize( size_t newsize )
{
	return false;
}

ssize_t BlueRemoteStream::CopyFrom( IBlueStream* source, size_t count )
{
	return -1;
}

ptrdiff_t BlueRemoteStream::GetPosition()
{
	return m_readLocation - m_data;
}

ptrdiff_t BlueRemoteStream::GetSize()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	return m_dataSize;
}

bool BlueRemoteStream::LockData( void** data, size_t size )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_data && m_dataSize && ((size == m_dataSize) || (size == 0)) )
	{
		*data = m_data;
		return true;
	}

	return false;
}

bool BlueRemoteStream::UnlockData()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	return true;
}

size_t BlueRemoteStream::WriteMemoryCallback( void* contents, size_t size, size_t nmemb, void* context )
{
	size_t realsize = size * nmemb;
	BlueRemoteStream* pThis = reinterpret_cast<BlueRemoteStream*>( context );
	pThis->ReceiveData( contents, realsize );
	return realsize;
}

void BlueRemoteStream::ReceiveData( void* data, size_t size )
{
	size_t newSize = m_dataSize + size;
	m_data = reinterpret_cast<uint8_t*>( CCP_REALLOC( "BlueRemoteStream", m_data, newSize ) );
	memcpy( m_data + m_dataSize, data, size );
	m_dataSize = newSize;
}

void BlueRemoteStream::InitializeCurl()
{
	static bool isInitialized = false;
	if( !isInitialized )
	{
		curl_global_init( CURL_GLOBAL_DEFAULT );
		isInitialized = true;
	}
}

bool BlueRemoteStream::VerifyContents( const char* expectedChecksum )
{
	MD5 checkSum;
	checkSum.update( reinterpret_cast<unsigned char*>( m_data ), (unsigned int)m_dataSize );
	checkSum.finalize();
	const char* checkSumAsHex = checkSum.hex_digest();
	if( strcmp( expectedChecksum, checkSumAsHex ) != 0 )
	{
		return false;
	}

	return true;
}

#endif