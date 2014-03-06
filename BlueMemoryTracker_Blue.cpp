////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		Sep 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#include "include/IBlueOS.h"
#include "BlueMemoryTracker.h"

#ifdef _WIN32
#include <Psapi.h>
#endif

extern bool g_isCallstackCaptureEnabled;

BLUE_DEFINE( MemoryTracker );

// Create an instance of the memory tracker
static CMemoryTracker s_memoryTrackerInstance;

// This pointer is used to register the memory tracker as an object under the blue
// module in Python (see in BluePython.cpp).
MemoryTracker* BeMemoryTracker = &s_memoryTrackerInstance;
BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "memoryTracker", BeMemoryTracker );

#if BLUE_WITH_PYTHON

static PyObject* PyMemoryTrackerDumpReportAsText( PyObject* self, PyObject* args )
{	
	const char* name = 0;

	if( !PyArg_ParseTuple( args, "s", &name ))
	{
		return NULL;
	}

	MemoryTrackerDumpReportAsText( name );
	Py_RETURN_NONE;
}

static PyObject* PyMemoryTrackerDumpReportAsBinary( PyObject* self, PyObject* args )
{	
	const char* name = 0;

	if( !PyArg_ParseTuple( args, "s", &name ))
	{
		return NULL;
	}

	MemoryTrackerDumpReportAsBinary( name );
	Py_RETURN_NONE;
}

#ifdef _WIN32
static PyObject* PyDumpModulesAsText( PyObject* self, PyObject* args )
{
	const char* filename = 0;

	if( !PyArg_ParseTuple( args, "s", &filename ))
	{
		return NULL;
	}

	HMODULE modules[1024];
	DWORD spaceNeeded = 0;

	HANDLE curProcess = GetCurrentProcess();

	if( EnumProcessModules( curProcess, modules, sizeof( modules ), &spaceNeeded ) )
	{
		int numModules = spaceNeeded / sizeof( HMODULE );
		long totalSize = 0;

		FILE* file;
		fopen_s( &file, filename, "w" );
		fprintf( file, "Module, Size, Full name\n" );

		for( int i = 0; i < numModules; ++i )
		{
			MODULEINFO moduleInfo;
			char baseName[256];
			char fullName[256];

			if( !GetModuleBaseName( curProcess, modules[i], baseName, _countof( baseName )))
			{
				CCP_LOGWARN( "DumpModulesAsText: Failed to get module base name - skipping it" );
				continue;
			}

			if( !GetModuleInformation( curProcess, modules[i], &moduleInfo, sizeof( MODULEINFO )) )
			{
				CCP_LOGWARN( "DumpModulesAsText: Failed to get module information for %s - skipping it", baseName );
				continue;
			}

			if( !GetModuleFileName( modules[i], fullName, _countof( fullName )) )
			{
				CCP_LOGWARN( "DumpModulesAsText: Failed to get module file name for %s - skipping it", baseName );
				continue;
			}

			totalSize += moduleInfo.SizeOfImage;

			fprintf( file, "%s, %d, %s\n", baseName, moduleInfo.SizeOfImage, fullName );
		}

		fprintf( file, "\n\n%d modules\n%ld bytes", numModules, totalSize );
		fclose( file );
	}
	
	Py_RETURN_NONE;
}
#endif

static PyObject* PyMemoryTrackerGetCount( PyObject* self, PyObject* args )
{	
	if( !PyArg_ParseTuple( args, "" ))
	{
		return NULL;
	}

	size_t count;
	MemoryTrackerGetCount( count );

	return PyInt_FromSize_t( count );
}

static PyObject* PyMemoryTrackerGetSize( PyObject* self, PyObject* args )
{	
	if( !PyArg_ParseTuple( args, "" ))
	{
		return NULL;
	}

	size_t size;
	MemoryTrackerGetSize( size );

	return PyInt_FromSize_t( size );
}

static PyObject* PyMemoryTrackerCallstackCaptureEnable( PyObject* self, PyObject* args )
{	
	bool isEnabled = false;
	if( !PyArg_ParseTuple( args, "b", &isEnabled ))
	{
		return NULL;
	}

	g_isCallstackCaptureEnabled = isEnabled;
	Py_RETURN_NONE;
}

#ifdef _WIN32
static PyObject* PyMemoryTrackerGetProcessHeapsCount( PyObject* self, PyObject* args )
{	
	if( !PyArg_ParseTuple( args, "" ))
	{
		return NULL;
	}

	DWORD count = ::GetProcessHeaps( 0, NULL );

	return PyInt_FromLong( count );
}

static PyObject* PyMemoryTrackerGetHeapSize( PyObject* self, PyObject* args )
{	
	HANDLE heap = NULL;
	if( !PyArg_ParseTuple( args, "l", &heap ))
	{
		return NULL;
	}

	size_t size = GetHeapSizeWithHeapWalk( heap );
	if( size >= 0 )
	{
		return PyLong_FromUnsignedLong( (unsigned int)size );
	}
	else
	{
		PyErr_SetString( PyExc_RuntimeError, "Couldn't get heap size" );
		return NULL;
	}
}

static PyObject* PyMemoryTrackerGetAllHeaps( PyObject* self, PyObject* args )
{	
	if( !PyArg_ParseTuple( args, "" ))
	{
		return NULL;
	}

	HANDLE heaps[256];

	DWORD count = ::GetProcessHeaps( 256, heaps );

	PyObject* result = PyDict_New();
	for( DWORD i = 0; i < count; ++i )
	{
		HANDLE heap = heaps[i];
		size_t size = GetHeapSizeWithHeapWalk( heap );

		if( size == (size_t)-1 )
		{
			PyErr_SetString( PyExc_RuntimeError, "Couldn't get heap size" );
			Py_DECREF( result );
			return NULL;
		}
		
		PyObject* key = PyLong_FromLongLong( (intptr_t)heap );
		PyObject* value = PyLong_FromUnsignedLong( (unsigned int)size );
		
		PyDict_SetItem( result, key, value );
		
		Py_DECREF( key );
		Py_DECREF( value );
	}

	return result;
}

static PyObject* PyMemoryTrackerGetUnknownHeaps( PyObject* self, PyObject* args )
{	
	if( !PyArg_ParseTuple( args, "" ))
	{
		return NULL;
	}

	HANDLE heaps[256];

	DWORD count = ::GetProcessHeaps( 256, heaps );

	PyObject* result = PyDict_New();
	for( DWORD i = 0; i < count; ++i )
	{
		HANDLE heap = heaps[i];
		if( BeMemoryTracker->IsKnownHeap( heap ) )
		{
			continue;
		}

		size_t size = GetHeapSizeWithHeapWalk( heap );

		if( size == (size_t)-1 )
		{
			PyErr_SetString( PyExc_RuntimeError, "Couldn't get heap size" );
			Py_DECREF( result );
			return NULL;
		}

		PyObject* key = PyLong_FromLongLong( (intptr_t)heap );
		PyObject* value = PyLong_FromUnsignedLong( (unsigned int)size );

		PyDict_SetItem( result, key, value );

		Py_DECREF( key );
		Py_DECREF( value );
	}

	return result;
}

static PyObject* PyMemoryTrackerGetMainProcessHeap( PyObject* self, PyObject* args )
{	
	if( !PyArg_ParseTuple( args, "" ))
	{
		return NULL;
	}

	HANDLE heap = ::GetProcessHeap();

	return PyInt_FromSize_t( (size_t)heap );
}

static PyObject* PyMemoryTrackerGetBlueHeap( PyObject* self, PyObject* args )
{	
	if( !PyArg_ParseTuple( args, "" ))
	{
		return NULL;
	}

	extern HANDLE s_heap;
	return PyInt_FromSize_t( (size_t)s_heap );
}

static PyObject* PyMemoryTrackerGetCrtHeap( PyObject* self, PyObject* args )
{	
	if( !PyArg_ParseTuple( args, "" ))
	{
		return NULL;
	}

	return PyInt_FromSize_t( (size_t)_get_heap_handle() );
}

MAP_FUNCTION( "MemoryTrackerGetCrtHeap", PyMemoryTrackerGetCrtHeap, "" );

#if _DEBUG
static const char* kUnknown = "<Unknown>";

// In debug builds we can hook into malloc to get unified tracking information

int CrtAllocHookToMemoryTracker( 
	int allocType, 
	void* userData, 
	size_t size, 
	int blockType, 
	long requestNumber, 
	const unsigned char* filename, 
	int lineNumber )
{
	if( blockType == _CRT_BLOCK )
	{
		return TRUE;
	}

	if( !filename )
	{
		filename = (const unsigned char *)kUnknown;
	}

	switch( allocType )
	{
	case _HOOK_ALLOC:
		MemoryTrackerAdd( userData, size, "malloc", (const char*)filename, lineNumber );
		break;

	case _HOOK_REALLOC:
		MemoryTrackerRemove( userData );
		MemoryTrackerAdd( userData, size, "malloc", (const char*)filename, lineNumber );
		break;

	case _HOOK_FREE:
		MemoryTrackerRemove( userData );
		break;
	}

	return TRUE;
}
#endif

static PyObject* PyMemoryTrackerCrtHeapTrackEnable( PyObject* self, PyObject* args )
{	
	bool isEnabled = false;
	if( !PyArg_ParseTuple( args, "b", &isEnabled ))
	{
		return NULL;
	}

	if( isEnabled )
	{
		_CrtSetAllocHook( CrtAllocHookToMemoryTracker );
	}
	else
	{
		_CrtSetAllocHook( NULL );
	}

	Py_RETURN_NONE;
}
#endif

#endif

const Be::ClassInfo* MemoryTracker::ExposeToBlue()
{
	EXPOSURE_BEGIN( MemoryTracker, "" )
		MAP_INTERFACE( MemoryTracker )

#ifdef _WIN32
		MAP_ATTRIBUTE
		(
			"d3dHeap1",
			m_d3dHeap1,
			"Handle to first D3D heap",
			Be::READWRITE
		)
		
		MAP_ATTRIBUTE
		(
			"d3dHeap2",
			m_d3dHeap2,
			"Handle to second D3D heap",
			Be::READWRITE
		)
		
		MAP_ATTRIBUTE
		(
			"customHeap1",
			m_customHeap1,
			"Handle to first custom heap",
			Be::READWRITE
		)
		
		MAP_ATTRIBUTE
		(
			"customHeap2",
			m_customHeap2,
			"Handle to second custom heap",
			Be::READWRITE
		)
		
		MAP_ATTRIBUTE
		(
			"customHeap3",
			m_customHeap3,
			"Handle to third custom heap",
			Be::READWRITE
		)
#endif
		MAP_ATTRIBUTE
		(
			"loggingThreshold",
			m_loggingThreshold,
			"Threshold for memory logging - whenever any memory stat changes more\n"
			"than this threshold since last logging, all memory stats are logged out.",
			Be::READWRITE
		)

		MAP_METHOD_AND_WRAP
		(
			"SummaryReport",
			SummaryReport,
			"Generates a summary report and outputs it to the given filename"
		)

		MAP_METHOD( "DumpReportAsText", PyMemoryTrackerDumpReportAsText, "" );
		MAP_METHOD( "DumpReportAsBinary", PyMemoryTrackerDumpReportAsBinary, "" );
#ifdef _WIN32
		MAP_METHOD( "DumpModulesAsText", PyDumpModulesAsText, "" );
#endif

		MAP_METHOD
		( 
			"GetCount", 
			PyMemoryTrackerGetCount,
			"Get the count of all tracked allocations, i.e. any allocations\n"
			"from the Blue heap with memory tracking enabled."
		)
		MAP_METHOD
		( 
			"GetSize", 
			PyMemoryTrackerGetSize,
			"Get the total size of all tracked allocations, i.e. any allocations\n"
			"from the Blue heap with memory tracking enabled."
		)
		MAP_METHOD( "CallstackCaptureEnable", PyMemoryTrackerCallstackCaptureEnable, "" )

#ifdef _WIN32
		MAP_METHOD
		( 
			"GetHeapCount", 
			PyMemoryTrackerGetProcessHeapsCount, 
			"Returns the number of heaps owned by the process."
		)
		MAP_METHOD
		( 
			"GetHeapSize", 
			PyMemoryTrackerGetHeapSize, 
			"Get the size of the given heap\n\n"
			"Arguments:\n"
			"  heap - handle to a heap\n"
			"Return value:\n"
			"  The size of the given heap, obtained with a heap walk.\n"
			"\n"
			"Handles for different heaps are obtained with the Get<name>Heap functions\n"
			"of the memory tracker. Trinity also provides a GetD3DCreatedHeap function\n"
			"that returns a heap handle."
		)
		MAP_METHOD
		( 
			"GetAllHeaps", 
			PyMemoryTrackerGetAllHeaps, 
			"Returns a dict with info on all heaps owned by the process." 
		)
		MAP_METHOD
		( 
			"GetUnknownHeaps", 
			PyMemoryTrackerGetUnknownHeaps, 
			"Returns a dict with info on all unknown heaps owned by the process." 
		)
		MAP_METHOD
		( 
			"GetMainProcessHeap", 
			PyMemoryTrackerGetMainProcessHeap, 
			"Returns the handle of the main process heap."
		)
		MAP_METHOD
		( 
			"GetBlueHeap", 
			PyMemoryTrackerGetBlueHeap, 
			"Returns the handle of the Blue heap."
		)
		MAP_METHOD
		( 
			"GetCrtHeap", 
			PyMemoryTrackerGetCrtHeap, 
			"Returns the handle of the crt heap - this is likely the same as the Blue heap."
		)
		MAP_METHOD_AND_WRAP
		(
			"SetCustomHeapsToLargestHeaps",
			SetCustomHeapsToLargestHeaps,
			"Sets the custom heaps to the current largest heaps."
		)
#endif

#if _DEBUG
		MAP_METHOD( "MemoryTrackerCrtHeapTrackEnable", PyMemoryTrackerCrtHeapTrackEnable, "" );
#endif

	EXPOSURE_END()
}
