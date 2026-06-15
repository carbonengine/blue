// Copyright © 2013 CCP ehf.

#pragma once
#ifndef WatchdogThread_h
#define WatchdogThread_h

class WatchdogThread
{
public:
	struct ITimeoutHandler
	{
		virtual void NotifyOfTimeout() = 0;
	};

	WatchdogThread( const char* name );

	void Start( uint32_t timeoutInMs, ITimeoutHandler* timeoutHandler );
	void Stop();
	void Tick();

private:
	static uint32_t ThreadFunc( void* context );

	std::string m_name;
	uint32_t m_timeout;
	CcpAtomic<uint32_t> m_counter;
	CcpAtomic<uint32_t> m_isRunning;
	ITimeoutHandler* m_timeoutHandler;
	CcpThreadHandle_t m_thread;
};

#endif // WatchdogThread_h