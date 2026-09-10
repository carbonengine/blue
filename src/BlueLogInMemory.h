// Copyright © 2010 CCP ehf.

#pragma once
#ifndef BlueLogInMemory_h
#define BlueLogInMemory_h

#include "Blue.h"

BLUE_DECLARE( BlueLogInMemory );

// This is really a singleton, but need a regular class for Python exposure
// so can't really use the monostate pattern.
class BlueLogInMemory : public IRoot
{
public:
	EXPOSE_TO_BLUE();

	// Constructor is protected to prevent multiple instances.
	// Use GetInstance instead.

	~BlueLogInMemory();

	static BlueLogInMemory* GetInstance();

	// The log echo function that will be registered with the log system
	static void Log_s( CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message );
	
	void Log( CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message );

	// The following functions are meant for Python usage
	void Start();
	void Stop();

	bool IsActive();

#if BLUE_WITH_PYTHON
	PyObject* GetEntries( PyObject* args );
#endif

	void Clear();
	void SetCapacity( int size );
	int GetCapacity();
	void SetThreshold( int threshold );
	int GetThreshold();

	void SetSaveOnQuit( bool saveOnQuit );
	bool IsSaveOnQuit();

	void ExecuteSaveLogCallback();

protected:
	BlueLogInMemory( IRoot* lockobj = NULL );

	void InitializeEntries();

private:
	struct Entry
	{
		CCP::LogType severity;
		Be::Time timestamp;
		char facility[32];
		char object[32];
		char message[256*25];
	};
	TrackableStdVector<Entry> m_entries;
	int m_currentEntry;
	int m_entryCount;
	int m_capacity;
	CCP::LogType m_threshold;
	bool m_isActive;
	bool m_saveOnQuit;

#if BLUE_WITH_PYTHON
	PyObject* m_saveLogCallback;
#endif

};

typedef RootNoLock<BlueLogInMemory> CBlueLogInMemory;

#endif // BlueLogInMemory_h
