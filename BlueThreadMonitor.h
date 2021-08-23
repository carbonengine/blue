////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		December 2014
// Copyright:	CCP 2014
//

#pragma once
#ifndef BlueThreadMonitor_h
#define BlueThreadMonitor_h

#include "Include/IBlueThreadMonitor.h"

BLUE_CLASS( BlueThreadMonitor ) : public IBlueThreadMonitor
{
public:

	EXPOSE_TO_BLUE();

	BlueThreadMonitor( IRoot* lockobj = nullptr );

	//////////////////////////////////////////////////////////////////////////
	// IBlueThreadMonitor
	//
	Status SetThreadStatus( Status status ) override;

	//
	//////////////////////////////////////////////////////////////////////////

	std::vector<std::pair<uint32_t, IBlueThreadMonitor::Status>> GetStatus();

private:
	CcpMutex m_mapMutex;
	std::map<CcpThreadId_t, IBlueThreadMonitor::Status> m_status;
};

TYPEDEF_BLUECLASS( BlueThreadMonitor );

#endif // BlueThreadMonitor_h