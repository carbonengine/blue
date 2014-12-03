////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		November 2014
// Copyright:	CCP 2014
//

#include "StdAfx.h"
#include "BlueResFileSystemStuff.h"
#include "Stuffer.h"
#include "Include/BlueFileUtil.h"



BlueResFileSystemStuff::BlueResFileSystemStuff( IRoot* lockobj /*= nullptr */ )
{
}

bool BlueResFileSystemStuff::FileExists( const std::wstring& filename )
{
#if STUFFER_ENABLED
	if( wcsncmp( filename.c_str(), L"res:", 4 ) == 0 )
	{
		if( BeStuffer->HasData( SubstituteBlackForRedInFilename( filename ).c_str() + 4 ) )
		{
			return true;
		}
	}

#endif

	return false;
}

bool BlueResFileSystemStuff::IsDirectory( const std::wstring& dir )
{
#if STUFFER_ENABLED
	// TODO: If we change the search priority of stuff files, move this
	// to below the directory scanning.
	std::string aDir = (const char*)CW2A(dir.c_str());
	unsigned int attrs = Stuffer::GetStufferAttributes( aDir.c_str() );

	if( attrs & FILE_ATTRIBUTE_DIRECTORY )
	{
		return true;
	}
#endif

	return false;
}

void BlueResFileSystemStuff::GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results )
{
#if STUFFER_ENABLED
	Stuffer::GetStufferDirectoryContents( CW2A(dir), results );
#endif
}

bool BlueResFileSystemStuff::GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream )
{
#if STUFFER_ENABLED
	if( BeStuffer->GetStream( resPath, stream ) )
	{
		return true;
	}
#endif

	return false;
}

bool BlueResFileSystemStuff::ResolvePathW( const std::wstring& path, std::wstring& resolvedPath )
{
	return false;
}
