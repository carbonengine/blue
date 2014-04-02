////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		November 2011
// Copyright:	CCP 2011
//

#pragma once
#ifndef BluePaths_h
#define BluePaths_h

#include "include/IBluePaths.h"

BLUE_DECLARE( BluePaths );

class BluePaths : public IBluePaths
{
public:
	EXPOSE_TO_BLUE();

	// Constructor is private

	static bool Initialize();

	void ClearSearchPaths();

	//////////////////////////////////////////////////////////////////////////
	// IBluePaths
	virtual void InitializeStdAppPaths();

	virtual void SetSearchPathW( const char* key, const wchar_t* value );
	virtual const wchar_t* GetSearchPathW( const char* key );

	virtual std::wstring ResolvePathW( const std::wstring& path );
	virtual std::wstring ResolvePathForWritingW( const std::wstring& path );
	virtual std::wstring ResolvePathToRootW( const std::string& root, const std::wstring& path );

	virtual void GetExpandedSearchPaths( const char* key, std::vector<std::wstring>& paths );

	// Get the contents of the given directory
	virtual void GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results );

	// Returns true if the path is a directory
	virtual bool IsDirectory( const std::wstring& dir );

	// Returns true if the file exists. Checking for .red files returns true if a corresponding
	// black file exists and BeResMan->GetSubstituteBlackForRed() is set.
	virtual bool FileExists( const std::wstring& filename );

	// Dump current search paths to log
	virtual void LogPaths();

	// Same as FileExists, but does not attempt black for red substitution.
	virtual bool FileExistsWithoutSubstitution( const wchar_t* filename );

	// Get a stream from a resource path
	virtual bool GetStreamFromPathW( const wchar_t* path, IBlueStream** stream );

	Be::Result<std::string> GetFileContentsWithYield( const std::wstring& path, IBlueStream** contents );
	//
	//////////////////////////////////////////////////////////////////////////

protected:
	BluePaths( IRoot* lockobj = NULL );
	~BluePaths();

private:
	bool InitializeHelper();

	// Initial working directory. This is used to resolve relative search paths.
	std::wstring m_initialWorkingDirectory;

	// Search paths as added to the system
	typedef TrackableStdHashMap<std::string, std::wstring> SearchPathMap_t;
	SearchPathMap_t m_searchPaths;

	// Search paths expanded, for quicker resolving of paths
	typedef TrackableStdHashMap<std::string, std::vector<std::wstring>> ExpandedSearchPathMap_t;
	ExpandedSearchPathMap_t m_expandedSearchPaths;

	// Helper function to expand search paths - called after any entry is changed
	void ExpandSearchPaths();

#if BLUE_WITH_PYTHON
	PyObject* GetAllSearchPathsAsDict();
	PyObject* GetExpandedSearchPathsAsDict();
#endif

	std::vector<std::wstring> ListDirFromScript( const std::wstring& dir );
	Be::Result<std::string> Open( const std::wstring& filename, const std::string& mode, IBlueStream** stream );
};

TYPEDEF_BLUECLASS( BluePaths );

#endif