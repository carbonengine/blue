// Copyright © 2013 CCP ehf.

#pragma once
#ifndef RemoteFileCache_h
#define RemoteFileCache_h

#include "IBluePersist.h"

extern double g_thresholdForWarningLongDownloadTime;
extern double g_thresholdForAbortingLongDownloadTime;

BLUE_CLASS( RemoteFileCache ) :
	public IRoot
{
public:
	EXPOSE_TO_BLUE();

	RemoteFileCache();

	bool DownloadFileIndex( const std::string& url );
	void AddFileIndex( const std::string& fileIndex );

	void SetCacheFolder( const wchar_t* folderName );
	void SetServer( const char* url );
	void SetPrefix( const char* prefix );

	Be::Result<std::string> GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream );

	bool FileExists( const wchar_t* resPath );
	bool IsCachedLocally( const wchar_t* resPath );
	std::wstring GetLocallyCachedName( const wchar_t* resPath );
	bool IsDirectory( const wchar_t* resPath );
	Be::Result<std::string> ListDir( const wchar_t* resPath, std::list<std::string>& contents );

	void RegisterOnServerFailedCallback( BlueScriptCallback callback );

private:
	std::string m_server;
	std::string m_backupServer;
	std::string m_prefix;
	std::wstring m_cacheFolder;
	bool m_registerDownloadErrors;
	int m_numSequentialPrimaryServerDownloadErrors;
	int m_primaryServerFailThreshold;

	struct FileInfo
	{
		std::string cachedName;
		std::string checksum;
		uint64_t size;
		std::atomic<bool> verified;
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
	bool m_verifyContentsOnSave;
	bool m_verifyContentsOnLoad;

	BlueScriptCallback m_on_server_failed_callback;

private:
	bool AddFileIndexFromStream( IBlueStream* stream );
	void AddFileIndexImpl( const char* contents, ssize_t size );

	Be::Result<std::string> CreateFileStreamForCachedFile( const std::wstring &cachedName, const std::string& checksum, IBlueStream** stream );
	FileInfo* GetFileInfo( const wchar_t* resPath );
	void AddResPathToFolderIndex( const std::string& resPath );
	bool ValidateResPath( const wchar_t* resPath, std::string& validatedPath );
	void CacheContentsOfRemoteStream( class BlueRemoteStream* stream, const std::wstring& cachedName, const wchar_t* resPath );

	bool TryDownload( std::string server, std::string filename, BlueRemoteStream* remoteStream, size_t expectedSize, const wchar_t* resPath );

	double GetThresholdForWarningLongDownloadTime() { return g_thresholdForWarningLongDownloadTime; }
	void SetThresholdForWarningLongDownloadTime( double val ) { g_thresholdForWarningLongDownloadTime = val; }

	double GetThresholdForAbortingLongDownloadTime() { return g_thresholdForAbortingLongDownloadTime; }
	void SetThresholdForAbortingLongDownloadTime( double val ) { g_thresholdForAbortingLongDownloadTime = val; }
};

TYPEDEF_BLUECLASS( RemoteFileCache );

extern RemoteFileCache* BeRemoteFileCache;

#endif // RemoteFileCache_h
