////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		October 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"

#if USE_RESFILE_2

#include "RemoteFileCache.h"
#include "BlueFileStream.h"
#include "BlueRemoteStream.h"
#include "Include/IBluePaths.h"
#include "Include/BlueFileUtil.h"
#include "md5.h"

namespace
{
	void AtomicRename( const std::wstring& src, const std::wstring& dst )
	{
		wchar_t srcBuffer[MAX_PATH];
		wchar_t dstBuffer[MAX_PATH];
		GetFullPathNameW( src.c_str(), MAX_PATH, srcBuffer, NULL );
		GetFullPathNameW( dst.c_str(), MAX_PATH, dstBuffer, NULL );

		if( !MoveFileW( srcBuffer, dstBuffer ) )
		{
			CCP_LOGWARN( "Couldn't rename file (%d)", GetLastError() );
		}
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

bool RemoteFileCache::DownloadFileIndex( const char* url )
{
	BlueRemoteStreamPtr remoteStream;
	remoteStream.CreateInstance();
	bool isOK = remoteStream->Open( url );

	if(	!isOK )
	{
		return false;
	}
	
	char* contents = nullptr;
	ssize_t size = remoteStream->GetSize();

	if( !remoteStream->LockData( (void**)&contents, size ) )
	{
		return false;
	}

	SetFileIndexImpl( contents, size );

	return true;
}

void RemoteFileCache::SetCacheFolder( const wchar_t* folderName )
{
	m_cacheFolder = folderName;
}

Be::Result<std::string> RemoteFileCache::GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream )
{
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

	if( BePaths->FileExists( cachedName ) )
	{
		return CreateFileStreamForCachedFile(cachedName, stream);
	}

	// File does not exist in the cache folder - download and add to cache
	std::string resUrl = m_server;
	resUrl += m_prefix;
	resUrl += CW2A( resId.c_str() );

	BlueRemoteStreamPtr remoteStream;
	remoteStream.CreateInstance();
	bool isOK = remoteStream->Open( resUrl.c_str() );

	if( isOK )
	{
		ssize_t size = remoteStream->GetSize();
		++m_filesDownloaded;
		m_bytesDownloaded += size;

		if( size != info.size )
		{
			return Be::Result<std::string>( "Size does not match expected value" );
		}

		void* data = nullptr;
		if( remoteStream->LockData( &data, size ) )
		{
			MD5 checkSum;
			checkSum.update( reinterpret_cast<unsigned char*>( data ), (unsigned int)size );
			checkSum.finalize();
			if( strcmp( info.checksum.c_str(), checkSum.hex_digest() ) != 0 )
			{
				return Be::Result<std::string>( "Checksum does not match expected value" );
			}

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

				AtomicRename( tempNameOnDisk, cachedNameOnDisk );
			}
			remoteStream->UnlockData();

			++m_filesCached;
			m_bytesCached += size;
		}

		*stream = remoteStream.Detach();
		return Be::Result<std::string>();
	}

	return Be::Result<std::string>( "Couldn't download file" );
}

Be::Result<std::string> RemoteFileCache::CreateFileStreamForCachedFile( std::wstring &cachedName, IBlueStream** &stream )
{
	BlueFileStreamPtr fileStream;
	fileStream.CreateInstance();

	std::wstring cachedNameOnDisk = BePaths->ResolvePathW( cachedName );
	
	for( int retries = 0; retries < 5; ++retries )
	{
		if( fileStream->Open( cachedNameOnDisk.c_str(), BlueFileStream::OM_READONLY ) )
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
			name.pop_back();
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

#endif
