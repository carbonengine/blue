////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		November 2014
// Copyright:	CCP 2014
//

#pragma once
#ifndef BlueResFileSystemStuff_h
#define BlueResFileSystemStuff_h

#include "Include/IBlueResFileSystem.h"

BLUE_CLASS( BlueResFileSystemStuff ) : public IBlueResFileSystem
{
public:
	EXPOSE_TO_BLUE();

	BlueResFileSystemStuff( IRoot* lockobj = nullptr );

	//////////////////////////////////////////////////////////////////////////
	// IBlueResFileSystem

	virtual bool FileExists( const std::wstring& filename );
	virtual bool IsDirectory( const std::wstring& dir );
	virtual void GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results );
	virtual bool GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream );
	virtual bool ResolvePathW( const std::wstring& path, std::wstring& resolvedPath );

	//////////////////////////////////////////////////////////////////////////

};

TYPEDEF_BLUECLASS( BlueResFileSystemStuff );

#endif // BlueResFileSystemStuff_h