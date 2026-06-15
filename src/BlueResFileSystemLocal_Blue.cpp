// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "BlueResFileSystemLocal.h"

BLUE_DEFINE_INTERFACE( IBlueResFileSystem );
BLUE_DEFINE( BlueResFileSystemLocal );

const Be::ClassInfo* BlueResFileSystemLocal::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueResFileSystemLocal, "" )
		MAP_INTERFACE( IBlueResFileSystem )

		MAP_PROPERTY_READONLY
		(
			"initialWorkingDirectory",
			GetInitialWorkingDirectory,
			"The initial working directory, as it was when the module was initialized."
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
			":param key: path prefix\n"
			":param value: semicolumn separated list of paths or prefixes"
		)

		MAP_METHOD_AND_WRAP
		(
			"GetSearchPath",
			GetSearchPathW,
			"Gets a search path for resolving file paths to the underlying file system\n"
			":param key: path prefix\n"
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
			ResolvePath,
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
			":param root: path prefix\n"
			":param path: physical file path\n"
			":returns: A Blue path based on the given root if a match was found. Otherwise an empty string."
		)

		MAP_METHOD_AND_WRAP
		(
			"GetAllSearchPaths",
			GetAllSearchPaths,
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
	EXPOSURE_END()
}