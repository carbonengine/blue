// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#if BLUE_WITH_PYTHON
#ifdef _WIN32

#include "NetworkLogger.h"

#include "zlib.h"
#include <string>

#include <BlueStatistics.h>

//---------------------------------------------------
// Uncomment this define to turn this files optimization off
//#define OptimizeOff
#ifdef OptimizeOff
#pragma message(__FILE__"local optimizations are off")
#pragma optimize("", off)
#endif

CCP_STATS_DECLARE( networkLoggingMessages, "NetworkLogging/Messages", false, CST_COUNTER_HIGH, "The number of log messages we have recived" );
CCP_STATS_DECLARE( networkLoggingMessagesDropped, "NetworkLogging/MessagesDropped", false, CST_COUNTER_HIGH, "The number of messages dropped for some reason" );
CCP_STATS_DECLARE( networkLoggingBufferFull, "Network/funcLogging/BufferFull", false, CST_COUNTER_HIGH, "Number of times we encountered a inter-thread full buffer" );
CCP_STATS_DECLARE( networkLoggingBufferOutOfOrder, "NetworkLogging/BufferOutOfOrder", false, CST_COUNTER_HIGH, "Number of times we encountered a out of order message from the inter-thread buffer" );
CCP_STATS_DECLARE( networkLoggingBytesSent, "NetworkLogging/BytesSent", false, CST_COUNTER_HIGH, "The amount of bytes sent out via network logging" );
CCP_STATS_DECLARE( networkLoggingPacketsSent, "NetworkLogging/PacketsSent", false, CST_COUNTER_HIGH, "The amount of network packets sent out via network logging" );

namespace CCP
{
	// the struct holding the server address we send log messages to
	static struct sockaddr_in s_networkLoggingServerAddr;

	// The socket which we open and use to send log messages to the server defined in s_networkLoggingServerAddr
	static SOCKET s_networkLoggingSocket = INVALID_SOCKET;

	// Flag which is set to true once network logging has been turned on
	static bool s_networkLoggingEnabled = false;

	// the role string is set when logging is turned on and is set into every log message
	static char* s_role = NULL ;
	static char* s_hostname = NULL;

	static char* s_networkLoggingServerName = NULL;
	static int s_networkLoggingPort;

	// how big is each udp chunk. 1420 bytes is just under the default ethernet MTU
	const static int MAX_CHUNK_SIZE = 1420;

	const static unsigned int  BUFFER_SIZE = 1024*256;
	const static char* MESSAGE_TRUNCATED_NOTICE = " -- MESSAGE TRUNCATED";

	static const int s_logType2syslog[ CCP::LOGTYPE_COUNT ] = { 6, 5, 4, 3 };	
	static unsigned long long s_logID = 1;

	static const int NETWORKMESSAGE_BUFFER_SIZE = 128;
	static MessageCircularBuffer * s_messageBuffer = NULL;
	static HANDLE s_logMessageWritten = CreateEvent( 0, false, false, "NetworkLogging/m_logMessageWritten" );
	static uintptr_t s_threadHandle = NULL;

	// static work buffer for zipping messages so we don't have to malloc for each one
	Bytef *s_zippedDataBuffer = NULL;

	const static int CHUNKING_WORK_BUFFER_SIZE = MAX_CHUNK_SIZE + 32 + 2 + 4;
	static char* s_chunk_work_buffer = NULL;

	MessageCircularBuffer::MessageCircularBuffer(unsigned int size) : m_readIndex(0), m_writeIndex(0), m_size(size)
	{
		InitializeCriticalSection ( &m_lock );
		m_data = CCP_NEW("MessageCircularBuffer/Data") NetworkLogMessage [m_size];
		for (unsigned int i = 0; i< m_size; i++)
		{
			this->m_data[i] = NetworkLogMessage();
			this->m_data[i].id = 0;
		}
	}

	MessageCircularBuffer::~MessageCircularBuffer()
	{
		CCP_DELETE[] m_data;
		DeleteCriticalSection( &m_lock );
	}

	bool MessageCircularBuffer::IsFull()
	{
		EnterCriticalSection( &m_lock );
		bool full =  ( ((m_writeIndex + 1) % m_size) == m_readIndex );
		LeaveCriticalSection( &m_lock );
		return full;
	}


	bool MessageCircularBuffer::IsEmpty()
	{
		EnterCriticalSection( &m_lock );
		bool empty = (m_readIndex == m_writeIndex);
		LeaveCriticalSection( &m_lock );
		return empty;
	}

	NetworkLogMessage* MessageCircularBuffer::Enque()
	{
		EnterCriticalSection( &m_lock );
		NetworkLogMessage* msg = &s_messageBuffer->m_data[ s_messageBuffer->m_writeIndex ];
		s_messageBuffer->m_writeIndex++;
		s_messageBuffer->m_writeIndex %= m_size; // wrap around
		LeaveCriticalSection( &m_lock );
		return msg;
	}

	NetworkLogMessage* MessageCircularBuffer::Deque()
	{
		EnterCriticalSection( &m_lock );
		NetworkLogMessage *msg = &s_messageBuffer->m_data[ m_readIndex ];
		m_readIndex++;;
		m_readIndex %= m_size; // wrap around				
		LeaveCriticalSection( &m_lock );
		return msg;
	}


	bool SetupNetworkLogging(const char* serverName, int port, CCP::LogType threshold, const char* role, char* errorMessageOut, int errorMessageBufferSize)
	{
		if (s_networkLoggingEnabled == true)
		{
			// Already setup, close it and set it up again with the net config
			DisableNetworkLogging();
		}

		// Setup the server address struct
		memset( (char*)&s_networkLoggingServerAddr, 0, sizeof( s_networkLoggingServerAddr ) );
		s_networkLoggingServerAddr.sin_family = AF_INET;

		unsigned long addr = inet_addr( serverName );
		if ( addr == INADDR_NONE || addr == INADDR_ANY )
		{
			// could not convert ip to address
			strcpy_s(errorMessageOut, 256, "Address resolved to INADDR_NONE or INADDR_ANY");
			return false;
		}

		s_networkLoggingServerAddr.sin_addr.s_addr = addr;
		s_networkLoggingServerAddr.sin_port = htons( port );

		// Create the socket
		s_networkLoggingSocket = socket( PF_INET, SOCK_DGRAM, 0 ); 
		if ( s_networkLoggingSocket < 0 )
		{
			int errorCode = WSAGetLastError();
			sprintf_s(errorMessageOut, 256, "Error opening socket. WSA Error Code: %d", errorCode);
			return false;
		}

		// Make the socket nonblocking
		unsigned long noBlock = 1;
		ioctlsocket(s_networkLoggingSocket, FIONBIO, &noBlock);

		size_t tempSize = strlen(role)+1;
		s_role = CCP_NEW("NetworkLogger/role") char[tempSize];
		strcpy_s(s_role, tempSize, role);

		s_hostname = CCP_NEW("NetworkLogger/hostname") char[256];
		memset(s_hostname, NULL, 256);
		gethostname(s_hostname, 256);

		tempSize = strlen(serverName)+1;
		s_networkLoggingServerName = CCP_NEW("NetworkLogger/server") char[tempSize];
		strcpy_s(s_networkLoggingServerName, tempSize, serverName);
		s_networkLoggingPort = port;

		CCP::s_messageBuffer = CCP_NEW("NetworkLogger/MessageCircularBuffer") MessageCircularBuffer( NETWORKMESSAGE_BUFFER_SIZE );

		// Allocate the static working buffers so we're not allocating all the time
		CCP::s_zippedDataBuffer = CCP_NEW("NetworkLogger/zippedBuffer") Bytef[BUFFER_SIZE];
		s_chunk_work_buffer = CCP_NEW("NetworkLogger/Chunk") char[CHUNKING_WORK_BUFFER_SIZE];

		// Let's go !
		s_networkLoggingEnabled = true;

		s_logID = 1;

		// Let's start the worker thread
		if (CCP::s_threadHandle == NULL)
		{
			// Start the network thread
			CCP::s_threadHandle = _beginthread( CCP::NetworkLoggingThreadFunc, 0, 0 );
		}

		// Instruct the CCP logging system to echo to the debugger output window.
		CCP::RegisterLogEcho( &CCP::LogToNetwork, threshold, true, CCP::LOG_ECHO_REQUIRES_PRIVILEGE_CHECK );

		return true;
	}

	void DisableNetworkLogging()
	{
		if (s_networkLoggingEnabled)
		{
			// Let's go !
			s_networkLoggingEnabled = false;

			// Stop sending logging messages to the network handler
			CCP::UnregisterLogEcho( &CCP::LogToNetwork );

			if (s_role != NULL)
			{
				CCP_DELETE[] s_role;
				s_role = NULL;
			}
			if (s_hostname != NULL)
			{
				CCP_DELETE[] s_hostname;
				s_hostname = NULL;
			}
			if (s_networkLoggingServerName != NULL)
			{
				CCP_DELETE[] s_networkLoggingServerName;
				s_networkLoggingEnabled = NULL;
			}


			// close the socket
			closesocket( s_networkLoggingSocket );

			if (s_messageBuffer != NULL)
			{
				CCP_DELETE CCP::s_messageBuffer;
				s_messageBuffer = NULL;
			}

			if (s_chunk_work_buffer != NULL)
			{
				CCP_DELETE CCP::s_chunk_work_buffer;
				s_chunk_work_buffer = NULL;
			}

			if (s_zippedDataBuffer != NULL)
			{
				CCP_DELETE CCP::s_zippedDataBuffer;
				s_zippedDataBuffer = NULL;
			}


			CCP::s_threadHandle = NULL;

			// Wake up the worker thread so it know's to terminate itself
			SetEvent( s_logMessageWritten );

		}
	}

	// utility method to json escape strings
	void escape(std::string *data)
	{
		std::string replaced;
		std::string::iterator it;
		for(it = data->begin(); it < data->end(); it++)
		{
			std::string replacement;
			switch (*it)
			{
			case '"': 
				replacement = "\\\"";
				break;
			case '\\': 
				replacement = "\\\\";
				break;
			case '\b': 
				replacement = "\\b";
				break;
			case '\f': 
				replacement = "\\f";
				break;
			case '\n': 
				replacement = "\\n";
				break;
			case '\r': 
				replacement = "\\r";
				break;
			case '\t': 
				replacement = "\\t";
				break;
			default:
				// Replace all the unprintable characters
				if (*it < 0x20 || *it == 0x7f)
				{
					char buf[7];
					sprintf_s(buf, sizeof(buf), "\\u%04x", *it);
					replacement = std::string(buf);
				}
				break;
			}
			if(replacement.length() > 0)
			    replaced.append(replacement);
			else
			    replaced.push_back(*it);
		}
		// copy the replaced string to the original buffer
		*data = replaced;
	}


	/* ===========================================================================
	Compresses the source buffer into the destination buffer. The level
	parameter has the same meaning as in deflateInit.  sourceLen is the byte
	length of the source buffer. Upon entry, destLen is the total size of the
	destination buffer, which must be at least 0.1% larger than sourceLen plus
	12 bytes. Upon exit, destLen is the actual size of the compressed buffer.

	compress2 returns Z_OK if success, Z_MEM_ERROR if there was not enough
	memory, Z_BUF_ERROR if there was not enough room in the output buffer,
	Z_STREAM_ERROR if the level parameter is invalid.
	*/
	int compress2 (Bytef* dest, uInt* destLen, const Bytef* source, const uLong sourceLen, const int level)
	{
		z_stream stream;
		int err;

		stream.next_in = (Bytef*)source;
		stream.avail_in = (uInt)sourceLen;
		stream.next_out = dest;
		stream.avail_out = *destLen;
		if (stream.avail_out != *destLen)
		{
			return Z_BUF_ERROR;
		}

		stream.zalloc = (alloc_func)0;
		stream.zfree = (free_func)0;
		stream.opaque = (voidpf)0;

		err = deflateInit(&stream, level);
		if (err != Z_OK) return err;

		err = deflate(&stream, Z_FINISH);
		if (err != Z_STREAM_END) {
			deflateEnd(&stream);
			return err == Z_OK ? Z_BUF_ERROR : err;
		}
		*destLen = stream.total_out;

		err = deflateEnd(&stream);

		return err;
	}


	void LogToNetwork( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* message )
	{
		CCP_STATS_ZONE( __FUNCTION__ );
		CCP_STATS_INC(networkLoggingMessages);
		if ( !s_networkLoggingEnabled )
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
		while ( s_messageBuffer->IsFull() )
		{
			CCP_STATS_INC( networkLoggingBufferFull );

			// Yield execution to the thread reading of the buffer
			SwitchToThread();
		}

		// Grab the next free slot
		NetworkLogMessage* msg = s_messageBuffer->Enque();

		// copy into the network message
		strcpy_s( msg->object, 32, logObject.object );
		strcpy_s( msg->facility, 32, logObject.facility );
		strcpy_s( msg->message, MAX_MESSAGE_SIZE, message );

		time_t timestamp;
		time(&timestamp);
		msg->time = timestamp;
		msg->type = (int) type;
		msg->id = s_logID;

		s_logID++;

		// wake up the network thread
		SetEvent( s_logMessageWritten );

	}

	void CCP::NetworkLoggingThreadFunc( void* arg )
	{
		CCP_STATS_ZONE( __FUNCTION__ );
		int ret = 0;
		int waitTime = INFINITE;

		char buffer[CCP::BUFFER_SIZE];

		unsigned long long lc = 0;

		CCP::NetworkLogMessage * msg = NULL;
		do
		{
			// check here in case we just got woken up
			if (!s_networkLoggingEnabled)
			{
				// We're out !
				_endthread();
				return;
			}

			while ( !s_messageBuffer->IsEmpty() )
			{
				msg = s_messageBuffer->Deque();	

				if ( (msg->id-1) != lc )
				{
					CCP_STATS_INC( networkLoggingBufferOutOfOrder );
					//std::cout << "out of order ! missing" << (msg->id-1) - lc << "\n";
				}
				lc = msg->id;

				// Map internal loglevels to syslog levels
				if ( msg->type>3 )
				{
					CCP_STATS_INC(networkLoggingMessagesDropped);
					// unknown loglevel
					continue;
				}
				int logLevel = s_logType2syslog[ msg->type];

				int jsonLenght = 0;

				std::string strMessage;
				strMessage.assign( msg->message,  std::min( strlen(msg->message), MAX_MESSAGE_SIZE) );
				// If the message is this big, we truncate it
				if ( strlen(msg->message) >= MAX_MESSAGE_SIZE )
				{
					strMessage.append( MESSAGE_TRUNCATED_NOTICE );
				}

				escape(&strMessage);

				// create the short version and make sure it does not end in "\" which would mess upp the json encoding
				std::string strShortMessage;
				strShortMessage.assign( msg->message, 256 );
				if (strShortMessage.at( strShortMessage.length()-1 ) == '\\')
				{
					strShortMessage.replace( strShortMessage.length() -1, 1, " ");
				}				
				
				escape(&strShortMessage);
				
				jsonLenght = sprintf_s(
					buffer, 
					BUFFER_SIZE,
					"{  \"version\": \"1.0\", \
						\"facility\": \"%s\", \
						\"_object\": \"%s\", \
						\"_pid\" : %d, \
						\"host\": \"%s\", \
						\"short_message\": \"%s\", \
						\"full_message\": \"%s\", \
						\"timestamp\": %I64u, \
						\"level\": %d, \
						\"role\": \"%s\", \
						\"logID\": %I64u \
						}", 
					msg->facility,
					msg->object,					
					GetCurrentProcessId(),
					s_hostname,
					strShortMessage.c_str(),
					strMessage.c_str(),
					msg->time,
					logLevel,					
					s_role,
					msg->id
					);

				if (jsonLenght == 0)
				{
					// Error sprint_f'ing the json string
					CCP_STATS_INC(networkLoggingMessagesDropped);
					continue;
				}





				// With zlib, you are guaranteed the compressed result will be at most the size of the source plus 12 plus 0.1%
				unsigned int zippedDataBufferSize = ( jsonLenght +( ( (jsonLenght) / 1000 ) + 1 ) + 12 );
				if ( zippedDataBufferSize > BUFFER_SIZE )
				{
					// Lets guard ourself against huge messages
					CCP_STATS_INC( networkLoggingMessagesDropped );
					continue;
				}

				int compressRes = compress2( s_zippedDataBuffer, &zippedDataBufferSize, (Bytef*)&buffer, jsonLenght, Z_DEFAULT_COMPRESSION );
				if ( compressRes != Z_OK )
				{
					CCP_STATS_INC(networkLoggingMessagesDropped);
					continue;
				}

				if ( zippedDataBufferSize <= CCP::MAX_CHUNK_SIZE )
				{
					// We fit in one chunk, just spit it out !
					sendto(
						s_networkLoggingSocket,
						(char*) s_zippedDataBuffer,
						zippedDataBufferSize,
						0,
						(LPSOCKADDR) &s_networkLoggingServerAddr,
						sizeof( sockaddr_in )
						);

					CCP_STATS_INC(networkLoggingPacketsSent);
					CCP_STATS_ADD(networkLoggingBytesSent, zippedDataBufferSize);
				}
				else
				{
					// We need to split stuff into multiple UDP packets with a common header

					// how many chunks do we have
					int numChunks = (zippedDataBufferSize / MAX_CHUNK_SIZE) + 1;

					//  we need a random header for this message
					const static int MESSAGE_ID_SIZE = 32;
					char messageID[MESSAGE_ID_SIZE];
					for (int i = 0; i<MESSAGE_ID_SIZE; i++)
					{
						messageID[i] = rand();
					}

					// some counters used for splitting
					const static int CHUNK_HEADER_SIZE = 2 + MESSAGE_ID_SIZE + 4;
					unsigned int startingAt = 0;
					unsigned int bufferIndex = 0;
					unsigned int until = 0;
					unsigned int sliceSize = 0;
					unsigned int chunkSize = 0;

					for ( char chunkID = 0; chunkID<numChunks; chunkID++ )
					{
						startingAt = chunkID * MAX_CHUNK_SIZE;
						until = (chunkID + 1) * MAX_CHUNK_SIZE;
						until = std::min(until, zippedDataBufferSize);
						sliceSize = until - startingAt;
						chunkSize = sliceSize + CHUNK_HEADER_SIZE;
						memset( s_chunk_work_buffer, NULL, CHUNKING_WORK_BUFFER_SIZE );

						// Set the header on the chunked packet

						// 0x1e 0x0f is the header of a chunked GELF message 
						s_chunk_work_buffer[0] = 0x1e;
						s_chunk_work_buffer[1] = 0x0f;
						memcpy(s_chunk_work_buffer+2, messageID, MESSAGE_ID_SIZE);
						s_chunk_work_buffer[MESSAGE_ID_SIZE+3] = chunkID;
						s_chunk_work_buffer[MESSAGE_ID_SIZE+5] = numChunks;

						// Cope the slice of the data into the packet
						memcpy( 
							s_chunk_work_buffer + CHUNK_HEADER_SIZE,			// data starts at byte 12 because of the header
							s_zippedDataBuffer + startingAt,		// we start copying from here
							sliceSize									// and copy this much
							);

						// of it goes
						sendto(
							s_networkLoggingSocket,
							s_chunk_work_buffer,
							chunkSize,
							0,
							(LPSOCKADDR) &s_networkLoggingServerAddr,
							sizeof( sockaddr_in )
							);

						CCP_STATS_INC(networkLoggingPacketsSent);
						CCP_STATS_ADD(networkLoggingBytesSent, chunkSize);

						// need to know where to start for the next message
						bufferIndex += chunkSize;
					}
				}
			}


			if (!s_networkLoggingEnabled)
			{
				// We're out !
				_endthread();
				return;
			}


		} while ( ((ret = WaitForSingleObject( CCP::s_logMessageWritten , waitTime)) != WAIT_FAILED) && s_networkLoggingEnabled);
	}

}
/* Python functions to enable and disable this */



PyObject* PyEnableNetworkLogging( PyObject* self, PyObject* args )
{
	char *serverName;
	char *role;
	int serverPort;
	int threshold = 0;

	if( PyArg_ParseTuple( args, "sis|i", &serverName, &serverPort, &role, &threshold ) )
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
		memset(errorMessage, NULL, errorMessageBufferSize);

		bool ret = CCP::SetupNetworkLogging(  serverName, serverPort, (CCP::LogType) threshold, role, errorMessage, errorMessageBufferSize );
		if (!ret)
		{
			return PyErr_SetString(PyExc_RuntimeError, errorMessage), 0;
		}
		Py_RETURN_TRUE;

	}

	Py_RETURN_FALSE;
}

MAP_FUNCTION( 
	"EnableNetworkLogging", 
	PyEnableNetworkLogging, 
	"Enables echoing of log to the network\n"
	":param serverName: server name\n"
	":type serverName: str\n"
	":param port: server port number\n"
	":type port: int\n"
	":param role: role name\n"
	":type role: str\n"
	":param threshold: minimal severity level of the message to be logged\n"
	":type threshold: int\n"
	":rtype: bool"
	);


void DisableNetworkLogging()
{
	CCP::DisableNetworkLogging( );
}

MAP_FUNCTION_AND_WRAP( "DisableNetworkLogging", DisableNetworkLogging, "Disable network logging" );

PyObject* PyGetNetworkLoggingState( PyObject* self, PyObject* args )
{
	PyObject *t;

	if ( CCP::s_networkLoggingEnabled )
	{
		t = PyTuple_New(3);
		PyTuple_SetItem(t, 0, PyUnicode_FromString( CCP::s_networkLoggingServerName  ));
		PyTuple_SetItem(t, 1, PyLong_FromLong( CCP::s_networkLoggingPort ));

		int isFull = 0;
		if (CCP::s_messageBuffer->IsFull())
		{
			isFull = 1;
		}
		PyTuple_SetItem(t, 2, PyBool_FromLong( isFull ));

		return t;
	}
	Py_RETURN_FALSE;

}

MAP_FUNCTION( 
	"GetNetworkLoggingState", 
	PyGetNetworkLoggingState, 
	"Tell you where the server is logging to\n"
	":rtype: bool | (str, long, bool)" );

#ifdef OptimizeOff
#pragma optimize("", on)
#endif

#endif
#endif
