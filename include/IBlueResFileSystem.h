// Copyright © 2014 CCP ehf.

#pragma once
#ifndef IBlueResFileSystem_h
#define IBlueResFileSystem_h

BLUE_DECLARE_INTERFACE( IBlueStream );

BLUE_INTERFACE( IBlueResFileSystem ) : public IRoot
{
	virtual bool FileExists( const std::wstring& filename ) = 0;
	virtual bool IsDirectory( const std::wstring& dir ) = 0;
	virtual void GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results ) = 0;
	virtual bool GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream ) = 0;
	virtual bool ResolvePathW( const std::wstring& path, std::wstring& resolvedPath ) = 0;
};

#endif // IBlueResFileSystem_h