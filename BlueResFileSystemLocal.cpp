////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		November 2014
// Copyright:	CCP 2014
//

#include "StdAfx.h"
#include "BlueResFileSystemLocal.h"
#include "BlueFileStream.h"
#include "Include/IBluePaths.h"
#include "Include/BlueFileUtil.h"

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "BlueResFileSystemLocal" );

#ifdef __ORBIS__
int wcscasecmp(const wchar_t *s1, const wchar_t *s2)
{
	return 0;
}
#endif

namespace
{
#ifdef __ANDROID__
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
#endif

	void ToLower( std::string &s )
	{
		for( size_t i = 0; i < s.size(); ++i )
		{
			s[i] = tolower(s[i]);
		}
	}

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

}


BlueResFileSystemLocal::BlueResFileSystemLocal( IRoot* lockobj /*= nullptr */ )
{
}

bool BlueResFileSystemLocal::Initialize(const std::wstring& initialPath)
{
	m_initialWorkingDirectory = initialPath;

	if (!CcpIsPathDirectory(m_initialWorkingDirectory.c_str())) {
		CCP_LOGWARN("\"%S\" is not a valid directory, falling back to current working directory", m_initialWorkingDirectory.c_str());
		m_initialWorkingDirectory = CcpGetCurrentWorkingDirectory();
	}
	CCP_LOG( "Current working directory is %S", m_initialWorkingDirectory.c_str() );
	InitializeStdAppPaths();

	return true;
}

bool BlueResFileSystemLocal::FileExists( const std::wstring& filename )
{
	std::wstring filenameOnDisk;
	if( ResolvePathW( filename, filenameOnDisk ) )
	{
		return CcpIsPathExistingFile( filenameOnDisk, filenameOnDisk );
	}

	return false;
}

bool BlueResFileSystemLocal::IsDirectory( const std::wstring& dir )
{
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

				if( CcpIsPathDirectory( candidate.c_str() ) )
				{
					return true;
				}
			}
		}
	}

	return false;
}

void BlueResFileSystemLocal::GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results )
{
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

				if( CcpIsPathDirectory( candidate.c_str() ) )
				{
					CcpGetDirectoryContents( candidate, results );
				}
			}
		}
	}
}

bool BlueResFileSystemLocal::GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream )
{
	BlueFileStreamPtr fileStream;
	fileStream.CreateInstance();

	std::wstring filenameOnDisk;
	if( ResolvePathW( resPath, filenameOnDisk ) )
	{
		CCP_LOG_CH( s_ch, "Opening %S (%S)", resPath, filenameOnDisk.c_str() );

		if( fileStream->Open( filenameOnDisk.c_str(), CCP_OM_READONLY, CCP_SM_RWSHARING ) )
		{
			*stream = fileStream.Detach();
			return true;
		}
	}

	return false;
}

void BlueResFileSystemLocal::SetSearchPathW( const char* key, const wchar_t* value )
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

const wchar_t* BlueResFileSystemLocal::GetSearchPathW( const char* key )
{
	return m_searchPaths[key].c_str();
}

void BlueResFileSystemLocal::ClearSearchPaths()
{
	m_searchPaths.clear();
	ExpandSearchPaths();
}

bool BlueResFileSystemLocal::ResolvePathW( const std::wstring& path, std::wstring& resolvedPath )
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
				if( CcpIsPathExistingFile( candidate, candidate ) )
				{
					// Found an existing file
					if( CcpIsPathDirectory( candidate.c_str() ) )
					{
						// Make sure directory paths end with a slash
						if( candidate[candidate.length() - 1] != L'/' )
						{
							candidate.push_back( L'/' );
						}
					}
					resolvedPath = CcpGetAbsolutePath( candidate );
					return true;
				}
			}
		}
	}

	std::wstring absPath;

	// See if this is a valid path already
	if( CcpIsPathExistingFile( path, absPath ) )
	{
		resolvedPath = absPath;
		return true;
	}

	// If we get here then the prefix is either not recognized as a search path
	// or no file was found.

	return false;
}

std::wstring BlueResFileSystemLocal::ResolvePathForWritingW( const std::wstring& path )
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
				return CcpGetAbsolutePath( candidate );
			}
		}
	}

	return CcpGetAbsolutePath( path );
}

std::wstring BlueResFileSystemLocal::ResolvePathToRootW( const std::string& root, const std::wstring& path )
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

void BlueResFileSystemLocal::GetExpandedSearchPaths( const char* key, std::vector<std::wstring>& paths )
{
	std::string keyLower = key;
	ToLower( keyLower );

	ExpandedSearchPathMap_t::iterator found = m_expandedSearchPaths.find( keyLower );
	if( found != m_expandedSearchPaths.end() )
	{
		paths.insert( paths.end(), found->second.begin(), found->second.end() );
	}
}

void BlueResFileSystemLocal::ExpandSearchPaths()
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

void BlueResFileSystemLocal::LogPaths()
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

void BlueResFileSystemLocal::InitializeStdAppPaths()
{
#if CCP_STACKLESS
	std::wstring module = CcpExecutablePath();

#ifdef _WIN32
    const wchar_t *sep = L"\\";
#else
    const wchar_t *sep = L"/";
#endif

    if( m_searchPaths.find( "bin" ) == m_searchPaths.end() )
	{
		//no bin has been set.  By default, it is the directory where we are found
		std::wstring bin = sep;
		std::wstring::size_type s = module.rfind(sep);
		if( s != std::wstring::npos )
		{
			bin = module.substr(0, s);
		}

		m_searchPaths["bin"] = bin;
	}

	if( m_searchPaths.find( "root" ) == m_searchPaths.end() )
	{
		//no root has been set.  By default, it is the directory above the current module
		std::wstring root = module.substr(0, module.rfind(sep));
		if( root.rfind(sep) != std::wstring::npos )
		{
			root = root.substr(0, root.rfind(sep));
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

    const auto stdFolderNamesCount = sizeof(stdFolderNames) / sizeof(stdFolderNames[0]);
	static const wchar_t* defnames[stdFolderNamesCount] =
	{L"res", L"bin", L"lib;stdlib", L"macros", 0, L"script", L"cache", L"cache"};

	for( int i = 0; i < stdFolderNamesCount; ++i )
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
				if( CcpIsPathRelative( s.c_str() ) )
				{
					s.insert( 0, L"app:");
				}

				start = value.find_first_not_of( delimiter, end );
				end = value.find_first_of( delimiter, end + 1 );

				path += s + delimiter;
			}

			std::wstring s = value.substr( start, end - start );
			if( CcpIsPathRelative( s.c_str() ) )
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

std::wstring BlueResFileSystemLocal::GetInitialWorkingDirectory()
{
	return m_initialWorkingDirectory;
}

std::wstring BlueResFileSystemLocal::ResolvePath( const wchar_t* path )
{
	std::wstring result;
	ResolvePathW( path, result );
	return result;
}

std::map<std::string, std::vector<std::wstring>> BlueResFileSystemLocal::GetExpandedSearchPathsAsDict()
{
	return GetExpandedSearchPaths();
}

std::vector<std::wstring> BlueResFileSystemLocal::ListDirFromScript( const std::wstring& dir )
{
	std::set<std::wstring> results;
	GetDirectoryContents( dir.c_str(), results );
	std::vector<std::wstring> list( begin( results ), end( results ) );
	return list;
}
