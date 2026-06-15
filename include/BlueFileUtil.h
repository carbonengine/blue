// Copyright © 2010 CCP ehf.

#pragma once
#ifndef BlueFileUtil_H
#define BlueFileUtil_H

#include <string>

// Normalize wide-string paths
BLUEIMPORT bool NormalizeResPath( const wchar_t* path, std::wstring &result );

// Normalize narrow-string paths
BLUEIMPORT bool NormalizeResPath( const char* path, std::string& result );

// Get protocol part of the res path ("res" or "dynamic")
BLUEIMPORT bool GetResProtocol( const wchar_t* path, std::wstring &result );

// Get protocol part of the res path ("res" or "dynamic")
BLUEIMPORT bool GetResProtocol( const char* path, std::string &result );

BLUEIMPORT void NormalizeSlashes( std::wstring& fname );
BLUEIMPORT std::wstring ConvertRelativeToAbsolutePath( const wchar_t* path );

bool AdjustFilenameForLanguageCode( std::wstring &filenameToOpen, std::wstring &tmpfilename );

std::wstring SubstituteBlackForRedInFilename( const std::wstring& filename );
std::wstring SubstituteRedForBlackInFilename( const std::wstring& filename );
#endif // BlueFileUtil_H

