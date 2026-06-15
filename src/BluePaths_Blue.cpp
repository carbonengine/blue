// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#include "BluePaths.h"
#include "IBluePersist.h"

static bool GetBePaths( const Be::IID& riid, void** ppv )
{
	return BePaths->QueryInterface( riid, ppv );
}

BLUE_DEFINE_NO_REGISTER( BluePaths );
BLUE_REGISTER_CLASS_EX( BluePaths, GetBePaths, Be::ClassRegistration::DISABLE_PYTHON_CONSTRUCTION );

const Be::ClassInfo* BluePaths::ExposeToBlue()
{
	EXPOSURE_BEGIN( BluePaths, "BluePaths handles search paths" )
		MAP_INTERFACE( IBluePaths )

		MAP_PROPERTY_READONLY
		(
			"initialWorkingDirectory",
			GetInitialWorkingDirectory,
			"The initial working directory, as it was when the module was initialized."
		)

		MAP_ATTRIBUTE
		(
			"cacheFileExistance",
			m_cacheFileExistance,
			"When this attribute is set, BluePaths will cache existance of local files (for methods\n"
			"like exists or FileExistsLocally). Using this flags speeds up those methods considerably\n"
			"at the expence of possible false positive results.",
			Be::READWRITE
		)
		
		MAP_METHOD_AND_WRAP
		(
			"InitializeStdAppPaths",
			InitializeStdAppPaths,
			"Ensures that standard app paths exist, with sensible defaults."
		)

		MAP_METHOD_AND_WRAP
		(
			"SetSearchPath",
			SetSearchPathW,
			"Sets a search path for resolving file paths to the underlying file system\n"
			":param key: \n"
			":param value: "
		)
		
		MAP_METHOD_AND_WRAP
		(
			"GetSearchPath",
			GetSearchPathW,
			"Gets a search path for resolving file paths to the underlying file system\n"
			":param key: \n"
			":returns: Value associated with key"
		)
		
		MAP_METHOD_AND_WRAP
		(
			"ClearSearchPaths",
			ClearSearchPaths,
			"Clears all search paths for resolving file paths to the underlying file system"
		)
		
		MAP_METHOD_AND_WRAP
		(
			"ResolvePath",
			ResolvePathW,
			"Resolves a path to an underlying file path using registered search paths\n"
			":param path: res path"
			":returns: Path usable for the underlying file system that corresponds to the given path."
		)

		MAP_METHOD_AND_WRAP
		(
			"ResolvePathForWriting",
			ResolvePathForWritingW,
			"Resolves a path to an underlying file path using registered search paths."
			"\nThe difference between this function and ResolvePath is that this one"
			"\nreturns the first possible path, rather than testing for the existence"
			"\nof a file.\n"
			":param  path: res path\n"
			":returns: Path usable for the underlying file system that corresponds to the given path."
		)

		MAP_METHOD_AND_WRAP
		(
			"ResolvePathToRoot",
			ResolvePathToRootW,
			"Resolves a file system path back to a Blue path with the given root\n"
			":param root: \n"
			":param path: \n"
			":returns: A Blue path based on the given root if a match was found. Otherwise an empty string."
		)

		MAP_METHOD_AND_WRAP
		(
			"GetAllSearchPaths",
			GetAllSearchPathsAsDict,
			"Returns all search paths in a dictionary"
		)

		MAP_METHOD_AND_WRAP
		(
			"GetExpandedSearchPaths",
			GetExpandedSearchPathsAsDict,
			"Returns all search paths, fully expanded, as a dictionary."
		)

		MAP_METHOD_AND_WRAP
		(
			"listdir",
			ListDirFromScript,
			"Returns a list containing the names of the entries in the directory given.\n"
			":param path: directory res path"
		)

		MAP_METHOD_AND_WRAP
		(
			"isdir",
			IsDirectory,
			"Returns True if the given path is a directory, otherwise False.\n"
			":param path: res path"
		)

		MAP_METHOD_AND_WRAP
		(
			"exists",
			FileExists,
			"Returns True if the given path exists as a file, otherwise False.\n"
			":param filename: res path"
		)

		MAP_METHOD_AND_WRAP_OPTIONAL_ARGS
		(
			"open",
			Open,
			1,
			"Opens the given file. Raises a RuntimeError if it fails to open.\n"
			":param filename: res path to the file\n"
			":param openMode: open mode string"
		)

		MAP_METHOD_AND_WRAP
		(
			"GetFileContentsWithYield",
			GetFileContentsWithYield,
			"Returns a stream for reading a file\n"
			":param path: res path to the file"
		)

		MAP_METHOD_AND_WRAP
		(
			"FileExistsLocally",
			FileExistsLocally,
			"Returns True if the given path exists as a file, cached locally in the case of remote files.\n"
			":param filename: res path"
		)

		MAP_METHOD_AND_WRAP
		(
			"RegisterFileSystemBeforeLocal",
			RegisterFileSystemBeforeLocal,
			"Registers a file system that is searched before the local file system. Currently available values\n"
			"are 'Remote' (case sensitive)\n"
			":param systemName: file system name"
		)

		MAP_METHOD_AND_WRAP
		(
			"RegisterFileSystemAfterLocal",
			RegisterFileSystemAfterLocal,
			"Registers a file system that is searched after the local file system. Currently available values\n"
			"are 'Remote' (case sensitive)\n"
			":param systemName: file system name"
		)

		MAP_METHOD_AND_WRAP
		(
			"UnregisterFileSystem",
			UnregisterFileSystem,
			"Unregisters a previously registered file system. Currently available values are 'Remote' (case sensitive).\n"
			":param name: file system name\n"
			":raises IndexError: if file system is not registered"
		)

		MAP_METHOD_AND_WRAP
		(
			"IsFileSystemRegistered",
			IsFileSystemRegistered,
			"Check if the file system is registered\n"
			":param name: file system name"
		)

	EXPOSURE_END()
}
