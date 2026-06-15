// Copyright © 2011 CCP ehf.

#pragma once
#ifndef CCP_NETWORKLOGGER_H
#define CCP_NETWORKLOGGER_H

#include "Logger/logger.h"
#include "Blue.h"

namespace CCP
{
	const static size_t MAX_MESSAGE_SIZE = 100*128;
	// Preallocated number of messages so we don't have to allocate them at runtime
	// if we have more than this number of pending messages, we will block until it's free

	struct NetworkLogMessage
	{
		char facility[32];
		char object[32];
		int type;
		Be::Time time;
		char message[MAX_MESSAGE_SIZE];
		unsigned long long id;
	};

	// Class that holds a pre-allocated amount of messages in a circular buffer. 
	class MessageCircularBuffer
	{
	private:
		unsigned int m_size;
		volatile unsigned int m_writeIndex;
		volatile unsigned int m_readIndex;
		NetworkLogMessage* m_data;
		CRITICAL_SECTION m_lock;

	public:

		// Constructor
		MessageCircularBuffer(unsigned int size);

		// Deconstructor
		~MessageCircularBuffer( );

		// Returns true if the circular buffer is full. Will continue to return true until someone deques an item from the buffer
		bool IsFull();

		// Returns true if the circular buffer has no pending messages
		bool IsEmpty();

		// Returns a pointer to the next free message in the buffer
		NetworkLogMessage* Enque();

		// Returns a pointer to the next message in the buffer and advances the read index
		NetworkLogMessage* Deque();

		void Lock();

		void Unlock();
	};



	// Sets up network logging which will forward messages in to the network via UDP to the address specified
	bool SetupNetworkLogging(const char* serverName, int port, CCP::LogType threshold, const char* role, char* errorMessageOut, int errorMessageBufferSize);

	// Disables network logging if it has been enabled
	void DisableNetworkLogging();

	// Logging call back which will forward logging messages. Will not do anything if SetupNetworkLogging has not been called
	void LogToNetwork( CcpLogChannel_t& logObject, LogType type, unsigned long userData, const char* message );

	// Thread function for the thread that takes care of writing stuff out to the socket
	void NetworkLoggingThreadFunc( void* arg );

}

#endif
