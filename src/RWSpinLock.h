// Copyright © 2011 CCP ehf.

#pragma once
#ifndef RWSPINLOCK_HPP
#define RWSPINLOCK_HPP

#include <CcpMutex.h>

namespace Ccp
{

//#define RW_TRACK_HISTORY
const long c_historyDepth = 256; // only valid if TRACK_HISTORY is defined
//------------------------------------------------------------------------------
// very fast read/write lock. There is a bias toward write-locks, this
// class spin-locks to acquire, NOT an OS event, so write lock is meant
// to be held for a very short time
class RWSpinlock
{
public:
	RWSpinlock(const char* owner, const char* name) : m_waiterCount(0), m_accessCount(0), m_lock(owner, name)
	{
#ifdef RW_TRACK_HISTORY
		m_writeLockOwner = 0;
		memset( (char *)m_readLockHistory, 0, c_historyDepth * sizeof(long) );
#endif
	}

#ifdef RW_TRACK_HISTORY
	volatile long m_readLockHistory[c_historyDepth];
	long m_writeLockOwner;
#endif

	int readLock()
	{
		int spins = 0;
        m_lock.Acquire();

		while( m_waiterCount || (m_accessCount < 0) )
		{
			m_lock.Release();

			while( m_waiterCount || (m_accessCount < 0) )
			{
				spins++;
                CcpThreadYield();
			}

			m_lock.Acquire();
		}

		m_accessCount++;

        m_lock.Release();

#ifdef RW_TRACK_HISTORY
		long id = GetCurrentThreadId();
		for( long i=0; i<c_historyDepth; i++ )
		{
			if ( !InterlockedCompareExchange(m_readLockHistory + i, id, 0) )
			{
				break;
			}
		}
#endif

		return spins;
	}

	int writeLock()
	{
		int spins = 0;
		m_lock.Acquire();

		m_waiterCount++;

		while( m_accessCount )
		{
            m_lock.Release();

			while( m_accessCount )
			{
				spins++;
                CcpThreadYield();
			}

            m_lock.Acquire();
		}

		m_waiterCount--;
		m_accessCount--;

        m_lock.Release();

#ifdef RW_TRACK_HISTORY
		memset( (char *)m_readLockHistory, 0, c_historyDepth * sizeof(long) );
		m_writeLockOwner = CcpGetCurrentThreadId();
#endif

		return spins;
	}

	void unlock()
	{
        m_lock.Acquire();
		if ( m_accessCount > 0 )
		{
#ifdef RW_TRACK_HISTORY
			long id = CcpGetCurrentThreadId();
			for( long i=0; i<c_historyDepth ; i++ )
			{
				if (m_readLockHistory[i] == id )
				{
					m_readLockHistory[i] = 0;
				}
			}
#endif
			m_accessCount--;
		}
		else
		{
#ifdef RW_TRACK_HISTORY
			m_writeLockOwner = 0;
#endif
			m_accessCount++;
		}
        m_lock.Release();
	}

private:

	volatile long m_waiterCount;
	volatile long m_accessCount;
	CcpMutex m_lock;
};

}

#endif
