#pragma once
#ifndef CARBON_IO_H
#define CARBON_IO_H
/*************************************************************************

CarbonIO.h

Author:    Curt Hartung
Created:   Jun 2010
OS:        Win32
Project:   Carbon IO

Description: implementation of IO Completion Ports for stackless

(c) CCP 2010

***************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#include <winsock2.h> // must be included before windows.h
#include <windows.h>
#include <new>

#include <socket_semantics.h>
#include <Python.h>

#include <Ws2tcpip.h>
#include <mswsock.h>

#include "stackless_api.h"
#include "api.h"
#include "dll_exports.h"
#include "protocol.h"
#include "stats.h"
#include "ssl_pipe.h"

#include <CcpUtils/LinkHash.h>
#include <CcpUtils/RWSpinLock.h>
#include <CcpUtils/ScopedLocks.h>

typedef struct _PySocketSockObject PySocketSockObject;
typedef union sock_addr sock_addr_t;
extern "C" void ciolog( const char* format, ... );

extern "C" void initcarbonio( void );

//------------------------------------------------------------------------------
//#define DISABLE_CIO_OPTIMIZATIONS
//#define DEBUG_LOG
//#define DEBUG_EMIT_LOG
#define D_CONNECT(a) //a
#define D_ADDTOCIO(a) //a
#define D_HANDLEUDPREAD(a) //a
#define D_HANDLEREAD(a) //a
#define D_RECVEX(a) //a
#define D_COMPRESS(a) //a
#define D_DECOMPRESS(a) //a
#define D_PROCESSRAW(a) //a
#define D_FAILALL(a) //a
#define D_SEND(a) //a
#define D_SENDPACKET(a) //a
#define D_SEQUENCE(a) //a
#define D_PACKET(a) //a
#define D_HANDLESEND(a) //a
#define D_GETADDRINFO(a) //a
#define D_WORKER(a) //a
#define D_GIL(a) //a
#define D_REAPSOCKET(a) //a
#define D_REF(a) //a
//#define D_REFCOUNT
#define D_REFHISTORY(a) //a
#define D_ACCEPT(a) //a
#define D_TRIGGERREAD(a) //a
#define D_SSL(a) //a
#define D_WAKEUP(a) //a
#define D_POPHEAD(a) //a
#define D_SOCKET(a) //a
#define D_JOBFIFO(a) //a
#define D_POSTDECOMPRESS(a) //a
#define D_ON_TASKLET_SCHEDULED(a) //a
#define D_HOUSEKEEP(a) //a
#define D_ONTSCHED(a) //a

// helper macros for debug purposes
//#define DESTROY_ON_DEC
#ifdef D_REFCOUNT
 #define DEC_COMPLETION_REF(a) decCompletionRefEx((a), __LINE__)
 #define INC_COMPLETION_REF(a) incCompletionRefEx((a), __LINE__)
#else
 #ifdef DESTROY_ON_DEC
  #define DEC_COMPLETION_REF(a) { if (!InterlockedDecrement(&((a)->refCount))) { m_singleton.deleteCompletionUnit(a); } }
  #define INC_COMPLETION_REF(a) InterlockedIncrement(&((a)->refCount));
 #else
  #define DEC_COMPLETION_REF(a) { if (!InterlockedDecrement(&(a)->refCount)) SetEvent((a)->refReachedZero); }
  #define INC_COMPLETION_REF(a) InterlockedIncrement(&(a)->refCount);
 #endif
#endif

//------------------------------------------------------------------------------
class CarbonIO
{
public:

	bool init(); // must be called at least once; subsequent calls have no effect

	PyObject* formatStats( SOCKET fd );
	PyObject* getTimerStats();
	void resetTimers() { m_events.clear(); }
	void resetStats() { m_stats.reset(); }
	void setTimersEnabled( bool enable ) { m_enableTimers = enable; }

	bool areTimersEnabled() { return m_enableTimers; }

	// the callback returns wether or not it handled the packet, the
	// first 'true' causes the traversal to end and the packet to never
	// be forwarded up to a python readpacket() call
	void addPacketCallbackPostDecompress( CioDataCallback packetCallback );
	void removePacketCallbackPostDecompress( CioDataCallback packetCallback );
	void setOnTaskletScheduledCallback( void (*callback)(bool force) ) { m_onTaskletScheduledCallback = callback; }
	void setOnTaskletScheduledEvent( HANDLE event ) { m_onTaskletScheduledEvent = &event; }
	void wakeupTasklets();

	static CarbonIO* singleton() { return &m_singleton; } // there can be only one

	bool isSocketValid( SOCKET fd );
	SOCKET socket( int family, int type, int proto );
	int connect( SOCKET fd, sock_addr_t *addr, int addrlen, bool &timeoutFlag, float timeout );
	SOCKET accept( SOCKET fd, struct sockaddr *addr, int *addrlen, int* timeoutFlag, float timeout );
	int close( HANDLE fd ); // shut down a socket and close it-- note the close is queued, not immidiate
	int shutdown( SOCKET fd, int how ); // end communications but do NOT close the socket

	inline PyObject* recv( SOCKET fd, int len, int flags, float timeout );
	inline int recvInto( SOCKET fd, char *buf, int len, int flags );
	inline PyObject* recvPacket( SOCKET fd, bool withOOB );
	inline PyObject* recvFrom( SOCKET fd, int len, struct sockaddr* from, int* fromlen );
	inline int recvFromInto( SOCKET fd, char *buf, int len, struct sockaddr* from, int* fromlen );

	PyObject* getBytesWaiting( SOCKET fd );

	int setBlockingSend( SOCKET fd, int block );
	bool send( SOCKET fd, char *data, int len, int flags );
	int sendTo( SOCKET fd, char *data, int len, int flags, struct sockaddr* to, int* tolen );
	int sendSequence( SOCKET fd, PyObject *obj, int flags );
	int sendSequenceTo( SOCKET fd, PyObject *obj, int flags, struct sockaddr* to, int* tolen );
	bool sendPacket( SOCKET fd, PyObject *args );
	inline bool externalSendPacket( SOCKET fd, const char* data, const unsigned int len, const char* OOBData, const unsigned int OOBLen );
	inline unsigned int getMaxPacketSize( const unsigned int len, const unsigned int OOBLen ) { return sizeof(unsigned int)*2 + len + OOBLen; }
	inline bool externalSendFormattedPacket( const SOCKET fd, const char* data, const unsigned int len );
	inline bool formatPacket( char* buf, unsigned int* len, const char* data, const unsigned int dataLen, const char* OOBData, const unsigned int OOBLen, bool suppressCompression );

	PyObject* setCompressionThreshold( SOCKET fd, PyObject *args );
	PyObject* setGlobalCompressionThreshold( PyObject *args );
	PyObject* setCompressionMinRatio( PyObject *args );
	PyObject* setCompressionLevel( PyObject *args );
	PyObject* setCompressionType( PyObject *args );

	int setMaxPacketSize( SOCKET fd, PyObject *args );
	int getAddrInfo( const char *nodename, const char *servname, struct addrinfo *hints, struct addrinfo **res );
	hostent *getHostByName( const char* name );
	hostent *getHostByAddr( const char *addr, int len, int type );

	void enableSSL( SOCKET fd );
	void setSSLClientCertificate( PyObject *obj );
	void setSSLServerCertificate( PyObject *obj );
	void setSSLPrivateClientKey( PyObject *obj );
	void setSSLPrivateServerKey( PyObject *obj );
	void setSSLHandshakeMaxSeconds( PyObject *obj ) { m_SSLHandshakeNegotiationSeconds = PyInt_AsLong(obj); }

#ifdef DEBUG_LOG
	void setErrorLogCallback( void (*callback)(const char* msg) ) { }
	void setStatusLogCallback( void (*callback)(const char* msg) ) { }
#else
	void setErrorLogCallback( void (*callback)(const char* msg) ) { m_errorMessageCallback = callback; }
	void setStatusLogCallback( void (*callback)(const char* msg) ) { m_statusMessageCallback = callback; }
#endif

	PyObject* setWakeupMethod( int method );
	PyObject* getWakeupMethod();

private:

	CarbonIO();
	~CarbonIO();

	//------------------------------
	// when a negative number is passed through the 'size' parameter
	// for a worker thread, it is interpreted as a command
	enum ESizeMagicNumbers
	{
		ceJobReapSocket = -1,
		ceJobAccept = -2,
		ceJobConnect = -3,
		ceJobGetAddrInfo = -4,
		ceJobGetHostByName = -5,
		ceJobGetHostByAddr = -6,
	};

	//------------------------------
	struct SPacket
	{
		char *data; // actual data
		unsigned int packetLen; // how long the meaninful (packet) data is
		unsigned int payloadOffset;
		char *auxData; // for the case of a decompressed packet, the actual data
		unsigned int auxDataLen; // how much aux Data there is (valid if auxData is non-null)
		char *oobData; // out-of-band data, if there was any
		int oobLen; // if oob is non-null
		unsigned int bufferLen; // how long the actual buffer has been allocated to
		long long serialNumber;
		struct sockaddr addr;
		int addrlen;
		int flags; // used when sending
		bool streamSend;
		SPacket *next;
	};
	CRITICAL_SECTION m_packetFreeListLock;
	SPacket* m_packetFreeList;

	SPacket* getPacket();
	void freePacket( SPacket *packet );
	static void freeAuxData( SPacket &packet ) { delete[] packet.auxData; packet.auxData = 0; packet.auxDataLen = 0; }
	static void appendDataToPacket( SPacket *packet, const char *data, const unsigned int len );
	static inline void ensurePacketSpace( SPacket *packet, const unsigned int len );
	inline SPacket* createPacketFromOutgoingData( const char* data, const unsigned int len, const char* OOBData, const unsigned int OOBLen );
	bool sendPacketEx( SOCKET fd, SPacket *packet, bool GILOwned );

	// represents work a tasklet wants done on its behalf with bi-directional data
	struct SCompletionUnit;
	struct SJob
	{
		int ret;
		int err;
		PyTaskletObject* tasklet;
		long long t0;
		bool oorReported;
		long long serial;

		SJob *next; // for whatever list the job is in
		SJob *nextSequence; // for the sequence list

		// general-use params for the particular job this is. This
		// appears a bit slapdash, and it is. TODO: think of a better
		// way to do this, inheritance is out because these are pooled
		// and allocated once.
		union
		{
			SOCKET socket;
			HANDLE handle;
		};

		int param1;
		void *param1p;
		bool param1b;
		int len;
		int addrlen;
		char name1[128];
		char name2[128];
		char *name1p;
		char *name2p;
		struct addrinfo hints;
		SOCKADDR_STORAGE address;
		void* result;
	};
	SJob* m_jobFreeList;
	SJob* getJob(); // finds a free one and returns it (always suceeds, even if one must be created)
	CRITICAL_SECTION m_jobFreeLock;
	void freeJob( SJob* job );
	void releaseJob( SJob* job );

	//------------------------------
	struct SWriteOverlap : public OVERLAPPED
	{
		bool send;
		SCompletionUnit* parent;
	};

	//------------------------------
	struct SCompletionUnit : public OVERLAPPED // WARNING: this struct is memset to zero for init
	{
		bool send; // flag to indicate to the callback if this was a send [MUST ALWAYS COME FIRST IN STRUCT TO MAP TO WRITE]
		HANDLE workHandle; // IO handle associated with this completion unit
		bool wasZeroByteRead; // only done when necessary
		int consecutiveNoYieldReads; // how many times a read has completed without yielding the tasklet

		long long packetSerialNumber; // upward-counting serializer to indicate packet order (if requested)
		int threadCallSerial; // for debugging

		SOCKADDR_STORAGE from;
		int fromlen;

		SWriteOverlap writeCompletion; // OVERLAP structure for writes associated with this FD

		CRITICAL_SECTION unitLock; // control access to this completion object, with the exception of the send-pre-q

		SPacket *queuedData; // where data that is being shifted out lives.

		CRITICAL_SECTION sendLock; // sendLock specifically controls access to these three members:
		bool sendRunning;
		SPacket *rawQueuedDataHead;
		SPacket *rawQueuedDataTail;

		SslPipe *ssl;
		time_t handshakeTimeout; // when the handshake will time out

		// queue of jobs waiting on results, this is in support of the
		// original implementation which allows multiple threads to
		// post reads to the same descriptor.
		SJob *readJobsWaiting;
		SJob *readJobsWaitingTail;
		// A list of the same jobs (separate next pointers) which
		// maintains FIFO, only the GIL-locked reader thread has the
		// authority to link or unlink
		SJob *jobSequence;
		SJob *jobSequenceTail;
		// How many read jobs are still un-awoken (completely, out of the scheduler)
		long readJobsUnscheduled;

		int compressionThreshold; // if zero, use global value
		int blockingSend;
		SJob *jobsToWakeOnSend; // list of jobs waiting for this handle to finish sending
		unsigned int queuedBytes; // running count of how many bytes are queued for fast overflow checking
		WSABUF wsaBuf; // container for the outgoing pointers (since stackless will swap out the stack)

		char buf[c_systemPageSize]; // receive buffer

		unsigned int maxPacketSize;
		unsigned int queuedIncomingBytes;
		bool readTriggered;
		bool isPacketConnection; // is this connection accumulating packets or just stream data
		bool udpConnection; // if this is a udp socket its handled differently through the logic
		SPacket *packetListHead; // list of received packets
		SPacket *packetListTail; // for fast insertion

		// when true, this work unit has been closed and will be reaped
		// as soon as the completion event is processed, check before
		// attempting to use.
		bool reap;

		// when true, the library has marked this socket as dead, but
		// it has not been acknowledged by Python yet as gone, and so
		// not closed() and won't be until this ack happens, to prevent
		// re-use of the handle
		int dead;
		volatile long refCount;
#ifndef DESTROY_ON_DEC
		HANDLE refReachedZero;
#endif

		D_REFHISTORY(int refhistoryPos);
		D_REFHISTORY(int refhistory[100]);
		D_REFHISTORY(int refhistoryVal[100]);

		CioSocketStats stats;
	};
	Ccp::RWSpinlock m_completionListLock;
	Ccp::LinkHash<SCompletionUnit*> m_completionList; // hashed by work handle
	CRITICAL_SECTION m_sslEnableLock;
	Ccp::LinkHash<bool> m_sslEnabled; // which descriptors have SSL enabled

	HANDLE m_completionHandle; // the completion port

	SCallbackEntry* m_packetCallbackChainPostDecompress;
	void (*m_onTaskletScheduledCallback)(bool force);
	HANDLE *m_onTaskletScheduledEvent;
	inline void onTaskletScheduled( bool force );

	static void workerThread( void *arg );
	static void housekeepThread( void *arg );

	//------------------------------
	struct SAcceptQueue
	{
		SJob* head;
		SJob* tail;
	};
	Ccp::LinkHash<SAcceptQueue> m_acceptQueueList; // hashed by work handle
	CRITICAL_SECTION m_acceptQueueLock;
	void queueAccept( SJob* job );
	void acceptEx( SJob* job );
	void connectEx( SJob *job );
	SJob* m_wakeUpQueue;
	CRITICAL_SECTION m_wakeUpQueueLock;
	void wakeUpTaskletChain( SJob* job, bool force =false );
	inline void failAllTasklets( SJob* list, int errType =0 );
	static inline SJob* getWakeupList( SCompletionUnit* completion );
	inline void spinUntilNoReadJobsScheduled( SCompletionUnit* completion );

	struct STimerTrackParam
	{
		HANDLE timer;
		SCompletionUnit* completion;
		SJob* job;
	};
	static VOID WINAPI timerRoutine( PVOID param, unsigned char timerOrWaitFired );
	int recvEx( SOCKET fd, char *buf, PyObject **obj, bool includeOOB, int len, int flags, struct sockaddr* from, int* fromlen, float timeout );
	SCompletionUnit* addToCio( HANDLE fd );
	void installPacketizer( SCompletionUnit* completion );
	bool prepareSequence( PyObject *obj, SPacket *packet );
	static int triggerRead( SCompletionUnit* completion );
	inline bool blockCurrentTasklet( SJob *job );
	static inline void linkReadJob( SCompletionUnit* completion, SJob *job );
	static inline void linkSequenceJob( SCompletionUnit* completion, SJob *job );
	static inline void unlinkSequenceJob( SCompletionUnit* completion, SJob *job );
	static inline SJob* popHeadReadJob( SCompletionUnit* completion );
	static inline SPacket* popHeadPacket( SCompletionUnit* completion, int len );
	void dataReceived( SCompletionUnit *completion, const char* data, const unsigned int len );
	static inline bool isPacketValid( SPacket *packet );

	static void reverseJobList( SJob* &job );
	static int wakeUpTaskletsEx( void* arg );
	inline bool wakeupEx( SJob* job );
	static inline void decCompletionRefEx( SCompletionUnit* completion, int location );
	static inline void incCompletionRefEx( SCompletionUnit* completion, int location );

	void destroyCompletionUnit( SCompletionUnit *completion );
	inline void deleteCompletionUnit( SCompletionUnit* completion );
	bool processRawQueuedData( SCompletionUnit *completion );
	void setPyError( const char* msg, int err =0 ); // if zero is passed, WSAGetLastError() is substituted
	static long long hashPointer( long long value ) { return ((value >> 3) | (value<<61)); } // pointers are unique but aligned
	bool checkAndCompress( const char* data, const unsigned int len, char **outbuf, unsigned int *outlen );
	void checkForStaleSSLHandshakes();
	void handleRead( int size, SCompletionUnit *completion );
	void handleUDPRead( int size, SCompletionUnit *completion );
	void handleSendCompletion( SWriteOverlap *write );
	inline bool blockOnSend( SCompletionUnit* completion );
	bool compress( const char *in, unsigned int inLen, char **out, unsigned int *outLen );
	bool decompress( SPacket *packet );
	inline long long getPerformanceCounter();
	bool okayToTriggerRead( SCompletionUnit* completion ) { return (!c_incomingQueueHaltLevel || (completion->queuedIncomingBytes < c_incomingQueueHaltLevel)); }
	inline void startSendRunning( SCompletionUnit* completion );
	//--------------------------------------
	// helper class for timing contexts
	class CPerformanceTime
	{
	public:
		CPerformanceTime( const char* label ) : m_label(label) { m_timeIn = m_singleton.getPerformanceCounter(); }
		~CPerformanceTime() { if (m_timeIn) m_singleton.m_events.inc( m_label, m_singleton.getPerformanceCounter() - m_timeIn ); }
	private:
		long long m_timeIn;
		const char *m_label;
	};

	static CarbonIO m_singleton;
	CioStats m_stats;
	CioEventTracker m_events;
	bool m_running;

	// autoritative copies of the stats
	volatile long m_spawnedWorkerThreads;
	volatile long m_busyWorkerThreads;
	volatile long m_outstandingJobs;
	volatile long m_jobPoolSize;
	volatile long m_packetsAllocated;
	volatile long m_packetFreeListSize;
	volatile long m_blockedThreads;
	long long m_gilAcquireTimeTemp;
	unsigned int m_compressionThreshold; // smallest packet to attempt compression on
	unsigned int m_compressionMinRatio; // 0-100 compression ratio outgoing packets must be below to be sent compressed
	unsigned int m_compressionLevel; // what zlib level is used for compresison
	unsigned int m_compressionType; // how it will be compressed (supported: zlib and snappy)
	time_t m_SSLHandshakeNegotiationSeconds; // how many seconds can elapse before a connection is considered dead
	bool m_enableTimers; // allow the performance counter to be hit. a lot.

	CRITICAL_SECTION m_staleSSLHandshakeLock;
	time_t m_lastStaleCheck;

	int m_wakeupMethod;

	static void emitErrorMessage( const char* format, ... );
	static void emitStatusMessage( const char* format, ... );
	void (*m_errorMessageCallback)( const char* msg );
	void (*m_statusMessageCallback)( const char* msg );
};

//------------------------------------------------------------------------------
PyObject* CarbonIO::recv( SOCKET fd, int len, int flags, float timeout )
{
	PyObject *obj = 0;
	recvEx( fd, 0, &obj, false, len, flags, 0, 0, timeout );
	return obj;
}

//------------------------------------------------------------------------------
int CarbonIO::recvInto( SOCKET fd, char *buf, int len, int flags )
{
	return recvEx( fd, buf, 0, false, len, flags, 0, 0, 0 );
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::recvPacket( SOCKET fd, bool withOOB )
{
	PyObject *obj;
	int len = recvEx( fd, 0, &obj, withOOB, -1, 0, 0, 0, 0 );
	return len >= 0 ? obj : NULL;
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::recvFrom( SOCKET fd, int len, struct sockaddr* from, int* fromlen )
{
	if ( !from || !fromlen )
	{
		return 0; //Todo: wrong, need pyerror
	}

	PyObject *obj;
	int res = recvEx( fd, 0, &obj, false, len, 0, from, fromlen, 0 );
	return res >= 0 ? obj : NULL;
}

//------------------------------------------------------------------------------
int CarbonIO::recvFromInto( SOCKET fd, char *buf, int len, struct sockaddr* from, int* fromlen )
{
	return recvEx( fd, buf, false, 0, len, 0, from, fromlen, 0 );
}

//------------------------------------------------------------------------------
bool CarbonIO::externalSendPacket( SOCKET fd, const char* data, const unsigned int len, const char* OOBData, const unsigned int OOBLen )
{
	SPacket *packet = createPacketFromOutgoingData( data, len, OOBData, OOBLen );
	sendPacketEx( fd, packet, false );
	return true;
}

//------------------------------------------------------------------------------
bool CarbonIO::externalSendFormattedPacket( const SOCKET fd, const char* data, const unsigned int len )
{
	if ( !data || !len )
	{
		return true;
	}

	SPacket* packet = getPacket();
	appendDataToPacket( packet, data, len );
	packet->streamSend = true; // presumed to be in wire format
	return sendPacketEx( fd, packet, false );
}

//------------------------------------------------------------------------------
bool CarbonIO::formatPacket( char* buf,
							 unsigned int* len,
							 const char* data,
							 const unsigned int dataLen,
							 const char* OOBData,
							 const unsigned int OOBLen,
							 bool suppressCompression )
{
	if ( !buf || !data || !len )
	{
		return false;
	}

	int pos;
	if ( OOBData && OOBLen )
	{
		*(unsigned int *)buf = (dataLen + OOBLen + sizeof(unsigned int)) | ceHeaderExpectPayloadOffset;
		*(unsigned int *)(buf + sizeof(unsigned int)) = OOBLen;
		memcpy( buf + sizeof(unsigned int)*2, OOBData, OOBLen );
		pos = OOBLen + sizeof(unsigned int)*2;
	}
	else
	{
		*(unsigned int *)buf = dataLen;
		pos = sizeof(unsigned int);
	}

	char *outbuf = 0;
	unsigned int outlen;
	if ( !suppressCompression
		 && dataLen >= m_compressionThreshold
		 && checkAndCompress(data, dataLen, &outbuf, &outlen) )
	{
		*len = pos + outlen;
		*(unsigned int *)buf &= ceHeaderBitsMask; // knock off old size
		*(unsigned int *)buf |= *len - sizeof(unsigned int); // plug in NEW size
		*(unsigned int *)buf |= m_compressionType;
		memcpy( buf + pos, outbuf, outlen );
		delete[] outbuf;
	}
	else
	{
		memcpy( buf + pos, data, dataLen );
		*len = dataLen + pos;
	}

	return true;
}

//------------------------------------------------------------------------------
CarbonIO::SJob* CarbonIO::getWakeupList( SCompletionUnit *completion )
{
	SJob *list = completion->readJobsWaiting;
	if ( list )
	{
		for( SJob* link = list ; link ; link = link->next )
		{
			linkSequenceJob( completion, link );
			completion->readJobsUnscheduled++;
		}

		completion->readJobsWaitingTail->next = completion->jobsToWakeOnSend;
	}
	else
	{
		list = completion->jobsToWakeOnSend;
	}

	completion->jobsToWakeOnSend = 0;
	completion->readJobsWaiting = 0;
	completion->readJobsWaitingTail = 0;

	return list;
}

//------------------------------------------------------------------------------
void CarbonIO::spinUntilNoReadJobsScheduled( SCompletionUnit* completion )
{
	// make sure if there are any other tasklets with legitimate read
	// traffic they are delivered before the (usually bad) news of this
	// one is returned

	m_stats.spunWaitingForReadJobsScheduled++;

	int tries = 500;
	while( completion->readJobsUnscheduled && --tries )
	{
		PyObject* ret = PyStackless_Schedule( Py_None, 0 );
		Py_XDECREF( ret );
	}

	if ( !tries )
	{
		m_stats.noReadJobsScheduledGaveUp++;
	}
}

//------------------------------------------------------------------------------
void CarbonIO::ensurePacketSpace( SPacket *packet, const unsigned int len )
{
	if ( packet->bufferLen < len )
	{
		char *old = packet->data;
		packet->data = new(std::nothrow) char[ len ];
		if ( packet->packetLen ) // if there is a valid packet in here then copy it to the new buffer
		{
			memcpy( packet->data, old, packet->packetLen );
		}
		delete[] old;
		packet->bufferLen = len;
	}
}

//------------------------------------------------------------------------------
CarbonIO::SPacket* CarbonIO::createPacketFromOutgoingData( const char* data, const unsigned int len, const char* OOBData, const unsigned int OOBLen )
{
	SPacket *packet = getPacket();
	ensurePacketSpace( packet, getMaxPacketSize(len, OOBLen) );
	formatPacket( packet->data, &packet->packetLen, data, len, OOBData, OOBLen, true );
	return packet;
}

//------------------------------------------------------------------------------
void CarbonIO::onTaskletScheduled( bool force )
{
	if ( m_onTaskletScheduledEvent )
	{
		D_ONTSCHED(ciolog("onTaskletScheduled: hitting event" ));
		SetEvent( *m_onTaskletScheduledEvent );
	}
	if ( m_onTaskletScheduledCallback )
	{
		D_ONTSCHED(ciolog("onTaskletScheduled: callback [%s]", force ? "true":"false" ));
		m_onTaskletScheduledCallback(force);
	}
}

//------------------------------------------------------------------------------
void CarbonIO::failAllTasklets( SJob *list, int errType /*=0*/ )
{
	// wake up any waiting taklets with a failed return value
	SJob *L = list;
	while( L )
	{
		D_FAILALL(ciolog("failing tasklet [%p] on w/[%d]", L->tasklet, errType ));

		SJob *next = L->next;
		L->ret = WSA_INVALID_HANDLE;
		L->err = errType;
		L->param1 = 0;
		L->param1p = 0;
		L = next;
	}

	wakeUpTaskletChain( list );
}

//------------------------------------------------------------------------------
bool CarbonIO::blockCurrentTasklet( SJob *job )
{
	// put the tasklet in here for CarbonIO to wake us later
	job->tasklet = (PyTaskletObject *)PyStackless_GetCurrent();
	PyObject* ret = PyStackless_Schedule( Py_None, 1 );
	// now I have woken up
	Py_XDECREF( ret );
	m_singleton.m_events.inc( "request::Ready::OK", m_singleton.getPerformanceCounter() - job->t0 );

	// At this point, job->tasklet is either NULL, meaning that I own the job,
	// or it is non-zero, meaning that CarbonIO didn't wake us up and it will reap
	// the job in due course.
	return ret != NULL;
}

//------------------------------------------------------------------------------
void CarbonIO::linkReadJob( SCompletionUnit* completion, SJob *job )
{
	// link into the read job queue, shared with the work threads
	// (which has the authority and requirement of popping them off)
	job->next = 0;
	if ( completion->readJobsWaitingTail )
	{
		completion->readJobsWaitingTail->next = job;
		completion->readJobsWaitingTail = job;
	}
	else
	{
		completion->readJobsWaitingTail = job;
		completion->readJobsWaiting = job;
	}
}

//------------------------------------------------------------------------------
void CarbonIO::linkSequenceJob( SCompletionUnit* completion, SJob *job )
{
	D_JOBFIFO(ciolog("linking [%p] into [%d]", job, (int)completion->workHandle));

	// link into a FIFO which is used only by the GIL-locked reader
	// thread to ensure sequencing
	// TODO:  This should work with some packet-sequence number to ensure
	// we wake up in the order of incoming data.
	job->nextSequence = 0;
	if ( completion->jobSequenceTail )
	{
		completion->jobSequenceTail->nextSequence = job;
		completion->jobSequenceTail = job;
	}
	else
	{
		completion->jobSequenceTail = job;
		completion->jobSequence = job;
	}
}

//------------------------------------------------------------------------------
void CarbonIO::unlinkSequenceJob( SCompletionUnit* completion, SJob *job )
{
	D_JOBFIFO(ciolog("unlinking [%p] from [%d]", job, (int)completion->workHandle));

	// this link should always be first in normal operations, but could be anywhere
	SJob *prev = 0;
	for( SJob *J = completion->jobSequence; J; J=J->nextSequence )
	{
		if ( J == job )
		{
			if ( prev )
			{
				prev->nextSequence = J->nextSequence; // in the middle of the list
			}
			else
			{
				completion->jobSequence = J->nextSequence; // head of the list
			}

			if ( completion->jobSequenceTail == J )
			{
				completion->jobSequenceTail = prev; // we were the tail, new tail is the prev link
			}

			break;
		}

		prev = J;
	}
}

//------------------------------------------------------------------------------
// this function assumes completion->readJobsWaiting is valid!!
CarbonIO::SJob* CarbonIO::popHeadReadJob( SCompletionUnit* completion )
{
	SJob *job = completion->readJobsWaiting;

	if ( job->next )
	{
		// new head (tail is still the tail)
		completion->readJobsWaiting = job->next;
	}
	else
	{
		// empty the list
		completion->readJobsWaitingTail = 0;
		completion->readJobsWaiting = 0;
	}

	job->next = 0; // make sure nothign is linked after this job

	return job;
}

//------------------------------------------------------------------------------
bool CarbonIO::isPacketValid( SPacket *packet )
{
	return packet
			&& packet->packetLen >= 4
			&& (packet->auxData
				|| (packet->packetLen == (*(int *)packet->data & ceHeaderSizeMask) + sizeof(int)));
}

//------------------------------------------------------------------------------
CarbonIO::SPacket* CarbonIO::popHeadPacket( SCompletionUnit* completion, int len )
{
	SPacket* ret = 0;
	if ( completion->isPacketConnection ) // ignore len, we know we want a full packet
	{
		if ( isPacketValid(completion->packetListHead) )
		{
			ret = completion->packetListHead;
			if ( completion->packetListHead->next )
			{
				completion->packetListHead = completion->packetListHead->next;
			}
			else
			{
				completion->packetListHead = 0;
				completion->packetListTail = 0;
			}

			completion->queuedIncomingBytes -= ret->packetLen;
			ret->next = 0;
		}
	}
	else if ( len != -1 ) // -1 is undefined here
	{
		// by definition the head must equal tail since a
		// non-packetized stream collects data in one place.

		if ( completion->packetListHead )
		{
			if ( (int)completion->packetListHead->packetLen > len )
			{
				D_POPHEAD(ciolog("popping LESS THAN want[%d] have[%d] ->[%p]", len, completion->packetListHead->packetLen, completion->packetListHead->next));

				// return the head packet but truncate the size, copy in
				// the leftover data to a NEW packet, inserted at the head
				ret = completion->packetListHead;

				completion->packetListHead = m_singleton.getPacket();
				completion->packetListHead->next = ret->next;
				completion->packetListTail = completion->packetListHead;
				appendDataToPacket( completion->packetListHead, ret->data + len, ret->packetLen - len );
				ret->packetLen = len;
			}
			else
			{
				D_POPHEAD(ciolog("popping want[%d] have[%d] ->[%p]", len, completion->packetListHead->packetLen, completion->packetListHead->next));

				// otherwise we have less tshan or equal to the amount of
				// data requested, return it
				ret = completion->packetListHead;
				completion->packetListHead = 0;
				completion->packetListTail = 0;
			}

			ret->next = 0;
		}
	}

	return ret;
}

//------------------------------------------------------------------------------
bool CarbonIO::wakeupEx( SJob* job )
{
	bool result = false;
	if ( job->tasklet )
	{
		// I wake up this tasklet, and clear the pointer.  The tasklet
		// will see this cleared pointer and know that it was woken by CarbonIO
		// and that it owns the job, and should reap it itself.
		PyTaskletObject *tasklet = job->tasklet;
		job->tasklet = NULL;
		int fail = PyTasklet_Insert(tasklet);
		Py_DECREF( tasklet );
		if (fail)
		{
			PyObject *msg = PyString_FromString("wakeupEx");
			if ( !msg ) // just in case
			{
				msg = Py_None;
				Py_INCREF(msg);
			}
			PyErr_WriteUnraisable(msg);
			Py_XDECREF(msg);
		}
		else
		{
			result = true; // a successful wakeup was done.
		}
		m_singleton.m_events.inc( "event::dispatch", m_singleton.getPerformanceCounter() - job->t0 );
		if ( areTimersEnabled() )
		{
			m_singleton.m_events.incval( "event::queuelen", (float)PyStackless_GetRunCountTs(slp_initial_tstate) );
		}
	}
	else
	{
		// The tasklet woke up on its own, being scheduled by someone else.
		// It will have noticed job->tasklet being non-zero and cleared it.
		// It also knows that CarbonIO didn't wake it up and therefore it couldn't
		// clear the job.  This is where CarbonIO reaps the job instead.
		freeJob( job );
	}
	return result;
}

//------------------------------------------------------------------------------
void CarbonIO::decCompletionRefEx( SCompletionUnit* completion, int location )
{
	if ( completion->refCount <= 0 )
	{
		ciolog("Tried to dec refcount [%d] below zero!", (int)completion->workHandle);
	}
	else if ( !InterlockedDecrement(&completion->refCount) )
	{
		D_REFHISTORY(completion->refhistoryVal[completion->refhistoryPos] = completion->refCount);
		D_REFHISTORY(completion->refhistory[completion->refhistoryPos++] = -location);
		D_REFHISTORY(if (completion->refhistoryPos >= 100) completion->refhistoryPos = 0);
		D_REF(ciolog("[%d] <decref [%d] now [%d]", location, (int)completion->workHandle, completion->refCount));

#ifdef DESTROY_ON_DEC
		m_singleton.deleteCompletionUnit( completion );
#else
		SetEvent( completion->refReachedZero );
#endif
	}
	else
	{
		D_REFHISTORY(completion->refhistoryVal[completion->refhistoryPos] = completion->refCount);
		D_REFHISTORY(completion->refhistory[completion->refhistoryPos++] = -location);
		D_REFHISTORY(if (completion->refhistoryPos >= 100) completion->refhistoryPos = 0);
		D_REF(ciolog("[%d] <decref [%d] now [%d]", location, (int)completion->workHandle, completion->refCount));
	}
}

//------------------------------------------------------------------------------
void CarbonIO::deleteCompletionUnit( SCompletionUnit* completion )
{
	// I was the last reference, finish cleanup
	while( completion->packetListHead )
	{
		// this should be impossible
		SPacket *next = completion->packetListHead->next;
		freePacket( completion->packetListHead );
		completion->packetListHead = next;
	}

	while( completion->queuedData )
	{
		// this should also be impossible
		SPacket *next = completion->queuedData->next;
		freePacket( completion->queuedData );
		completion->queuedData = next;
	}

	delete completion->ssl;
	DeleteCriticalSection( &completion->unitLock );
	DeleteCriticalSection( &completion->sendLock );
#ifndef DESTROY_ON_DEC
	CloseHandle( completion->refReachedZero );
#endif
	emitStatusMessage( "[%d] destroyed", (int)completion->workHandle );

	delete completion;
}

//------------------------------------------------------------------------------
void CarbonIO::incCompletionRefEx( SCompletionUnit* completion, int location )
{
	InterlockedIncrement( &completion->refCount );

	D_REFHISTORY(completion->refhistoryVal[completion->refhistoryPos] = completion->refCount);
	D_REFHISTORY(completion->refhistory[completion->refhistoryPos++] = location);
	D_REFHISTORY(if (completion->refhistoryPos >= 100) completion->refhistoryPos = 0);
	D_REF(ciolog("[%d] >incref [%d] now [%d]", location, (int)completion->workHandle, completion->refCount));
}

//------------------------------------------------------------------------------
bool CarbonIO::blockOnSend( SCompletionUnit* completion )
{
	// spin a tasklet that needs to wait on a send completion.

	if ( !completion->sendRunning ) // trivial
	{
		return true;
	}

	Ccp::CriticalLockScoped ulock( completion->unitLock );

	SJob *job = getJob();

	if ( !completion->sendRunning ) // one last check, these sends are asyc and FAST, good chance its done
	{
		freeJob( job );
		return true;
	}

	job->next = completion->jobsToWakeOnSend;
	completion->jobsToWakeOnSend = job;

	ulock.release();

	bool result = blockCurrentTasklet( job );
	releaseJob( job );
	return result;
}

//------------------------------------------------------------------------------
long long CarbonIO::getPerformanceCounter()
{
	if ( !m_enableTimers )
	{
		return 0;
	}

	LARGE_INTEGER i;
	QueryPerformanceCounter(&i);
	return i.QuadPart;
}

//------------------------------------------------------------------------------
void CarbonIO::startSendRunning( SCompletionUnit *completion )
{
	if ( completion->ssl && !completion->ssl->handshakeComplete() )
	{
		// can't start a send while handshake is negotiating. (It will
		// automatically start once the handshake is complete)
		return;
	}

	if ( !completion->sendRunning )
	{
		completion->sendRunning = true;
		INC_COMPLETION_REF( completion );
		PostQueuedCompletionStatus( m_completionHandle, 0, 0, &(completion->writeCompletion) );
	}
}

#endif
