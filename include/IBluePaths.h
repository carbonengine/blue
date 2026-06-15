// Copyright © 2011 CCP ehf.

#pragma once
#ifndef IBluePaths_h
#define IBluePaths_h

#include "Blue.h"

struct IBlueStream;

BLUE_INTERFACE( IBluePaths ) : public IRoot
{
	virtual void InitializeStdAppPaths() = 0;

	virtual Be::Result<std::string> SetSearchPathW( const char* key, const wchar_t* value ) = 0;
	virtual const wchar_t* GetSearchPathW( const char* key ) = 0;

	virtual std::wstring ResolvePathW( const std::wstring& path ) = 0;
	virtual std::wstring ResolvePathForWritingW( const std::wstring& path ) = 0;
	virtual std::wstring ResolvePathToRootW( const std::string& root, const std::wstring& path ) = 0;

	virtual void GetExpandedSearchPaths( const char* key, std::vector<std::wstring>& paths ) = 0;

	// Get the contents of the given directory
	virtual void GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results ) = 0;

	// Returns true if the path is a directory
	virtual bool IsDirectory( const std::wstring& dir ) = 0;

	// Returns true if the file exists. Checking for .red files returns true if a corresponding
	// black file exists and BeResMan->GetSubstituteBlackForRed() is set.
	virtual bool FileExists( const std::wstring& filename ) = 0;

	// Dump current search paths to log
	virtual void LogPaths() = 0;

	// Returns true if the file exists on the local disk (not in stuff files or in the remote file cache).
	virtual bool FileExistsLocally( const wchar_t* filename ) = 0;

	virtual bool FileNeedsDownload( const wchar_t* filename ) = 0;

	// Get a stream from a resource path
	virtual bool GetStreamFromPathW( const wchar_t* path, IBlueStream** stream ) = 0;

	virtual Be::Result<std::string> GetFileContentsWithYield( const std::wstring& path, IBlueStream** contents ) = 0;

};

extern BLUEIMPORT IBluePaths* BePaths;
extern "C"
{
	BLUEIMPORT bool BlueInitializePaths( const std::wstring& initialPath );
	BLUEIMPORT IBluePaths* BlueGetBluePaths();
}

#endif
