////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#include "BlueFileStream.h"
#include "BlueMemStream.h"
#include "BlueResManBackgroundCall.h"
#include "include/IBlueOS.h"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <share.h>
#elif !defined( __ORBIS__ )
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#if CCP_STACKLESS
#include "CcpUtils/PyCpp.h"
#define STACKLESS_ALLOWTHREADS() Ccp::PyAllowThreads allowThreads( true )
#else
#define STACKLESS_ALLOWTHREADS()
#endif
#include "Include/IBlueThreadMonitor.h"

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

#ifdef _WIN32

	int ConvertShareMode( BlueFileStream::ShareMode shareMode )
	{
		int shflag = 0;

		switch( shareMode )
		{
		case BlueFileStream::SM_NOSHARING:
			shflag = _SH_DENYRW;
			break;

		case BlueFileStream::SM_READSHARING:
			shflag = _SH_DENYWR;
			break;

		case BlueFileStream::SM_RWSHARING:
			shflag = _SH_DENYNO;
			break;
		}

		return shflag;
	}


	int ConvertOpenMode( BlueFileStream::OpenMode mode )
	{
		int oflag = 0;

		switch( mode )
		{
		case BlueFileStream::OM_READONLY:
			oflag = _O_RDONLY;
			break;

		case BlueFileStream::OM_READWRITE:
			oflag = _O_CREAT | _O_RDWR;
			break;
		}

		return oflag;
	}

	int OpenFile( const wchar_t* filename, BlueFileStream::OpenMode mode, BlueFileStream::ShareMode shareMode  )
	{
		int oflag = ConvertOpenMode( mode );
		int shflag = ConvertShareMode( shareMode );

		int fd;
		errno_t error = _wsopen_s( &fd, filename, oflag | _O_BINARY, shflag, _S_IREAD | _S_IWRITE );

		return fd;
	}

	int CreateFile( const wchar_t* filename, BlueFileStream::ShareMode shareMode )
	{
		int fd;

		int shflag = ConvertShareMode( shareMode );
		errno_t error = _wsopen_s( &fd, filename, _O_CREAT| _O_TRUNC | O_RDWR | _O_BINARY, shflag, _S_IREAD | _S_IWRITE );

		return fd;
	}

	void CloseFile( int fd )
	{
		_close( fd );
	}

	ssize_t ReadFromFile( int fd, void* buf, size_t numBytes )
	{
		return _read( fd, buf, (unsigned int)numBytes );
	}

	ssize_t WriteToFile( int fd, const void* buf, size_t numBytes )
	{
		return _write( fd, buf, (unsigned int)numBytes );
	}

	off_t Lseek( int fd, off_t offset, int whence )
	{
		return _lseek( fd, offset, whence );
	}

	off_t Tell( int fd )
	{
		return _tell( fd );
	}

#elif defined( __ORBIS__ )

	int OpenFile( BlueFileStream::OpenMode mode, const wchar_t* filename )
	{
		int oflag;
		if( mode == BlueFileStream::OM_READONLY )
		{
			oflag = SCE_KERNEL_O_RDONLY;
		}
		else
		{
			oflag = SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC;
		}

		std::string filenameA;
		filenameA = CW2A( filename );
		int fd = sceKernelOpen( filenameA.c_str(), oflag, SCE_KERNEL_S_IRWU );

		return fd;
	}

	int CreateFile( const wchar_t* filename )
	{
		std::string filenameA;
		filenameA = CW2A( filename );
		int fd = sceKernelOpen( filenameA.c_str(), SCE_KERNEL_O_CREAT| SCE_KERNEL_O_TRUNC | SCE_KERNEL_O_RDWR, SCE_KERNEL_S_IRWU );

		return fd;
	}

	void CloseFile( int fd )
	{
		sceKernelClose( fd );
	}

	ssize_t ReadFromFile( int fd, void* buf, size_t numBytes )
	{
		return sceKernelRead( fd, buf, numBytes );
	}

	ssize_t WriteToFile( int fd, const void* buf, size_t numBytes )
	{
		return sceKernelWrite( fd, buf, numBytes );
	}

	off_t Lseek( int fd, off_t offset, int whence )
	{
		return sceKernelLseek( fd, offset, whence );
	}

	off_t Tell( int fd )
	{
		return sceKernelLseek( fd, 0, SCE_KERNEL_SEEK_CUR );
	}

	int ConvertShareMode( BlueFileStream::ShareMode shareMode, int shflag );

#else
	int ConvertShareMode( BlueFileStream::ShareMode shareMode )
	{
		int shflag = 0;
#ifndef __ANDROID__
		switch( shareMode )
		{
		case BlueFileStream::SM_NOSHARING:
			shflag = O_EXLOCK;
			break;

		case BlueFileStream::SM_READSHARING:
			shflag = O_SHLOCK;
			break;

		case BlueFileStream::SM_RWSHARING:
			shflag = 0;
			break;
		}
#endif
		return shflag;
	}

	int ConvertOpenMode( BlueFileStream::OpenMode mode )
	{
		int oflag = 0;

		switch( mode )
		{
		case BlueFileStream::OM_READONLY:
			oflag = O_RDONLY;
			break;

		case BlueFileStream::OM_READWRITE:
			oflag = O_CREAT | O_RDWR;
			break;
		}

		return oflag;
	}


	int OpenFile( const wchar_t* filename, BlueFileStream::OpenMode mode, BlueFileStream::ShareMode shareMode )
	{
		int oflag = ConvertOpenMode( mode );
		int shflag = ConvertShareMode( shareMode );
		int fd = open( CW2A( filename ), oflag | shflag, S_IRUSR | S_IWUSR );
        
		return fd;
	}
    
	int CreateFile( const wchar_t* filename )
	{
		int fd = open( CW2A( filename ), O_CREAT| O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR );
        
		return fd;
	}
    
	int CreateFile( const wchar_t* filename, BlueFileStream::ShareMode shareMode )
	{
		int shflag = ConvertShareMode( shareMode );
		int fd = open( CW2A( filename ), O_CREAT| O_TRUNC | O_RDWR | shflag, S_IRUSR | S_IWUSR );
        
		return fd;
	}
    
	void CloseFile( int fd )
	{
		close( fd );
	}
    
	ssize_t ReadFromFile( int fd, void* buf, size_t numBytes )
	{
		return read( fd, buf, (unsigned int)numBytes );
	}
    
	ssize_t WriteToFile( int fd, const void* buf, size_t numBytes )
	{
		return write( fd, buf, (unsigned int)numBytes );
	}
    
	off_t Lseek( int fd, off_t offset, int whence )
	{
		return lseek( fd, offset, whence );
	}
    
	off_t Tell( int fd )
	{
		return lseek( fd, 0, SEEK_CUR );
	}
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

bool BlueFileStream::Open( const wchar_t* filename, OpenMode mode, ShareMode shareMode )
{
	STACKLESS_ALLOWTHREADS();

	ScopedThreadStatus threadStatus( IBlueThreadMonitor::BTS_LOADING );

	m_fileDescriptor = OpenFile( filename, mode, shareMode );

	if( m_fileDescriptor == INVALID_FILE )
	{
		BeOS->SetError(BE32, Clsid(), "Open failed on \"%S\"", filename );
		return false;
	}

	return true;
}

bool BlueFileStream::Create( const wchar_t* filename )
{
	STACKLESS_ALLOWTHREADS();

	m_fileDescriptor = CreateFile( filename, SM_NOSHARING );

	if( m_fileDescriptor == INVALID_FILE )
	{
		BeOS->SetError(BE32, Clsid(), "Create failed on \"%S\"", filename );
		return false;
	}

	return true;
}


void BlueFileStream::Close()
{
	if( m_fileDescriptor != INVALID_FILE )
	{
		CloseFile( m_fileDescriptor );
	}

	m_fileDescriptor = INVALID_FILE;
}

ptrdiff_t BlueFileStream::Read( void* dest, ptrdiff_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	STACKLESS_ALLOWTHREADS();

	ScopedThreadStatus threadStatus( IBlueThreadMonitor::BTS_LOADING );

	if( count == -1 )
	{
		// Count of -1 is taken to mean the remainder of the file
		count = INT_MAX;
	}

	ssize_t bytesRead;
	bytesRead = ReadFromFile( m_fileDescriptor, dest, count );

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

	if( !m_fileDescriptor )
	{
		BeOS->SetError(BE32, Clsid(), "Couldn't Write - file is not open");
		return -1;
	}

	STACKLESS_ALLOWTHREADS();

	ssize_t wrote;
	wrote = WriteToFile( m_fileDescriptor, source, count );

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

	if( Lseek( m_fileDescriptor, (long)distance, method ) )
	{
		return false;
	}

	return true;
}

ptrdiff_t BlueFileStream::GetPosition()
{
	long pos = Tell( m_fileDescriptor );
	return pos;
}

ptrdiff_t BlueFileStream::GetSize()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( !m_fileDescriptor )
	{
		return -1;
	}

	long curPos = Tell( m_fileDescriptor );
	if( Lseek( m_fileDescriptor, 0, SEEK_END ) == -1 )
	{
		BeOS->SetError( BEDEF, Clsid(), "Couldn't get file size" );
		return -1;
	}
	long size = Tell( m_fileDescriptor );
	if( Lseek( m_fileDescriptor, curPos, SEEK_SET ) == -1 )
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

	STACKLESS_ALLOWTHREADS();

	m_fileDescriptor = OpenFile( filename, OM_READONLY, SM_READSHARING );

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

		if( Be::IsSuccess( result ) )
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