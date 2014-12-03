////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		October 2013
// Copyright:	CCP 2013
//

#pragma once
#ifndef RemoteFileCache_h
#define RemoteFileCache_h

#include "include/IBluePersist.h"

BLUE_CLASS( RemoteFileCache ) :
	public IRoot
{
public:
	EXPOSE_TO_BLUE();

	RemoteFileCache();

	bool DownloadFileIndex( const std::string& url );
	void SetFileIndex( const std::string& fileIndex );

	void SetCacheFolder( const wchar_t* folderName );
	void SetServer( const char* url );
	void SetPrefix( const char* prefix );

	Be::Result<std::string> GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream );

	bool FileExists( const wchar_t* resPath );
	bool IsCachedLocally( const wchar_t* resPath );
	std::wstring GetLocallyCachedName( const wchar_t* resPath );
	bool IsDirectory( const wchar_t* resPath );
	Be::Result<std::string> ListDir( const wchar_t* resPath, std::list<std::string>& contents );

private:
	std::string m_server;
	std::string m_backupServer;
	std::string m_prefix;
	std::wstring m_cacheFolder;

	struct FileInfo
	{
		std::string cachedName;
		std::string checksum;
		uint64_t size;
	};

	typedef std::map<std::string, FileInfo> FileIndex;
	FileIndex m_fileIndex;

	typedef std::map<std::string, std::set<std::string>> FolderIndex;
	FolderIndex m_folderIndex;

	uint64_t m_bytesDownloaded;
	uint64_t m_bytesCached;
	uint32_t m_filesDownloaded;
	uint32_t m_filesCached;
	uint32_t m_filesUsedFromCache;

	bool m_fullHeaderLogging;
	bool m_verifyContents;

private:
	bool SetFileIndexFromStream( IBlueStream* stream );
	void SetFileIndexImpl( const char* contents, ssize_t size );

	Be::Result<std::string> CreateFileStreamForCachedFile( const std::wstring &cachedName, IBlueStream** stream );
	bool GetFileInfo(  const wchar_t* resPath, FileInfo& fileInfo );
	void AddResPathToFolderIndex( const std::string& resPath );
	bool ValidateResPath( const wchar_t* resPath, std::string& validatedPath );
	Be::Result<std::string> VerifyChecksum( void* data, ssize_t size, const FileInfo &info );
	void CacheContentsOfRemoteStream( class BlueRemoteStream* stream, const std::wstring& cachedName, const wchar_t* resPath );

	bool TryDownload( std::string server, std::string filename, BlueRemoteStream* remoteStream, size_t expectedSize, const wchar_t* resPath );
};

TYPEDEF_BLUECLASS( RemoteFileCache );

extern RemoteFileCache* BeRemoteFileCache;

#endif // RemoteFileCache_h