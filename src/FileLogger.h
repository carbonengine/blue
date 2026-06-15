// Copyright © 2011 CCP ehf.

#pragma once
#ifndef CCP_FILELOGGER_H
#define CCP_FILELOGGER_H

namespace CCP
{
	const static unsigned int  MAX_MESSAGE_SIZE = 100*128;
	// Preallocated number of messages so we don't have to allocate them at runtime
	// if we have more than this number of pending messages, we will block until it's free

	struct FileLogMessage
	{
		char facility[32];
		char object[32];
		int type;
		CcpDateTime systemtime;
		char message[MAX_MESSAGE_SIZE];
		unsigned long long id;
	};

	// Class that holds a pre-allocated amount of messages in a circular buffer. 
	class FileMessageCircularBuffer
	{
	private:
		unsigned int m_size;
		volatile unsigned int m_writeIndex;
		volatile unsigned int m_readIndex;
		FileLogMessage* m_data;
		CcpMutex m_lock;

	public:

		// Constructor
		FileMessageCircularBuffer(unsigned int size);

		// Deconstructor
		~FileMessageCircularBuffer( );

		// Returns true if the circular buffer is full. Will continue to return true until someone deques an item from the buffer
		bool IsFull();

		// Returns true if the circular buffer has no pending messages
		bool IsEmpty();

		// Returns a pointer to the next free message in the buffer
		FileLogMessage* Enque();

		// Returns a pointer to the next message in the buffer and advances the read index
		FileLogMessage* Deque();

		void Lock();

		void Unlock();
	private:
		FileMessageCircularBuffer( const FileMessageCircularBuffer& ) /* = delete */;
		FileMessageCircularBuffer& operator=( const FileMessageCircularBuffer& ) /* = delete */;
	};



	// Sets up file logging which will forward messages in to the network via UDP to the address specified
	bool SetupFileLogging(CCP::LogType threshold, const char* role, char* errorMessageOut, int errorMessageBufferSize);

	// Disables file logging if it has been enabled
	void DisableFileLogging();

	// Logging call back which will forward logging messages. Will not do anything if SetupFileLogging has not been called
	void LogToFile( CcpLogChannel_t& logObject, LogType type, unsigned long userData, const char* message );

	// Thread function for the thread that takes care of writing stuff out to the socket
	uint32_t FileLoggingThreadFunc( void* arg );

}

#endif
