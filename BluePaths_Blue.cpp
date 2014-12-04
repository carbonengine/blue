////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		November 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#include "BluePaths.h"
#include "include/IBluePersist.h"

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
			"Sets a search path for resolving file paths to the underlying file system"
			"\nArguments:"
			"\n   key"
			"\n   value"
		)
		
		MAP_METHOD_AND_WRAP
		(
			"GetSearchPath",
			GetSearchPathW,
			"Gets a search path for resolving file paths to the underlying file system"
			"\nArguments:"
			"\n   key"
			"\nReturns:"
			"\n   Value associated with key"
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
			"Resolves a path to an underlying file path using registered search paths"
			"\nArguments:"
			"\n   path"
			"\nReturns:"
			"\n   Path usable for the underlying file system that corresponds to the given path."
		)

		MAP_METHOD_AND_WRAP
		(
			"ResolvePathForWriting",
			ResolvePathForWritingW,
			"Resolves a path to an underlying file path using registered search paths."
			"\nThe difference between this function and ResolvePath is that this one"
			"\nreturns the first possible path, rather than testing for the existence"
			"\nof a file."
			"\nArguments:"
			"\n   path"
			"\nReturns:"
			"\n   Path usable for the underlying file system that corresponds to the given path."
		)

		MAP_METHOD_AND_WRAP
		(
			"ResolvePathToRoot",
			ResolvePathToRootW,
			"Resolves a file system path back to a Blue path with the given root"
			"\nArguments:"
			"\n   root"
			"\n   path"
			"\nReturns:"
			"\n   A Blue path based on the given root if a match was found. Otherwise an empty string."
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
			"Returns a list containing the names of the entries in the directory given."
		)

		MAP_METHOD_AND_WRAP
		(
			"isdir",
			IsDirectory,
			"Returns True if the given path is a directory, otherwise False."
		)

		MAP_METHOD_AND_WRAP
		(
			"exists",
			FileExists,
			"Returns True if the given path exists as a file, otherwise False."
		)

		MAP_METHOD_AND_WRAP
		(
			"open",
			Open,
			"Opens the given file. Raises a RuntimeError if it fails to open."
		)

		MAP_METHOD_AND_WRAP
		(
			"GetFileContentsWithYield",
			GetFileContentsWithYield,
			""
		)

		MAP_METHOD_AND_WRAP
		(
			"FileExistsLocally",
			FileExistsLocally,
			"Returns True if the given path exists as a file, cached locally in the case of remote files."
		)

		MAP_METHOD_AND_WRAP
		(
			"RegisterFileSystemBeforeLocal",
			RegisterFileSystemBeforeLocal,
			"Registers a file system that is searched before the local file system. Currently available values\n"
			"are 'Stuff' and 'Remote' (case sensitive)"
		)

		MAP_METHOD_AND_WRAP
		(
			"RegisterFileSystemAfterLocal",
			RegisterFileSystemAfterLocal,
			"Registers a file system that is searched after the local file system. Currently available values\n"
			"are 'Stuff' and 'Remote' (case sensitive)"
		)

		MAP_PROPERTY
		(
			"thresholdForWarningLongDownloads",
			GetThresholdForWarningLongDownloadTime, SetThresholdForWarningLongDownloadTime,
			"Threshold (in seconds) for issuing warnings on long downloads. Set to zero to disable warnings altogether."
		)

	EXPOSURE_END()
}
