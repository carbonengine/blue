// Copyright © 2014 CCP ehf.

#pragma once

#ifndef BlueResManBackgroundCall_h
#define BlueResManBackgroundCall_h

// Forward declare type from the <Scheduler.h> header, include will be in .cpp file
struct PyChannelObject;

class IBlueResManBackgroundCall
{
public:
	virtual void Perform() = 0;
};

struct BlueResManBackgroundCall
{
public:
	// Issue a call on a background thread. Returns true if the call was issued,
	// false if not (can happen if the calling tasklet is killed).
	static bool Issue( IBlueResManBackgroundCall* theCall, uint32_t flags = 0 );

private:
	BlueResManBackgroundCall( IBlueResManBackgroundCall* theCall, uint32_t flags = 0 );

	~BlueResManBackgroundCall();

	void AddToQueue();

	// Waits on the channel, blocking this tasklet.
	bool Wait();

	// This gets called on the background thread
	static void DoTheCall( void* pContext );

	// This gets called on the main thread, in Update
	static void MarkAsDone( void* pContext );

private:
	CcpAtomic<uint32_t> m_id;
	CcpAtomic<uint32_t> m_mainQueueId;
	uint32_t m_flags;
	CcpAtomic<uint32_t> m_isCanceling;

#if CCP_STACKLESS
	PyChannelObject* m_channel;
#endif

	IBlueResManBackgroundCall* m_backgroundCall;
};


#endif // BlueResManBackgroundCall_h