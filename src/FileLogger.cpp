// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#include "FileLogger.h"

#include "zlib.h"
#include <string>

#include <iostream>
#include <fstream>

#include <BlueStatistics.h>

//---------------------------------------------------
// Uncomment this define to turn this files optimization off
//#define OptimizeOff
#ifdef OptimizeOff
#pragma message(__FILE__"local optimizations are off")
#pragma optimize("", off)
#endif

CCP_STATS_DECLARE( fileLoggingMessages, "FileLogging/Messages", false, CST_COUNTER_HIGH, "The number of log messages we have recived" );
CCP_STATS_DECLARE( fileLoggingMessagesDropped, "FileLogging/MessagesDropped", false, CST_COUNTER_HIGH, "The number of messages dropped for some reason" );
CCP_STATS_DECLARE( fileLoggingBufferFull, "FileLogging/BufferFull", false, CST_COUNTER_HIGH, "Number of times we encountered a inter-thread full buffer" );
CCP_STATS_DECLARE( fileLoggingBufferOutOfOrder, "FileLogging/BufferOutOfOrder", false, CST_COUNTER_HIGH, "Number of times we encountered a out of order message from the inter-thread buffer" );
CCP_STATS_DECLARE( fileLoggingBytesSent, "FileLogging/BytesSent", false, CST_COUNTER_HIGH, "The amount of bytes sent out via file logging" );
CCP_STATS_DECLARE( fileLoggingPacketsSent, "FileLogging/PacketsSent", false, CST_COUNTER_HIGH, "The amount of file packets sent out via file logging" );

namespace CCP
{
	// Flag which is set to true once network logging has been turned on
	static bool s_fileLoggingEnabled = false;

	// the role string is set when logging is turned on and is set into every log message
	static char* s_role = nullptr ;

	static std::ofstream s_outstream;
	static char s_currentLogPath[256];
	static char* s_logDirectory = nullptr;

	static const int s_logType2syslog[ CCP::LOGTYPE_COUNT ] = { 6, 5, 4, 3 };	
	static unsigned long long s_logID = 1;

	static FileMessageCircularBuffer * s_fileLogMessageBuffer = nullptr;
	static CcpSemaphore s_logMessageWritten;
	static CcpThreadHandle_t s_fileLoggingThreadHandle = 0;

	FileMessageCircularBuffer::FileMessageCircularBuffer(unsigned int size) : 
		m_size(size),
		m_writeIndex(0), 
		m_readIndex(0), 
		m_lock( "FileMessageCircularBuffer", "m_lock" )
	{
		m_data = CCP_NEW("FileMessageCircularBuffer/Data") FileLogMessage [m_size];
		for (unsigned int i = 0; i< m_size; i++)
		{
			this->m_data[i] = FileLogMessage();
			this->m_data[i].id = 0;
		}
	}

	FileMessageCircularBuffer::~FileMessageCircularBuffer()
	{
		CCP_DELETE[] m_data;
	}

	bool FileMessageCircularBuffer::IsFull()
	{
		CcpAutoMutex lock( m_lock );
		bool full =  ( ((m_writeIndex + 1) % m_size) == m_readIndex );
		return full;
	}


	bool FileMessageCircularBuffer::IsEmpty()
	{
		CcpAutoMutex lock( m_lock );
		bool empty = (m_readIndex == m_writeIndex);
		return empty;
	}

	FileLogMessage* FileMessageCircularBuffer::Enque()
	{
		FileLogMessage* msg = &s_fileLogMessageBuffer->m_data[ s_fileLogMessageBuffer->m_writeIndex ];
		s_fileLogMessageBuffer->m_writeIndex++;
		s_fileLogMessageBuffer->m_writeIndex %= m_size; // wrap around
		return msg;
	}

	FileLogMessage* FileMessageCircularBuffer::Deque()
	{
		FileLogMessage *msg = &s_fileLogMessageBuffer->m_data[ m_readIndex ];
		m_readIndex++;;
		m_readIndex %= m_size; // wrap around				
		return msg;
	}

	void FileMessageCircularBuffer::Lock()
	{
		m_lock.Acquire();
	}

	void FileMessageCircularBuffer::Unlock()
	{
		m_lock.Release();
	}


	bool SetupFileLogging(CCP::LogType threshold, const char* logDirectory, const char* role, char* errorMessageOut, int errorMessageBufferSize)
	{
		if (s_fileLoggingEnabled == true)
		{
			// Already setup, close it and set it up again with the net config
			DisableFileLogging();
		}

		s_role = CCP_STRDUP("FileLogger/role", role );
		s_logDirectory = CCP_STRDUP("FileLogger/s_logDirectory", logDirectory );

		// Let's go !
		s_fileLoggingEnabled = true;

		s_logID = 1;

		CCP::s_fileLogMessageBuffer = CCP_NEW("FileLogger/FileLogMessageBuffer") FileMessageCircularBuffer( 128 );

		// Let's start the worker thread
		if( !CCP::s_fileLoggingThreadHandle )
		{
			// Start the file thread
			CCP::s_fileLoggingThreadHandle = CcpCreateThread( CCP::FileLoggingThreadFunc, nullptr, CCP_THREAD_PRIORITY_NORMAL );
		}

		CCP::RegisterLogEcho( &CCP::LogToFile, threshold, true, CCP::LOG_ECHO_REQUIRES_PRIVILEGE_CHECK );

		return true;
	}

	void DisableFileLogging()
	{
		if (s_fileLoggingEnabled)
		{
			// Let's go !
			s_fileLoggingEnabled = false;

			// Stop sending logging messages to the file handler
			CCP::UnregisterLogEcho( &CCP::LogToFile );

			if (s_role )
			{
				CCP_DELETE[] s_role;
				s_role = nullptr;
			}

			if (s_fileLogMessageBuffer )
			{
				CCP_DELETE CCP::s_fileLogMessageBuffer;
				s_fileLogMessageBuffer = nullptr;
			}

			CCP::s_fileLoggingThreadHandle = 0;

			// Wake up the worker thread so it know's to terminate itself
			s_logMessageWritten.Signal();

		}
	}


	void LogToFile( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* message )
	{
		CCP_STATS_ZONE( __FUNCTION__ );
		CCP_STATS_INC(fileLoggingMessages);
		if ( !s_fileLoggingEnabled )
		{
			// Early out
			return;
		}

		if(strlen(message) == 0)
		{
			// guard aginst empty messages since they don't make a valid GELF message
			return;
		}

		// if the buffer is full, we wait a little bit
		while ( s_fileLogMessageBuffer->IsFull() )
		{
			CCP_STATS_INC( fileLoggingBufferFull );

			// Yield execution to the thread reading of the buffer
			CcpThreadSleep( 0 );
		}

		// Grab the next free slot
		s_fileLogMessageBuffer->Lock();
		FileLogMessage* msg = s_fileLogMessageBuffer->Enque();

		// copy into the file message
		strncpy_s( msg->object, 32, logObject.object, _TRUNCATE );
		strncpy_s( msg->facility, 32, logObject.facility, _TRUNCATE );
		strncpy_s( msg->message, MAX_MESSAGE_SIZE, message, _TRUNCATE ); 

		TimeAsDateTime( msg->systemtime, TimeNow() );

		msg->type = (int) type;
		msg->id = s_logID;

		s_logID++;

		s_fileLogMessageBuffer->Unlock();

		// wake up the file thread
		s_logMessageWritten.Signal();

	}

	uint32_t FileLoggingThreadFunc( void* arg )
	{
		CCP_STATS_ZONE( __FUNCTION__ );

		unsigned long long lc = 0;

		CCP::FileLogMessage * msg = NULL;
		do
		{
			// check here in case we just got woken up
			if (!s_fileLoggingEnabled)
			{
				// We're out !
				return 0;
			}

			while ( !s_fileLogMessageBuffer->IsEmpty() )
			{
				s_fileLogMessageBuffer->Lock();
				msg = s_fileLogMessageBuffer->Deque();	

				// make sure we are logging to the correct file
				char filename[256];

				sprintf_s( filename, "%s\\%s-%04d.%02d.%02d.%02d-%d.log", 
					s_logDirectory, 
					s_role,
					msg->systemtime.year, 
					msg->systemtime.month, 
					msg->systemtime.day, 
					msg->systemtime.hour, 
					CcpGetCurrentProcessId() );
				if (strcmp( filename, s_currentLogPath) != 0)
				{
					// rolling to a new file
					if( s_outstream.is_open() )
					{
						s_outstream.close();
					}

					s_outstream.open( filename );
					sprintf_s( s_currentLogPath, "%s", filename );
				}

				if ( (msg->id-1) != lc )
				{
					CCP_STATS_INC( fileLoggingBufferOutOfOrder );
					//std::cout << "out of order ! missing" << (msg->id-1) - lc << "\n";
				}
				lc = msg->id;

				// Map internal loglevels to syslog levels
				if ( msg->type>3 )
				{
					CCP_STATS_INC(fileLoggingMessagesDropped);
					// unknown loglevel
					continue;
				}
				int logLevel = s_logType2syslog[ msg->type];

				char timestamp[64];
				sprintf_s( timestamp, "[%04d-%02d-%02d %02d:%02d:%02d:%03dZ]", 
					msg->systemtime.year, 
					msg->systemtime.month, 
					msg->systemtime.day, 
					msg->systemtime.hour, 
					msg->systemtime.minute, 
					msg->systemtime.second, 
					msg->systemtime.milliseconds
					);
				s_outstream << timestamp << "\t";

				s_outstream << msg->id << "\t";
				s_outstream << logLevel << "\t";
				s_outstream << CcpGetCurrentProcessId() << "\t";
				s_outstream << s_role << "\t";
				s_outstream << msg->facility << "\t";
				s_outstream << msg->object << "\t\n";
				s_outstream << msg->message << "\r\n";

				s_outstream.flush();
				s_fileLogMessageBuffer->Unlock();
			}


			if (!s_fileLoggingEnabled)
			{
				// We're out !
				return 0;
			}
		} while ( s_logMessageWritten.Wait() && s_fileLoggingEnabled );

		return 0;
	}
}
/* Python functions to enable and disable this */



#if BLUE_WITH_PYTHON
PyObject* PyEnableFileLogging( PyObject* self, PyObject* args )
{
	char *role;
	char *logDirectory;
	int threshold = 0;

	if( PyArg_ParseTuple( args, "ss|i", &logDirectory, &role, &threshold ) )
	{
		if( threshold < CCP::LOGTYPE_LOWEST )
		{
			threshold = CCP::LOGTYPE_LOWEST;
		}
		if( threshold > CCP::LOGTYPE_HIGHEST )
		{
			threshold = CCP::LOGTYPE_HIGHEST;
		}

		static const int errorMessageBufferSize = 256;
		char errorMessage[errorMessageBufferSize];
		memset(errorMessage, 0, errorMessageBufferSize);

		bool ret = CCP::SetupFileLogging( (CCP::LogType) threshold, logDirectory, role, errorMessage, errorMessageBufferSize );
		if (!ret)
		{
			PyErr_SetString(PyExc_RuntimeError, errorMessage);
			return nullptr;
		}
		Py_RETURN_TRUE;

	}

	Py_RETURN_FALSE;
}

MAP_FUNCTION( 
	"EnableFileLogging", 
	PyEnableFileLogging, 
	"Enables echoing of log to the file\n"
	":param dirPath: log directory path\n"
	":param role: role name\n"
	":param threshold: Optional[int]\n"
	":rtype: bool" );


void DisableFileLogging()
{
	CCP::DisableFileLogging( );
}

MAP_FUNCTION_AND_WRAP( "DisableFileLogging", DisableFileLogging, "Disabled file logging" );

PyObject* PyGetFileLoggingState( PyObject* self, PyObject* args )
{
	if ( CCP::s_fileLoggingEnabled )
	{
		PyObject* t = PyTuple_New(3);
		PyTuple_SetItem(t, 0, PyUnicode_FromString( "" ));
		PyTuple_SetItem(t, 1, PyLong_FromLong( 1  ));

		int isFull = 0;
		if (CCP::s_fileLogMessageBuffer->IsFull())
		{
			isFull = 1;
		}
		PyTuple_SetItem(t, 2, PyBool_FromLong( isFull ));

		return t;
	}
	Py_RETURN_FALSE;

}

MAP_FUNCTION( 
	"GetFileLoggingState", 
	PyGetFileLoggingState, 
	"Tell you where the server is logging to\n"
	":rtype: bool | (str, int, bool)" );
#endif

#ifdef OptimizeOff
#pragma optimize("", on)
#endif
