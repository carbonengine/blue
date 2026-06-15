// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#include "BlueFileStream.h"
#include "BlueMemStream.h"
#include "BlueResManBackgroundCall.h"
#include <BlueStatistics.h>
#include "IBlueOS.h"

#if CCP_STACKLESS
#include "BluePyCpp.h"
#define PY_ALLOWTHREADS() Ccp::PyAllowThreads allowThreads( true )
#else
#define PY_ALLOWTHREADS()
#endif
#include "IBlueThreadMonitor.h"

CCP_STATS_DECLARE( fileStreamBytesRead, "Blue/BlueFileStream/BytesRead", false, CST_COUNTER_HIGH, "Number of bytes read from local files using BlueFileStream" );
CCP_STATS_DECLARE( fileStreamBytesWritten, "Blue/BlueFileStream/BytesWritten", false, CST_COUNTER_HIGH, "Number of bytes written from local files using BlueFileStream" );

namespace
{
	const int INVALID_FILE = -1;

#if CCP_STACKLESS
	class BackgroundReader : public IBlueResManBackgroundCall
	{
	public:
		BackgroundReader( BlueFileStream* stream, const wchar_t* filename ) :
		  m_stream( stream ),
			  m_filename( filename )
		  {
		  }

		  virtual ~BackgroundReader() = default;

		  virtual void Perform()
		  {
			  m_result = m_stream->ReadEntireFile( m_filename.c_str(), m_contents );
		  }

		  Be::Result<std::string> GetResult()
		  {
			  return m_result;
		  }

		  const std::string& GetContents()
		  {
			  return m_contents;
		  }

	private:
		std::wstring m_filename;
		BlueFileStreamPtr m_stream;
		std::string m_contents;
		Be::Result<std::string> m_result;
	};
#endif

}

BlueFileStream::BlueFileStream() :
	m_fileDescriptor( INVALID_FILE ),
	m_data( nullptr ),
	m_dataSize( 0 )
{
}

BlueFileStream::~BlueFileStream()
{
	if( m_data )
	{
		UnlockData();
	}

	Close();
}

bool BlueFileStream::Open( const wchar_t* filename, CcpOpenMode mode, CcpShareMode shareMode )
{
	PY_ALLOWTHREADS();

	ScopedThreadStatus threadStatus( IBlueThreadMonitor::BTS_LOADING );

	m_fileDescriptor = CcpOpenFile( filename, mode, shareMode );

	if( m_fileDescriptor == INVALID_FILE )
	{
		return false;
	}

	return true;
}

bool BlueFileStream::Create( const wchar_t* filename )
{
	PY_ALLOWTHREADS();

	m_fileDescriptor = CcpCreateFile( filename, CCP_SM_NOSHARING );

	if( m_fileDescriptor == INVALID_FILE )
	{
		return false;
	}

	return true;
}


void BlueFileStream::Close()
{
	if( m_fileDescriptor != INVALID_FILE )
	{
		CcpCloseFile( m_fileDescriptor );
	}

	m_fileDescriptor = INVALID_FILE;
}

ptrdiff_t BlueFileStream::Read( void* dest, ptrdiff_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	PY_ALLOWTHREADS();

	ScopedThreadStatus threadStatus( IBlueThreadMonitor::BTS_LOADING );

	if( count == -1 )
	{
		// Count of -1 is taken to mean the remainder of the file
		count = INT_MAX;
	}

	ssize_t bytesRead;
	bytesRead = CcpReadFromFile( m_fileDescriptor, dest, count );

	if( bytesRead == -1 )
	{
		BeOS->SetError(BE32, Clsid(), "Couldn't Read");
		return -1;
	}

	CCP_STATS_ADD( fileStreamBytesRead, bytesRead );

	return bytesRead;
}

ptrdiff_t BlueFileStream::Write( const void* source, size_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_fileDescriptor == INVALID_FILE )
	{
		BeOS->SetError(BE32, Clsid(), "Couldn't Write - file is not open");
		return -1;
	}

	PY_ALLOWTHREADS();

	ssize_t wrote;
	wrote = CcpWriteToFile( m_fileDescriptor, source, count );

	if( wrote == -1 )
	{
		BeOS->SetError(BE32, Clsid(), "Couldn't Write");
		return -1;
	}
	CCP_STATS_ADD( fileStreamBytesWritten, wrote );

	return wrote;
}

ptrdiff_t BlueFileStream::Seek( ptrdiff_t distance, SeekOrigin method )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( CcpLseek( m_fileDescriptor, (long)distance, method ) )
	{
		return false;
	}

	return true;
}

ptrdiff_t BlueFileStream::GetPosition()
{
	long pos = CcpTell( m_fileDescriptor );
	return pos;
}

ptrdiff_t BlueFileStream::GetSize()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_fileDescriptor == INVALID_FILE )
	{
		return -1;
	}

	long curPos = CcpTell( m_fileDescriptor );
	if( CcpLseek( m_fileDescriptor, 0, SEEK_END ) == -1 )
	{
		BeOS->SetError( BEDEF, Clsid(), "Couldn't get file size" );
		return -1;
	}
	long size = CcpTell( m_fileDescriptor );
	if( CcpLseek( m_fileDescriptor, curPos, SEEK_SET ) == -1 )
	{
		BeOS->SetError( BEDEF, Clsid(), "Couldn't get file size" );
		return -1;
	}
	return size;
}

bool BlueFileStream::LockData( void** data, size_t size )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_data )
	{
		return false;
	}

	m_dataSize = GetSize();
	if( !m_dataSize )
	{
		return false;
	}

	m_data = CCP_MALLOC( "BlueFileStream/m_data", m_dataSize );
	if( !m_data )
	{
		return false;
	}

	if( !Seek( 0, SO_BEGIN ) )
	{
		return false;
	}

	if( Read( m_data, m_dataSize ) != m_dataSize )
	{
		m_dataSize = 0;
		CCP_FREE( m_data );
		m_data = NULL;
		return false;
	}

	*data = m_data;
	return true;

}

bool BlueFileStream::UnlockData()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( !m_data )
	{
		return false;
	}

	m_dataSize = 0;
	CCP_FREE( m_data );
	m_data = NULL;

	return true;
}

Be::Result<std::string> BlueFileStream::ReadEntireFile( const wchar_t* filename, std::string& contents )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	PY_ALLOWTHREADS();

	m_fileDescriptor = CcpOpenFile( filename, CCP_OM_READONLY, CCP_SM_RWSHARING );

	if( m_fileDescriptor == INVALID_FILE )
	{
		return Be::Result<std::string>( "Open failed" );
	}

	ssize_t size = GetSize();
	if( size < 0 )
	{
		return Be::Result<std::string>( "GetSize failed" );
	}

	contents.resize( size );

	ssize_t bytesRead = Read( &contents[0], size );

	Close();

	if( bytesRead < size )
	{
		return Be::Result<std::string>( "Read failed" );
	}

	return Be::Result<std::string>();
}


Be::Result<std::string> BlueFileStream::ReadEntireFileWithYield( const wchar_t* filename, std::string& contents )
{
	CCP_STATS_ZONE( __FUNCTION__ );

#if CCP_STACKLESS
	BackgroundReader* reader = CCP_NEW( "ReadEntireFileWithYield/reader" )
		BackgroundReader( this, filename );
	Be::Result<std::string> result;


	if( BlueResManBackgroundCall::Issue( reader ) )
	{
		result = reader->GetResult();

		if( BeIsSuccess( result ) )
		{
			contents = reader->GetContents();
		}
	}
	else
	{
		result = Be::Result<std::string>( "Tasklet killed" );
	}

	CCP_DELETE reader;
	return result;

#else

	return ReadEntireFile( filename, contents );

#endif
}
