// Copyright © 2013 CCP ehf.

#include "StdAfx.h"

#include "RemoteFileCache.h"
#include "BlueFileStream.h"
#include "BlueRemoteStream.h"
#include "IBluePaths.h"
#include "BlueFileUtil.h"
#include <BlueStatistics.h>

#ifdef __ANDROID__
#include <errno.h>
#endif
#include "md5.h"

CCP_STATS_DECLARE( remoteFileCacheGetStream, "Blue/RemoteFileCache/GetStreamFromPath", false, CST_TIME, "Total time spent in RemoteFileCache::GetStreamFromPathW" );
CCP_STATS_DECLARE( remoteFileCacheGetStreamCount, "Blue/RemoteFileCache/GetStreamFromPathCount", false, CST_COUNTER_LOW, "Number of calls to RemoteFileCache::GetStreamFromPathW" );
CCP_STATS_DECLARE( remoteFileCacheFailedPrimary, "Blue/RemoteFileCache/FailedPrimary", false, CST_COUNTER_LOW, "Count of failed downloads from primary server" );
CCP_STATS_DECLARE( remoteFileCacheFailedBackup, "Blue/RemoteFileCache/FailedBackup", false, CST_COUNTER_LOW, "Count of failed downloads from backup server" );
CCP_STATS_DECLARE( remoteFileCacheCorruptDownloads, "Blue/RemoteFileCache/CorruptDownloads", false, CST_COUNTER_LOW, "Count of corrupt downloads" );
CCP_STATS_DECLARE( remoteFileCacheCorruptFiles, "Blue/RemoteFileCache/CorruptFiles", false, CST_COUNTER_LOW, "Count of corrupt files in the cache" );

namespace
{
	CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "RemoteFileCache" );
}

RemoteFileCache::RemoteFileCache() :
	m_server( "http://127.0.0.1:5000" ),
	m_prefix( "/res/"),
	m_cacheFolder( L"cache:/ResFiles"),
	m_registerDownloadErrors( true ),
	m_numSequentialPrimaryServerDownloadErrors( 0 ),
	m_primaryServerFailThreshold( 10 ),
	m_bytesDownloaded( 0 ),
	m_bytesCached( 0 ),
	m_filesDownloaded( 0 ),
	m_filesCached( 0 ),
	m_filesUsedFromCache( 0 ),
	m_fullHeaderLogging( false ),
	m_verifyContentsOnSave( true ),
	m_verifyContentsOnLoad( true ),
	m_on_server_failed_callback()
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
		CreateFileStreamForCachedFile( cachedName, std::string(), &stream );
	}

	if( !stream )
	{
		CCP_LOG_CH( s_ch, "Downloading index from %s", url.c_str() );

		BlueRemoteStreamPtr remoteStream;
		remoteStream.CreateInstance();
		remoteStream->SetFullHeaderLogging( m_fullHeaderLogging );
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
	
	return AddFileIndexFromStream( stream );
}

void RemoteFileCache::SetCacheFolder( const wchar_t* folderName )
{
	m_cacheFolder = folderName;
}

void RemoteFileCache::SetServer( const char* url )
{
	m_server = url;
}

void RemoteFileCache::SetPrefix( const char* prefix )
{
	m_prefix = prefix;
}

Be::Result<std::string> RemoteFileCache::GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream )
{
	CCP_STATS_SCOPED_TIME( remoteFileCacheGetStream );
	CCP_STATS_INC( remoteFileCacheGetStreamCount );

	*stream = nullptr;

	FileInfo* info = GetFileInfo( resPath );
	if( !info )
	{
		return Be::Result<std::string>( "File does not exist on remote server" );
	}

	// Does file exist in the cache folder? If so, open it as a file stream and return it
	std::wstring resId = static_cast<const wchar_t*>( CA2W( info->cachedName.c_str() ) );
	std::wstring cachedName = m_cacheFolder;
	cachedName += L"/";
	cachedName += resId;

	if( BePaths->FileExistsLocally( cachedName.c_str() ) )
	{
		std::string checksum;
		if( m_verifyContentsOnLoad && !info->verified )
		{
			checksum = info->checksum;
		}
		auto result = CreateFileStreamForCachedFile( cachedName, checksum, stream );
		if( BeIsSuccess( result ) )
		{
			info->verified = true;
			return result;
		}

		CCP_LOGERR_CH( s_ch, "%s - %S", result.value.c_str(), cachedName.c_str() );
		CcpRemoveFile( cachedName.c_str() );
	}

	// File does not exist in the cache folder - download and add to cache

	BlueRemoteStreamPtr remoteStream;
	remoteStream.CreateInstance();

	size_t expectedSize = static_cast<size_t>( info->size );
	std::string filename( CW2A( resId.c_str() ) );
	
	bool isOK = TryDownload( m_server, filename, remoteStream, expectedSize, resPath);
	if( !isOK )
	{
		CCP_STATS_INC( remoteFileCacheFailedPrimary );

		if( !m_backupServer.empty() )
		{

			CCP_LOGERR_CH( s_ch, "Download failed for %S from %s - retrying from backup server: %s", resPath, m_server.c_str(), m_backupServer.c_str() );

			if( m_primaryServerFailThreshold > -1 )
			{
				m_numSequentialPrimaryServerDownloadErrors++;
				if( m_numSequentialPrimaryServerDownloadErrors > m_primaryServerFailThreshold )
				{
					// Number of sequetial failures exceeding threshold, set backup url as primary url to attempt to bring back performance
					CCP_LOGERR_CH( s_ch, "Primary server sequential failures has exceeded the failure threshold of: %d, defaulting to backup server: %s", m_primaryServerFailThreshold, m_backupServer.c_str() );
					m_server = m_backupServer;
				}
			}

			if( m_registerDownloadErrors && m_on_server_failed_callback )
			{
				//Callback into python to register exception that will show up on Sentry
				m_on_server_failed_callback.CallVoid( "Primary", resPath, m_server.c_str() );
				
			}

			isOK = TryDownload( m_backupServer, filename, remoteStream, expectedSize, resPath );
			if( !isOK )
			{
				if( m_registerDownloadErrors && m_on_server_failed_callback )
				{
					//Callback into python to register exception that will show up on Sentry
					m_on_server_failed_callback.CallVoid( "Backup", resPath, m_backupServer.c_str() );
				}

				CCP_STATS_INC( remoteFileCacheFailedBackup );
			}
		}
		else
		{
			m_numSequentialPrimaryServerDownloadErrors = 0;
		}
	}

	if( isOK )
	{
		ssize_t size = remoteStream->GetSize();
		++m_filesDownloaded;
		m_bytesDownloaded += size;

		if( size != info->size )
		{
			CCP_STATS_INC( remoteFileCacheCorruptDownloads );
			return Be::Result<std::string>( "Size does not match expected value" );
		}

		if( m_verifyContentsOnSave )
		{
			if( !remoteStream->VerifyContents( info->checksum.c_str() ) )
			{
				CCP_STATS_INC( remoteFileCacheCorruptDownloads );
				return Be::Result<std::string>( "Checksum does not match expected value" );
			}
		}

		CacheContentsOfRemoteStream( remoteStream, cachedName, resPath );

		*stream = remoteStream.Detach();
		return Be::Result<std::string>();
	}

	return Be::Result<std::string>( "Couldn't download file" );
}

bool VerifyChecksum( IBlueStream* stream, const std::string& expectedChecksum )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	auto size = stream->GetSize();
	CcpMallocBuffer data( "VerifyChecksum/data", size );

	stream->Read( data.get(), data.size() );
	stream->Seek( 0, IBlueStream::SO_BEGIN );

	MD5 checkSum;
	checkSum.update( reinterpret_cast<unsigned char*>(data.get()), (unsigned int)data.size() );
	checkSum.finalize();

	char buffer[33];
	const char* checkSumAsHex = checkSum.hex_digest( buffer );
	if( strcmp( expectedChecksum.c_str(), checkSumAsHex ) != 0 )
	{
		return false;
	}

	return true;
}

Be::Result<std::string> RemoteFileCache::CreateFileStreamForCachedFile( const std::wstring &cachedName, const std::string& checksum, IBlueStream** stream )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	BlueFileStreamPtr fileStream;
	fileStream.CreateInstance();

	std::wstring cachedNameOnDisk = BePaths->ResolvePathW( cachedName );
	
	for( int retries = 0; retries < 5; ++retries )
	{
		if( fileStream->Open( cachedNameOnDisk.c_str(), CCP_OM_READONLY, CCP_SM_RWSHARING ) )
		{
			if( checksum.empty() || VerifyChecksum( fileStream, checksum ) )
			{
				*stream = fileStream.Detach();
				++m_filesUsedFromCache;
				return Be::Result<std::string>();
			}
			else
			{
				CCP_STATS_INC( remoteFileCacheCorruptFiles );
				return Be::Result<std::string>( "File is corrupt" );
			}
		}

		CcpThreadSleep( 10 );
	}

	return Be::Result<std::string>( "Couldn't open cached file" );
}

bool RemoteFileCache::FileExists( const wchar_t* resPath )
{
	return GetFileInfo( resPath ) != nullptr;
}

bool RemoteFileCache::IsCachedLocally( const wchar_t* resPath )
{
	return BePaths->FileExistsLocally( GetLocallyCachedName( resPath ).c_str() );
}

std::wstring RemoteFileCache::GetLocallyCachedName( const wchar_t* resPath )
{
	FileInfo* info = GetFileInfo( resPath );
	if( !info )
	{
		return resPath;
	}

	std::wstring resId = static_cast<const wchar_t*>( CA2W( info->cachedName.c_str() ) );
	std::wstring cachedName = m_cacheFolder;
	cachedName += L"/";
	cachedName += resId;

	return cachedName;
}

RemoteFileCache::FileInfo* RemoteFileCache::GetFileInfo( const wchar_t* resPath )
{
	std::string validatedPath;
	if( !ValidateResPath( resPath, validatedPath ) )
	{
		return nullptr;
	}

	auto foundIt = m_fileIndex.find( validatedPath.c_str() );
	if( foundIt == m_fileIndex.end() )
	{
		return nullptr;
	}

	return &foundIt->second;
}

void RemoteFileCache::AddFileIndexImpl( const char* contents, ssize_t size )
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

			FileInfo& fi = m_fileIndex[resPath];
			fi.cachedName = cachedName;
			fi.checksum = checksumAsString;
			fi.size = atoi( sizeAsString.c_str() );
			fi.verified = false;

			AddResPathToFolderIndex( resPath );
		}
	}
}

void RemoteFileCache::RegisterOnServerFailedCallback( BlueScriptCallback callback )
{
	m_on_server_failed_callback = callback;
}

void RemoteFileCache::AddFileIndex( const std::string& fileIndex )
{
	AddFileIndexImpl( fileIndex.c_str(), fileIndex.size() );
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

namespace
{
	ssize_t GetFileSize( const wchar_t* filePath )
	{
		BlueFileStreamPtr fileStream;
		fileStream.CreateInstance();
		if( fileStream && fileStream->Open( filePath, CCP_OM_READONLY, CCP_SM_READSHARING ) )
		{
			auto size = fileStream->GetSize();
			fileStream->Close();
			return size;
		}
		return -1;
	}
}

void RemoteFileCache::CacheContentsOfRemoteStream( BlueRemoteStream* stream, const std::wstring& cachedName, const wchar_t* resPath )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Write contents to a temp file, then rename. This prevents cases where the file
	// was found but another process was still writing to it.
	std::wstring cachedNameOnDisk = BePaths->ResolvePathForWritingW(cachedName);
	std::wstring tempNameOnDisk = cachedNameOnDisk + L".tmp";

	// Checks for existing files are to deal with the cases where other clients (or
	// the launcher) are downloading the same files. We don't start a download if
	// the file is already cached locally, but two clients may start downloading the
	// same file, then report errors when trying to cache it if another client has
	// already done so.
	if( CcpIsPathExistingFile( cachedNameOnDisk ) )
	{
		return;
	}

	if( CcpIsPathExistingFile( tempNameOnDisk ) )
	{
		// Other clients (or the launcher) may have downloaded this file and are
		// about to save it to the cache, but either they failed somehow and
		// left the temp file behind, or we simply got here first. In any case,
		// we'll attempt to delete the file and replace it with our own copy.
		// See https://jira.ccpgames.com/browse/TQ-127440
		CcpRemoveFile(tempNameOnDisk);
	}

	void* data = nullptr;
	ssize_t size = stream->GetSize();

	if( stream->LockData( &data, size ) )
	{
		BlueFileStreamPtr fileStream;
		fileStream.CreateInstance();

		if (fileStream->Create(tempNameOnDisk.c_str()))
		{
			fileStream->Write(data, size);
			fileStream->Close();

			auto writtenSize = GetFileSize( tempNameOnDisk.c_str() );
			if( writtenSize == size )
			{
				CCP_LOG_CH( s_ch, "Cached %S as %S", resPath, cachedNameOnDisk.c_str() );

				if( CcpRenameFile( tempNameOnDisk, cachedNameOnDisk ) )
				{
					++m_filesCached;
					m_bytesCached += size;
				}
				else if( !CcpIsPathExistingFile( cachedNameOnDisk ) )
				{
					CCP_LOGWARN_CH( s_ch, "Failed to rename file %S, yet it does not exist", cachedNameOnDisk.c_str() );
				}
			}
			else if( writtenSize >= 0 )
			{
				CCP_LOGWARN_CH( 
					s_ch, 
					"Failed to write the entire downloaded file %S to disk: written %" CCP_SIZET_FORMAT " bytes, while downloaded size is %" CCP_SIZET_FORMAT " bytes", 
					tempNameOnDisk.c_str(),
					size_t( writtenSize ), 
					size_t( size ) );
				CcpRemoveFile( tempNameOnDisk );
			}
			else
			{
				CCP_LOGWARN_CH( s_ch, "Failed to verify file %S size", tempNameOnDisk.c_str() );
				CcpRemoveFile( tempNameOnDisk );
			}
		}
		else if( !CcpIsPathExistingFile( tempNameOnDisk ) )
		{
			CCP_LOGWARN_CH( s_ch, "Couldn't create file %S, yet it does not exist", tempNameOnDisk.c_str() );
		}

		stream->UnlockData();
	}
}

bool RemoteFileCache::AddFileIndexFromStream( IBlueStream* stream )
{
	char* contents = nullptr;
	ssize_t size = stream->GetSize();

	if( !stream->LockData( (void**)&contents, size ) )
	{
		return false;
	}

	AddFileIndexImpl( contents, size );

	return true;
}

bool RemoteFileCache::TryDownload( std::string server, std::string filename, BlueRemoteStream* remoteStream, size_t expectedSize, const wchar_t* resPath )
{
	std::string resUrl = server;
	resUrl += m_prefix;
	resUrl += filename;

	return remoteStream->Open( resUrl.c_str(), expectedSize, resPath );
}
