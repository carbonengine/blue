// Copyright © 2013 CCP ehf.

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

	void SummaryReport( const char* filename );
	void DumpReportAsText( const char* filename );
	void DumpReportAsBinary( const char* filename );
	size_t GetCount();
	size_t GetSize();
	void CallstackCaptureEnable( bool enable );

private:
	void UpdateDetailedTracking();
	bool IsAboveLoggingThreshold( int64_t pythonMemory, int64_t m_lastLoggedPython );
	void PrintFieldToFile( FILE* file, const char* name, size_t totalSize );

private:
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