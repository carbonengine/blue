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
#include "include/CcpStatistics.h"

#if CCP_STACKLESS
#include "CcpUtils/PyCpp.h"
#endif

BlueRemoteStream::BlueRemoteStream() :
	m_curl( nullptr ),
	m_data( nullptr ),
	m_readLocation( nullptr ),
	m_dataSize( 0 )
{
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

	m_curl = curl_easy_init();
	curl_easy_setopt( m_curl, CURLOPT_URL, resUrl );
	curl_easy_setopt( m_curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback );
	curl_easy_setopt( m_curl, CURLOPT_WRITEDATA, (void*)this );
	curl_easy_setopt( m_curl, CURLOPT_FAILONERROR, 1 );

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

ssize_t BlueRemoteStream::Read( void* dest, ssize_t count )
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

ssize_t BlueRemoteStream::Write( const void* source, size_t count )
{
	return -1;
}

ssize_t BlueRemoteStream::Seek( ssize_t distance, BLUESEEK method )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( method == BS_BEGIN )
	{
		m_readLocation = m_data + distance;
	}
	else if( method == BS_CURRENT )
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

ssize_t BlueRemoteStream::GetPosition()
{
	return m_readLocation - m_data;
}

ssize_t BlueRemoteStream::GetSize()
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

#endif