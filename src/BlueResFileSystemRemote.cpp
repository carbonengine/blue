// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "BlueResFileSystemRemote.h"
#include "RemoteFileCache.h"
#include "BlueFileUtil.h"


BlueResFileSystemRemote::BlueResFileSystemRemote( IRoot* lockobj /*= nullptr */ )
{
}

bool BlueResFileSystemRemote::FileExists( const std::wstring& filename )
{
	return BeRemoteFileCache->FileExists( SubstituteBlackForRedInFilename( filename ).c_str() );
}

bool BlueResFileSystemRemote::IsDirectory( const std::wstring& dir )
{
	return BeRemoteFileCache->IsDirectory( dir.c_str() );
}

void BlueResFileSystemRemote::GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results )
{
	std::list<std::string> remoteFileCacheContents;
	auto remoteFileCacheResult = BeRemoteFileCache->ListDir( dir, remoteFileCacheContents );
	if( BeIsSuccess( remoteFileCacheResult ) )
	{
		for( auto it = remoteFileCacheContents.begin(); it != remoteFileCacheContents.end(); ++it )
		{
			std::wstring ws = static_cast<const wchar_t*>( CA2W( it->c_str() ) );
			results.insert( SubstituteRedForBlackInFilename( ws ) );
		}
	}
}

bool BlueResFileSystemRemote::GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream )
{
	std::wstring filename = SubstituteBlackForRedInFilename( resPath );
	auto result = BeRemoteFileCache->GetStreamFromPathW( filename.c_str(), stream );
	return BeIsSuccess( result );
}

bool BlueResFileSystemRemote::ResolvePathW( const std::wstring& path, std::wstring& resolvedPath )
{
	if( BeRemoteFileCache->FileExists( SubstituteBlackForRedInFilename( path ).c_str() ) )
	{
		resolvedPath = CcpGetAbsolutePath( BeRemoteFileCache->GetLocallyCachedName( path.c_str() ) );
		return true;
	}

	return false;
}
