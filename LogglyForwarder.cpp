////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		May 2015
// Copyright:	CCP 2015
//

#include "StdAfx.h"
#include "LogglyForwarder.h"
#include "include/IBluePaths.h"

static CcpThread* s_sendThread = nullptr;

LogglyForwarder::LogglyForwarder() : 
	m_queueMutex("LogglyForwarder", "m_queueMutex"),
	m_queue("LogglyForwarder/m_queue")
{
}

LogglyForwarder* LogglyForwarder::GetInstance()
{
	static LogglyForwarder s_instance;
	return &s_instance;
}

// This is registered as log echo function - it simply forwards to the Log member function of the LogglyForwarder
void LogglyForwarder::Log_s(CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message)
{
	GetInstance()->Log(channel, type, userData, message);
}

// The read callback for curl - this is what curl reads to send to the destination
size_t LogglyForwarder::read_callback(void *ptr, size_t size, size_t nmemb, void *context)
{
	if (size*nmemb < 1)
	{
		return 0;
	}

	WritePackage *wp = static_cast<WritePackage *>(context);

	if (wp->curData < wp->data + wp->dataSize)
	{
		size_t toCopy = size * nmemb;
		size_t sizeLeft = wp->data + wp->dataSize - wp->curData;
		if (toCopy > sizeLeft)
		{
			toCopy = sizeLeft;
		}
		memcpy(ptr, wp->curData, toCopy);
		wp->curData += toCopy;
		return toCopy;
	}

	CCP_DELETE wp;
	return 0;
}

// The write callback for curl - for writing the response from the server
size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	// We don't care about the results sent from Loggly
	return size * nmemb;
}

// The log echo function
void LogglyForwarder::Log(CcpLogChannel_t& channel, CCP::LogType type, unsigned long userData, const char* message)
{
	static const char* s_logType[] = { "info", "notice", "warning", "error" };

	char timestamp[64];

#ifdef WIN32
	SYSTEMTIME systemtime;
	GetSystemTime(&systemtime);

	sprintf_s(timestamp, "[%04d.%02d.%02d %02d:%02d:%02d.%03dZ]",
		systemtime.wYear,
		systemtime.wMonth,
		systemtime.wDay,
		systemtime.wHour,
		systemtime.wMinute,
		systemtime.wSecond,
		systemtime.wMilliseconds
		);
#else
	// TODO: Get timestamp on other platforms
	strcpy(timestamp, "Missing timestamp");
#endif

	char sanitizedMessage[1536];

	int writeIndex = 0;
	for( int i = 0; writeIndex < sizeof( sanitizedMessage ) - 1; ++i )
	{
		char c = message[i];
		if( c == 0 )
		{
			break;
		}
		switch( c ) {
		case '"':
			sanitizedMessage[writeIndex++] = '\\';
			sanitizedMessage[writeIndex++] = '"';
			break;
		case '\n':
			sanitizedMessage[writeIndex++] = '\\';
			sanitizedMessage[writeIndex++] = 'n';
			break;

		default:
			sanitizedMessage[writeIndex++] = c;
		}
	}
	sanitizedMessage[writeIndex] = 0;

	const char* formatString = "{\"timestamp\": \"%s\", \"sessionid\": \"%s\", \"type\": \"%s\", \"module\": \"%s\", \"channel\": \"%s\", \"message\": \"%s\"}";

	char jsonMsg[2048];
	sprintf_s(jsonMsg, formatString, timestamp, m_sessionId.c_str(), s_logType[type], channel.facility, channel.object, sanitizedMessage);

	LogJson(jsonMsg);
}

// Registers a log echo function and starts sending log messages at the given threshold to Loggly
// at the given url. The messages are sent as a json package, with one field being the given session id.
void LogglyForwarder::Start( CCP::LogType threshold, const std::string& url, const std::string& sessionId )
{
	m_sessionId = sessionId;
	if (!m_isActive)
	{
		if (!s_sendThread)
		{
			s_sendThread = CCP_NEW("LogglyForwarder/s_sendThread") CcpThread(&LogglyForwarder::SendThreadFunc, url);
		}
		CCP::RegisterLogEcho(Log_s, threshold, true);
		m_isActive = true;
	}
}

// Stops forwarding log messages to Loggly
void LogglyForwarder::Stop()
{
	CCP::UnregisterLogEcho(Log_s);
	m_isActive = false;
}

void LogglyForwarder::LogJson(char * jsonMsg)
{
	WritePackage* wp = CCP_NEW("WritePackage") WritePackage(jsonMsg);

	CcpAutoMutex locker(m_queueMutex);
	m_queue.push_back(wp);
	m_queueSignal.Signal();
}

LogglyForwarder::WritePackage* LogglyForwarder::PopQueue()
{
	m_queueSignal.Wait();

	CcpAutoMutex locker(m_queueMutex);
	if( !m_queue.empty() )
	{
		auto item = m_queue.front();
		m_queue.pop_front();
		return item;
	}
	return nullptr;
}

void LogglyForwarder::SendThreadFunc( std::string url )
{
	CURL* curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	std::wstring cert_str = BePaths->ResolvePathW(L"bin://cacert.pem");
	CW2A cert_path(cert_str.c_str());
	curl_easy_setopt(curl, CURLOPT_CAINFO, static_cast<const char*>(cert_path));

	auto headerlist = curl_slist_append(nullptr, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

	while (true)
	{
		auto wp = GetInstance()->PopQueue();
		if( wp )
		{
			curl_easy_setopt(curl, CURLOPT_READDATA, wp);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, wp->dataSize);
			CURLcode res = curl_easy_perform(curl);
		}
	}
}

void EnableLogglyLogging( int threshold, const std::string& url, const std::string& sessionId )
{
	if (threshold < CCP::LOGTYPE_LOWEST)
	{
		threshold = CCP::LOGTYPE_LOWEST;
	}
	if (threshold > CCP::LOGTYPE_HIGHEST)
	{
		threshold = CCP::LOGTYPE_HIGHEST;
	}
	LogglyForwarder::GetInstance()->Start((CCP::LogType)threshold, url, sessionId);
}

void DisableLogglyLogging()
{
	LogglyForwarder::GetInstance()->Stop();
}

bool IsLogglyEnabled()
{
	return LogglyForwarder::GetInstance()->IsActive();
}

MAP_FUNCTION_AND_WRAP( "EnableLogglyLogging", EnableLogglyLogging, "Enable Loggly logging" );
MAP_FUNCTION_AND_WRAP("DisableLogglyLogging", DisableLogglyLogging, "Disable Loggly logging");
MAP_FUNCTION_AND_WRAP( "IsLogglyEnabled", IsLogglyEnabled, "Returns true if Loggly logging is enabled" );
