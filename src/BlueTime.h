// Copyright © 2004 CCP ehf.

/* 
	*************************************************************************

	BlueTime.h

	Project:   Blue

	Description:   

	  Declarations for the core time related classes in blue.

	  Provides the classes SysTickTime, HiResTime and SuperTime.

	  SysTickTime is only used to implement SuperTime  
	  HiResTime is used in TaskletTimer.h
	  SuperTime is used in BlueOS.h as "SuperTime mWallclock"

	Dependencies:

	  Blue

	*************************************************************************
*/

#pragma once
#ifndef BlueTime_H
#define BlueTime_H

// ----------------------------------------------------------------------------
// Description:
//   An abstract base class for a clock
// ----------------------------------------------------------------------------
class BlueClock
{
public:
	// Get the value
	virtual Be::Time Get() = 0;

	// Get the number of timing units per second
	//
	// NB: This is NOT necessarily equivalent to the underling clock freq,
	// because the timing quantum might not be 1 for some clocks.
	//
	// e.g. Win32's GetTickCount always reports in units of milliseconds,
	// giving 1000 units per second, but only has an update frequency of
	// around 60Hz (depending on the machine)
	//
	virtual Be::Time GetUnitsPerSecond() = 0;
};

// ----------------------------------------------------------------------------
// Description:
//   This class provides the system's "time since boot" in 1e-7 second units.
//   Implmented with GetTickCount - considered to be accurate in the long-run,
//   but has low resolution (system dependent, but often just 16ms).
// ----------------------------------------------------------------------------
class SysTickTime : public BlueClock
{
public:
	Be::Time Get();
	Be::Time GetUnitsPerSecond();
};

// ----------------------------------------------------------------------------
// Description:
//   This class provides a high resolution "time since boot" timer using QPC.
//   High resolution, but prone to various problems on some systems.
//   (Problems can include rate variations, stall-and-catchup bursts etc).
// ----------------------------------------------------------------------------
class HiResTime : public BlueClock
{
public:
	Be::Time Get();
	Be::Time GetUnitsPerSecond();
};

// ----------------------------------------------------------------------------
// Description:
//   This class merges SysTickTime and HiResTime, trying to keep tight to
//   SysTickTime and using HiResTime to interpolate as best as we can.
//   
//   This is achieved via two mechanisms:
//     1)  Keeping a running average of the ratio between the clocks and advancing
//         along that slope based solely on the high resolution clock.  This covers
//         the standard case of sane operation fully.
//     2)  A strong corrective force which kicks in if we notice that we've deviated
//         too far from the reliable clock.  Its strength is dependent on how much
//         error is present, so it gracefully converges down to nothingness once
//         sanity returns.  This is to cover extreme variance in the high resolution
//         clock which we have seen under stressful computing situations.
//         See defect 75152 for motivation
//
//   Uses units of 1e-7 seconds (i.e. the 100ns units of a Win32 FILETIME)
//   The value returned is, roughly-speaking, the time since boot.
// ----------------------------------------------------------------------------
class SuperTime : public BlueClock
{
public:
	SuperTime();

	Be::Time Get();
	Be::Time GetUnitsPerSecond();
	
private:
	// Inner Timers
	//
	static SysTickTime s_ReliableTime;
	static HiResTime s_HiResTime;
	Be::Time GetHiRes() const { return s_HiResTime.Get(); }
	Be::Time GetReliable() const { return s_ReliableTime.Get(); }

	double m_ReliablePerHighRes;  // Our best guess at the current ratio between the deltas of the two clocks
	Be::Time m_LastTime;  // Simply the last return value of the Get function
    Be::Time m_HiResBase, m_ReliableBase;  // The respective times at the last sample interval
    Be::Time m_LastHiRes, m_LastReliable;  // The times at the last query
    double m_KnownError;  // This is a running average of how far off of the accurate clock we've been

    double m_dTimeRemainder; // The remainder, in sub-tenths-of-nanoseconds amounts, we need to carry between queries.

//#define _SUPERDEBUG
#ifdef _SUPERDEBUG
	FILE *out;
#endif
};

#endif // BlueTime_H