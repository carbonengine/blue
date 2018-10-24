////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		November 2014
// Copyright:	CCP 2014
//

#pragma once
#ifndef BlueResFileSystemLocal_h
#define BlueResFileSystemLocal_h

#include "Include/IBlueResFileSystem.h"

BLUE_CLASS( BlueResFileSystemLocal ) : public IBlueResFileSystem
{
public:
	EXPOSE_TO_BLUE();

	BlueResFileSystemLocal( IRoot* lockobj = nullptr );

	//////////////////////////////////////////////////////////////////////////
	// IBlueResFileSystem

	virtual bool FileExists( const std::wstring& filename );
	virtual bool IsDirectory( const std::wstring& dir );
	virtual void GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results );
	virtual bool GetStreamFromPathW( const wchar_t* resPath, IBlueStream** stream );
	virtual bool ResolvePathW( const std::wstring& path, std::wstring& resolvedPath );

	//////////////////////////////////////////////////////////////////////////

	bool Initialize();
	void InitializeStdAppPaths();

	void SetSearchPathW( const char* key, const wchar_t* value );
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
	ExpandedSearchPathMap_t GetExpandedSearchPaths() { return m_expandedSearchPaths; }

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

	// Helper function to expand search paths - called after any entry is changed
	void ExpandSearchPaths();
};

TYPEDEF_BLUECLASS( BlueResFileSystemLocal );

#endif // BlueResFileSystemLocal_h