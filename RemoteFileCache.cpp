////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		October 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"

#include "RemoteFileCache.h"
#include "BlueFileStream.h"
#include "BlueRemoteStream.h"
#include "Include/IBluePaths.h"
#include "Include/BlueFileUtil.h"

#ifdef __ANDROID__
#include <errno.h>
#endif

CCP_STATS_DECLARE( remoteFileCacheGetStream, "Blue/RemoteFileCache/GetStreamFromPathW", false, CST_TIME, "Total time spent in RemoteFileCache::GetStreamFromPathW" );

namespace
{
	CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "RemoteFileCache" );

	void RenameFile( const std::wstring& src, const std::wstring& dst )
	{
		CCP_STATS_ZONE( __FUNCTION__ );

#ifdef _WIN32
		wchar_t srcBuffer[MAX_PATH];
		wchar_t dstBuffer[MAX_PATH];
		GetFullPathNameW( src.c_str(), MAX_PATH, srcBuffer, NULL );
		GetFullPathNameW( dst.c_str(), MAX_PATH, dstBuffer, NULL );

		if( !MoveFileW( srcBuffer, dstBuffer ) )
		{
			CCP_LOGWARN_CH( s_ch, "Couldn't rename file (%d)", GetLastError() );
		}
#else
        if( rename( CW2A( src.c_str() ), CW2A( dst.c_str() ) ) )
        {
			CCP_LOGWARN_CH( s_ch, "Couldn't rename file (%d)", errno );
        }
#endif
	}
}

RemoteFileCache::RemoteFileCache() :
	m_server( "http://127.0.0.1:5000" ),
	m_prefix( "/res/"),
	m_cacheFolder( L"cache:/ResFiles"),
	m_bytesDownloaded( 0 ),
	m_bytesCached( 0 ),
	m_filesDownloaded( 0 ),
	m_filesCached( 0 ),
	m_filesUsedFromCache( 0 )
{
}

bool RemoteFileCache::DownloadFileIndex( const std::string& index )
{
	std::string url = m_server;
	url += m_prefix;
	url += index;

	std::wstring cachedName = m_cacheFolder;
	cachedName += L"/";
	cachedName += CA2W( index.c_str() );

	IBlueStreamPtr stream;

	if( BePaths->FileExists( cachedName ) )
	{
		CCP_LOG_CH( s_ch, "Index %s exists locally", index.c_str() );
		CreateFileStreamForCachedFile( cachedName, &stream );
	}

	if( !stream )
	{
		CCP_LOG_CH( s_ch, "Downloading index from %s", url.c_str() );

		BlueRemoteStreamPtr remoteStream;
		remoteStream.CreateInstance();
		bool isOK = remoteStream->Open( url.c_str(), 0 );

		if(	!isOK )
		{
			return false;
		}

		CacheContentsOfRemoteStream( remoteStream, cachedName, L"remote file index" );

		stream = remoteStream;
	}

	if( !stream )
	{
		return false;
	}
	
	return SetFileIndexFromStream( stream );
}

void RemoteFileCache::SetCacheFolder( const wchar_t* folderName )
{
	m_cacheFolder = folderName;
}

Be::Result<std::string> RemoteFileCache::GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream )
{
	CCP_STATS_SCOPED_TIME( remoteFileCacheGetStream );

	*stream = nullptr;

	FileInfo info;
	if( !GetFileInfo( resPath, info ) )
	{
		return Be::Result<std::string>( "File does not exist on remote server" );
	}

	// Does file exist in the cache folder? If so, open it as a file stream and return it
	std::wstring resId = static_cast<const wchar_t*>( CA2W( info.cachedName.c_str() ) );
	std::wstring cachedName = m_cacheFolder;
	cachedName += L"/";
	cachedName += resId;

	if( BePaths->FileExistsLocally( cachedName.c_str() ) )
	{
		return CreateFileStreamForCachedFile(cachedName, stream);
	}

	// File does not exist in the cache folder - download and add to cache
	std::string resUrl = m_server;
	resUrl += m_prefix;
	resUrl += CW2A( resId.c_str() );

	BlueRemoteStreamPtr remoteStream;
	remoteStream.CreateInstance();
	bool isOK = remoteStream->Open( resUrl.c_str(), static_cast<size_t>( info.size ) );

	if( isOK )
	{
		ssize_t size = remoteStream->GetSize();
		++m_filesDownloaded;
		m_bytesDownloaded += size;

		if( size != info.size )
		{
			return Be::Result<std::string>( "Size does not match expected value" );
		}

		CacheContentsOfRemoteStream( remoteStream, cachedName, resPath );

		*stream = remoteStream.Detach();
		return Be::Result<std::string>();
	}

	return Be::Result<std::string>( "Couldn't download file" );
}

Be::Result<std::string> RemoteFileCache::CreateFileStreamForCachedFile( const std::wstring &cachedName, IBlueStream** stream )
{
	BlueFileStreamPtr fileStream;
	fileStream.CreateInstance();

	std::wstring cachedNameOnDisk = BePaths->ResolvePathW( cachedName );
	
	for( int retries = 0; retries < 5; ++retries )
	{
		if( fileStream->Open( cachedNameOnDisk.c_str(), BlueFileStream::OM_READONLY, BlueFileStream::SM_READSHARING ) )
		{
			*stream = fileStream.Detach();
			++m_filesUsedFromCache;
			return Be::Result<std::string>();
		}

		CcpThreadSleep( 10 );
	}

	CCP_DEBUG_BREAK();

	return Be::Result<std::string>( "Couldn't open cached file" );
}

bool RemoteFileCache::FileExists( const wchar_t* resPath )
{
	FileInfo info;
	return GetFileInfo( resPath, info );
}

bool RemoteFileCache::IsCachedLocally( const wchar_t* resPath )
{
	FileInfo info;
	if( !GetFileInfo( resPath, info ) )
	{
		return false;
	}

	// Does file exist in the cache folder? If so, open it as a file stream and return it
	std::wstring resId = static_cast<const wchar_t*>( CA2W( info.cachedName.c_str() ) );
	std::wstring cachedName = m_cacheFolder;
	cachedName += L"/";
	cachedName += resId;

	return BePaths->FileExistsLocally( cachedName.c_str() );
}

bool RemoteFileCache::GetFileInfo( const wchar_t* resPath, struct FileInfo& fileInfo )
{
	std::string validatedPath;
	if( !ValidateResPath( resPath, validatedPath ) )
	{
		return false;
	}

	auto foundIt = m_fileIndex.find( validatedPath.c_str() );
	if( foundIt == m_fileIndex.end() )
	{
		return false;
	}

	fileInfo = foundIt->second;
	return true;
}

void RemoteFileCache::SetFileIndexImpl( const char* contents, ssize_t size )
{
	const char* p = contents;
	ssize_t ix = 0;
	while( ix < size )
	{
		std::string line;
		while( *p && *p != '\n' )
		{
			line.push_back( *p );
			++p;
			++ix;
		}

		++p;
		++ix;

		if( !line.empty() )
		{
			std::string resPath;
			std::string cachedName;
			std::string checksumAsString;
			std::string sizeAsString;

			size_t start = 0;
			auto commaPos = line.find( ',', start );
			resPath = line.substr( start, commaPos - start );

			start = commaPos + 1;
			commaPos = line.find( ',', start );
			cachedName = line.substr( start, commaPos - start );

			start = commaPos + 1;
			commaPos = line.find( ',', start );
			checksumAsString = line.substr( start, commaPos - start );

			start = commaPos + 1;
			commaPos = line.find( ',', start );
			sizeAsString = line.substr( start, commaPos - start );

			FileInfo fi;
			fi.cachedName = cachedName;
			fi.checksum = checksumAsString;
			fi.size = atoi( sizeAsString.c_str() );

			m_fileIndex[resPath] = fi;

			AddResPathToFolderIndex( resPath );
		}
	}
}

void RemoteFileCache::SetFileIndex( const std::string& fileIndex )
{
	SetFileIndexImpl( fileIndex.c_str(), fileIndex.size() );
}

void RemoteFileCache::AddResPathToFolderIndex( const std::string& resPath )
{
	auto slashIx = resPath.find( '/' );

	while( slashIx != std::string::npos )
	{
		std::string folder = resPath.substr( 0, slashIx + 1 );

		auto start = slashIx + 1;
		slashIx = resPath.find( '/', start );
		auto count = slashIx;
		if( slashIx != std::string::npos )
		{
			// Leave the slash on the end - handy to denote folders
			count -= start - 1;
		}
		auto component = resPath.substr( start, count );

		auto foundIt = m_folderIndex.find( folder );
		if( foundIt == m_folderIndex.end() )
		{
			std::set<std::string> entry;
			entry.insert( component );
			m_folderIndex[folder] = entry;
		}
		else
		{
			foundIt->second.insert( component );
		}
	}
}

Be::Result<std::string> RemoteFileCache::ListDir( const wchar_t* resPath, std::list<std::string>& contents )
{
	std::string validatedPath;
	if( !ValidateResPath( resPath, validatedPath ) )
	{
		return Be::Result<std::string>( "Not a valid res path" );
	}

	if( validatedPath[validatedPath.size() - 1] != '/' )
	{
		validatedPath.push_back( '/' );
	}

	auto foundIt = m_folderIndex.find( validatedPath.c_str() );
	if( foundIt == m_folderIndex.end() )
	{
		return Be::Result<std::string>( "Directory not found" );
	}

	auto& contentsAsSet = foundIt->second;
	for( auto it = contentsAsSet.begin(); it != contentsAsSet.end(); ++it )
	{
		std::string name = *it;
		if( name.back() == '/' )
		{
#ifdef __ANDROID__
            name.erase( name.begin() + name.length() - 1, name.end() );
#else
			name.pop_back();
#endif
		}
		contents.push_back( name );
	}

	return Be::Result<std::string>();
}

bool RemoteFileCache::ValidateResPath( const wchar_t* resPath, std::string& validatedPath )
{
	std::wstring normalizedResPath;
	if( !NormalizeResPath( resPath, normalizedResPath ) )
	{
		return false;
	}

	validatedPath = CW2A( normalizedResPath.c_str() );
	return true;
}

bool RemoteFileCache::IsDirectory( const wchar_t* resPath )
{
	std::string validatedPath;
	if( !ValidateResPath( resPath, validatedPath ) )
	{
		return false;
	}

	if( validatedPath[validatedPath.size() - 1] != '/' )
	{
		validatedPath.push_back( '/' );
	}

	auto foundIt = m_folderIndex.find( validatedPath.c_str() );
	return foundIt != m_folderIndex.end();
}

void RemoteFileCache::CacheContentsOfRemoteStream( BlueRemoteStream* stream, const std::wstring& cachedName, const wchar_t* resPath )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	void* data = nullptr;
	ssize_t size = stream->GetSize();

	if( stream->LockData( &data, size ) )
	{
		BlueFileStreamPtr fileStream;
		fileStream.CreateInstance();

		// Write contents to a temp file, then rename. This prevents cases where the file
		// was found but another process was still writing to it.
		std::wstring cachedNameOnDisk = BePaths->ResolvePathForWritingW( cachedName );
		std::wstring tempNameOnDisk = cachedNameOnDisk + L".tmp";

		if( fileStream->Create( tempNameOnDisk.c_str() ) )
		{
			fileStream->Write( data, size );
			fileStream->Close();

			CCP_LOG_CH( s_ch, "Cached %S as %S", resPath, cachedNameOnDisk.c_str() );

			RenameFile( tempNameOnDisk, cachedNameOnDisk );

			++m_filesCached;
			m_bytesCached += size;
		}

		stream->UnlockData();
	}
}

bool RemoteFileCache::SetFileIndexFromStream( IBlueStream* stream )
{
	char* contents = nullptr;
	ssize_t size = stream->GetSize();

	if( !stream->LockData( (void**)&contents, size ) )
	{
		return false;
	}

	SetFileIndexImpl( contents, size );

	return true;
}
