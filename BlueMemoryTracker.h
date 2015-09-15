////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		January 2013
// Copyright:	CCP 2013
//

#pragma once
#ifndef BlueMemoryTracker_h
#define BlueMemoryTracker_h

// This class is used to expose the memory tracking to Python
class MemoryTracker : public IRoot
{
public:
	EXPOSE_TO_BLUE();

	MemoryTracker( IRoot* lockobj = NULL );
	~MemoryTracker() {};

	void SetFullCapture( bool b );
	void Update();

#ifdef _WIN32
	bool IsKnownHeap( HANDLE heap );
	void SetCustomHeapsToLargestHeaps();
#endif
	void SummaryReport( const char* filename );

private:
	void UpdateDetailedTracking();
	bool IsAboveLoggingThreshold( int64_t pythonMemory, int64_t m_lastLoggedPython );
	void PrintFieldToFile( FILE* file, const char* name, size_t totalSize );

private:
#ifdef _WIN32
	intptr_t m_d3dHeap1;
	intptr_t m_d3dHeap2;
	intptr_t m_customHeap1;
	intptr_t m_customHeap2;
	intptr_t m_customHeap3;
#endif

	int64_t m_lastLoggedWorkingSet;
	int64_t m_lastLoggedPageFileUsage;

	bool m_isFullCapture;
	int64_t m_loggingThreshold;

	int64_t m_lastLoggedMalloc;

#if CCP_STACKLESS
	int64_t m_lastLoggedPython;
#endif
};

TYPEDEF_BLUECLASS( MemoryTracker );

extern BLUEIMPORT MemoryTracker* BeMemoryTracker;

#endif // BlueMemoryTracker_h