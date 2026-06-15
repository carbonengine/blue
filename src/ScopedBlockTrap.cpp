// Copyright © 2013 CCP ehf.

#include "StdAfx.h"
#include "ScopedBlockTrap.h"
#include <Scheduler.h>

ScopedBlockTrap::ScopedBlockTrap()
{
#if CCP_STACKLESS
	m_tasklet = reinterpret_cast<PyTaskletObject*>( SchedulerAPI()->PyScheduler_GetCurrent() );
	if( m_tasklet ) 
	{
		m_originalBlocktrapState = SchedulerAPI()->PyTasklet_GetBlockTrap( m_tasklet );
		SchedulerAPI()->PyTasklet_SetBlockTrap( m_tasklet, 1 );
	}
#endif
}

ScopedBlockTrap::~ScopedBlockTrap()
{
#if CCP_STACKLESS
	//restore block trap
	if( m_tasklet )
	{
		SchedulerAPI()->PyTasklet_SetBlockTrap( m_tasklet, m_originalBlocktrapState );
		Py_DECREF( m_tasklet );
	}
#endif
}
