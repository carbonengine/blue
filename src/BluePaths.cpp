// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#include "BluePaths.h"
#include "RemoteFileCache.h"
#include "BackgroundReader.h"
#include "BlueResFile.h"
#include "BlueFileStream.h"
#include "IBlueResMan.h"
#include "IBlueOS.h"
#include "BlueFileUtil.h"

IBluePaths* BePaths = nullptr;
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "paths", BePaths );

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "BePaths" );

BLUEIMPORT IBluePaths* BlueGetBluePaths()
{
	return BePaths;
}

BLUEIMPORT bool BlueInitializePaths(const std::wstring& initialPath)
{
	return BluePaths::Initialize(initialPath);
}

BluePaths::BluePaths( IRoot* lockobj /*= NULL */ ) :
	m_existingFilesMutex( "BluePaths", "m_existingFilesMutex" ),
	m_cacheFileExistance( true )
{
}

BluePaths::~BluePaths()
{
}

bool BluePaths::Initialize(const std::wstring& initialPath)
{
	if( BePaths )
	{
		return true;
	}

	static CBluePaths instance;
	BePaths = &instance;

	return instance.InitializeHelper(initialPath);
}

bool BluePaths::InitializeHelper(const std::wstring& initialPath)
{
	m_localFileSystem.CreateInstance();
	m_localFileSystem->Initialize(initialPath);

#if CCP_STACKLESS
	InitializeStdAppPaths();
#endif

	m_resFileSystems.push_back( BlueCastPtr<IBlueResFileSystem>( m_localFileSystem ));

	return true;
}

Be::Result<std::string> BluePaths::SetSearchPathW( const char* key, const wchar_t* value )
{
	return m_localFileSystem->SetSearchPathW( key, value );
}

const wchar_t* BluePaths::GetSearchPathW( const char* key )
{
	return m_localFileSystem->GetSearchPathW( key );
}

void BluePaths::ClearSearchPaths()
{
	m_localFileSystem->ClearSearchPaths();
}

std::wstring BluePaths::ResolvePathW( const std::wstring& path )
{
	std::wstring result;

	for( auto it = m_resFileSystems.begin(); it != m_resFileSystems.end(); ++it )
	{
		if( (*it)->ResolvePathW( path, result ) )
		{
			return result;
		}
	}
	
	return ResolvePathForWritingW( path );
}

std::wstring BluePaths::ResolvePathForWritingW( const std::wstring& path )
{
	return m_localFileSystem->ResolvePathForWritingW( path );
}

std::wstring BluePaths::ResolvePathToRootW( const std::string& root, const std::wstring& path )
{
	return m_localFileSystem->ResolvePathToRootW( root, path );
}

void BluePaths::GetExpandedSearchPaths( const char* key, std::vector<std::wstring>& paths )
{
	m_localFileSystem->GetExpandedSearchPaths( key, paths );
}

void BluePaths::GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results )
{
	for( auto it = m_resFileSystems.begin(); it != m_resFileSystems.end(); ++it )
	{
		(*it)->GetDirectoryContents( dir, results );
	}
}

bool BluePaths::IsDirectory( const std::wstring& dir )
{
	for( auto it = m_resFileSystems.begin(); it != m_resFileSystems.end(); ++it )
	{
		if( (*it)->IsDirectory( dir ) )
		{
			return true;
		}
	}

	return false;
}

std::map<std::string, std::wstring> BluePaths::GetAllSearchPathsAsDict()
{
	return m_localFileSystem->GetAllSearchPaths();
}


std::map<std::string, std::vector<std::wstring>> BluePaths::GetExpandedSearchPathsAsDict()
{
	return m_localFileSystem->GetExpandedSearchPaths();
}

bool BluePaths::FileExists( const std::wstring& filename )
{
	for( auto it = m_resFileSystems.begin(); it != m_resFileSystems.end(); ++it )
	{
		if( *it != m_localFileSystem && (*it)->FileExists( filename ) )
		{
			return true;
		}
	}
	if( m_cacheFileExistance )
	{
		CcpAutoMutex lock( m_existingFilesMutex );
		auto found = m_existingFiles.find( filename );
		if( found != end( m_existingFiles ) )
		{
			return true;
		}
	}
	auto exists = m_localFileSystem->FileExists( filename );
	if( exists && m_cacheFileExistance )
	{
		CcpAutoMutex lock( m_existingFilesMutex );
		m_existingFiles.insert( filename );
	}
	return exists;
}

bool BluePaths::FileExistsLocally( const wchar_t* filename )
{
	if( m_cacheFileExistance )
	{
		CcpAutoMutex lock( m_existingFilesMutex );
		auto found = m_existingFiles.find( filename );
		if( found != end( m_existingFiles ) )
		{
			return true;
		}
	}

	if( BeRemoteFileCache->FileExists( filename ) )
	{
		if( BeRemoteFileCache->IsCachedLocally( filename ) )
		{
			if( m_cacheFileExistance )
			{
				CcpAutoMutex lock( m_existingFilesMutex );
				m_existingFiles.insert( filename );
			}
			return true;
		}
	}

	if( m_localFileSystem->FileExists( filename ) )
	{
		if( m_cacheFileExistance )
		{
			CcpAutoMutex lock( m_existingFilesMutex );
			m_existingFiles.insert( filename );
		}
		return true;
	}

	return false;
}

bool BluePaths::FileNeedsDownload( const wchar_t* filename )
{
	if( BeRemoteFileCache->FileExists( filename ) )
	{
		if( !BeRemoteFileCache->IsCachedLocally( filename ) )
		{
			return true;
		}
	}

	// File is either not available for download, or it is already cached
	return false;
}

void BluePaths::LogPaths()
{
	m_localFileSystem->LogPaths();
}

bool BluePaths::GetStreamFromPathW( const wchar_t* path, IBlueStream** stream )
{
	CCP_ASSERT( stream );
	if( !stream )
	{
		return false;
	}

	Be::Result<std::string> result;

	bool isRes = !wcsncmp(path, L"res:", 4);

	// respreview: is defined in Jessica when in content mode
	bool isResPreview = !wcsncmp(path, L"respreview:", 11);

	std::wstring filenameToOpen = path;
	std::wstring languageSpecificFilename = path;

	bool tryLang = false; //no separate language try
	if( isRes || isResPreview )
	{
		tryLang = AdjustFilenameForLanguageCode( filenameToOpen, languageSpecificFilename );
	}

	if( tryLang )
	{
		if( BePaths->FileExists( languageSpecificFilename ) )
		{
			filenameToOpen = languageSpecificFilename;
		}
	}

	for( auto it = m_resFileSystems.begin(); it != m_resFileSystems.end(); ++it )
	{
		if( (*it)->GetStreamFromPathW( filenameToOpen.c_str(), stream ) )
		{
			return true;
		}
	}

	return false;
}


std::vector<std::wstring> BluePaths::ListDirFromScript( const std::wstring& dir )
{
	std::set<std::wstring> results;
	BePaths->GetDirectoryContents( dir.c_str(), results );

	std::vector<std::wstring> finalResults( results.size() );
	std::copy( results.begin(), results.end(), finalResults.begin() );

	return finalResults;
}

Be::Result<std::string> BluePaths::Open( const std::wstring& filename, Be::Optional<std::string> mode, IBlueStream** stream )
{
	if( mode.IsAssigned() )
	{
		std::string fileMode = mode;
		if( fileMode.size() > 0 )
		{
			if( fileMode[0] == 'w' )
			{
				BlueFileStreamPtr fileStream;
				fileStream.CreateInstance();
				if( fileStream->Create( filename.c_str() ) )
				{
					*stream = fileStream.Detach();
					return Be::Result<std::string>();
				}
				return Be::Result<std::string>("Couldn't create file");
			}
			else if( fileMode[0] == 'a' )
			{
				BlueFileStreamPtr fileStream;
				fileStream.CreateInstance();
				if( fileStream->Open( filename.c_str(), CCP_OM_READWRITE, CCP_SM_NOSHARING ) )
				{
					fileStream->Seek( 0, ICcpStream::SO_END );
					*stream = fileStream.Detach();
					return Be::Result<std::string>();
				}
				return Be::Result<std::string>("Couldn't open file");
			}
		}
	}

	if( GetStreamFromPathW( filename.c_str(), stream ) )
	{
		return Be::Result<std::string>();
	}

	return Be::Result<std::string>("Couldn't open file");
}

Be::Result<std::string> BluePaths::GetFileContentsWithYield( const std::wstring& path, IBlueStream** contents )
{
	Be::Result<std::string> result;
	BackgroundReader* backgroundReader = CCP_NEW( "GetFileContents/reader" ) BackgroundReader( path );
	if( BlueResManBackgroundCall::Issue( backgroundReader ) )
	{
		if( BeIsSuccess( backgroundReader->GetResult() ) )
		{
			backgroundReader->TakeContents( contents );
		}
		else
		{
			*contents = nullptr;
			result = backgroundReader->GetResult();
		}
	}
	else
	{
		result = std::string( "Tasklet killed" );
	}

	CCP_DELETE backgroundReader;

	return result;
}

void BluePaths::InitializeStdAppPaths()
{
	m_localFileSystem->InitializeStdAppPaths();
}

Be::Result<std::string> BluePaths::RegisterFileSystemBeforeLocal( std::string fs )
{
	return RegisterFileSystem( fs, BEFORE );
}

Be::Result<std::string> BluePaths::RegisterFileSystemAfterLocal( std::string fs )
{
	return RegisterFileSystem( fs, AFTER );
}

Be::Result<std::string> BluePaths::RegisterFileSystem( std::string fs, BeforeOrAfter beforeOrAfter )
{
	fs = "BlueResFileSystem" + fs;

	IBlueResFileSystemPtr fileSystem;
	if( !BeClasses->CreateInstanceFromName( fs.c_str(), GetIBlueResFileSystemIID(), (void**)&fileSystem ) )
	{
		std::string result = "Couldn't create class " + fs;
		return Be::Result<std::string>( result );
	}

	if( beforeOrAfter == BEFORE )
	{
		m_resFileSystems.insert( m_resFileSystems.begin(), fileSystem );
	}
	else
	{
		m_resFileSystems.push_back( fileSystem );
	}

	return Be::Result<std::string>();
}

BlueStdResult BluePaths::UnregisterFileSystem( std::string fs )
{
	fs = "BlueResFileSystem" + fs;
	auto found = std::find_if( 
		m_resFileSystems.begin(), 
		m_resFileSystems.end(), 
		[&]( IBlueResFileSystem* x ) { return fs == x->ClassType()->mClassId->GetName(); } );
	if( found == m_resFileSystems.end() )
	{
		return BlueStdResult( BLUE_STD_RESULT_INDEX_ERROR, "File system not found" );
	}
	m_resFileSystems.erase( found );
	return BlueStdResult( BLUE_STD_RESULT_OK );
}

bool BluePaths::IsFileSystemRegistered( std::string fs ) const
{
	fs = "BlueResFileSystem" + fs;
	auto found = std::find_if( 
		m_resFileSystems.begin(), 
		m_resFileSystems.end(), 
		[&]( IBlueResFileSystem* x ) { return fs == x->ClassType()->mClassId->GetName(); } );
	return found != m_resFileSystems.end();
}
