////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		April 2013
// Copyright:	CCP 2013
//

#include "StdAfx.h"
#include "include/ScopedBlockTrap.h"

#if CCP_STACKLESS
#include "stackless_api.h"
#endif

ScopedBlockTrap::ScopedBlockTrap()
{
#if CCP_STACKLESS
	m_tasklet = reinterpret_cast<PyTaskletObject*>( PyStackless_GetCurrent() );
	if( m_tasklet ) 
	{
		m_originalBlocktrapState = PyTasklet_GetBlockTrap( m_tasklet );
		PyTasklet_SetBlockTrap( m_tasklet, 1 );
	}
#endif
}

ScopedBlockTrap::~ScopedBlockTrap()
{
#if CCP_STACKLESS
	//restore block trap
	if( m_tasklet )
	{
		PyTasklet_SetBlockTrap( m_tasklet, m_originalBlocktrapState );
		Py_DECREF( m_tasklet );
	}
#endif
}
