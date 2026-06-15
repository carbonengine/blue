// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "BlueThreadMonitor.h"

BlueThreadMonitor::BlueThreadMonitor( IRoot* lockobj /*= nullptr */ ) :
	m_mapMutex( "BlueThreadMonitor", "mapMutex")
{

}

IBlueThreadMonitor::Status BlueThreadMonitor::SetThreadStatus( Status status )
{
	CcpAutoMutex lock( m_mapMutex );

	IBlueThreadMonitor::Status prevStatus = IBlueThreadMonitor::BTS_IDLE;

	auto foundIt = m_status.find( CcpGetCurrentThreadId() );
	if( foundIt != m_status.end() )
	{
		prevStatus = foundIt->second;
	}

	m_status[CcpGetCurrentThreadId()] = status;

	return prevStatus;
}

std::vector<std::pair<uint32_t, IBlueThreadMonitor::Status>> BlueThreadMonitor::GetStatus()
{
	CcpAutoMutex lock( m_mapMutex );

	std::vector<std::pair<uint32_t, IBlueThreadMonitor::Status>> result;
	result.reserve( m_status.size() );

	for( auto it = m_status.begin(); it != m_status.end(); ++it )
	{
		result.push_back( std::make_pair( uint32_t( uintptr_t( it->first ) ), it->second ) );
	}

	return result;
}
