////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		January 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"

#include "BlueMemoryTracker.h"

#ifdef _WIN32
#include <Psapi.h>
#endif

CCP_STATS_DECLARE( beMemory,					"Blue/Memory/Malloc", false, CST_MEMORY, "The amount of memory allocated via CCP_MALLOC" );
CCP_STATS_DECLARE( trackedAllocationsCount,		"Blue/Memory/trackedAllocationsCount", false, CST_COUNTER_HIGH, "Number of tracked allocations live in the system" );
CCP_STATS_DECLARE( trackedAllocationsSize,		"Blue/Memory/trackedAllocationsSize", false, CST_MEMORY, "Combined size of tracked allocations live in the system" );

#if CCP_STACKLESS
CCP_STATS_DECLARE( pyMemory,					"Blue/Memory/Python", false, CST_MEMORY, "The amount of memory allocated for Python" );
#endif

#ifdef _WIN32
CCP_STATS_DECLARE( workingSetSize,				"Blue/Memory/WorkingSet", false, CST_MEMORY, "The working set size as reported by the OS" );
CCP_STATS_DECLARE( pageFileUsage,				"Blue/Memory/PageFileUsage", false, CST_MEMORY, "Page file usage as reported by the OS" );
CCP_STATS_DECLARE( processHeap,					"Blue/Memory/ProcessHeap", false, CST_MEMORY, "The amount of memory allocated in the process heap" );
CCP_STATS_DECLARE( crtHeap,						"Blue/Memory/CrtHeap", false, CST_MEMORY, "The amount of memory allocated in the crt heap" );
CCP_STATS_DECLARE( crtHeapUnaccounted,			"Blue/Memory/CrtHeapUnaccounted", false, CST_MEMORY, "The amount of memory allocated in the crt heap that is not accounted for" );
CCP_STATS_DECLARE( d3dHeap1,					"Blue/Memory/D3DHeap1", false, CST_MEMORY, "The amount of memory allocated in the first D3D heap" );
CCP_STATS_DECLARE( d3dHeap2,					"Blue/Memory/D3DHeap2", false, CST_MEMORY, "The amount of memory allocated in the second D3D heap" );
CCP_STATS_DECLARE( customHeap1,					"Blue/Memory/CustomHeap1", false, CST_MEMORY, "The amount of memory allocated in the first custom heap" );
CCP_STATS_DECLARE( customHeap2,					"Blue/Memory/CustomHeap2", false, CST_MEMORY, "The amount of memory allocated in the second custom heap" );
CCP_STATS_DECLARE( customHeap3,					"Blue/Memory/CustomHeap3", false, CST_MEMORY, "The amount of memory allocated in the third custom heap" );
CCP_STATS_DECLARE( trackingHeap,				"Blue/Memory/TrackingHeap", false, CST_MEMORY, "The amount of memory allocated for tracking memory allocations" );
CCP_STATS_DECLARE( allHeaps,					"Blue/Memory/AllHeaps", false, CST_MEMORY, "The amount of memory allocated in all heaps owned by the process" );
CCP_STATS_DECLARE( unknownHeaps,				"Blue/Memory/UnknownHeaps", false, CST_MEMORY, "The amount of memory allocated in all unidentified heaps owned by the process" );
#endif

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "Memory" );

MemoryTracker::MemoryTracker( IRoot* lockobj /*= NULL */ ) :
#ifdef _WIN32
	m_d3dHeap1( NULL ),
	m_d3dHeap2( NULL ),
	m_customHeap1( NULL ),
	m_customHeap2( NULL ),
	m_customHeap3( NULL ),
	m_lastLoggedWorkingSet( 0 ),
	m_lastLoggedPageFileUsage( 0 ),
#endif
#if CCP_STACKLESS
	m_lastLoggedPython( 0 ),
#endif
	m_isFullCapture( false ),
	m_loggingThreshold( 50*1024*1024 ),
	m_lastLoggedMalloc( 0 )
{
}

void MemoryTracker::Update()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	UpdateDetailedTracking();

	size_t n;
	if( MemoryTrackerGetCount( n ) )
	{
		CCP_STATS_SET( trackedAllocationsCount, n );
	}
	if( MemoryTrackerGetSize( n ) )
	{
		CCP_STATS_SET( trackedAllocationsSize, n );
	}

	auto mallocMemory = CCPMallocUsage();
	CCP_STATS_SET( beMemory, mallocMemory );

#ifdef _WIN32
	size_t workingSetMemory = 0;
	size_t pageFileMemory = 0;
	PROCESS_MEMORY_COUNTERS mc;
	if( GetProcessMemoryInfo( GetCurrentProcess(), &mc, sizeof(mc)) )
	{
		workingSetMemory = mc.WorkingSetSize;
		pageFileMemory = mc.PagefileUsage;
		CCP_STATS_SET( workingSetSize, mc.WorkingSetSize );
		CCP_STATS_SET( pageFileUsage, mc.PagefileUsage );
	}
#endif

#if CCP_STACKLESS
	auto pythonMemory = PySys_GetPyMalloced();
	CCP_STATS_SET( pyMemory, pythonMemory );

	bool logMemory = false;

	if( IsAboveLoggingThreshold( pythonMemory, m_lastLoggedPython ) )
	{
		logMemory = true;
	}
	if( IsAboveLoggingThreshold( mallocMemory, m_lastLoggedMalloc ) )
	{
		logMemory = true;
	}
	if( IsAboveLoggingThreshold( workingSetMemory, m_lastLoggedWorkingSet ) )
	{
		logMemory = true;
	}
	if( IsAboveLoggingThreshold( pageFileMemory, m_lastLoggedPageFileUsage ) )
	{
		logMemory = true;
	}

	if( logMemory )
	{
		m_lastLoggedPython = pythonMemory;
		m_lastLoggedMalloc = mallocMemory;
		m_lastLoggedWorkingSet = workingSetMemory;
		m_lastLoggedPageFileUsage = pageFileMemory;

		CCP_LOGNOTICE_CH( 
			s_ch, 
			"Python memory: %lldK, Blue memory: %lldK, Working set size: %lldK, Page file usage: %lldK", 
			m_lastLoggedPython / 1024,
			m_lastLoggedMalloc / 1024,
			m_lastLoggedWorkingSet / 1024,
			m_lastLoggedPageFileUsage / 1024
		);
	}
#endif
}

#ifdef _WIN32
bool MemoryTracker::IsKnownHeap( HANDLE heap )
{
	if( heap == ::GetProcessHeap() )
	{
		return true;
	}

	if( heap == (HANDLE)_get_heap_handle() )
	{
		return true;
	}

	if( heap == (HANDLE)m_d3dHeap1 )
	{
		return true;
	}

	if( heap == (HANDLE)m_d3dHeap2 )
	{
		return true;
	}

	if( heap == (HANDLE)m_customHeap1 )
	{
		return true;
	}

	if( heap == (HANDLE)m_customHeap2 )
	{
		return true;
	}

	if( heap == (HANDLE)m_customHeap3 )
	{
		return true;
	}

	return false;
}

static int FindLargestHeapIx( HANDLE* heaps, size_t* sizes, int count )
{
	size_t largest = 0;
	int largestIx = 0;
	for( int i = 0; i < count; ++i )
	{
		if( sizes[i] > largest )
		{
			largest = sizes[i];
			largestIx = i;
		}
	}

	return largestIx;
}

void MemoryTracker::SetCustomHeapsToLargestHeaps()
{
	HANDLE heaps[256];
	size_t sizes[256];

	DWORD count = ::GetProcessHeaps( 256, heaps );

	for( DWORD i = 0; i < count; ++i )
	{
		HANDLE heap = heaps[i];
		size_t size = GetHeapSizeWithHeapWalk( heap );

		if( size == (size_t)-1 )
		{
			size = 0;
		}

		sizes[i] = size;
	}

	int largestIx = FindLargestHeapIx( heaps, sizes, count );
	m_customHeap1 = (intptr_t)heaps[largestIx];
	sizes[largestIx] = 0;

	largestIx = FindLargestHeapIx( heaps, sizes, count );
	m_customHeap2 = (intptr_t)heaps[largestIx];
	sizes[largestIx] = 0;

	largestIx = FindLargestHeapIx( heaps, sizes, count );
	m_customHeap3 = (intptr_t)heaps[largestIx];
}
#endif

void MemoryTracker::SetFullCapture( bool b )
{
	m_isFullCapture = b;
}

void MemoryTracker::SummaryReport( const char* filename )
{
	FILE* file;
	fopen_s( &file, filename, "w" );

	fprintf( file, "Memory allocations by name\n" );
	fprintf( file, "-----------------------------------------------------------------------------\n" );
	MemoryTrackerSummaryReportToFile( file );
	fprintf( file, "\n\n" );

	fprintf( file, "Memory statistics\n" );
	fprintf( file, "-----------------------------------------------------------------------------\n" );
#if CCP_STACKLESS
	fprintf( file, "Python reported memory, %d\n", PySys_GetPyMalloced() );
#endif
	fprintf( file, "CCP Malloc usage, %" CCP_SIZET_FORMAT "\n", CCPMallocUsage() );

#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS mc;
	if( GetProcessMemoryInfo( GetCurrentProcess(), &mc, sizeof(mc)) )
	{
		fprintf( file, "Working set size, %d\n", mc.WorkingSetSize );
		fprintf( file, "Page file usage, %d\n", mc.PagefileUsage );
	}

	if( g_isMemoryTrackingEnabled )
	{
		size_t processHeapSize = 0;
		size_t crtHeapSize = 0;
		size_t d3dHeap1Size = 0;
		size_t d3dHeap2Size = 0;
		size_t trackingHeapSize = 0;

		HANDLE heaps[256];
		DWORD count;

		if( m_isFullCapture )
		{
			count = ::GetProcessHeaps( 256, heaps );
		}
		else
		{
			heaps[0] = GetProcessHeap();
			heaps[1] = (HANDLE)_get_heap_handle();
			heaps[2] = MemoryTrackerGetHeapForTracking();
			count = 3;
		}

		size_t totalSize = 0;

		for( DWORD i = 0; i < count; ++i )
		{
			HANDLE heap = heaps[i];
			size_t size = GetHeapSizeWithHeapWalk( heap );

			if( size == (size_t)-1 )
			{
				continue;
			}

			if( heap == ::GetProcessHeap() )
			{
				processHeapSize = size;
			}
			else if( heap == (HANDLE)_get_heap_handle() )
			{
				crtHeapSize = size;
			}
			else if( heap == (HANDLE)m_d3dHeap1 )
			{
				d3dHeap1Size = size;
			}
			else if( heap == (HANDLE)m_d3dHeap2 )
			{
				d3dHeap2Size = size;
			}
			else if( heap == MemoryTrackerGetHeapForTracking() )
			{
				trackingHeapSize = size;
			}

			totalSize += size;
		}

		size_t unaccountedSize = crtHeapSize - CCPMallocUsage();

		fprintf( file, "All heaps, %" CCP_SIZET_FORMAT "\n", totalSize );
		fprintf( file, "Process heap, %" CCP_SIZET_FORMAT "\n", processHeapSize );
		fprintf( file, "CRT heap, %" CCP_SIZET_FORMAT "\n", crtHeapSize );
		fprintf( file, "CRT heap unaccounted, %" CCP_SIZET_FORMAT "\n", unaccountedSize );
		fprintf( file, "D3D heap 1, %" CCP_SIZET_FORMAT "\n", d3dHeap1Size );
		fprintf( file, "D3D heap 2, %" CCP_SIZET_FORMAT "\n", d3dHeap2Size );
		fprintf( file, "Tracking heap, %" CCP_SIZET_FORMAT "\n", trackingHeapSize );
	}
#endif
	fclose( file );

}

void MemoryTracker::UpdateDetailedTracking()
{
	if( g_isMemoryTrackingEnabled )
	{
#ifdef _WIN32
		size_t processHeapSize = 0;
		size_t crtHeapSize = 0;
		size_t d3dHeap1Size = 0;
		size_t d3dHeap2Size = 0;
		size_t customHeap1Size = 0;
		size_t customHeap2Size = 0;
		size_t customHeap3Size = 0;
		size_t trackingHeapSize = 0;

		HANDLE heaps[256];
		DWORD count;

		if( m_isFullCapture )
		{
			count = ::GetProcessHeaps( 256, heaps );
		}
		else
		{
			heaps[0] = GetProcessHeap();
			heaps[1] = (HANDLE)_get_heap_handle();
			heaps[2] = MemoryTrackerGetHeapForTracking();
			count = 3;
		}

		size_t totalSize = 0;

		for( DWORD i = 0; i < count; ++i )
		{
			HANDLE heap = heaps[i];
			size_t size = GetHeapSizeWithHeapWalk( heap );

			if( size == (size_t)-1 )
			{
				continue;
			}

			if( heap == ::GetProcessHeap() )
			{
				processHeapSize = size;
				CCP_STATS_SET( processHeap, processHeapSize );
			}
			else if( heap == (HANDLE)_get_heap_handle() )
			{
				crtHeapSize = size;
				CCP_STATS_SET( crtHeap, crtHeapSize );
			}
			else if( heap == (HANDLE)m_d3dHeap1 )
			{
				d3dHeap1Size = size;
				CCP_STATS_SET( d3dHeap1, d3dHeap1Size );
			}
			else if( heap == (HANDLE)m_d3dHeap2 )
			{
				d3dHeap2Size = size;
				CCP_STATS_SET( d3dHeap2, d3dHeap2Size );
			}
			else if( heap == (HANDLE)m_customHeap1 )
			{
				customHeap1Size = size;
				CCP_STATS_SET( customHeap1, customHeap1Size );
			}
			else if( heap == (HANDLE)m_customHeap2 )
			{
				customHeap2Size = size;
				CCP_STATS_SET( customHeap2, customHeap2Size );
			}
			else if( heap == (HANDLE)m_customHeap3 )
			{
				customHeap3Size = size;
				CCP_STATS_SET( customHeap3, customHeap3Size );
			}
			else if( heap == MemoryTrackerGetHeapForTracking() )
			{
				trackingHeapSize = size;
				CCP_STATS_SET( trackingHeap, trackingHeapSize );
			}

			totalSize += size;
		}

		CCP_STATS_SET( allHeaps, totalSize );

		size_t unknownSize = totalSize;
		unknownSize -= processHeapSize;
		unknownSize -= crtHeapSize;
		unknownSize -= d3dHeap1Size;
		unknownSize -= d3dHeap2Size;
		unknownSize -= customHeap1Size;
		unknownSize -= customHeap2Size;
		unknownSize -= customHeap3Size;
		unknownSize -= trackingHeapSize;

		CCP_STATS_SET( unknownHeaps, unknownSize );


		size_t unaccountedSize = crtHeapSize - CCPMallocUsage();
		CCP_STATS_SET( crtHeapUnaccounted, unaccountedSize );
		// TODO: Do something useful on non-win32 platforms
#endif
	}
}

bool MemoryTracker::IsAboveLoggingThreshold( int64_t current, int64_t last )
{
	if( !m_loggingThreshold )
	{
		return false;
	}

	int64_t delta;
	if( current > last )
	{
		delta = current - last;
	}
	else
	{
		delta = last - current;
	}

	if( delta > m_loggingThreshold )
	{
		return true;
	}
	else
	{
		return false;
	}
}
