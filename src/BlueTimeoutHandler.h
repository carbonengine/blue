// Copyright © 2014 CCP ehf.

#pragma once
#ifndef BlueTimeoutHandler_h
#define BlueTimeoutHandler_h

#include "WatchdogThread.h"

class BlueTimeoutHandler : public WatchdogThread::ITimeoutHandler
{
public:
	BlueTimeoutHandler();

	virtual void NotifyOfTimeout();

	void Reset();

private:
	bool m_hasReportedTimeout;

#if BLUE_WITH_PYTHON
	static uint32_t PythonDiagnosticFunction( void* context );
#endif
};

#endif // BlueTimeoutHandler_h
