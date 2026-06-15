// Copyright © 2004 CCP ehf.

/* 
	*************************************************************************

	BlueTime.cpp

	Project:   Blue

	Description:   

	  Implementation of the core time related classes in blue.

	  Provides the classes SysTickTime, HiResTime and SuperTime.

	Dependencies:

	  Blue

	*************************************************************************
*/

#include "StdAfx.h"

#include "BlueTime.h"

//-----------------------------------------------------------------------------
// Uncomment this define to turn this file's optimization off
//#define OptimizeOff
#ifdef OptimizeOff
#pragma warning (disable:4748)
#pragma message(__FILE__" local optimizations are off") 
#pragma optimize("", off)
#endif
//-----------------------------------------------------------------------------
//#define ENABLE_DIAGNOSTICS
#ifdef ENABLE_DIAGNOSTICS
	// Diagnostic Print
	#define DPRINT printf
#else
	// Defined to do nothing
	#define DPRINT( formatString, ... )
#endif
#undef ENABLE_DIAGNOSTICS
//-----------------------------------------------------------------------------


// ===========
// SysTickTime
// ===========

// The SysTickTime class is assumed to give a highly reliable,
// but slowly ticking system time.

// ----------------------------------------------------------------------------
// Description:
//   Returns the current timer value, in the expected units.
// ----------------------------------------------------------------------------
Be::Time SysTickTime::Get()
{
    const Be::Time Scale = /*GetUnitsPerSecond()*/ 10000000
        / 1000;
    return CcpGetTickCount() * Scale;
}

// ----------------------------------------------------------------------------
// Description:
//   As per name.
// ----------------------------------------------------------------------------
Be::Time SysTickTime::GetUnitsPerSecond()
{
	return Be::Time(1e7);
}


// =========
// HiResTime
// =========

// ----------------------------------------------------------------------------
// Description:
//   Returns the current timer value, in the expected units.
// ----------------------------------------------------------------------------
Be::Time HiResTime::Get()
{
	return CcpGetTimestamp();
}

// ----------------------------------------------------------------------------
// Description:
//  Query the declared "frequency" of the HiResTime. It is only approximate -
//  In particular, the actual rate may change due to power-saving etc on some
//  systems, although this is technically a bug (i.e. in defiance of the spec).
// ----------------------------------------------------------------------------
Be::Time HiResTime::GetUnitsPerSecond()
{
	return CcpGetTimestampFrequency();
}


// =========
// SuperTime
// =========

// Static members shared by all instances
SysTickTime SuperTime::s_ReliableTime;
HiResTime SuperTime::s_HiResTime;

// ----------------------------------------------------------------------------
// Description:
//   A default construtor to initialise my members.
// ----------------------------------------------------------------------------
SuperTime::SuperTime()
{
	m_HiResBase = GetHiRes();
	m_ReliableBase = GetReliable();
    m_LastHiRes = m_HiResBase;
    m_LastReliable = m_ReliableBase;

	m_LastTime = m_ReliableBase;

	m_ReliablePerHighRes = s_ReliableTime.GetUnitsPerSecond() / (double)s_HiResTime.GetUnitsPerSecond(); // Theoretical model.
    m_KnownError = 0;
    m_dTimeRemainder = 0;

    #ifdef _SUPERDEBUG
        DWORD myPid = GetCurrentProcessId();
        char fname[256];
        sprintf_s(fname, "supertime-PID%lu.csv", myPid);
	    fopen_s(&out, fname, "w");
    #endif
}

// ----------------------------------------------------------------------------
// Description:
//   Returns the current timer value, in the expected units.
// ----------------------------------------------------------------------------
Be::Time SuperTime::Get()
{
    #ifdef ENABLE_DIAGNOSTICS
        static int queriesPerSecond = 0;
        queriesPerSecond++;
        static double adjustmentsMade = 0;
    #endif

	// Grab the latest samples from out two timers
	Be::Time hiRes = GetHiRes(); // Assumed to be fine-grained but prone to drift
	Be::Time reliable = GetReliable(); // Assumed to be coarse-grained but accurate

    double dHiRes = double(hiRes - m_LastHiRes);

    Be::Time newTime = m_LastTime;
    if (dHiRes > 0) {
        double dTime = dHiRes * m_ReliablePerHighRes + m_dTimeRemainder;

        double adjustmentAmount = 0;
        if (m_KnownError > 50000) {
            // We're more than 5ms ahead of the reliable clock, slow down!
            // Shave off .1% of time for every ms over 5ms
            double adjustmentFactor = std::min(1.0, (m_KnownError - 50000) / 10000000.0);
            // Cap the adjustment at the known error amount so we don't over-shoot.
            adjustmentAmount = std::min(m_KnownError, dTime * adjustmentFactor);
        } else if (m_KnownError < -20000) {
            // Same thing, but with negatives.  Tighter boundry because normal error is presumably
            // due to querying the reliable timer after its movement, which should only give positive error.
            double adjustmentFactor = std::max(-1.0, (m_KnownError + 20000) / 10000000.0);
            adjustmentAmount = std::max(m_KnownError, dTime * adjustmentFactor);
        }
        m_KnownError -= adjustmentAmount;
        dTime -= adjustmentAmount;
        #ifdef ENABLE_DIAGNOSTICS
            adjustmentsMade += fabs(adjustmentAmount);
        #endif

        Be::Time integralDTime = Be::Time(dTime);
        m_dTimeRemainder = dTime - integralDTime;
        newTime += integralDTime;
    }

    if (reliable != m_LastReliable) {
        // Every time the reliable clock gives us motion, nudge our error accounting
        // This implicitly assumes that we're sampling often enough that we catch the
        // reliable clock shortly after it increments.  The handling of the error has
        // a deadzone to account for this assumption.
        m_KnownError = .95 * m_KnownError + .05 * double(newTime - reliable);
    }

	static const double samplingInterval = (double)s_ReliableTime.GetUnitsPerSecond() * 1;
    if (reliable - m_ReliableBase > samplingInterval) {
        // Allright, enough time has passed that we can get a new look at our
        // reliable/hires ratio and update it
        double dHiRes = double(hiRes - m_HiResBase);
        double dReliable = double(reliable - m_ReliableBase);

        m_ReliablePerHighRes = .95 * m_ReliablePerHighRes + .05 * (dReliable/ dHiRes);
        m_HiResBase = hiRes;
        m_ReliableBase = reliable;
        
		DPRINT( "m_ReliablePerHighres: %f, Time:  %I64u, Error: %.2fms, %d queries, %.2fms adjustments\n", m_ReliablePerHighRes, newTime, m_KnownError / 10000.0, queriesPerSecond, adjustmentsMade / 10000.0 );
        #ifdef ENABLE_DIAGNOSTICS
            queriesPerSecond = 0;
            adjustmentsMade = 0;
        #endif
    }

    m_LastHiRes = hiRes;
    m_LastReliable = reliable;
    m_LastTime = newTime;

    #ifdef _SUPERDEBUG
	    fprintf(out, "%I64u,%I64u,%I64u,%f,%f\n", hiRes, reliable, newTime, m_ReliablePerHighRes, m_KnownError);
    #endif

    return newTime;
}

// ----------------------------------------------------------------------------
// Description:
//   As per name.
// ----------------------------------------------------------------------------
Be::Time SuperTime::GetUnitsPerSecond()
{
	return Be::Time(1e7);
}
	
//-----------------------------------------------------------------------------
#ifdef OptimizeOff
#pragma optimize("", on)
#endif
//-----------------------------------------------------------------------------
