////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#include "BlueResFile2.h"
#include "Stuffer.h"
#include "BlueFileStream.h"
#include "BlueRemoteStream.h"
#include "include/IBlueOS.h"
#include "include/IBluePaths.h"

#if BLUE_WITH_PYTHON
#ifdef _WIN32
#include "CcpUtils/PyCpp.h"
#endif
#endif
#include "Include/BlueFileUtil.h"
#include "RemoteFileCache.h"

extern Stuffer* BeStuffer;

bool RESFILE_2_CLASSNAME::FileExists( const wchar_t* filename )
{
	return BePaths->FileExists( filename );
}

bool RESFILE_2_CLASSNAME::Open( const char* filename, bool readOnly )
{
	return OpenW(CA2W(filename), readOnly);
}


bool RESFILE_2_CLASSNAME::Close()
{
	if( !m_stream )
	{
		return false;
	}

	m_stream.Unlock();

	return true;
}

bool RESFILE_2_CLASSNAME::OpenW( const wchar_t* filename, bool readOnly )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	bool isRes = !wcsncmp(filename, L"res:", 4);

	std::wstring filenameToOpen = filename;
	std::wstring languageSpecificFilename = filename;

	bool tryLang = false; //no separate language try
	if( isRes )
	{
		tryLang = AdjustFilenameForLanguageCode( filenameToOpen, languageSpecificFilename );
	}

	if( tryLang )
	{
		if( BePaths->FileExists( languageSpecificFilename ) )
		{
			filenameToOpen = languageSpecificFilename;
		}
	}

#if STUFFER_ENABLED
	bool tryStuffer = isRes && readOnly; //stuffers are only for readonly
	if( tryStuffer )
	{
		if( BeStuffer->GetStream( filenameToOpen.c_str(), &m_stream ) )
		{
			return true;
		}
	}
#endif

#if USE_RESFILE_2
	if( readOnly )
	{
		auto result = BeRemoteFileCache->GetStreamFromPathW( filename, &m_stream );
		if( Be::IsSuccess( result ) )
		{
			return true;
		}
		else
		{
			CCP_LOGERR( result.value.c_str() );
		}
	}
#endif

	std::wstring filenameOnDisk = BePaths->ResolvePathW( filenameToOpen );

	BlueFileStreamPtr fileStream;
	fileStream.CreateInstance();
	m_stream = fileStream;

	return fileStream->Open( filenameOnDisk.c_str(), readOnly ? BlueFileStream::OM_READONLY : BlueFileStream::OM_READWRITE );
}

bool RESFILE_2_CLASSNAME::CreateW( const wchar_t* filename )
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
	m_stream = fileStream;

	return fileStream->Create( filenameOnDisk.c_str() );
}

bool RESFILE_2_CLASSNAME::FileExistsW( const wchar_t* filename )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	return BePaths->FileExists( filename );
}

bool RESFILE_2_CLASSNAME::Preload( bool & )
{
	return false;
}

bool RESFILE_2_CLASSNAME::PreloadInProgress()
{
	return false;
}

ssize_t RESFILE_2_CLASSNAME::Read( void* dest, ssize_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->Read( dest, count );
	}

	return -1;
}

ssize_t RESFILE_2_CLASSNAME::Write( const void* source, size_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->Write( source, count );
	}

	return -1;
}

ssize_t RESFILE_2_CLASSNAME::Seek( ssize_t distance, BLUESEEK method )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->Seek( distance, method );
	}

	return -1;
}

ssize_t RESFILE_2_CLASSNAME::GetPosition()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->GetPosition();
	}

	return -1;
}

ssize_t RESFILE_2_CLASSNAME::GetSize()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->GetSize();
	}

	return -1;
}

bool RESFILE_2_CLASSNAME::LockData( void** data, size_t size )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->LockData( data, size );
	}

	return false;
}

bool RESFILE_2_CLASSNAME::UnlockData()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_stream )
	{
		return m_stream->UnlockData();
	}

	return false;
}
