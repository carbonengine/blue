////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		May 2015
// Copyright:	CCP 2015
//

#pragma once
#ifndef LogglyForwarder_h
#define LogglyForwarder_h

#include "curl\curl.h"

BLUE_DECLARE(LogglyForwarder);

class LogglyForwarder
{
public:
	// Constructor is protected to prevent multiple instances.
	// Use GetInstance instead.

	static LogglyForwarder* GetInstance();

	void Start(CCP::LogType threshold, const char* url, const char* sessionId);
	void Stop();

	void LogJson(char * jsonMsg);

protected:
	LogglyForwarder();

	// The log echo function that will be registered with the log system
	static void Log_s(CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message);
	static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *context);
	static void SendThreadFunc(const char* url);

	void Log(CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message);

	bool m_isActive;
	CURL* m_curl;
	std::string m_sessionId;

	struct WritePackage
	{
		WritePackage(const char* d)
		{
			data = CCP_STRDUP("WritePackage/data", d);
			dataSize = strlen(data);
			curData = data;
		}

		~WritePackage()
		{
			CCP_FREE(data);
		}

		char* data;
		size_t dataSize;
		const char* curData;
	};

	TrackableStdDeque<WritePackage*> m_queue;
	CcpMutex m_queueMutex;
	CcpSemaphore m_queueSignal;

	WritePackage* PopQueue();
};

#endif // BlueLogInMemory_h