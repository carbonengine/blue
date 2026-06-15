// Copyright © 2011 CCP ehf.

#pragma once
#ifndef SCOPEDLOCKS_HPP
#define SCOPEDLOCKS_HPP

#include "RWSpinLock.h"

namespace Ccp
{

//------------------------------------------------------------------------------
// Helper class for the above ReadWrite lock
class RWSpinlockReadScoped
{
public:
	RWSpinlockReadScoped( RWSpinlock& lock ) { m_lock = &lock; lock.readLock(); m_held = true;}
	~RWSpinlockReadScoped() { release(); }
	void release() { if ( m_held ) { m_lock->unlock(); m_held = false; } }
private:
	RWSpinlock *m_lock;
	bool m_held;
};

//------------------------------------------------------------------------------
class RWSpinlockWriteScoped
{
public:
	RWSpinlockWriteScoped( RWSpinlock& lock ) { m_lock = &lock; lock.writeLock(); m_held = true; }
	~RWSpinlockWriteScoped() { release(); }
	void release() { if ( m_held ) { m_lock->unlock(); m_held = false; } }
private:
	RWSpinlock *m_lock;
	bool m_held;
};

#if _WIN32
//------------------------------------------------------------------------------
// Helper class for critical sections
class __declspec( dllexport ) CriticalLockScoped
{
public:
	CriticalLockScoped( CRITICAL_SECTION& lock )
	{
		m_lock = &lock;
		EnterCriticalSection( m_lock );
		m_held = true;
	}
	~CriticalLockScoped()
	{
		release();
	}
	void release()
	{
		if( m_held )
		{
			LeaveCriticalSection( m_lock );
			m_held = false;
		}
	}

private:
	CRITICAL_SECTION* m_lock;
	bool m_held;
};
#endif

}

#endif
