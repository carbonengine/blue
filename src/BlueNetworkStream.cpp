// Copyright © 2015 CCP ehf.

#include "StdAfx.h"
#include "BlueNetworkStream.h"
#include "IBlueResMan.h"
#include "BlueRemoteStream.h"
#include <BlueStatistics.h>


BlueNetworkStream::BlueNetworkStream()
	:m_data( "BlueHttpStream::m_data" ),
	m_blockWritePosition( 0 ),
	m_blockReadPosition( 0 ),
	m_readPosition( 0 ),
	m_size( 0 ),
	m_receivedSize( 0 ),
	m_sizeAvailable( 0, 1 ),
	m_dataMutex( "BlueHttpStream", "m_dataMutex" ),
	m_dataAvailable( 0, 1 ),
	m_backgroundCallId( 0 ),
	m_state( UNINITIALIZED ),
	m_transferResult( CURLE_OK )
{
}

BlueNetworkStream::~BlueNetworkStream()
{
	Close();
}

BlueStdResult BlueNetworkStream::Open( const char* url )
{
	Close();
	m_state = OPENED;
	m_url = url;
	BeResMan->AddToQueue( 
		BRMQ_BACKGROUND, 
		&PerformTransferHelper, 
		this, 
		0, 
		&m_backgroundCallId );
	return BlueStdResult( BLUE_STD_RESULT_OK );
}

void BlueNetworkStream::Close()
{
	if( m_state == UNINITIALIZED )
	{
		return;
	}
	m_state = FINISHED;
	m_dataAvailable.Signal();
	BeResMan->CancelFromQueue( BRMQ_BACKGROUND, m_backgroundCallId );

	m_url = "";
	m_blockReadPosition = 0;
	m_blockWritePosition = 0;

	for( auto it = m_data.begin(); it != m_data.end(); ++it )
	{
		CCP_DELETE[] it->data;
	}
	m_data.clear();

	m_readPosition = 0;
	m_size = 0;
	m_receivedSize = 0;
	m_headers = "";
	m_state = UNINITIALIZED;
}

ptrdiff_t BlueNetworkStream::Read( void* dest, ptrdiff_t count )
{
	ptrdiff_t read = 0;
	uint8_t* data = static_cast<uint8_t*>( dest );
	while( count > 0 )
	{
		m_dataMutex.Acquire();
		size_t block = size_t( m_readPosition / BLOCK_SIZE );
		while( m_state == OPENED && ( m_data.size() <= block || ( m_data.size() == block + 1 && m_blockWritePosition == m_blockReadPosition ) ) )
		{
			m_dataMutex.Release();
			m_dataAvailable.Wait();
			m_dataMutex.Acquire();
		}
		ptrdiff_t availableSize = ( m_data.size() > block + 1 ? BLOCK_SIZE : m_blockWritePosition ) - m_blockReadPosition;
		if( m_data.empty() )
		{
			count = 0;
		}
		else if( availableSize )
		{
			auto copySize = std::min( availableSize, count );
			memcpy( data, m_data[block].data + m_blockReadPosition, copySize );
			count -= copySize;
			data += copySize;
			read += copySize;
			m_blockReadPosition += copySize;
			m_readPosition += copySize;
			if( m_blockReadPosition >= BLOCK_SIZE )
			{
				m_blockReadPosition = 0;
			}
		}
		else if( m_state != OPENED )
		{
			count = 0;
		}
		m_dataAvailable.Signal();
		m_dataMutex.Release();
	}
	return read;
}

ptrdiff_t BlueNetworkStream::Write( const void* source, size_t count )
{
	return -1;
}

ptrdiff_t BlueNetworkStream::Seek( ptrdiff_t distance, SeekOrigin method )
{
	switch( method )
	{
	case SO_CURRENT:
		distance = m_readPosition + distance;
	case SO_BEGIN:
		if( distance < m_readPosition )
		{
			m_dataMutex.Acquire();
			m_readPosition = distance;
			m_blockReadPosition = m_readPosition % BLOCK_SIZE;
			m_dataMutex.Release();
		}
		else
		{
			m_dataMutex.Acquire();
			while( ptrdiff_t( m_receivedSize ) < distance )
			{
				m_dataMutex.Release();
				m_dataAvailable.Wait();
				m_dataMutex.Acquire();
			}
			m_readPosition = distance;
			m_blockReadPosition = m_readPosition % BLOCK_SIZE;
			m_dataMutex.Release();
		}
		m_dataAvailable.Signal();
		return m_readPosition;
	default:
		return -1;
	}
}

ptrdiff_t BlueNetworkStream::GetPosition()
{
	return m_readPosition;
}

ptrdiff_t BlueNetworkStream::GetSize()
{
	m_sizeAvailable.Wait();
	return m_size;
}

bool BlueNetworkStream::LockData( void** data,	size_t size	)
{
	return false;
}

bool BlueNetworkStream::UnlockData()
{
	return false;
}

BlueStdResult BlueNetworkStream::ReadData( Be::Optional<size_t> size, PyObject*& contents )
{
	if( m_state == UNINITIALIZED )
	{
		return BlueStdResult( BLUE_STD_RESULT_VALUE_ERROR, "I/O operation on closed file" );
	}
	if( size.IsAssigned() )
	{
		if( size == 0 )
		{
			contents = PyBytes_FromString("");
			return BlueStdResult( BLUE_STD_RESULT_OK );
		}
		CcpMallocBuffer data( "BlueHttpStream::ReadData", size );
		auto readSize = Read( data.get(), size );
		if( m_transferResult != CURLE_OK )
		{
			return BlueStdResult( BLUE_STD_RESULT_IO_ERROR );
		}
		contents = PyBytes_FromStringAndSize( data.get(), readSize );
		return BlueStdResult( BLUE_STD_RESULT_OK );
	}
	else
	{
		contents = PyBytes_FromString("");
		while( true )
		{
			CcpMallocBuffer data( "BlueHttpStream::ReadData", 1024 );
			auto read = Read( data.get(), ptrdiff_t( data.size() ) );
			PyBytes_ConcatAndDel( &contents, PyBytes_FromStringAndSize( data.get(), read ) );
			if ( !contents ) {
				return BlueStdResult( BLUE_STD_RESULT_RUNTIME_ERROR );
			}
			if( read != 1024 )
			{
				break;
			}
		}
		if( m_transferResult != CURLE_OK )
		{
			return BlueStdResult( BLUE_STD_RESULT_IO_ERROR );
		}
		return BlueStdResult( BLUE_STD_RESULT_OK );
	}
}

BlueStdResult BlueNetworkStream::Tell( ptrdiff_t& position )
{
	if( m_state == UNINITIALIZED )
	{
		return BlueStdResult( BLUE_STD_RESULT_VALUE_ERROR, "I/O operation on closed file" );
	}
	position = GetPosition();
	return BlueStdResult( BLUE_STD_RESULT_OK );
}

void BlueNetworkStream::PerformTransfer()
{
	CURL* connection = curl_easy_init();
	curl_easy_setopt( connection, CURLOPT_FAILONERROR, 1 );
	curl_easy_setopt( connection, CURLOPT_URL, m_url.c_str() );
	curl_easy_setopt( connection, CURLOPT_WRITEFUNCTION, WriteMemoryCallback );
	curl_easy_setopt( connection, CURLOPT_WRITEDATA, (void*)this );
	curl_easy_setopt( connection, CURLOPT_HEADERFUNCTION, WriteHeaderCallback );
	curl_easy_setopt( connection, CURLOPT_HEADERDATA, (void*)this );
#if LIBCURL_VERSION_MAJOR > 7 || LIBCURL_VERSION_MINOR >= 32
	curl_easy_setopt( connection, CURLOPT_XFERINFOFUNCTION, ProgressCallback );
	curl_easy_setopt( connection, CURLOPT_XFERINFODATA, (void*)this );
#else
	curl_easy_setopt( connection, CURLOPT_PROGRESSFUNCTION, ProgressCallback );
	curl_easy_setopt( connection, CURLOPT_PROGRESSDATA, (void*)this );
#endif
	GetProxySettings( m_url.c_str(), connection );

	m_transferResult = curl_easy_perform( connection );
	m_size = m_receivedSize;
	m_state = FINISHED;
	m_sizeAvailable.Signal();
	m_dataAvailable.Signal();
}

void BlueNetworkStream::PerformTransferHelper( void* context )
{
	static_cast<BlueNetworkStream*>( context )->PerformTransfer();
}

size_t BlueNetworkStream::WriteMemoryCallback( void* contents, size_t size, size_t nmemb, void* context )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	size_t realsize = size * nmemb;
	auto self = static_cast<BlueNetworkStream*>( context );
	self->ReceiveData( contents, realsize );
	return realsize;
}

size_t BlueNetworkStream::WriteHeaderCallback( void* contents, size_t size, size_t nmemb, void* context )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	size_t realsize = size * nmemb;
	auto self = static_cast<BlueNetworkStream*>( context );
	self->ReceiveHeaders( static_cast<char*>( contents ), realsize );
	return realsize;
}

void BlueNetworkStream::ReceiveData( const void* data, size_t size )
{
	size_t initialSize = size;
	CcpAutoMutex lock( m_dataMutex );

	auto begin = static_cast<const uint8_t*>( data );
	while( size )
	{
		if( m_data.empty() )
		{
			Block newBlock;
			newBlock.data = CCP_NEW( "BlueHttpStream::Block::data" ) uint8_t[BLOCK_SIZE];
			m_data.push_back( newBlock );
			m_blockWritePosition = 0;
		}
		auto blockSize = std::min( BLOCK_SIZE - m_blockWritePosition, size );
		auto& block = m_data.back();
		memcpy( block.data + m_blockWritePosition, begin, blockSize );
		if( blockSize >= size )
		{
			m_blockWritePosition += size;
			size = 0;
		}
		else
		{
			begin += blockSize;
			size -= blockSize;
			Block newBlock;
			newBlock.data = CCP_NEW( "BlueHttpStream::Block::data" ) uint8_t[BLOCK_SIZE];
			m_data.push_back( newBlock );
			m_blockWritePosition = 0;
		}
	}
	m_receivedSize += initialSize;
	m_dataAvailable.Signal();
}

void BlueNetworkStream::ReceiveHeaders( const void* data, size_t size )
{
	m_headers.append( static_cast<const char*>( data ), size );
	auto found = m_headers.find( "Content-Length: " );
	if( found != m_headers.npos )
	{
		if( sscanf_s( m_headers.c_str() + found, "%" CCP_SIZET_FORMAT, &size ) == 1 )
		{
			m_size = size;
			m_sizeAvailable.Signal();
		}
	}
}

#if LIBCURL_VERSION_MAJOR > 7 || LIBCURL_VERSION_MINOR >= 32

int BlueNetworkStream::ProgressCallback( void *context, curl_off_t, curl_off_t, curl_off_t, curl_off_t )
{
	return static_cast<BlueNetworkStream*>( context )->m_state != OPENED;
}

#else

int BlueNetworkStream::ProgressCallback( void *context, double, double, double ultotal, double ulnow )
{
	return static_cast<BlueNetworkStream*>( context )->m_state != OPENED;
}

#endif
