// Copyright © 2010 CCP ehf.

#include "StdAfx.h"
#include "BlueFileUtil.h"
#include <vector>
#include <cctype>
#include <cwctype>
#include "IBlueOS.h"
#include "IBluePaths.h"

// --------------------------------------------------------------------------------------
// Description:
//   Normalizes resource (res:/...) path: makes it lowercase,  converts back slashes to 
//   forward slashes, removes redundant slashes, handles '.' and '..' directories. 
//   If the input resource path is not a valid resource path (does not start with 'res:/' 
//   or contains too many '..') the function copies the original unmodified path to 
//   output string and returns  false.  Works with wide-character strings
// Arguments:
//   path - Resource path to normalize
//   result (out) - Normalized resource path
// Return value:
//   true If the input resource path was valid
//   false If the input resource path was invalid
// --------------------------------------------------------------------------------------
bool NormalizeResPath( const wchar_t* path, std::wstring& result )
{
	if( !path )
	{
		result = L"";
		return false;
	}

	if( wcsncmp( path, L"dynamic:/", 9 ) == 0 || wcsncmp( path, L"dynamic:\\", 9 ) == 0 )
	{
		result = path;
		const wchar_t* slash = wcschr( path + 9, L'/' );
		const wchar_t* backSlash = wcschr( path + 9, L'\\' );
		if ( slash == 0 || (backSlash && backSlash < slash) )
		{
			slash = backSlash;
		}
		if( slash == 0 )
		{
			slash = path + wcslen( path );
		}
		if( slash == path )
		{
			return false;
		}
		for( int i = 0; i < slash - path; ++i )
		{
			result[i] = std::towlower( result[i] );
		}
		if( slash - path < ptrdiff_t( result.length() ) && result[slash - path] == L'\\' )
		{
			result[slash - path] = L'/';
		}
		return true;
	}
	
	if( wcsncmp( path, L"res:/", 5 ) != 0 && wcsncmp( path, L"res:\\", 5 ) != 0 )
	{
		result = path;
		return false;
	}

	size_t length = wcslen( path );
	result.reserve( length );
	result = L"res:/";

	std::vector<size_t> components;
	components.reserve( length );

	const wchar_t *currentStart = path + 5;
	for( const wchar_t *current = currentStart; ; ++current )
	{
		if( *current == L'/' || *current == L'\\' || *current == 0 )
		{
			size_t componentLength = current - currentStart;
			if( componentLength > 0 && ( componentLength > 1 || *currentStart != L'.' ) )
			{
				if( componentLength == 2 && currentStart[0] == L'.' && currentStart[1] == L'.' )
				{
					if( components.empty() )
					{
						result = path;
						return false;
					}
					result.resize( components.back() );
					components.pop_back();
				}
				else
				{
					size_t prevLength = result.length();
					if( !components.empty() )
					{
						result.append( 1, L'/' );
					}
					for( const wchar_t *letter = currentStart; letter < current; ++letter )
					{
						result.append( 1, std::tolower( *letter ) );
					}
					//result.append( currentStart, componentLength );
					components.push_back( prevLength );
				}
			}
			currentStart = current + 1;
		}
		if( *current == 0 )
		{
			break;
		}
	}

	return true;
}

// --------------------------------------------------------------------------------------
// Description:
//   Normalizes resource (res:/...) path: makes it lowercase,  converts back slashes to 
//   forward slashes, removes redundant slashes, handles '.' and '..' directories. 
//   If the input resource path is not a valid resource path (does not start with 'res:/' 
//   or contains too many '..') the function copies the original unmodified path to 
//   output string and returns  false.  Works with narrow-character strings
// Arguments:
//   path - Resource path to normalize
//   result (out) - Normalized resource path
// Return value:
//   true If the input resource path was valid
//   false If the input resource path was invalid
// --------------------------------------------------------------------------------------
bool NormalizeResPath( const char* path, std::string& result )
{
	if( !path )
	{
		result = "";
		return false;
	}

	if( strncmp( path, "dynamic:/", 9 ) == 0 )
	{
		result = path;
		const char* slash = strchr( path + 9, '/' );
		const char* backSlash = strchr( path + 9, '\\' );
		if ( slash == 0 || ( backSlash && backSlash < slash ) )
		{
			slash = backSlash;
		}
		if( slash == 0 )
		{
			slash = path + strlen( path );
		}
		if( slash == path )
		{
			return false;
		}
		for( int i = 0; i < slash - path; ++i )
		{
			result[i] = std::tolower( result[i] );
		}
		if( result[slash - path] == '\\' )
		{
			result[slash - path] = '/';
		}
		return true;
	}
	
	if( strncmp( path, "res:/", 5 ) != 0 )
	{
		result = path;
		return false;
	}

	size_t length = strlen( path );
	result.reserve( length );
	result = "res:/";

	std::vector<size_t> components;
	components.reserve( length );

	const char* currentStart = path + 5;
	for( const char* current = currentStart; ; ++current )
	{
		if( *current == '/' || *current == '\\' || *current == 0 )
		{
			size_t componentLength = current - currentStart;
			if( componentLength > 0 && ( componentLength > 1 || *currentStart != '.' ) )
			{
				if( componentLength == 2 && currentStart[0] == '.' && currentStart[1] == '.' )
				{
					if( components.empty() )
					{
						result = path;
						return false;
					}
					result.resize( components.back() );
					components.pop_back();
				}
				else
				{
					size_t prevLength = result.length();
					if( !components.empty() )
					{
						result.append( 1, '/' );
					}
					for( const char* letter = currentStart; letter < current; ++letter )
					{
						result.append( 1, std::tolower( *letter ) );
					}
					//result.append( currentStart, componentLength );
					components.push_back( prevLength );
				}
			}
			currentStart = current + 1;
		}
		if( *current == 0 )
		{
			break;
		}
	}

	return true;
}

// --------------------------------------------------------------------------------------
// Description:
//   Extracts protocol name ("res", "dynamic", etc.) from resource path. Protocol name is
//   the string preceeding ':' character in the path.
// Arguments:
//   path - Resource path to normalize
//   result (out) - Protocol name
// Return value:
//   true If the resource path contains protocol
//   false If the resource path does not contain protocol (invalid)
// --------------------------------------------------------------------------------------
bool GetResProtocol( const wchar_t* path, std::wstring &result )
{
	const wchar_t* pos = wcschr( path, ':' );
	if( pos == NULL )
	{
		result = L"";
		return false;
	}

	result = std::wstring( path, pos - path );
	return true;
}

// --------------------------------------------------------------------------------------
// Description:
//   Extracts protocol name ("res", "dynamic", etc.) from resource path. Protocol name is
//   the string preceeding ':' character in the path.
// Arguments:
//   path - Resource path to normalize
//   result (out) - Protocol name
// Return value:
//   true If the resource path contains protocol
//   false If the resource path does not contain protocol (invalid)
// --------------------------------------------------------------------------------------
bool GetResProtocol( const char* path, std::string &result )
{
	const char* pos = strchr( path, ':' );
	if( pos == NULL )
	{
		result = "";
		return false;
	}

	result = std::string( path, pos - path );
	return true;
}

BLUEIMPORT void NormalizeSlashes( std::wstring& fname )
{
	std::wstring::size_type i;
	for(i = 0; i < fname.size(); ++i) {
		if (fname[i] == L'\\')
			fname[i] = L'/';
		if (i && fname[i] == L'/' && fname[i-1] == L'/') {
			fname.erase(i, 1);
			--i;
		}
	}
}

BLUEIMPORT std::wstring ConvertRelativeToAbsolutePath( const wchar_t* path )
{
#ifdef _WIN32
	wchar_t normPathTmp[MAX_PATH];
	DWORD len = GetFullPathNameW( path, MAX_PATH, normPathTmp, NULL );
	if( len == 0 )
	{
		normPathTmp[0] = 0;
	}
#else
	CW2A pathA( path );
	char normPathTmpA[PATH_MAX];
	char* resolvedPath = realpath( pathA, normPathTmpA );
	if( !resolvedPath )
	{
		return L"";
	}
	CA2W tmp( resolvedPath );
	const wchar_t* normPathTmp = (const wchar_t*)tmp;
#endif

	std::wstring result = normPathTmp;
	return result;
}

// Description:
// Adjusts the filename according to language id set, and builds language specific filename
// if needed.
// Arguments:
//   filenameToOpen
//   languageSpecificFilename
// Return value:
//   Returns true if language specific file should be searched for.
//   Returns false when the language specific file is not needed.
bool AdjustFilenameForLanguageCode( std::wstring &filenameToOpen, std::wstring &languageSpecificFilename )
{
	bool tryLang = false;

	const wchar_t* filename = filenameToOpen.c_str();

	//language code management.  Find last dot.  And if we find a dot behind it with upper alpha chars, they are a LC
	const wchar_t* lastDot = wcsrchr(filename, L'.');

	if( lastDot )
	{
		const wchar_t* languageCodeFromFilename = 0;
		if( lastDot-3 >= filename && lastDot[-3] == L'.' )
		{
			if (std::iswupper(lastDot[-2]) && std::iswalpha(lastDot[-2]) &&
				std::iswupper(lastDot[-1]) && std::iswalpha(lastDot[-1]) )
			{
				languageCodeFromFilename = lastDot-2;
			}
		}

		//get language code, or clear for English.
		std::wstring lc;
		if( !languageCodeFromFilename )
		{
			lc = BeOS->GetLanguageId();
			if (lc == L"EN")
			{
				lc.clear();
			}
		}
		if( !languageCodeFromFilename && lc.size() )
		{
			//a global language code is in effect, try using that first.
			languageSpecificFilename = filename;
			languageSpecificFilename.insert(lastDot-filename, L"."+lc);
			tryLang = true;
		}
		else if (languageCodeFromFilename)
		{
			//special case for a given EN language code
			if( !wcsncmp(languageCodeFromFilename, L"EN", 2) )
			{
				languageSpecificFilename = filename;
				languageSpecificFilename.erase(languageCodeFromFilename-filename, 3); //remove the EN. dude
				filenameToOpen = languageSpecificFilename;
			}
		}
	}

	return tryLang;
}


std::wstring SubstituteBlackForRedInFilename( const std::wstring& filename )
{
	const wchar_t *dot = wcsrchr(filename.c_str(), L'.');
	if( dot && wcscmp( dot, L".red" ) == 0 )
	{
		std::wstring blackFilename = filename;
		blackFilename.resize( blackFilename.size() - 3 ); // strip off red
		blackFilename.append( L"black" );

		return blackFilename;
	}

	return filename;
}


std::wstring SubstituteRedForBlackInFilename( const std::wstring& filename )
{
	const wchar_t *dot = wcsrchr(filename.c_str(), L'.');
	if( dot && wcscmp( dot, L".black" ) == 0 )
	{
		std::wstring blackFilename = filename;
		blackFilename.resize( blackFilename.size() - 5 ); // strip off black
		blackFilename.append( L"red" );

		return blackFilename;
	}

	return filename;
}


