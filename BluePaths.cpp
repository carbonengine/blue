////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		November 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#include "BluePaths.h"
#include "BlueResFile.h"
#include "Stuffer.h"
#include "RemoteFileCache.h"
#include "include/IBlueResMan.h"
#include "include/BlueFileUtil.h"
#include "Include/IBlueOS.h"
#ifdef __ANDROID__
#include <errno.h>
#endif

IBluePaths* BePaths = nullptr;
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "paths", BePaths );

#if defined(_WIN32)
#elif defined(__ORBIS__)
#include <sys/types.h>
// TODO: Implement these
char *realpath(const char *path, char *resolved_path)
{
	return nullptr;
}
typedef void* DIR;
DIR *opendir(const char *name)
{
	return nullptr;
}
int closedir(DIR *dirp)
{
	return -1;
}
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
	*result = nullptr;
	return 1;
}

#else
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

BLUEIMPORT bool BlueInitializePaths()
{
	return BluePaths::Initialize();
}

namespace
{
#ifdef _WIN32
	std::wstring GetAbsolutePath( const std::wstring& name )
	{
		std::wstring absName;
		wchar_t buffer[MAX_PATH];
		DWORD len = GetFullPathNameW( name.c_str(), MAX_PATH, buffer, NULL );
		if( len > 0 )
		{
			absName = buffer;
			std::replace( absName.begin(), absName.end(), L'\\', L'/' );
		}
		return absName;
	}

	// Returns true iff name is the path of an existing file.
	// adjustedName is the absolute path to the file
	bool IsPathExistingFile( const std::wstring& name, std::wstring& absName )
	{
		wchar_t buffer[MAX_PATH];
		DWORD len = GetFullPathNameW( name.c_str(), MAX_PATH, buffer, NULL );
		if( len > 0 )
		{
			absName = buffer;
			std::replace( absName.begin(), absName.end(), L'\\', L'/' );
			DWORD attr = GetFileAttributesW( buffer );
			if( attr != INVALID_FILE_ATTRIBUTES )
			{
				return true;
			}
		}

		return false;
	}

	bool IsPathDirectory( const wchar_t* name )
	{
		DWORD attr = GetFileAttributesW( name );
		if( attr == INVALID_FILE_ATTRIBUTES )
		{
			return false;
		}
		else
		{
			return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
		}
	}

	void GetDirectoryContentsForPlatform( std::wstring candidate, std::set<std::wstring> &tmpResults )
	{
		std::wstring folderWithWildcard = candidate;
		folderWithWildcard += L"\\*";
		WIN32_FIND_DATAW findFileData;
		HANDLE h = FindFirstFileW( folderWithWildcard.c_str(), &findFileData );
		if( h != INVALID_HANDLE_VALUE )
		{
			BOOL isMore = TRUE;
			do
			{
				bool isSpecial = (wcscmp( findFileData.cFileName, L"." ) == 0);
				isSpecial = isSpecial || (wcscmp( findFileData.cFileName, L".." ) == 0);
				if( !isSpecial )
				{
					std::wstring name = findFileData.cFileName;
					for( std::wstring::iterator it = name.begin(); it != name.end(); ++it )
					{
						*it = tolower( *it );
					}
					tmpResults.insert( name );
				}
				isMore = FindNextFileW( h, &findFileData );
			}
			while( isMore );
		}
	}
#else

	std::wstring GetAbsolutePath( const std::wstring& name )
	{
#ifdef __ORBIS__
		std::wstring absName = name;
		if( absName.find( L"/host/" ) == 0 )
		{
			std::replace( absName.begin() + 6, absName.end(), L'/', L'\\' );
			std::replace( absName.begin(), absName.end(), L'|', L':' );
		}

		return absName;
#else
        char buffer[PATH_MAX];
        if( !realpath( CW2A( name.c_str() ), buffer ) )
        {
            return name;
        }
        return std::wstring( CA2W( buffer ) );
#endif
	}
    
	void BreakPathIntoComponents( const std::string& path, std::vector<std::string>& components )
	{
		size_t endPos = 0;
		size_t startPos = path.find_first_of( '/', endPos );
		while( startPos != std::string::npos )
		{
			++startPos;
			endPos = path.find_first_of( '/', startPos );
			size_t len;
			if( endPos == std::string::npos )
			{
				len = endPos;
			}
			else
			{
				len = endPos - startPos;
			}
			std::string comp = path.substr( startPos, len );

			components.push_back( comp );
			startPos = endPos;
		} 
	}

	// Removes any '.' entries, and collapses '..' entries with the entry preceding it.
	// For example, '/root/a/b/../.././c' becomes '/root/c'.
	void CollapseRelativePaths( std::vector<std::string>& components )
	{
		for( auto it = components.begin(); it != components.end(); )
		{
			if( *it == "." ) 
			{
				it = components.erase( it );
			}
			else
			{
				auto next = it + 1;
				if( next != components.end() )
				{
					if( *next == ".." )
					{
						components.erase( next );
						components.erase( it );
						it = components.begin();
					}
					else
					{
						++it;
					}
				}
				else
				{
					++it;
				}
			}
		}
	}

	void GetPossibleCasings( const std::string& path, const std::string& nextComponent, std::vector<std::string>& possibleCasings )
	{
		DIR* dpdf = opendir( path.c_str() );

		if( dpdf )
		{
			struct dirent entry;
			struct dirent* result;
			int resultCode = readdir_r( dpdf, &entry, &result );
			while( (resultCode == 0) && result )
			{
				if( strcasecmp( nextComponent.c_str(), result->d_name ) == 0 )
				{
					possibleCasings.push_back( result->d_name );
				}
				resultCode = readdir_r( dpdf, &entry, &result );
			}

			closedir( dpdf );
		}
	}

	// Return value indicates whether search should continue or not.
	// If true, continue searching.
	// If false, searching is over. In this case, the result string contains the string if found, otherwise it is empty
	// meaning the search was exhausted without finding a match.
	bool FindCasingCorrectedPath( const std::string& path, const std::vector<std::string>& components, size_t compIx, std::string& result )
	{
		std::vector<std::string> possibleCasings;
		GetPossibleCasings( path, components[compIx], possibleCasings );

		for( auto it = possibleCasings.begin(); it != possibleCasings.end(); ++it )
		{
			std::string candidate = path;
			if( compIx > 0 )
			{
				candidate += '/';
			}
			candidate += *it;

			if( compIx == components.size() - 1 )
			{
				// We're at a leaf node
#ifdef __ORBIS__
				SceKernelStat statData;
				int statResult = sceKernelStat( candidate.c_str(), &statData );
#else
				struct stat statData;
				int statResult = stat( candidate.c_str(), &statData );
#endif
				if( statResult == 0 )
				{
					result = candidate;
					return false;
				}
				else
				{
					return true;
				}
			}
			else		
			{
				bool isMore = FindCasingCorrectedPath( candidate, components, compIx + 1, result );
				if( !isMore )
				{
					return false;
				}
			}
		}

		// Returning true here to keep the search going - we want to force the search to all leaf nodes
		return true;
	}

	bool IsPathExistingFile( const std::wstring& name, std::wstring& adjustedName )
	{
		std::string nameA = (const char*)CW2A( name.c_str() );
#ifdef __ORBIS__
		SceKernelStat statData;
		int statResult = sceKernelStat( nameA.c_str(), &statData );
#else
		struct stat statData;
		int statResult = stat( nameA.c_str(), &statData );
#endif

		if( statResult != 0 )
		{
			// Path not found - try to correct casing
			std::vector<std::string> components;
			BreakPathIntoComponents( nameA, components );
			CollapseRelativePaths( components );

			std::string root = "/";
			std::string adjustedNameA;
			FindCasingCorrectedPath( root, components, 0, adjustedNameA );
			if( !adjustedNameA.empty() )
			{
				adjustedName = CA2W( adjustedNameA.c_str() );
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return true;
		}
	}

	bool IsPathDirectory( const wchar_t* name )
	{
		std::string nameA = (const char*)CW2A( name );
#ifdef __ORBIS__
		SceKernelStat statData;
		int statResult = sceKernelStat( nameA.c_str(), &statData );
#else
		struct stat statData;
		int statResult = stat( nameA.c_str(), &statData );
#endif
		if( statResult == 0 )
		{
			return S_ISDIR( statData.st_mode );
		}
		else
		{
			return false;
		}
	}

	void GetDirectoryContentsForPlatform( std::wstring candidate, std::set<std::wstring> &tmpResults )
	{
		std::string folderName = (const char*)CW2A( candidate.c_str() );
		DIR* dpdf = opendir( folderName.c_str() );

		if( dpdf )
		{
			struct dirent entry;
			struct dirent* result;
			int resultCode = readdir_r( dpdf, &entry, &result );
			while( (resultCode == 0) && result )
			{
				bool isSpecial = (strcmp( result->d_name, "." ) == 0);
				isSpecial = isSpecial || (strcmp( result->d_name, ".." ) == 0);
				if( !isSpecial )
				{
					std::wstring name = (const wchar_t*)CA2W( result->d_name );
					for( std::wstring::iterator it = name.begin(); it != name.end(); ++it )
					{
						*it = tolower( *it );
					}
					tmpResults.insert( name );
				}
				resultCode = readdir_r( dpdf, &entry, &result );
			}

			closedir( dpdf );
		}
	}
#endif
}

BluePaths::BluePaths( IRoot* lockobj /*= NULL */ ) :
	m_searchPaths( "BlueOS/m_searchPaths" ),
	m_expandedSearchPaths( "BlueOS/m_expandedSearchPaths" )
{
}

BluePaths::~BluePaths()
{
}

bool BluePaths::Initialize()
{
	static CBluePaths instance;
	BePaths = &instance;

	return instance.InitializeHelper();
}

#ifdef __ORBIS__
char *getcwd(char *buf, size_t size)
{
	return nullptr;
}
#endif

bool BluePaths::InitializeHelper()
{
#ifdef _WIN32
	wchar_t cwdBuffer[MAX_PATH];
	int cdRes = GetCurrentDirectoryW( MAX_PATH, cwdBuffer );
	if( cdRes > 0 )
	{
		m_initialWorkingDirectory = cwdBuffer;
		std::replace( m_initialWorkingDirectory.begin(), m_initialWorkingDirectory.end(), L'\\', L'/' );

		CCP_LOG( "Current working directory is %S", m_initialWorkingDirectory.c_str() );
	}
	else
	{
		CCP_LOGERR( "Couldn't get current directory (%d, %d)", cdRes, GetLastError() );
	}
#else
	static const int BUFFER_SIZE = 1024;
	char cwdBuffer[BUFFER_SIZE];
	if( getcwd( cwdBuffer, BUFFER_SIZE ) )
	{
		m_initialWorkingDirectory = (const wchar_t*)CA2W( cwdBuffer );
	}
	else
	{
		CCP_LOGERR( "Couldn't get current directory (%d)", errno );
	}
#endif

#if CCP_STACKLESS
	InitializeStdAppPaths();
#endif

	return true;
}

void BluePaths::SetSearchPathW( const char* key, const wchar_t* value )
{
	if( !value || wcslen( value ) == 0 )
	{
		m_searchPaths.erase( key );
	}
	else
	{
		std::wstring s = value;
		std::replace( s.begin(), s.end(), L'\\', L'/' );
		m_searchPaths[key] = s;
	}

	ExpandSearchPaths();
}

const wchar_t* BluePaths::GetSearchPathW( const char* key )
{
	return m_searchPaths[key].c_str();
}

void BluePaths::ClearSearchPaths()
{
	m_searchPaths.clear();
	ExpandSearchPaths();
}

static void ToLower( std::string &s )
{
	for( size_t i = 0; i < s.size(); ++i )
	{
		s[i] = tolower(s[i]);
	}
}

std::wstring BluePaths::ResolvePathW( const std::wstring& path )
{
	// Look at the prefix - anything in front of ':' is a search
	// path that will be substituted.
	std::wstring value( path );
	size_t pos = value.find_first_of( L':' );
	if( pos != std::wstring::npos )
	{
		std::wstring keyW = value.substr( 0, pos );
		std::string key = (const char*)CW2A( keyW.c_str() );
		ToLower( key );

		// We have a prefix, look it up in our search paths map.
		// If found, the value associated with it is a list of paths.
		ExpandedSearchPathMap_t::iterator found = m_expandedSearchPaths.find( key );
		if( found != m_expandedSearchPaths.end() )
		{
			std::wstring rest = value.substr( pos + 1 );
			std::vector<std::wstring>& keyComponents = found->second;

			// We now have the list of paths in 'keyComponents' - try each
			// of them in turn and see if replacing the prefix with that
			// entry gives us the name of an existing file.
			std::vector<std::wstring>::iterator keyCompIt;
			for( keyCompIt = keyComponents.begin(); keyCompIt != keyComponents.end(); ++keyCompIt )
			{
				std::wstring candidate = *keyCompIt;
				if( !rest.empty() )
				{
					candidate += rest;
				}

				std::replace( candidate.begin(), candidate.end(), L'\\', L'/' );
				if( IsPathExistingFile( candidate, candidate ) )
				{
					// Found an existing file
					if( IsPathDirectory( candidate.c_str() ) )
					{
						// Make sure directory paths end with a slash
						if( candidate[candidate.length() - 1] != L'/' )
						{
							candidate.push_back( L'/' );
						}
					}
					return GetAbsolutePath( candidate );
				}
			}
		}
	}

	std::wstring absPath;

	// See if this is a valid path already
	if( IsPathExistingFile( path, absPath ) )
	{
		return absPath;
	}

	// If we get here then the prefix is either not recognized as a search path
	// or no file was found. Return the first possible expansion of a file name.

	return ResolvePathForWritingW( path );

}

std::wstring BluePaths::ResolvePathForWritingW( const std::wstring& path )
{
	// This function is similar to ResolvePathW except it doesn't
	// search through all search paths to see if a file exists.
	// We simply return the first possible expansion of a file name.
	std::wstring value( path );
	size_t pos = value.find_first_of( L':' );
	if( pos != std::wstring::npos )
	{
		std::wstring keyW = value.substr( 0, pos );
		std::string key = (const char*)CW2A( keyW.c_str() );
		ToLower( key );

		ExpandedSearchPathMap_t::iterator found = m_expandedSearchPaths.find( key );
		if( found != m_expandedSearchPaths.end() )
		{
			std::wstring rest = value.substr( pos + 1 );
			std::vector<std::wstring>& keyComponents = found->second;
			if( !keyComponents.empty() )
			{
				std::vector<std::wstring>::iterator keyCompIt = keyComponents.begin();
				std::wstring candidate = *keyCompIt;
				if( !rest.empty() )
				{
					candidate += rest;
				}
				return GetAbsolutePath( candidate );
			}
		}
	}

	return GetAbsolutePath( path );
}

#ifdef __ORBIS__
int wcscasecmp(const wchar_t *s1, const wchar_t *s2)
{
	return 0;
}
#endif

#ifdef __ANDROID__
namespace
{
    int wcscasecmp( const wchar_t* s1, const wchar_t* s2 )
    {
        for( ; *s1; s1++, s2++ )
        {
            wchar_t c1 = towlower( *s1 );
            wchar_t c2 = towlower( *s2 );
            if( c1 != c2 )
            {
                return int( c1 - c2 );
            }
        }
        return -*s2;
    }
}
#endif

bool AreStringsEqualIgnoringCase( const std::wstring& a, const std::wstring& b )
{
#ifdef _MSC_VER
	size_t len = a.size();
	if( _wcsnicmp( a.c_str(), b.c_str(), len ) == 0 )
	{
		return true;
	}
	else
	{
		return false;
	}
#else
	if( wcscasecmp( a.c_str(), b.c_str() ) == 0 )
	{
		return true;
	}
	else
	{
		return false;
	}
#endif
}

std::wstring BluePaths::ResolvePathToRootW( const std::string& root, const std::wstring& path )
{
	std::string rootLower = root;
	ToLower( rootLower );

	ExpandedSearchPathMap_t::iterator found = m_expandedSearchPaths.find( rootLower );
	if( found != m_expandedSearchPaths.end() )
	{
		std::wstring normPath = ConvertRelativeToAbsolutePath( path.c_str() );
		NormalizeSlashes( normPath );

		std::vector<std::wstring>& keyComponents = found->second;
		std::vector<std::wstring>::iterator keyCompIt;
		for( keyCompIt = keyComponents.begin(); keyCompIt != keyComponents.end(); ++keyCompIt )
		{
			std::wstring value = ConvertRelativeToAbsolutePath( keyCompIt->c_str() );
			NormalizeSlashes( value );

			size_t len = value.size();
			if( AreStringsEqualIgnoringCase( value, normPath ) )
			{
				std::wstring result = (const wchar_t*)CA2W( root.c_str() );
				result += L":";
				if( normPath[len] != L'/' )
				{
					// Only add the slash if it's not already present in 'path'
					result += L'/';
				}
				result += normPath.substr( len );

				return result;
			}
		}
	}

	return L"";
}

void BluePaths::GetExpandedSearchPaths( const char* key, std::vector<std::wstring>& paths )
{
	std::string keyLower = key;
	ToLower( keyLower );

	ExpandedSearchPathMap_t::iterator found = m_expandedSearchPaths.find( keyLower );
	if( found != m_expandedSearchPaths.end() )
	{
		paths.insert( paths.end(), found->second.begin(), found->second.end() );
	}
}

void BluePaths::GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results )
{
	std::set<std::wstring> tmpResults;

#if STUFFER_ENABLED
	// TODO: If we change the search priority of stuff files, move this
	// to below the directory scanning.
	Stuffer::GetStufferDirectoryContents( CW2A(dir), tmpResults );
#endif

#if USE_RESFILE_2
	std::list<std::string> remoteFileCacheContents;
	auto remoteFileCacheResult = BeRemoteFileCache->ListDir( dir, remoteFileCacheContents );
	if( Be::IsSuccess( remoteFileCacheResult ) )
	{
		for( auto it = remoteFileCacheContents.begin(); it != remoteFileCacheContents.end(); ++it )
		{
			std::wstring ws = static_cast<const wchar_t*>( CA2W( it->c_str() ) );
			tmpResults.insert( ws );
		}
	}
#endif

	// Look at the prefix - anything in front of ':' is a search
	// path that will be substituted.
	std::wstring value( dir );
	size_t pos = value.find_first_of( L':' );
	if( pos != std::wstring::npos )
	{
		std::wstring keyW = value.substr( 0, pos );
		std::string key = (const char*)CW2A( keyW.c_str() );
		ToLower( key );

		// We have a prefix, look it up in our search paths map.
		// If found, the value associated with it is a list of paths.
		ExpandedSearchPathMap_t::iterator found = m_expandedSearchPaths.find( key );
		if( found != m_expandedSearchPaths.end() )
		{
			std::wstring rest = value.substr( pos + 1 );
			std::vector<std::wstring>& keyComponents = found->second;

			// We now have the list of paths in 'keyComponents' - try each
			// of them in turn and see if replacing the prefix with that
			// entry gives us the name of an existing folder.
			std::vector<std::wstring>::iterator keyCompIt;
			for( keyCompIt = keyComponents.begin(); keyCompIt != keyComponents.end(); ++keyCompIt )
			{
				std::wstring candidate = *keyCompIt;
				if( !rest.empty() )
				{
					candidate += rest;
				}

				if( IsPathDirectory( candidate.c_str() ) )
				{
					GetDirectoryContentsForPlatform(candidate, tmpResults);
				}
			}
		}
	}


	if( BeResMan->GetSubstituteBlackForRed() )
	{
		for( auto it = tmpResults.begin(); it != tmpResults.end(); ++it )
		{
			auto filename = *it;
			const wchar_t *dot = wcsrchr(filename.c_str(), L'.');
			if( dot && wcscmp( dot, L".black" ) == 0 )
			{
				std::wstring redFilename = filename;
				redFilename.resize( redFilename.size() - 5 ); // strip off black
				redFilename.append( L"red" );
				results.insert( redFilename );
			}
			else
			{
				results.insert( filename );
			}
		}
	}
	else
	{
		results.insert( tmpResults.begin(), tmpResults.end() );
	}
}

bool BluePaths::IsDirectory( const std::wstring& dir )
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

#if USE_RESFILE_2
	if( BeRemoteFileCache->IsDirectory( dir.c_str() ) )
	{
		return true;
	}
#endif

	// Look at the prefix - anything in front of ':' is a search
	// path that will be substituted.
	std::wstring value( dir );
	size_t pos = value.find_first_of( L':' );
	if( pos != std::wstring::npos )
	{
		std::wstring keyW = value.substr( 0, pos );
		std::string key = (const char*)CW2A( keyW.c_str() );
		ToLower( key );

		// We have a prefix, look it up in our search paths map.
		// If found, the value associated with it is a list of paths.
		ExpandedSearchPathMap_t::iterator found = m_expandedSearchPaths.find( key );
		if( found != m_expandedSearchPaths.end() )
		{
			std::wstring rest = value.substr( pos + 1 );
			std::vector<std::wstring>& keyComponents = found->second;

			// We now have the list of paths in 'keyComponents' - try each
			// of them in turn and see if replacing the prefix with that
			// entry gives us the name of an existing folder.
			std::vector<std::wstring>::iterator keyCompIt;
			for( keyCompIt = keyComponents.begin(); keyCompIt != keyComponents.end(); ++keyCompIt )
			{
				std::wstring candidate = *keyCompIt;
				if( !rest.empty() )
				{
					candidate += rest;
				}

				if( IsPathDirectory( candidate.c_str() ) )
				{
					return true;
				}
			}
		}
	}

	return false;
}

void BluePaths::ExpandSearchPaths()
{
	const wchar_t delimiter = L';';

	// Start from scratch each time
	m_expandedSearchPaths.clear();

	// Expand the semi-colon delimited search path entries in m_searchPaths.
	// Each component is added as a separate string in a list of entries
	// stored under the same key as was used in m_searchPaths.
	for( SearchPathMap_t::const_iterator it = m_searchPaths.begin(); it != m_searchPaths.end(); ++it )
	{
		std::string key = it->first;
		ToLower( key );

		const std::wstring& value = it->second;

		std::vector<std::wstring> components;
		size_t start = 0;
		size_t end = value.find_first_of( delimiter );

		while( end != std::wstring::npos )
		{
			std::wstring s = value.substr( start, end - start );
			components.push_back( s );

			start = value.find_first_not_of( delimiter, end );
			end = value.find_first_of( delimiter, end + 1 );
		}

		std::wstring s = value.substr( start, end - start );
		components.push_back( s );

		m_expandedSearchPaths[key] = components;
	}

	// Expand any entries in m_expandedSearchPaths that use a defined search
	// path - i.e. where a ':' is found in the path. Keep doing this until
	// nothing is found to expand.
	bool continueExpanding = true;
	while( continueExpanding )
	{
		continueExpanding = false;

		ExpandedSearchPathMap_t::iterator it;
		for( it = m_expandedSearchPaths.begin(); it != m_expandedSearchPaths.end(); ++it )
		{
			bool somethingChanged = false;

			std::vector<std::wstring>& components = it->second;
			std::vector<std::wstring> newComponents;

			std::vector<std::wstring>::iterator innerIt;
			for( innerIt = components.begin(); innerIt != components.end(); ++innerIt )
			{
				const std::wstring& value = *innerIt;

				size_t pos = value.find_first_of( L':' );

				// The check for pos > 1 is to handle drive letters
				if( (pos != std::wstring::npos) && (pos > 1) )
				{
					somethingChanged = true;

					std::wstring keyW = value.substr( 0, pos );
					std::string key = (const char*)CW2A( keyW.c_str() );
					ToLower( key );

					ExpandedSearchPathMap_t::iterator found = m_expandedSearchPaths.find( key );
					if( found != m_expandedSearchPaths.end() )
					{
						std::wstring rest = value.substr( pos + 1 );
						std::vector<std::wstring>& keyComponents = found->second;
						std::vector<std::wstring>::iterator keyCompIt;
						for( keyCompIt = keyComponents.begin(); keyCompIt != keyComponents.end(); ++keyCompIt )
						{
							std::wstring expandedValue = *keyCompIt;
							if( !rest.empty() )
							{
								if( !expandedValue.empty() && expandedValue[expandedValue.length() - 1] != L'/' &&
									!rest.empty() && rest[0] != L'/' )
								{
									expandedValue += L'/';
								}
								expandedValue += rest;
							}
							newComponents.push_back( expandedValue );
						}
					}
				}
				else
				{
					// Some other entry might get expanded, need to add the original
					// value to the list here so it won't be lost if the list is
					// replaced here below.
					newComponents.push_back( value );
				}
			}

			if( somethingChanged )
			{
				// New entries were added
				it->second = newComponents;
				continueExpanding = true;
			}
		}
	}

	ExpandedSearchPathMap_t::iterator it;
	for( it = m_expandedSearchPaths.begin(); it != m_expandedSearchPaths.end(); ++it )
	{
		std::vector<std::wstring>& components = it->second;
		std::vector<std::wstring>::iterator innerIt;
		for( innerIt = components.begin(); innerIt != components.end(); ++innerIt )
		{
			std::wstring& value = *innerIt;

			if( value[0] == L'.' )
			{
				value = m_initialWorkingDirectory + L'/' + value;
			}
		}
	}
}

#if BLUE_WITH_PYTHON
PyObject* BluePaths::GetAllSearchPathsAsDict()
{
	PyObject* dict = PyDict_New();

	for( SearchPathMap_t::const_iterator it = m_searchPaths.begin(); it != m_searchPaths.end(); ++it )
	{
		const std::wstring& entry = it->second;
		PyObject* entryPy = PyUnicode_FromWideChar( entry.c_str(), entry.size() );
		PyDict_SetItemString( dict, it->first.c_str(), entryPy );
		Py_DECREF( entryPy );
	}

	return dict;
}

PyObject* BluePaths::GetExpandedSearchPathsAsDict()
{
	PyObject* dict = PyDict_New();

	for( ExpandedSearchPathMap_t::const_iterator it = m_expandedSearchPaths.begin(); it != m_expandedSearchPaths.end(); ++it )
	{
		size_t n = it->second.size();
		PyObject* list = PyList_New( n );
		for( unsigned int i = 0; i < n; ++i )
		{
			const std::wstring& entry = it->second[i];
			PyObject* entryPy = PyUnicode_FromWideChar( entry.c_str(), entry.size() );
			PyList_SET_ITEM( list, i, entryPy );
		}
		PyDict_SetItemString( dict, it->first.c_str(), list );
		Py_DECREF( list );
	}

	return dict;
}
#endif

bool BluePaths::FileExistsWithoutSubstitution( const wchar_t* filename )
{
#if STUFFER_ENABLED
	if( wcsncmp( filename, L"res:", 4 ) == 0 )
	{
		if( BeStuffer->HasData( filename + 4 ) )
		{
			return true;
		}
	}
#endif

#if USE_RESFILE_2
	if( BeRemoteFileCache->FileExists( filename ) )
	{
		return true;
	}
#endif

	std::wstring filenameOnDisk = ResolvePathW( filename );

	return IsPathExistingFile( filenameOnDisk, filenameOnDisk );
}

bool BluePaths::FileExists( const std::wstring& filename )
{
	if( BeResMan->GetSubstituteBlackForRed() )
	{
		const wchar_t *dot = wcsrchr(filename.c_str(), L'.');
		if( dot && wcscmp( dot, L".red" ) == 0 )
		{
			std::wstring blackFilename = filename;
			blackFilename.resize( blackFilename.size() - 3 ); // strip off red
			blackFilename.append( L"black" );

			if( FileExistsWithoutSubstitution( blackFilename.c_str() ) )
			{
				return true;
			}
		}
	}

	return FileExistsWithoutSubstitution( filename.c_str() );
}

void BluePaths::LogPaths()
{
	CCP_LOG( "BlueOS search paths:" );
	for( ExpandedSearchPathMap_t::const_iterator it = m_expandedSearchPaths.begin(); it != m_expandedSearchPaths.end(); ++it )
	{
		size_t n = it->second.size();
		CCP_LOG( "%s:", it->first.c_str() );
		for( unsigned int i = 0; i < n; ++i )
		{
			const std::wstring& entry = it->second[i];
			CCP_LOG( "    %S:", entry.c_str() );
		}
	}
}

bool BluePaths::GetStreamFromPathW( const wchar_t* path, IBlueStream** stream )
{
	CCP_ASSERT( stream );
	if( !stream )
	{
		return false;
	}

	CCP_LOG( "Opening %S", path );

	IResFilePtr tmp;

	if( !tmp.CreateInstance( GetResFileClsid() ) )
	{
		return false;
	}

	if( !tmp->OpenW( path, true ) )
	{
		BeOS->SetError(
			BEDEF, Clsid(), 
			"Couldn't open file '%S'", path
			);

		return false;
	}

	*stream = tmp.Detach();

	return true;
}

void BluePaths::InitializeStdAppPaths()
{
#if CCP_STACKLESS
	//get module name
	std::vector<wchar_t> tmp(MAX_PATH);
	while( GetModuleFileNameW(NULL, &tmp[0], (DWORD)tmp.size()) == tmp.size() )
	{
		tmp.resize(tmp.size()*2);
	}
	std::wstring module = &tmp[0];

	if( m_searchPaths.find( "bin" ) == m_searchPaths.end() )
	{
		//no bin has been set.  By default, it is the directory where we are found
		std::wstring bin = L"\\";
		std::wstring::size_type s = module.rfind(L"\\");
		if( s != std::wstring::npos )
		{
			bin = module.substr(0, s);
		}

		m_searchPaths["bin"] = bin;
	}

	if( m_searchPaths.find( "root" ) == m_searchPaths.end() )
	{
		//no root has been set.  By default, it is the directory above the current module
		std::wstring root = module.substr(0, module.rfind(L"\\"));
		if( root.rfind(L"\\") != std::wstring::npos )
		{
			root = root.substr(0, root.rfind(L"\\"));
		}

		m_searchPaths["root"] = root;
	}

	if( m_searchPaths.find( "app" ) == m_searchPaths.end() )
	{
		//no app has been set.  By default, it is the same as root
		m_searchPaths["app"] = L"root:/";
	}

	// fixup the rest.  cache and settings both point to root/cache originally.
	static const char* stdFolderNames[] =
	{"res", "bin", "lib", "macro", "stdlib", "script", "cache", "settings" };

	static const wchar_t* defnames[_countof(stdFolderNames)] = 
	{L"res", L"bin", L"lib;stdlib", L"macros", 0, L"script", L"cache", L"cache"};

	for( int i = 0; i < _countof(stdFolderNames); ++i )
	{
		if( m_searchPaths.find( stdFolderNames[i] ) == m_searchPaths.end() )
		{
			std::wstring value;

			if( !defnames[i] )
			{
				continue;
			}
			value = defnames[i];

			const wchar_t delimiter = L';';

			std::wstring path;

			size_t start = 0;
			size_t end = value.find_first_of( delimiter );

			while( end != std::wstring::npos )
			{
				std::wstring s = value.substr( start, end - start );
				if( PathIsRelativeW( s.c_str() ) )
				{
					s.insert( 0, L"app:");
				}

				start = value.find_first_not_of( delimiter, end );
				end = value.find_first_of( delimiter, end + 1 );

				path += s + delimiter;
			}

			std::wstring s = value.substr( start, end - start );
			if( PathIsRelativeW( s.c_str() ) )
			{
				s.insert( 0, L"app:");
			}
			path += s;

			m_searchPaths[stdFolderNames[i]] = path;
		}
	}
	ExpandSearchPaths();
#endif
}

std::vector<std::wstring> BluePaths::ListDirFromScript( const std::wstring& dir )
{
	std::set<std::wstring> results;
	BePaths->GetDirectoryContents( dir.c_str(), results );

	std::vector<std::wstring> finalResults( results.size() );
	std::copy( results.begin(), results.end(), finalResults.begin() );

	return finalResults;
}

Be::Result<std::string> BluePaths::Open( const std::wstring& filename, const std::string& mode, IBlueStream** stream )
{
	if( GetStreamFromPathW( filename.c_str(), stream ) )
	{
		return Be::Result<std::string>();
	}

	return Be::Result<std::string>("Couldn't open file");
}
