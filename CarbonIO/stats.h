#pragma once
#ifndef STATS_H
#define STATS_H
/*************************************************************************

stats.h

Author:    Curt Hartung
Created:   Sep 2010
OS:        Win32
Project:   CarbonIO

Description: structure describing the statistics gathered from CarbonIO

(c) CCP 2010

***************************************************************************/

#include <windows.h>
#include <CcpUtils/LinkHash.h>
#include <CcpUtils/ScopedLocks.h>

//------------------------------------------------------------------------------
struct CioSocketStats
{
	long packetsReceived;
	long long bytesReceived;
	long long bytesReceivedDecompressed; // after decompression (if the data is not compressed, this will still be added to)

	long packetsSent;
	long long bytesSent;
	long long bytesSentCompressed;
};

//------------------------------------------------------------------------------
struct CioStats
{
	void reset(); // reset periodics

	long busyThreads; // threads fill in this value when they wake up
	long workerThreads; // only filled 
	long outstandingJobs; // how many jobs have been allocated and are awaiting completion
	long jobPoolSize; // how large the total job (allocated + unallocated) pool is
	long blockedThreads; // how many worker threads are blocked on something (usually connect or accept)

	long packetsReceived;
	long long bytesReceived;
	long long bytesReceivedDecompressed; // after decompression (if the data is not compressed, this will still be added to)

	unsigned long long compressedAcceptedIn;
	unsigned long long compressedRejectedIn;
	unsigned long long compressedAccepted;
	unsigned long long compressedRejected;

	volatile long oorRecieved;
	volatile long oorGaveUp;
	volatile long noReadJobsScheduledGaveUp;
	volatile long spunWaitingForReadJobsScheduled;
	volatile long shutdownSpinWaitCount;
	
	long packetsSent;
	long long bytesSent;
	long long bytesSentCompressed;

	volatile long activeSockets;
	volatile long totalSockets;
	volatile long accepts;
	volatile long connects;
	
	// these stats are ESTIMATED, to save core-syncs they are not
	// atomically incremented
	long GILAcquiredNum; // how many times
	long GILCoalesced; // how many GIL locks were deferred to a master thread
	long long GILAcquireTime; // how much time total (nanosecs)
	long longestGILAcquireTime; // most had to wait (nanosecs)

	long zeroByteReadsRequired; // system got WSANOBUFFS and had to initiate a zero-byte read

	long partialSends; // how many times was the 'finish partial' called. this ought to always be zero

	long packetsAllocated; // how many packets are currently outstanding
	long packetFreeListSize; // how large the free-list is

	long pendingCallQueueFailure; // times it tried to wake with a pending call but the call failed back to dynamic context
};

//------------------------------------------------------------------------------
class CioEventTracker
{
public:

	CioEventTracker();
	~CioEventTracker();
	
	//------------------------------------------------------------------------------
	struct SEntry
	{
		char name[128];
		union
		{
			struct
			{
				long long time;
				long long timeSmoothed;
				long long varSmoothed;
				long long thirdSmoothed;
			} t;
			struct
			{
				float val;
				float valSmoothed;
				float varSmoothed;
				float thirdSmoothed;
			} f;
		};
		int count;
		char type;
	};

	SEntry *get( const char *name);
	void inc( const char* name, long long time );
	void incval( const char* name, float val );
	void incevt( const char* name, int count=1 );
	void clear();

	Ccp::LinkHash<SEntry> m_eventList;
	CRITICAL_SECTION m_listLock;
};

#endif
