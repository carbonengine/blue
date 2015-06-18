////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		May 2015
// Copyright:	CCP 2015
//

#pragma once
#ifndef LogglyForwarder_h
#define LogglyForwarder_h

#include "curl/curl.h"

BLUE_DECLARE(LogglyForwarder);

class LogglyForwarder
{
public:
	// Constructor is protected to prevent multiple instances.
	// Use GetInstance instead.

	static LogglyForwarder* GetInstance();

	void Start( CCP::LogType threshold, const std::string& url, const std::string& sessionId );
	void Stop();

	bool IsActive() { return m_isActive; }

	void LogJson(char * jsonMsg);

protected:
	LogglyForwarder();

	// The log echo function that will be registered with the log system
	static void Log_s(CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message);
	static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *context);
	static void SendThreadFunc( std::string url );

	void Log(CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message);

	bool m_isActive;
	CURL* m_curl;

	struct LogPackage
	{
		LogPackage(CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message) : m_channel(channel), m_type(type)
		{
#ifdef WIN32
			GetSystemTime( &m_systemtime );
#endif
			m_message = CCP_STRDUP( "LogPackage/m_message", message );
		}
		
		~LogPackage()
		{
			CCP_FREE( reinterpret_cast<void*>(m_message) );
		}

#ifdef WIN32
		SYSTEMTIME m_systemtime;
#endif
		CcpLogChannel_t m_channel;
		CCP::LogType m_type;
		char* m_message;
	};

	struct WritePackage
	{
		WritePackage(LogPackage* lp);

		~WritePackage()
		{
			CCP_FREE(m_data);
		}

		char* m_data;
		size_t m_dataSize;
		const char* m_curData;
	};

	TrackableStdDeque<LogPackage*> m_queue;
	CcpMutex m_queueMutex;
	CcpSemaphore m_queueSignal;

	LogPackage* PopQueue();
};

#endif // BlueLogInMemory_h