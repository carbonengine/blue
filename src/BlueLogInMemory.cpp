// Copyright © 2010 CCP ehf.

#include "StdAfx.h"
#include "blueloginmemory.h"
#include "IBlueOS.h"

static BlueLogInMemory* s_logInMemory = BlueLogInMemory::GetInstance();

namespace
{
	CcpMutex s_mutex( "BlueLogInMemory", "s_mutex" );
}

BlueLogInMemory* BlueLogInMemory::GetInstance()
{
	static CBlueLogInMemory s_instance;
	return &s_instance;
}

BlueLogInMemory::BlueLogInMemory( IRoot* lockobj /*= NULL */ ) :
	m_entries( "BlueLogInMemory/s_entries" ),
	m_currentEntry( 0 ),
	m_entryCount( 0 ),
	m_capacity( 256 ),
	m_threshold( CCP::LOGTYPE_NOTICE ),
	m_isActive( false ),
	m_saveOnQuit( false )
#if BLUE_WITH_PYTHON
	, m_saveLogCallback( nullptr )
#endif
{

}

BlueLogInMemory::~BlueLogInMemory()
{

}

void BlueLogInMemory::InitializeEntries()
{
	CcpAutoMutex guard( s_mutex );

	m_currentEntry = 0;
	m_entryCount = 0;
	m_entries.resize( m_capacity );
}

void BlueLogInMemory::Log_s( CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message )
{
	s_logInMemory->Log( channel, type, userData, message );
}

void BlueLogInMemory::Log( CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message )
{
	CcpAutoMutex guard( s_mutex );

	Entry& entry = m_entries[m_currentEntry];
	entry.severity = type;
	entry.timestamp = BeOS->GetActualTime();
	strncpy_s( entry.facility, channel.facility, sizeof( entry.facility ) - 1 );
	strncpy_s( entry.object, channel.object, sizeof( entry.object ) - 1 );
	strncpy_s( entry.message, message, sizeof( entry.message ) - 1 );

	++m_currentEntry;
	if( m_entryCount < m_capacity )
	{
		++m_entryCount;
	}
	if( m_currentEntry == m_capacity )
	{
		m_currentEntry = 0;
	}
}

void BlueLogInMemory::Start()
{
	if( !m_isActive )
	{
		InitializeEntries();

		CCP::RegisterLogEcho( Log_s, m_threshold, true );
		m_isActive = true;
	}
}

void BlueLogInMemory::Stop()
{
	CCP::UnregisterLogEcho( Log_s );
	m_isActive = false;
}

bool BlueLogInMemory::IsActive()
{
	return m_isActive;
}

#if BLUE_WITH_PYTHON
PyObject* BlueLogInMemory::GetEntries( PyObject* args )
{
	if( !PyArg_ParseTuple( args, "" ) )
	{
		return nullptr;
	}

	CcpAutoMutex guard( s_mutex );

	PyObject* result = PyList_New( m_entryCount );

	for( int i = 0; i < m_entryCount; ++i )
	{
		int srcIx = (m_currentEntry - 1 + m_capacity - i) % m_capacity;

		Entry& entry = m_entries[srcIx];

		PyObject* pyEntry = PyTuple_New( 5 );

		PyTuple_SET_ITEM( pyEntry, 0, PyUnicode_FromString( entry.facility ) );
		PyTuple_SET_ITEM( pyEntry, 1, PyUnicode_FromString( entry.object ) );
		PyTuple_SET_ITEM( pyEntry, 2, PyLong_FromLong( entry.severity ) );
		PyTuple_SET_ITEM( pyEntry, 3, PyLong_FromLongLong( entry.timestamp ) );
		PyTuple_SET_ITEM( pyEntry, 4, PyUnicode_FromString( entry.message ) );

		PyList_SET_ITEM( result, i, pyEntry );
	}

	return result;
}
#endif

void BlueLogInMemory::Clear()
{
	InitializeEntries();
}

void BlueLogInMemory::SetCapacity( int size )
{
	m_capacity = size;
	Clear();
}

int BlueLogInMemory::GetCapacity()
{
	return m_capacity;
}

void BlueLogInMemory::SetThreshold( int threshold )
{
	m_threshold = (CCP::LogType)threshold;

	if( m_isActive )
	{
		CCP::UnregisterLogEcho( Log_s );

		CCP::RegisterLogEcho( Log_s, m_threshold, true );
	}
}

int BlueLogInMemory::GetThreshold()
{
	return m_threshold;
}

void BlueLogInMemory::SetSaveOnQuit( bool saveOnQuit )
{
	m_saveOnQuit = saveOnQuit;
}
bool BlueLogInMemory::IsSaveOnQuit()
{
	return m_saveOnQuit;
}

void BlueLogInMemory::ExecuteSaveLogCallback()
{
#if BLUE_WITH_PYTHON
	//if we have a callback and our save on quit flag is true
	if( m_saveLogCallback && m_saveLogCallback != Py_None && m_saveOnQuit )
	{
		//call the save logs callback
		PyObject* ret = PyObject_CallFunction( m_saveLogCallback, (char*)"" );
		Py_XDECREF(ret);
	}
#endif
}
