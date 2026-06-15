// Copyright © 2013 CCP ehf.

#include "StdAfx.h"

#include "RemoteFileCache.h"

BLUE_DEFINE( RemoteFileCache );

const Be::ClassInfo* RemoteFileCache::ExposeToBlue()
{
	EXPOSURE_BEGIN( RemoteFileCache, "" )
		MAP_ATTRIBUTE
		(
			"server",
			m_server,
			"Server (and optionally port) to get files from.",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		(
			"backupServer",
			m_backupServer,
			"Backup server (and optionally port) to get files from if the download\n"
			"fails from the primary server.",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		(
			"prefix",
			m_prefix,
			"Prefix added to filenames to request from server - defaults to /res/",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		(
			"cacheFolder",
			m_cacheFolder,
			"Folder where downloaded files are cached",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		(
			"registerDownloadErrors",
			m_registerDownloadErrors,
			"Set true to register errors when download from primary server fails.\n"
			"Default: true",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		(
			"primaryServerFailThreshold",
			m_primaryServerFailThreshold,
			"Number of failures encountered from primary server before defaulting to backup server.\n"
			"set -1 to never fallback to backup server."
			"Default: -1",
			Be::READWRITE 
		)

		MAP_ATTRIBUTE
		(
			"bytesDownloaded",
			m_bytesDownloaded,
			"Number of bytes successfully downloaded from server.",
			Be::READ
		)
		
		MAP_ATTRIBUTE
		(
			"filesDownloaded",
			m_filesDownloaded,
			"Number of files successfully downloaded from server.",
			Be::READ
		)
		
		MAP_ATTRIBUTE
		(
			"bytesCached",
			m_bytesCached,
			"Number of bytes successfully cached after download. This may be lower than bytes\n"
			"downloaded, for example if another client instance also downloaded the same file,\n"
			"or if saving the file failed for any reason.",
			Be::READ
		)
		
		MAP_ATTRIBUTE
		(
			"filesCached",
			m_filesCached,
			"Number of files successfully cached after download. This may be lower than files\n"
			"downloaded, for example if another client instance also downloaded the same file,\n"
			"or if saving the file failed for any reason.",
			Be::READ
		)
		
		MAP_ATTRIBUTE
		(
			"filesUsedFromCache",
			m_filesUsedFromCache,
			"Number of files used from the cache, rather than downloading.",
			Be::READ
		)
		
		MAP_ATTRIBUTE
		(
			"fullHeaderLogging",
			m_fullHeaderLogging,
			"Log all headers received from the server on network problems.",
			Be::READWRITE
		)

		MAP_ATTRIBUTE
		(
			"verifyContentsOnSave",
			m_verifyContentsOnSave,
			"If set, the contents of downloaded files is verified against the md5 checksum\n"
			"before caching the file.",
			Be::READWRITE
		)
		
		MAP_ATTRIBUTE
		(
			"verifyContentsOnLoad",
			m_verifyContentsOnLoad,
			"If set, the contents of downloaded files is verified against the md5 checksum\n"
			"when using the file from the local cache.",
			Be::READWRITE
		)

		MAP_METHOD_AND_WRAP
		(
			"RegisterOnServerFailedCallback",
			RegisterOnServerFailedCallback,
			"Register a callback method to be called whenever download from server fails." 
		)
		
		MAP_METHOD_AND_WRAP
		(
			"DownloadFileIndex",
			DownloadFileIndex,
			"Download a file index from the given url.\n"
			"Returns True/False on success/failure.\n"
			":param url: URL for the file index"
		)

		MAP_METHOD_AND_WRAP
		(
			"AddFileIndex",
			AddFileIndex,
			"Load the file index directly from the given string.\n"
			":param fileIndex: string containing file index"
		)

		MAP_METHOD_AND_WRAP
		(
			"FileExists",
			FileExists,
			"Returns True if the file exists (either cached locally or available\n"
			"on the remote server.\n"
			":param path: res path"
		)

		MAP_METHOD_AND_WRAP
		(
			"GetStreamFromPath",
			GetStreamFromPathW,
			"Get a stream from the given path. This may trigger a download\n"
			"of a file from the currently set server.\n"
			":param path: res path"
		)

		MAP_METHOD_AND_WRAP
		(
			"ListDir",
			ListDir,
			"Returns a list of files in the directory\n"
			":param path: res path to a directory"
		)

		MAP_METHOD_AND_WRAP
		(
			"isdir",
			IsDirectory,
			"Checks if the given path is a directory or a file\n"
			":param path: res path"
		)

		MAP_PROPERTY
		(
			"thresholdForWarningLongDownloads",
			GetThresholdForWarningLongDownloadTime, SetThresholdForWarningLongDownloadTime,
			"Threshold (in seconds) for issuing warnings on long downloads. Set to zero to disable warnings altogether."
		)

		MAP_PROPERTY
		(
			"thresholdForAbortingLongDownloads",
			GetThresholdForAbortingLongDownloadTime, SetThresholdForAbortingLongDownloadTime,
			"Threshold (in seconds) for aborting long downloads. Set to zero to disable altogether."
		)

	EXPOSURE_END()
}
