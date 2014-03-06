////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		March 2014
// Copyright:	CCP 2014
//

#pragma once

#ifndef BlueResManBackgroundCall_h
#define BlueResManBackgroundCall_h

#if CCP_STACKLESS

struct BlueResManBackgroundCall
{
public:
	BlueResManBackgroundCall( uint32_t flags = 0 );

	~BlueResManBackgroundCall();

	void AddToQueue();

	// Waits on the channel, blocking this tasklet.
	bool Wait();

	// This gets called on the background thread
	static void ForwardMarkAsDone( void* pContext );

	// This gets called on the main thread, in Update
	static void MarkAsDone( void* pContext );

private:
	uint32_t m_flags;
	PyChannelObject* m_channel;
};

class BlueResManReadStreamArgs : public BlueResManBackgroundCall
{
public:
	void ReadStream( const std::wstring& filename, IBlueStream* stream );

private:
	std::wstring m_filename;
	IBlueStreamPtr m_destination;

	void AddToQueue();

	static void ForwardCopyStream( void* context );

	void CopyStreamImpl( );
};
#endif


#endif // BlueResManBackgroundCall_h
