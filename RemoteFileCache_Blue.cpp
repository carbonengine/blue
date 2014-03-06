////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		October 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"

#if USE_RESFILE_2

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
		
		MAP_METHOD_AND_WRAP
		(
			"DownloadFileIndex",
			DownloadFileIndex,
			"Download a file index from the given url.\n"
			"Returns True/False on success/failure."
		)

		MAP_METHOD_AND_WRAP
		(
			"SetFileIndex",
			SetFileIndex,
			"Set the file index directly from the given string."
		)

		MAP_METHOD_AND_WRAP
		(
			"FileExists",
			FileExists,
			"Returns True if the file exists (either cached locally or available\n"
			"on the remote server."
		)

		MAP_METHOD_AND_WRAP
		(
			"GetStreamFromPath",
			GetStreamFromPathW,
			"Get a stream from the given path. This may trigger a download\n"
			"of a file from the currently set server."
		)

		MAP_METHOD_AND_WRAP
		(
			"ListDir",
			ListDir,
			""
		)

		MAP_METHOD_AND_WRAP
		(
			"isdir",
			IsDirectory,
			""
		)
	EXPOSURE_END()
}

#endif
