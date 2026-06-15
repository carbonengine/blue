// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#include "BlueResFile2.h"
#include "BlueFileStream.h"
#include "BlueRemoteStream.h"
#include <BlueStatistics.h>
#include "IBlueOS.h"
#include "IBluePaths.h"

#include "BlueFileUtil.h"
#include "RemoteFileCache.h"

bool ResFile::FileExists( const wchar_t* filename )
{
	return BePaths->FileExists( filename );
}

bool ResFile::Open( const char* filename, bool readOnly )
{
	return OpenW(CA2W(filename), readOnly);
}


bool ResFile::Close()
{
	if( !m_stream )
	{
		return false;
	}

	m_stream.Unlock();

	return true;
}

bool ResFile::OpenW( const wchar_t* filename, bool readOnly )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( readOnly )
	{
		m_stream.Unlock();
		if( BePaths->GetStreamFromPathW( filename, &m_stream ) )
		{
			return true;
		}
	}

	std::wstring filenameOnDisk = BePaths->ResolvePathW( filename );

	BlueFileStreamPtr fileStream;
	fileStream.CreateInstance();
	m_stream = fileStream;

	CcpOpenMode openMode = readOnly ? CCP_OM_READONLY : CCP_OM_READWRITE;
	return fileStream->Open( filenameOnDisk.c_str(), openMode, CCP_SM_RWSHARING );
}

bool ResFile::CreateW( const wchar_t* filename )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	std::wstring filenameOnDisk = BePaths->ResolvePathForWritingW( filename );

	if( filenameOnDisk.empty() )
	{
		BeOS->SetError(BE32, Clsid(), "Create failed on \"%S\"", filename );
		return false;
	}

	BlueFileStreamPtr fileStream;
	fileStream.CreateInstance();

	if( !fileStream->Create( filenameOnDisk.c_str() ) )
	{
		BeOS->SetError(BE32, Clsid(), "Create failed on \"%S\"", filename);
		return false;
	}

	m_stream = fileStream;
	return true;
}

bool ResFile::FileExistsW( const wchar_t* filename )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	return BePaths->FileExists( filename );
}

bool ResFile::Preload( bool & )
{
	return false;
}

bool ResFile::PreloadInProgress()
{
	return false;
}

ptrdiff_t ResFile::Read( void* dest, ptrdiff_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->Read( dest, count );
	}

	return -1;
}

ptrdiff_t ResFile::Write( const void* source, size_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->Write( source, count );
	}

	return -1;
}

ptrdiff_t ResFile::Seek( ptrdiff_t distance, SeekOrigin method )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->Seek( distance, method );
	}

	return -1;
}

ptrdiff_t ResFile::GetPosition()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->GetPosition();
	}

	return -1;
}

ptrdiff_t ResFile::GetSize()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->GetSize();
	}

	return -1;
}

bool ResFile::LockData( void** data, size_t size )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->LockData( data, size );
	}

	return false;
}

bool ResFile::UnlockData()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->UnlockData();
	}

	return false;
}
