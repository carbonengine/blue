// Copyright © 2014 CCP ehf.

#ifndef _CALLBACKMAN_H_
#define _CALLBACKMAN_H_

#include <CcpSemaphore.h>
#include <CcpAtomic.h>
#include "IBlueCallbackMan.h"

BLUE_DECLARE( BlueCallbackMan );

// A callback manager manages requests from different threads to have the given callback
// execute on the callback manager's thread. The callback manager can create its own thread
// to run the callbacks, or the caller can call Update to process one callback.
// Callbacks are added to a queue, processed one at a time in the order the requests are received.
// 
// A callback manager with its own thread is intended for managing background tasks, such as
// loading and processing assets. The main thread also has a callback manager for managing tasks
// that need to happen on that thread, such as creating D3D resources.
//
class BlueCallbackMan : public IBlueCallbackMan
{
public:
	EXPOSE_TO_BLUE();

	BlueCallbackMan( IRoot* lockobj = NULL );
	~BlueCallbackMan();

	//////////////////////////////////////////////////////////////////////////
	// IBlueCallbackMan
	//

	// Adds the given callback to the callback queue. The callback will be called with the
	// given context. The return value is a unique identifier for the callback request that
	// can be used to cancel the request later, if necessary.
	// A separate queue is maintained for urgent requests - urgent requests are always handled
	// before regular requests.
	bool Add( CallbackFunc pCb, void* pContext, uint32_t flags, CcpAtomic<uint32_t>* id ) override;

	// Cancels a previously requested callback. If the callback is already in progress, the
	// calling thread is blocked until the callback finishes.
	void Cancel( uint32_t id ) override;

	// Creates a thread and sits in a loop, processing callbacks one at a time until stopped.
	void Run() override;

    void Pause() override;
    void Resume() override;
    bool IsPaused() override;

    // This is called in the Run loop. To Throttle it back, inherit from this class,
    // override this function to Pause when needed, Resuming from somewhere else.
    void Throttle() override {}

	// This should be called regularly to Resume the thread in case it has been Paused
	// by Throttle (see above).
	void Unthrottle() override {}

	// Stops the callback manager's thread, as created by Run.
	void Stop() override;

	// Processes the next callback on the queue. Returns true if a callback was processed,
	// false otherwise (indicating an empty queue).
	bool Update() override;

	bool IsEmpty() const override;

	uint32_t GetNextId() const override;

	unsigned int GetSize() const override;

	// Maximum time any entry spent in the queue since last call to ResetQueueStats (or from the start).
	// Time is measured in seconds.
	float GetTimeInQueueMax() const override;

	// Average time for entries spent in the queue since last call to ResetQueueStats (or from the start).
	// Time is measured in seconds.
	float GetTimeInQueueAverage() const override;

	// Resets the values for GetTimeInQueueMax() and GetTimeInQueueAverage().
	void ResetQueueStats() override;

	// Sets the thread priority.
	// Positive values boost the priority higher than normal,
	// negative values set the priority lower than normal.
	void SetPriority( int prio ) override;

	void SetThreadCount( unsigned int threadCount ) override;

	void SetName( const char* name ) override;
	//
	//////////////////////////////////////////////////////////////////////////

private:
	// Base name, used to identify thread(s) and mutexes.
	std::string m_name;

	struct CallbackEntry
	{
		uint32_t id;
		CallbackFunc pCb;
		void* pContext;
		uint64_t timeStamp;
		uint32_t fenceMask;
	};
	
	// Number of entries in the queue
	unsigned int m_size;

	typedef TrackableStdList<CallbackEntry> CallbackEntryList;
	CallbackEntryList m_queue;
	CallbackEntryList m_urgentQueue;
	CallbackEntryList m_fenceQueue;
	
	// Callback IDs are a simple incrementing counter.
	uint32_t m_nextId;

	// Mutex to guard access to the queue - adding and removing happen on different threads.
	mutable CcpMutex m_queueMutex;

	unsigned int m_threadCount;
	bool m_isRunningOwnThreads;

	struct ThreadData
	{
		ThreadData();

		// Thread index, used for fence bitmask
		uint32_t m_threadIndex;

		// ThreadData is used as the context for the thread proc
		BlueCallbackMan* m_owner;

		// Id of callback being processed, if any.
		uint32_t m_currentId;

		// Mutex acquired while callback is in progress, in case caller attempts to
		// cancel the callback in progress.
		CcpMutex m_cbInProgressMutex;

		// Handle to the threads that are created when Run is called.
		CcpThreadHandle_t m_threadHandle;

		// Sometimes the thread id is useful, for Telemetry in particular.
		CcpThreadId_t m_threadId;

		std::string m_name;
	};

	typedef TrackableStdVector<ThreadData*> ThreadDataVector;
	ThreadDataVector m_threads;

	// Thread priority - stored here in case it's set before the thread is created.
	int m_threadPriority;

	// If the callback manager runs its own thread and callback queue is empty, the thread
	// waits on this semaphore until something is added to its queue.
	CcpSemaphore m_alarm;

    // If the callback manager runs its own thread, it can be paused and resumed from the
    // outside. The thread waits on this semaphore whenever it's paused.
    CcpSemaphore m_pauseSemaphore;
    CcpAtomic<uint32_t> m_pauseCounter;

	// A flag to indicate to the callback manager's thread that Stop has been called (presumably
	// from a different thread).
	bool m_stop;

	// For statistics gathering
	uint64_t m_timeInQueueMax;
	uint64_t m_timeInQueueTotal;
	double m_timeInQueueAverage;
	unsigned int m_entriesProcessed;
private:
	static uint32_t StaticThreadProc( void* pContext );
	uint32_t ThreadProc( struct BlueCallbackMan::ThreadData* td );

	uint32_t AddHelper( CallbackFunc pCb, void *pContext, bool isUrgent, bool isFenced );
	bool UpdateThread( struct BlueCallbackMan::ThreadData* td );

	// Extract an entry from the given queue. Any fenced entries encountered are added
	// to the fence queue.
	bool ExtractFromQueue( CallbackEntryList &queue, CallbackEntry &entry, int threadIndex );

	// Remove the given id from the given queue, returning true if the entry was found
	bool RemoveFromQueue( CallbackEntryList &queue, uint32_t id );

};

TYPEDEF_BLUECLASS( BlueCallbackMan );


#endif
