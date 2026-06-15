// Copyright © 2013 CCP ehf.

#include "StdAfx.h"

#include "BlueMemoryTracker.h"

#include <BlueStatistics.h>

CCP_STATS_DECLARE( beMemory,					"Blue/Memory/Malloc", false, CST_MEMORY, "The amount of memory allocated via CCP_MALLOC" );
CCP_STATS_DECLARE( trackedAllocationsCount,		"Blue/Memory/trackedAllocationsCount", false, CST_COUNTER_HIGH, "Number of tracked allocations live in the system" );
CCP_STATS_DECLARE( trackedAllocationsSize,		"Blue/Memory/trackedAllocationsSize", false, CST_MEMORY, "Combined size of tracked allocations live in the system" );

#if CCP_STACKLESS
CCP_STATS_DECLARED_ELSEWHERE( pyMemory );
#endif

CCP_STATS_DECLARE( workingSetSize,				"Blue/Memory/WorkingSet", false, CST_MEMORY, "The working set size as reported by the OS" );
CCP_STATS_DECLARE( pageFileUsage,				"Blue/Memory/PageFileUsage", false, CST_MEMORY, "Page file usage as reported by the OS" );
#ifdef _WIN32
CCP_STATS_DECLARE( processHeap,					"Blue/Memory/ProcessHeap", false, CST_MEMORY, "The amount of memory allocated in the process heap" );
CCP_STATS_DECLARE( crtHeap,						"Blue/Memory/CrtHeap", false, CST_MEMORY, "The amount of memory allocated in the crt heap" );
CCP_STATS_DECLARE( crtHeapUnaccounted,			"Blue/Memory/CrtHeapUnaccounted", false, CST_MEMORY, "The amount of memory allocated in the crt heap that is not accounted for" );
CCP_STATS_DECLARE( trackingHeap,				"Blue/Memory/TrackingHeap", false, CST_MEMORY, "The amount of memory allocated for tracking memory allocations" );
CCP_STATS_DECLARE( allHeaps,					"Blue/Memory/AllHeaps", false, CST_MEMORY, "The amount of memory allocated in all heaps owned by the process" );
CCP_STATS_DECLARE( unknownHeaps,				"Blue/Memory/UnknownHeaps", false, CST_MEMORY, "The amount of memory allocated in all unidentified heaps owned by the process" );
#endif

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "Memory" );

MemoryTracker::MemoryTracker( IRoot* lockobj /*= NULL */ ) :
	m_lastLoggedWorkingSet( 0 ),
	m_lastLoggedPageFileUsage( 0 ),
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

	CcpProcessMemoryInfo info;
    CcpGetProcessMemoryInfo( info );
	CCP_STATS_SET( workingSetSize, info.workingSetSize );
	CCP_STATS_SET( pageFileUsage, info.pageFileUsage );

#if CCP_STACKLESS
	auto pythonMemory = static_cast<int64_t>( CCP_STATS_GET( pyMemory ) );

	bool logMemory = false;

	if( IsAboveLoggingThreshold( pythonMemory, m_lastLoggedPython ) )
	{
		logMemory = true;
	}
	if( IsAboveLoggingThreshold( mallocMemory, m_lastLoggedMalloc ) )
	{
		logMemory = true;
	}
	if( IsAboveLoggingThreshold( info.workingSetSize, m_lastLoggedWorkingSet ) )
	{
		logMemory = true;
	}
	if( IsAboveLoggingThreshold( info.pageFileUsage, m_lastLoggedPageFileUsage ) )
	{
		logMemory = true;
	}

	if( logMemory )
	{
		m_lastLoggedPython = pythonMemory;
		m_lastLoggedMalloc = mallocMemory;
		m_lastLoggedWorkingSet = info.workingSetSize;
		m_lastLoggedPageFileUsage = info.pageFileUsage;

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
	PrintFieldToFile( file, "Python reported memory", static_cast<int64_t>( CCP_STATS_GET( pyMemory ) ) );
#endif
	PrintFieldToFile( file, "CCP Malloc usage", CCPMallocUsage() );

	CcpProcessMemoryInfo info;
    if( CcpGetProcessMemoryInfo( info ) )
    {
        PrintFieldToFile( file, "Working set size", info.workingSetSize );
        PrintFieldToFile( file, "Page file usage", info.pageFileUsage );
    }
    
#ifdef _WIN32
	if( IsMemoryTrackingEnabled() )
	{
		size_t processHeapSize = 0;
		size_t crtHeapSize = 0;
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
			else if( heap == MemoryTrackerGetHeapForTracking() )
			{
				trackingHeapSize = size;
			}

			totalSize += size;
		}

		size_t unaccountedSize = crtHeapSize - CCPMallocUsage();

		PrintFieldToFile( file, "All heaps", totalSize );
		PrintFieldToFile( file, "Process heap", processHeapSize );
		PrintFieldToFile( file, "CRT heap", crtHeapSize );
		PrintFieldToFile( file, "CRT heap unaccounted", unaccountedSize );
		PrintFieldToFile( file, "Tracking heap", trackingHeapSize );
	}
#endif
	fclose( file );

}

void MemoryTracker::DumpReportAsText( const char* filename )
{
	MemoryTrackerDumpReportAsText( filename );
}

void MemoryTracker::DumpReportAsBinary( const char* filename )
{
	MemoryTrackerDumpReportAsBinary( filename );
}

size_t MemoryTracker::GetCount()
{
	size_t count;
	MemoryTrackerGetCount( count );
	return count;
}

size_t MemoryTracker::GetSize()
{
	size_t size;
	MemoryTrackerGetSize( size );
	return size;
}

void MemoryTracker::CallstackCaptureEnable( bool enable )
{
	SetCallStackCaptureEnabled( enable );
}

void MemoryTracker::UpdateDetailedTracking()
{
	if( IsMemoryTrackingEnabled() )
	{
#ifdef _WIN32
		size_t processHeapSize = 0;
		size_t crtHeapSize = 0;
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

void MemoryTracker::PrintFieldToFile( FILE* file, const char* name, size_t totalSize )
{
	fprintf( file, "%50s %12" CCP_SIZET_FORMAT "\n", name, totalSize );
}
