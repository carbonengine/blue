// Copyright © 2014 CCP ehf.

#pragma once
#ifndef BlueResFileSystemRemote_h
#define BlueResFileSystemRemote_h

#include "IBlueResFileSystem.h"

BLUE_CLASS( BlueResFileSystemRemote ) : public IBlueResFileSystem
{
public:
	EXPOSE_TO_BLUE();

	BlueResFileSystemRemote( IRoot* lockobj = nullptr );

	//////////////////////////////////////////////////////////////////////////
	// IBlueResFileSystem

	bool FileExists( const std::wstring& filename ) override;
	bool IsDirectory( const std::wstring& dir ) override;
	void GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results ) override;
	bool GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream ) override;
	bool ResolvePathW( const std::wstring& path, std::wstring& resolvedPath ) override;

	//////////////////////////////////////////////////////////////////////////
};

TYPEDEF_BLUECLASS( BlueResFileSystemRemote );

#endif // BlueResFileSystemRemote_h
