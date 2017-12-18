////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		Sep 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#include "Include/IBlueOS.h"
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

#ifdef _WIN32

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

static PyObject* PyMemoryTrackerGetCrtHeap( PyObject* self, PyObject* args )
{	
	if( !PyArg_ParseTuple( args, "" ))
	{
		return NULL;
	}

	return PyInt_FromSize_t( (size_t)_get_heap_handle() );
}

MAP_FUNCTION( 
	"MemoryTrackerGetCrtHeap", 
	PyMemoryTrackerGetCrtHeap, 
	"Returns CRT heap handle\n"
	":rtype: int" );

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
			"Generates a summary report and outputs it to the given filename\n"
			":param filename: path to the file"
		)

		MAP_METHOD_AND_WRAP( 
			"DumpReportAsText", 
			DumpReportAsText, 
			"Write text report into a file\n"
			":param filename: path to file" );
		MAP_METHOD_AND_WRAP( 
			"DumpReportAsBinary", 
			DumpReportAsBinary, 
			"Write binary report into a file\n"
			":param filename: path to file" );
#ifdef _WIN32
		MAP_METHOD_AND_WRAP( 
			"DumpModulesAsText", 
			DumpModulesAsText, 
			"Write a list of loaded modules into a file\n"
			":param filename: path to file" );
#endif

		MAP_METHOD_AND_WRAP
		( 
			"GetCount", 
			GetCount,
			"Get the count of all tracked allocations, i.e. any allocations\n"
			"from the Blue heap with memory tracking enabled."
		)
		MAP_METHOD_AND_WRAP
		( 
			"GetSize", 
			GetSize,
			"Get the total size of all tracked allocations, i.e. any allocations\n"
			"from the Blue heap with memory tracking enabled."
		)
		MAP_METHOD_AND_WRAP
		( 
			"CallstackCaptureEnable", 
			CallstackCaptureEnable, 
			"Enable/disable callstack capture for new allocations\n"
			":param enable: True to enable callstack capture" 
			)

#ifdef _WIN32
		MAP_METHOD_AND_WRAP
		( 
			"GetHeapCount", 
			GetProcessHeapsCount, 
			"Returns the number of heaps owned by the process."
		)
		MAP_METHOD_AND_WRAP
		( 
			"GetHeapSize", 
			GetHeapSize, 
			"Get the size of the given heap\n"
			"Handles for different heaps are obtained with the Get<name>Heap functions\n"
			"of the memory tracker. Trinity also provides a GetD3DCreatedHeap function\n"
			"that returns a heap handle.\n"
			":param heap: handle to a heap\n"
			":returns: The size of the given heap, obtained with a heap walk.\n"
		)
		MAP_METHOD
		( 
			"GetAllHeaps", 
			PyMemoryTrackerGetAllHeaps, 
			"Returns a dict with info on all heaps owned by the process.\n" 
			":rtype: dict[long, long]"
		)
		MAP_METHOD
		( 
			"GetUnknownHeaps", 
			PyMemoryTrackerGetUnknownHeaps, 
			"Returns a dict with info on all unknown heaps owned by the process.\n" 
			":rtype: dict[long, long]"
		)
		MAP_METHOD_AND_WRAP
		( 
			"GetMainProcessHeap", 
			GetMainProcessHeap, 
			"Returns the handle of the main process heap."
		)
		MAP_METHOD_AND_WRAP
		( 
			"GetBlueHeap", 
			GetBlueHeap, 
			"Returns the handle of the Blue heap."
		)
		MAP_METHOD
		( 
			"GetCrtHeap", 
			PyMemoryTrackerGetCrtHeap, 
			"Returns the handle of the crt heap - this is likely the same as the Blue heap.\n"
			":rtype: long"
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
