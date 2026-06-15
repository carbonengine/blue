// Copyright © 2014 CCP ehf.

#pragma once
#ifndef BlueResFileSystemLocal_h
#define BlueResFileSystemLocal_h

#include "IBlueResFileSystem.h"
#include <shared_mutex>

BLUE_CLASS( BlueResFileSystemLocal ) : public IBlueResFileSystem
{
public:
	EXPOSE_TO_BLUE();

	BlueResFileSystemLocal( IRoot* lockobj = nullptr );

	//////////////////////////////////////////////////////////////////////////
	// IBlueResFileSystem

	bool FileExists( const std::wstring& filename ) override;
	bool IsDirectory( const std::wstring& dir ) override;
	void GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results ) override;
	bool GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream ) override;
	bool ResolvePathW( const std::wstring& path, std::wstring& resolvedPath ) override;

	//////////////////////////////////////////////////////////////////////////

	bool Initialize(const std::wstring& initialPath);
	void InitializeStdAppPaths();

	Be::Result<std::string> SetSearchPathW( const char* key, const wchar_t* value );
	const wchar_t* GetSearchPathW( const char* key );
	void ClearSearchPaths();
	std::wstring ResolvePathForWritingW( const std::wstring& path );
	std::wstring ResolvePathToRootW( const std::string& root, const std::wstring& path );
	void GetExpandedSearchPaths( const char* key, std::vector<std::wstring>& paths );
	void LogPaths();
	std::wstring GetInitialWorkingDirectory();

	typedef std::map<std::string, std::wstring> SearchPathMap_t;
	typedef std::map<std::string, std::vector<std::wstring>> ExpandedSearchPathMap_t;

	SearchPathMap_t GetAllSearchPaths() { return m_searchPaths; }
	ExpandedSearchPathMap_t GetExpandedSearchPaths();

	std::wstring ResolvePath( const wchar_t* path );

	std::map<std::string, std::vector<std::wstring>> GetExpandedSearchPathsAsDict();
	std::vector<std::wstring> ListDirFromScript( const std::wstring& dir );
private:
	// Initial working directory. This is used to resolve relative search paths.
	std::wstring m_initialWorkingDirectory;

	// Search paths as added to the system
	SearchPathMap_t m_searchPaths;

	// Search paths expanded, for quicker resolving of paths
	ExpandedSearchPathMap_t m_expandedSearchPaths;

	// Expanded search paths can be accessed from multiple threads.
	std::shared_timed_mutex m_expandedSearchPathsMutex;

	// Helper function to expand search paths - called after any entry is changed
	bool ExpandSearchPaths();
};

TYPEDEF_BLUECLASS( BlueResFileSystemLocal );

#endif // BlueResFileSystemLocal_h
