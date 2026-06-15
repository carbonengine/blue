// Copyright © 2011 CCP ehf.

#pragma once
#ifndef BluePaths_h
#define BluePaths_h

#include "IBluePaths.h"
#include "BlueResFileSystemLocal.h"
#include "BlueResFileSystemRemote.h"

BLUE_DECLARE( BluePaths );

class BluePaths : public IBluePaths
{
public:
	EXPOSE_TO_BLUE();

	// Constructor is private

	static bool Initialize(const std::wstring& initialPath);

	void ClearSearchPaths();

	enum BeforeOrAfter
	{
		BEFORE,
		AFTER
	};

	Be::Result<std::string> RegisterFileSystem( std::string fs, BeforeOrAfter beforeOrAfter );
	Be::Result<std::string> RegisterFileSystemBeforeLocal( std::string fs );
	Be::Result<std::string> RegisterFileSystemAfterLocal( std::string fs );
	BlueStdResult UnregisterFileSystem( std::string fs );
	bool IsFileSystemRegistered( std::string fs ) const;

	//////////////////////////////////////////////////////////////////////////
	// IBluePaths
	void InitializeStdAppPaths() override;

	Be::Result<std::string> SetSearchPathW( const char* key, const wchar_t* value ) override;
	const wchar_t* GetSearchPathW( const char* key ) override;

	std::wstring ResolvePathW( const std::wstring& path ) override;
	std::wstring ResolvePathForWritingW( const std::wstring& path ) override;
	std::wstring ResolvePathToRootW( const std::string& root, const std::wstring& path ) override;

	void GetExpandedSearchPaths( const char* key, std::vector<std::wstring>& paths ) override;

	// Get the contents of the given directory
	void GetDirectoryContents( const wchar_t* dir, std::set<std::wstring>& results ) override;

	// Returns true if the path is a directory
	bool IsDirectory( const std::wstring& dir ) override;

	// Returns true if the file exists. Checking for .red files returns true if a corresponding
	// black file exists and BeResMan->GetSubstituteBlackForRed() is set.
	bool FileExists( const std::wstring& filename ) override;

	// Dump current search paths to log
	void LogPaths() override;

	bool FileExistsLocally( const wchar_t* filename ) override;

	bool FileNeedsDownload( const wchar_t* filename ) override;

	// Get a stream from a resource path
	bool GetStreamFromPathW( const wchar_t* path, IBlueStream** stream ) override;

	Be::Result<std::string> GetFileContentsWithYield( const std::wstring& path, IBlueStream** contents ) override;
	//
	//////////////////////////////////////////////////////////////////////////

protected:
	BluePaths( IRoot* lockobj = NULL );
	~BluePaths();

private:
	bool InitializeHelper(const std::wstring& initialPath);

	std::map<std::string, std::wstring> GetAllSearchPathsAsDict();
	std::map<std::string, std::vector<std::wstring>> GetExpandedSearchPathsAsDict();

	std::vector<std::wstring> ListDirFromScript( const std::wstring& dir );
	Be::Result<std::string> Open( const std::wstring& filename, Be::Optional<std::string> mode, IBlueStream** stream );

	std::wstring GetInitialWorkingDirectory() { return m_localFileSystem->GetInitialWorkingDirectory(); }

	BlueResFileSystemLocalPtr m_localFileSystem;

	std::vector<IBlueResFileSystemPtr> m_resFileSystems;

	std::unordered_set<std::wstring> m_existingFiles;
	CcpMutex m_existingFilesMutex;
	bool m_cacheFileExistance;
};

TYPEDEF_BLUECLASS( BluePaths );

#endif
