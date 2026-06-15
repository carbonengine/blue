// Copyright © 2014 CCP ehf.

#pragma once
#ifndef IBlueThreadMonitor_h
#define IBlueThreadMonitor_h

BLUE_INTERFACE( IBlueThreadMonitor ) : public IRoot
{
	enum Status
	{
		BTS_IDLE,
		BTS_WORKING,
		BTS_LOADING,
		BTS_DOWNLOADING,
	};

	// Set current status, returning previous status
	virtual Status SetThreadStatus( Status status ) = 0;
};

extern IBlueThreadMonitor* BeThreadMonitor;

class ScopedThreadStatus
{
public:
	ScopedThreadStatus( IBlueThreadMonitor::Status status )
	{
		m_prevStatus = BeThreadMonitor->SetThreadStatus( status );
	}

	~ScopedThreadStatus()
	{
		BeThreadMonitor->SetThreadStatus( m_prevStatus );
	}

private:
	IBlueThreadMonitor::Status m_prevStatus;
};

#endif // IBlueThreadMonitor_h