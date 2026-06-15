// Copyright © 2014 CCP ehf.

#pragma once

#ifndef IBlueCallbackMan_h
#define IBlueCallbackMan_h

#include <CcpAtomic.h>

BLUE_INTERFACE( IBlueCallbackMan ) : IRoot
{
	typedef void (*CallbackFunc)( void* pContext );

	static const uint32_t BCBF_NONE = 0;
	static const uint32_t BCBF_URGENT = 1;
	static const uint32_t BCBF_FENCE = 2;

	// Adds the given callback to the callback queue. The callback will be called with the
	// given context. The return value is a unique identifier for the callback request that
	// can be used to cancel the request later, if necessary.
	// A separate queue is maintained for urgent requests - urgent requests are always handled
	// before regular requests.
	virtual bool Add( CallbackFunc pCb, void* pContext, uint32_t flags, CcpAtomic<uint32_t>* id ) = 0;

	// Cancels a previously requested callback. If the callback is already in progress, the
	// calling thread is blocked until the callback finishes.
	virtual void Cancel( uint32_t id ) = 0;

	// Creates one or more threads and sits in a loop, processing callbacks until stopped.
	// Call SetThreadCount to set the number of threads created, and SetThreadPriority
	// to control the priority.
	virtual void Run() = 0;

	virtual void Pause() = 0;
	virtual void Resume() = 0;
	virtual bool IsPaused() = 0;

	// This is called in the Run loop. To Throttle it back, inherit from this class,
	// override this function to Pause when needed, Resuming from somewhere else.
	virtual void Throttle() = 0;

	// This should be called regularly to Resume the thread in case it has been Paused
	// by Throttle (see above).
	virtual void Unthrottle() = 0;

	// Stops the callback manager's thread, as created by Run.
	virtual void Stop() = 0;

	// Processes the next callback on the queue. Returns true if a callback was processed,
	// false otherwise (indicating an empty queue).
	virtual bool Update() = 0;

	virtual bool IsEmpty() const = 0;

	virtual uint32_t GetNextId() const = 0;

	virtual unsigned int GetSize() const = 0;

	// Maximum time any entry spent in the queue since last call to ResetQueueStats (or from the start).
	// Time is measured in seconds.
	virtual float GetTimeInQueueMax() const = 0;

	// Average time for entries spent in the queue since last call to ResetQueueStats (or from the start).
	// Time is measured in seconds.
	virtual float GetTimeInQueueAverage() const = 0;

	// Resets the values for GetTimeInQueueMax() and GetTimeInQueueAverage().
	virtual void ResetQueueStats() = 0;

	// Sets the thread priority for all threads running on this callback manager.
	// Positive values boost the priority higher than normal,
	// negative values set the priority lower than normal.
	virtual void SetPriority( int prio ) = 0;

	// Sets the number of threads created when Run is called.
	virtual void SetThreadCount( unsigned int count ) = 0;

	// Sets the name of this callback manager. This can be useful when debugging/profiling.
	virtual void SetName( const char* name ) = 0;

};

extern BLUEIMPORT IBlueCallbackMan* BeCallbackMan;

#endif
