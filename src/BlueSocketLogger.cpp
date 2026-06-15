// Copyright © 2015 CCP ehf.

#include "StdAfx.h"
#include "BlueSocketLogger.h"
#ifndef _WIN32
#include <sys/time.h>
#else
#include <WS2tcpip.h>
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach-o/dyld.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace
{
#ifdef _WIN32
typedef SOCKET Socket;
const Socket InvalidSocket = INVALID_SOCKET;
const int ShutdownAll = SD_BOTH;
#else
typedef int Socket;
const Socket InvalidSocket = -1;
const int ShutdownAll = SHUT_RDWR;
#define closesocket close
#endif

const uint32_t CURRENT_VERSION = 2;
    
enum MessageType
{
	// The first message sent from the client (see ConnectionMessage)
	CONNECTION,
	// "Normal" message with short text
	MESSAGE,
	// Start of the message with large text: consequent CONTINUATION messages contain other portion of text
	LARGE_MESSAGE,
	// Middle of the long message
	CONTINUATION,
	// Last part of the long message
	CONTINUATION_END,
};

struct ConnectionMessage
{
	static const size_t MESSAGE_MAX_PATH = 260;

	// Our API version
	uint32_t version;
    uint64_t pid;
    char machineName[32];
	char executablePath[MESSAGE_MAX_PATH];
};

struct TextMessage
{
	static const size_t TEXT_SIZE = 256;

	// Unix timestamp
	uint64_t timestamp;
    uint32_t severity;
    char module[32];
    char channel[32];
    char message[TEXT_SIZE];
};

struct Message
{
	uint32_t type;
	union
	{
		ConnectionMessage connection;
		TextMessage text;
	};
};

class Logger
{
public:
	Logger( const char* ip, int port )
		:m_thread(),
		m_stop( 0 ),
		m_messages( "BlueSocketLogger::m_messages" ),
		m_queueMutex( "BlueSocketLogger", "m_queueMutex" ),
		m_availableMessages( "BlueSocketLogger::m_availableMessages" ),
		m_availableMutex( "BlueSocketLogger", "m_availableMutex" )
	{
		m_machineName[0] = 0;
		m_executablePath[0] = 0;
		GatherProcessInfo();
		if( SetupSocket( ip, port ) )
		{
			m_thread = CcpCreateThread( &ThreadProc, this, CCP_THREAD_PRIORITY_NORMAL );
		}
	}

	~Logger()
	{
		if( m_socket != InvalidSocket )
		{
			shutdown( m_socket, ShutdownAll );
		}
		if( m_thread )
		{
			m_stop = 1;
			m_queueSignal.Signal();
			uint32_t result;
			CcpJoinThread( m_thread, result );
		}
		if( m_socket != InvalidSocket )
		{
			closesocket( m_socket );
		}
		for( auto it = m_availableMessages.begin(); it != m_availableMessages.end(); ++it )
		{
			CCP_DELETE *it;
		}
		for( auto it = m_messages.begin(); it != m_messages.end(); ++it )
		{
			CCP_DELETE *it;
		}
	}

	bool IsConnected() const
	{
		return m_thread != CcpThreadHandle_t() && m_stop == 0;
	}

	Message* CreateMessage()
	{
		CcpAutoMutex lock( m_availableMutex );
		if( !m_availableMessages.empty() )
		{
			auto message = m_availableMessages.back();
			m_availableMessages.pop_back();
			return message;
		}
		return CCP_NEW( "BlueSocketLogger::Message" ) Message;
	}

	void PushMessage( Message* message )
	{
		CcpAutoMutex lock( m_queueMutex );
		m_messages.push_back( message );
		m_queueSignal.Signal();
	}

	void Flush()
	{
		while( true )
		{
			if( !IsConnected() )
			{
				break;
			}
			{
				CcpAutoMutex lock( m_queueMutex );
				if( m_messages.empty() )
				{
					break;
				}
			}
			m_queueProcessedSignal.Wait();
		}
	}
private:
	void GatherProcessInfo()
	{
		m_pid = CcpGetCurrentProcessId();
#ifdef _WIN32
		DWORD tmp = sizeof( m_machineName );
		GetComputerNameA( m_machineName, &tmp );
		GetModuleFileNameA( nullptr, m_executablePath, sizeof( m_executablePath ) );
#elif defined( __APPLE__ )
		char buffer[1024] = { 0 };
		size_t size = sizeof( buffer );
		sysctlbyname( "kern.hostname", buffer, &size, nullptr, 0 );
		strcpy_s( m_machineName, _TRUNCATE, buffer );
		uint32_t pathSize = sizeof( buffer );
		if( _NSGetExecutablePath( buffer, &pathSize ) == 0 )
		{
			char* path = realpath( buffer, nullptr );
			if( path )
			{
				strcpy_s( m_executablePath, _TRUNCATE, path );
				free( path );
			}
		}
#endif
	}

	bool SetupSocket( const char* ip, int port )
	{
#ifdef _WIN32
		WSADATA wsaData;
		WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
#endif
		m_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
		if( m_socket == InvalidSocket )
		{
			return false;
		}
		struct sockaddr_in clientService = {0};
		clientService.sin_family = AF_INET;
		if ( inet_pton( clientService.sin_family, ip, &(clientService.sin_addr.s_addr) ) != 1 )
		{
			m_socket = InvalidSocket;
			return false;
		}
		clientService.sin_port = htons( port );
#ifdef _WIN32
		unsigned long nonBlocking = 1;
		ioctlsocket( m_socket, FIONBIO, &nonBlocking );

		if( connect( m_socket, reinterpret_cast<sockaddr*>( &clientService ), sizeof( clientService ) ) != 0 )
		{
			if( WSAGetLastError() == WSAEWOULDBLOCK )
			{
				fd_set writeFd;
				FD_ZERO( &writeFd );
				FD_SET( m_socket, &writeFd );
				timeval timeout;
				timeout.tv_sec = 1;
				timeout.tv_usec = 0;
				if( select( 0, nullptr, &writeFd, nullptr, &timeout ) )
				{
					int result;
					int resultLen = sizeof( result );
					if ( getsockopt( m_socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>( &result ), &resultLen ) == 0 && 
						result == 0 ) 
					{
						unsigned long blocking = 0;
						ioctlsocket( m_socket, FIONBIO, &blocking );
						return true;
					}
				}
			}
			closesocket( m_socket );
			m_socket = InvalidSocket;
			return false;
		}

		unsigned long blocking = 0;
		ioctlsocket( m_socket, FIONBIO, &blocking );
#else
		if( connect( m_socket, reinterpret_cast<sockaddr*>( &clientService ), sizeof( clientService ) ) != 0 )
		{
			closesocket( m_socket );
			m_socket = InvalidSocket;
			return false;
		}
#endif
		return true;
	}

	Message* PopMessage()
	{
		CcpAutoMutex lock( m_queueMutex );
		if( m_messages.empty() )
		{
			return nullptr;
		}
		auto message = m_messages.front();
		m_messages.pop_front();
		return message;
	}

	bool SendMessage( const Message* message )
	{
		auto buffer = reinterpret_cast<const char*>( message );
		auto size = sizeof( *message );
		while( size )
		{
			auto result = send( m_socket, buffer, int( size ), 0 );
			if( result == -1 )
			{
				return false;
			}
			size -= size_t( result );
			buffer += result;
		}
		return true;
	}

	void RecycleMessage( Message* message )
	{
		{
			CcpAutoMutex lock( m_availableMutex );
			if( m_availableMessages.size() < 100 )
			{
				m_availableMessages.push_back( message );
				return;
			}
		}
		CCP_DELETE message;
	}

	void RunClient()
	{
		Message first;
		first.type = CONNECTION;
		first.connection.version = CURRENT_VERSION;
		first.connection.pid = m_pid;
		strcpy_s( first.connection.machineName, m_machineName );
		strcpy_s( first.connection.executablePath, m_executablePath );
		if( !SendMessage( &first ) )
		{
			m_stop = 1;
		}

		while( m_stop == 0 )
		{
			m_queueSignal.Wait();
			while( auto message = PopMessage() )
			{
				if( m_stop != 0 )
				{
					break;
				}
				if( !SendMessage( message ) )
				{
					m_stop = 1;
				}
				RecycleMessage( message );
			}
			m_queueProcessedSignal.Signal();
		}
	}

	static uint32_t ThreadProc( void* context )
	{
		static_cast<Logger*>( context )->RunClient();
		return 0;
	}

	CcpAtomic<uint32_t> m_stop;
	TrackableStdDeque<Message*> m_messages;
	CcpMutex m_queueMutex;
	CcpSemaphore m_queueSignal;
	CcpSemaphore m_queueProcessedSignal;
	Socket m_socket;
	CcpThreadHandle_t m_thread;
	TrackableStdVector<Message*> m_availableMessages;
	CcpMutex m_availableMutex;
	uint64_t m_pid;
	char m_machineName[32];
	char m_executablePath[ConnectionMessage::MESSAGE_MAX_PATH];
};

Logger* s_logger = nullptr;

}

bool IsSocketLoggerConnected()
{
	return s_logger && s_logger->IsConnected();
}


bool StartSocketLogger( const char* ip, int port )
{
	if( s_logger )
	{
		if( s_logger->IsConnected() )
		{
			return false;
		}
		StopSocketLogger();
	}
	s_logger = CCP_NEW( "BlueSocketLogger" ) Logger( ip, port );
	if( !s_logger->IsConnected() )
	{
		CCP_DELETE s_logger;
		s_logger = nullptr;
		return false;
	}
	return true;
}

void StopSocketLogger()
{
	CCP_DELETE s_logger;
	s_logger = nullptr;
}

void FlushSocketLogger()
{
	if( s_logger )
	{
		s_logger->Flush();
	}
}

void LogToSocketLogger( CcpLogChannel_t& logObject, CCP::LogType type, unsigned long userData, const char* message )
{
	if( !s_logger || !s_logger->IsConnected() )
	{
		return;
	}

	uint64_t t = ( TimeNow() - 116444736000000000LL ) / 10000;

	auto length = strlen( message );
	for( size_t i = 0; i < length; i += TextMessage::TEXT_SIZE - 1 )
	{
		auto msg = s_logger->CreateMessage();

		msg->text.timestamp = t;
		if( i )
		{
			msg->type = i + TextMessage::TEXT_SIZE - 1 < length ? CONTINUATION : CONTINUATION_END;
		}
		else
		{
			msg->type = length > TextMessage::TEXT_SIZE - 1 ? LARGE_MESSAGE : MESSAGE;
		}
		msg->text.severity = type;
		strncpy_s( msg->text.module, logObject.facility, sizeof( msg->text.module ) - 1 );
		strncpy_s( msg->text.channel, logObject.object, sizeof( msg->text.module ) - 1 );
		auto chunk = std::min( TextMessage::TEXT_SIZE - 1, length - i );
		memcpy( msg->text.message, message + i, chunk );
		msg->text.message[chunk] = 0;
		s_logger->PushMessage( msg );
	}
}
