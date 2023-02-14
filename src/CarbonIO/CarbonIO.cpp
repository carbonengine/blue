/*************************************************************************

CarbonIO.cpp

Author:    Curt Hartung
Created:   Jun 2010
OS:        Win32
Project:   Carbon IO

Description: implementation of IO Completion Ports for stackless

(c) CCP 2010

***************************************************************************/
#include "StdAfx.h"
/* require win xp api level before including winsock2 and windows */
#include "CarbonIO.h"

#include <time.h>
#include <process.h>
#include <zlib.h>

#include "ssl_pipe.h"
#include "slsocketmodule.h"

#include <SimpleLog.h>

CarbonIO CarbonIO::m_singleton; // singleton

Ccp::SimpleLog g_log;

#ifdef DISABLE_CIO_OPTIMIZATIONS
#pragma optimize( "",  off )
#endif

//------------------------------------------------------------------------------
#include <stdarg.h>
static CRITICAL_SECTION m_logLock;
extern "C" void ciolog( const char* format, ... )
{
#ifdef DEBUG_LOG
	EnterCriticalSection( &m_logLock );
	
	char temp[64000];
	
	if ( !format || !format[0] )
	{
		return;
	}

	va_list arg;
	va_start( arg, format );
	int len = vsprintf( temp, format, arg );
	va_end ( arg );

	g_log.out( "%s", temp );

	LeaveCriticalSection( &m_logLock );
#endif
}

extern "C" void socket_set_extra_error_info( PyObject *o );
extern "C" PyObject* socket_set_error( int error );
//------------------------------------------------------------------------------
void CarbonIO::setPyError( const char* msg, int err /*=0*/ )
{
	socket_set_extra_error_info( PyString_FromString(msg) );
	socket_set_error( err ? err : WSAGetLastError() );
	if ( err )
	{
		WSASetLastError( err );
	}
//	ciolog( "PyErr: %s:%d", msg, err );
}

//------------------------------------------------------------------------------
static void CarbonIOLoopBackLog( const char* msg )
{
	ciolog( "%s", msg );
}

//------------------------------------------------------------------------------
CarbonIO::CarbonIO() :
	m_completionListLock( "CarbonIO", "m_completionListLock" )
{
#ifdef DEBUG_LOG
	char buf[256];
	sprintf( buf, "s:\\dev\\%d_log.txt", _getpid() );
//	sprintf( buf, "%d_log.txt", _getpid() );
	g_log.init( buf );
	printf("logging as [%s]\n", buf );

	InitializeCriticalSection( &m_logLock );
	m_errorMessageCallback = CarbonIOLoopBackLog;
	
	#ifdef DEBUG_EMIT_LOG
	m_statusMessageCallback = CarbonIOLoopBackLog;
	#endif
	
#else
	m_errorMessageCallback = 0;
	m_statusMessageCallback = 0;
#endif

	SslPipe::initSSL( D_SSL(true) ); // default is false, override if debugging

	m_running = true;

	m_onTaskletScheduledCallback = 0;
	m_onTaskletScheduledEvent = 0;

	m_compressionThreshold = c_defaultCompressionThreshold;
	m_compressionMinRatio = c_defaultMinRatio;
	m_compressionLevel = c_defaultCompressionLevel;
	m_compressionType = ceHeaderBitZlibCompressed;
	m_SSLHandshakeNegotiationSeconds = c_defaultSSLHandshakeNegotiationTimeSeconds;
		
	m_completionHandle = INVALID_HANDLE_VALUE;
	m_jobFreeList = 0;
	
	m_wakeUpQueue = 0;

	InitializeCriticalSection( &m_packetFreeListLock );
	InitializeCriticalSection( &m_wakeUpQueueLock );
	InitializeCriticalSection( &m_sslEnableLock );
	InitializeCriticalSection( &m_jobFreeLock );
	InitializeCriticalSection( &m_acceptQueueLock );
	InitializeCriticalSection( &m_staleSSLHandshakeLock );

	m_lastStaleCheck = 0;

	m_spawnedWorkerThreads = 0;
	m_busyWorkerThreads = 0;
	m_outstandingJobs = 0;
	m_jobPoolSize = 0;
	m_packetsAllocated = 0;
	m_packetFreeListSize = 0;
	m_blockedThreads = 0;

	m_enableTimers = false;

	m_packetCallbackChainPostDecompress = 0;

	m_wakeupMethod = WAKEUP_PENDING_CALL; // default to what TQ is currently using
}

//------------------------------------------------------------------------------
CarbonIO::~CarbonIO()
{
	m_running = false;

	// this is a singleton class which lives for as long as the app
	// does, there is no well-defined way to shut it down because of
	// all the shared references, threading and pooling. Just mark
	// things as dead and let the system recover the resources.

	DeleteCriticalSection( &m_packetFreeListLock );
	DeleteCriticalSection( &m_wakeUpQueueLock );
	DeleteCriticalSection( &m_sslEnableLock );
	DeleteCriticalSection( &m_jobFreeLock );
	DeleteCriticalSection( &m_acceptQueueLock );
 	DeleteCriticalSection( &m_staleSSLHandshakeLock );

	CloseHandle( m_completionHandle );

	for( SCompletionUnit **pcompletion = m_completionList.getFirst() ; pcompletion ; pcompletion = m_completionList.getNext() )
	{
		SCompletionUnit *completion = *pcompletion;
		if ( !completion->reap )
		{
			completion->reap = true;
			completion->dead = WSAENOTSOCK;
			::closesocket( (SOCKET)completion->workHandle ); // we are dying, just get out!
		}
	}

	SslPipe::shutdownSSL();

#ifdef DEBUG_LOG
	DeleteCriticalSection( &m_logLock );
#endif
}

//------------------------------------------------------------------------------
bool CarbonIO::init()
{
	if ( m_completionHandle != INVALID_HANDLE_VALUE ) // already been here?
	{
		return true;
	}

	memset( &m_stats, 0, sizeof(CioStats) );

	PyEval_InitThreads(); 
	
	// create the pool
	m_completionHandle = CreateIoCompletionPort( INVALID_HANDLE_VALUE, 0, 0, c_concurrentCioThreads ); 
	if ( m_completionHandle == INVALID_HANDLE_VALUE )
	{
		return false;
	}

	// start with two, more will spawn themselves as work becomes available
	m_singleton.m_spawnedWorkerThreads = 1;
	m_singleton.m_stats.workerThreads = 1;
	_beginthread( workerThread, 0, 0 );

	return true;
}

//------------------------------------------------------------------------------
bool CarbonIO::isSocketValid( SOCKET fd )
{
	Ccp::RWSpinlockReadScoped rlock( m_completionListLock );
	SCompletionUnit *completion = m_completionList.getItem( fd );
	bool ret = completion && !completion->dead && !completion->reap;
	return ret;	
}

//------------------------------------------------------------------------------
SOCKET CarbonIO::socket( int family, int type, int proto )
{
	SOCKET k = WSASocketW( family, type, proto, NULL, 0, WSA_FLAG_OVERLAPPED );

	// any special setup required for a connectionless/udp/datagram socket?
	if ( type == SOCK_DGRAM )
	{
		SCompletionUnit *completion = addToCio( (HANDLE)k );
		if ( completion )
		{
			completion->udpConnection = true;
		}
	}

	return k;
}

//------------------------------------------------------------------------------
int CarbonIO::connect( SOCKET fd, sock_addr_t *addr, int addrlen, bool &timeoutFlag, float timeout )
{
	if ( fd == INVALID_SOCKET )
	{
		setPyError( "tried to connect an invalid socket" );
		return -1;
	}

	SJob *job = getJob();
	job->socket = fd;
	job->addrlen = (int)std::min((unsigned long long)addrlen, sizeof(job->address));
	memcpy( &job->address, &(addr->in), job->addrlen );
	job->ret = -1;
	job->param1 = (int)(timeout > 0 ? timeout : 0) * 1000000;
	job->param1b = false;

	D_CONNECT(ciolog( "Connecting to [%s]:[%d]", inet_ntoa(*(struct in_addr *)&(addr->in.sin_addr)), ntohs(addr->in.sin_port) ));

	PostQueuedCompletionStatus( m_completionHandle, ceJobConnect, (ULONG_PTR)job, 0 );
	if ( !blockCurrentTasklet( job ) )
	{
		releaseJob( job );
		return -1;
	}

	int ret = job->ret;

	timeoutFlag = job->param1b;
	if ( timeoutFlag )
	{
		ret = WSAEWOULDBLOCK; // for connect_ex
	}
	else if ( ret != 0  )
	{
		setPyError( "connect operation system fail", ret );
		ret = -1;
	}

	releaseJob( job );
	return ret;
}

//------------------------------------------------------------------------------
SOCKET CarbonIO::accept( SOCKET fd, struct sockaddr *addr, int *addrlen, int* timeoutFlag, float timeout )
{
	SJob *job = getJob();
	job->socket = fd;
	job->param1 = (int)(timeout > 0 ? timeout : 0) * 1000000;
	
	D_ACCEPT(ciolog("entering accept for [%d]", (int)fd));

	PostQueuedCompletionStatus( m_completionHandle, ceJobAccept, (ULONG_PTR)job, 0 );

	if ( timeoutFlag )
	{
		*timeoutFlag = 0; // default to non-timed out
	}
	
	SOCKET ret = INVALID_SOCKET;
	if ( blockCurrentTasklet(job) )
	{
		ret = job->socket;
		if ( ret == INVALID_SOCKET )
		{
			if ( job->param1 == 1 )
			{
				if ( timeoutFlag )
				{
					*timeoutFlag = 1;
				}
			}
			else
			{
				setPyError( "accept fail", job->ret );
			}
		}
		else
		{
			if ( addr && addrlen )
			{
				*addrlen = std::min(*addrlen, job->addrlen);
				memcpy( addr, &job->address, *addrlen );
			}

			emitStatusMessage( "accept returned socket[%d] for[%d]", (int)job->socket, (int)fd );
		}
	}
	else
	{
		D_ACCEPT(ciolog("block failed for[%d]", (int)fd));
		ret = INVALID_SOCKET;
	}
	
	releaseJob( job );
	return ret;
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::formatStats( SOCKET fd )
{
	Ccp::RWSpinlockReadScoped rlock( m_completionListLock );

	SCompletionUnit *completion = m_completionList.getItem( (long long)fd );

	PyObject *obj = 0;
	if ( completion )
	{
		obj = Py_BuildValue("{sL sL sL sL si si}",
							"BytesReceived", completion->stats.bytesReceived,
							"BytesReceivedDecompressed", completion->stats.bytesReceivedDecompressed,
							"BytesSent", completion->stats.bytesSent,
							"BytesSentCompressed", completion->stats.bytesSentCompressed,
							"PacketsReceived", completion->stats.packetsReceived,
							"PacketsSent", completion->stats.packetsSent );
	}
	else
	{
		obj = Py_BuildValue("{sL "
							"sL "
							"sL "
							"sL "
							"si " // 5
							"si "
							"si " 
							"si "
							"si "
							"si " // 10
							"si "
							"si "
							"si "
							"si "
							"si " // 15
							"si "
							"sL "
							"si "
							"si "
							"si " // 20
							"si "
							"si "
							"sL "
							"sL "
							"sL " // 25
							"sL "
							"si "
							"si "
							"si "
							"si " // 30
							"si "
							"si "
							"si}",
							"BytesReceived", m_stats.bytesReceived,
							"BytesReceivedDecompressed", m_stats.bytesReceivedDecompressed,
							"BytesSent", m_stats.bytesSent,
							"BytesSentCompressed", m_stats.bytesSentCompressed,
							"PacketsReceived", m_stats.packetsReceived, // 5
							"PacketsSent", m_stats.packetsSent,
							"ActiveSockets", m_stats.activeSockets,
							"TotalSockets", m_stats.totalSockets,
							"Accepts", m_stats.accepts,
							"Connects", m_stats.connects, // 10
							"BusyThreads", m_stats.busyThreads - m_stats.blockedThreads,
							"WorkerThreads", m_stats.workerThreads,
							"OutstandingJobs", m_stats.outstandingJobs,
							"JobPoolSize", m_stats.jobPoolSize,
							"GILAcquiredNum", m_stats.GILAcquiredNum, // 15
							"GILCoalesced", m_stats.GILCoalesced,
							"GILAcquireTime", m_stats.GILAcquireTime,
							"LongestGILAcquireTime", m_stats.longestGILAcquireTime,
							"ZeroByteReadsRequired", m_stats.zeroByteReadsRequired,
							"PacketsAllocated", m_stats.packetsAllocated, // 20
							"PacketFreeListSize", m_stats.packetFreeListSize,
							"BlockedThreads", m_stats.blockedThreads,
							"CompressedBytesAccepted", m_stats.compressedAccepted,
							"CompressedBytesRejected", m_stats.compressedRejected,
							"AverageCompressionRatioAccepted", m_stats.compressedAcceptedIn ? (m_stats.compressedAccepted * 100LL) / m_stats.compressedAcceptedIn : 0, // 25
							"AverageCompressionRatioRejected", m_stats.compressedRejectedIn ? (m_stats.compressedRejected * 100LL) / m_stats.compressedRejectedIn : 0,
							"OutOfOrderReceived", m_stats.oorRecieved,
							"OutOfOrderGaveUp", m_stats.oorGaveUp,
							"ShutdownSpinWaitCount", m_stats.shutdownSpinWaitCount,
							"PendingCallQueueFailure", m_stats.pendingCallQueueFailure, // 30
							"WakeupMethod", m_wakeupMethod,
							"noReadJobsScheduledGaveUp", m_stats.noReadJobsScheduledGaveUp,
							"spunWaitingForReadJobsScheduled", m_stats.spunWaitingForReadJobsScheduled
						   );
	}
		
	return obj;
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::getTimerStats()
{
	if ( !m_events.m_eventList.count() )
	{
		Py_RETURN_NONE;
	}
	
	LARGE_INTEGER pf;
	QueryPerformanceFrequency( &pf );
	double factor = 1.0 / pf.QuadPart;
	PyObject *list( PyList_New(0) );

	if ( !list )
	{
		Py_RETURN_NONE;
	}
	
	PyObject *entry;
	Ccp::CriticalLockScoped lock( m_events.m_listLock );
	for( CioEventTracker::SEntry *event = m_events.m_eventList.getFirst() ;
		 event ;
		 event = m_events.m_eventList.getNext() )
	{
		if (event->type == 0)
		{
			entry = Py_BuildValue( "siiffff",
							   (const char*)event->name,
							   (int)event->type,
							   event->count,
							   (double)(event->t.time * factor),
							   (double)(event->t.timeSmoothed * factor),
							   (double)(event->t.varSmoothed * factor * factor),
							   (double)(event->t.thirdSmoothed * factor * factor * factor) );
		}
		else
		{
			entry = Py_BuildValue( "siiffff",
							   (const char*)event->name,
							   (int)event->type,
							   event->count,
							   (double)(event->f.val),
							   (double)(event->f.valSmoothed),
							   (double)(event->f.varSmoothed),
							   (double)(event->f.thirdSmoothed) );
		}
		if ( !entry || PyList_Append(list, entry) )
		{
			Py_XDECREF(entry);
			Py_DECREF(list);
			return NULL;
		}
		Py_DECREF(entry);
	}

	return list;
}

//------------------------------------------------------------------------------
void CarbonIO::addPacketCallbackPostDecompress( CioDataCallback packetCallback )
{
	SCallbackEntry *entry = new(std::nothrow) SCallbackEntry;
	entry->callback = packetCallback;
	entry->next = m_packetCallbackChainPostDecompress;
	m_packetCallbackChainPostDecompress = entry;
}

//------------------------------------------------------------------------------
void CarbonIO::removePacketCallbackPostDecompress( CioDataCallback packetCallback )
{
	SCallbackEntry *prev = 0;
	for ( SCallbackEntry *entry = m_packetCallbackChainPostDecompress; entry; entry = entry->next )
	{
		if ( entry->callback == packetCallback )
		{
			if ( prev )
			{
				prev->next = entry->next;
			}
			else
			{
				m_packetCallbackChainPostDecompress = entry->next;
			}

			delete entry;
			break;
		}

		prev = entry;
	}
}

//------------------------------------------------------------------------------
int CarbonIO::close( HANDLE fd )
{
	D_REAPSOCKET(ciolog("close for[%d] entering", (int)fd ));

	EnterCriticalSection( &m_sslEnableLock );
	m_sslEnabled.remove( (long long)fd );
	LeaveCriticalSection( &m_sslEnableLock );

	m_completionListLock.readLock();
	SCompletionUnit *completion = m_completionList.getItem( (long long)fd );

	if ( completion )
	{
		if ( !completion->reap )
		{
			INC_COMPLETION_REF( completion );
			m_completionListLock.unlock();

			// even if this fails we don't care, do the close work
			blockOnSend( completion ); // make sure no sends are in progress
			
			Ccp::CriticalLockScoped ulock( completion->unitLock );

			if ( !completion->reap ) // check again to avoid a race
			{
				emitStatusMessage( "close/reap call for [%p]", fd );
				D_REAPSOCKET(ciolog("reaping [%d] [%s]", (int)completion->workHandle, completion->sendRunning ? "true":"false" ));
				
				completion->reap = true;
				completion->dead = WSAENOTSOCK;

				// shut down the receive channel to kick out the pending read
				::shutdown( (SOCKET)completion->workHandle, SD_RECEIVE );
				
				// release the reference granted by the initial
				// addToCio() call
				ulock.release();
				
				PostQueuedCompletionStatus( m_completionHandle, ceJobReapSocket, 0, completion ); // a worker thread completes the death
			}
			else
			{
				ulock.release();
				D_REAPSOCKET(ciolog("[%d] lost reap-race", (int)fd ));
			}
			
			DEC_COMPLETION_REF( completion );
		}
		else
		{
			m_completionListLock.unlock();
			D_REAPSOCKET(ciolog("[%d] already been reaped", (int)fd ));
		}
	}
	else
	{
		m_completionListLock.unlock();
		emitStatusMessage( "close/reap call for [%p] but we do not manage it, performing blind ::closesocket()", fd );
		D_REAPSOCKET(ciolog("[%d] ineligible for reap, performing ::closesocket()", (int)fd ));
		::closesocket( (SOCKET)fd );
	}

	onTaskletScheduled( false ); // even though a tasklet wasn't scheduled, some waits are expecting wakeup here

	return 0;
}

//------------------------------------------------------------------------------
int CarbonIO::shutdown( SOCKET fd, int how )
{
	D_REAPSOCKET(ciolog("shutting down [%d]", (int)fd));

	m_completionListLock.readLock();
	SCompletionUnit *completion = m_completionList.getItem( (long long)fd );

	int ret = 0;
	
	if ( completion )
	{
		emitStatusMessage( "shutdown[%d] call for [%d]", how, (int)fd );
		
		INC_COMPLETION_REF( completion );
		m_completionListLock.unlock();

		blockOnSend( completion ); // make sure no sends are in progress

		ret = ::shutdown( fd, how );

		DEC_COMPLETION_REF( completion );
	}
	else
	{
		emitStatusMessage( "shutdown[%d] call for [%d] but we do not manage it", how, (int)fd );

		m_completionListLock.unlock();
	}
	
	return ret;
}

//------------------------------------------------------------------------------
int CarbonIO::setMaxPacketSize( SOCKET fd, PyObject *args )
{
	int size = -1;
	int oldSize = 0;

	Ccp::RWSpinlockReadScoped rlock( m_completionListLock );

	SCompletionUnit *completion = m_completionList.getItem( (long long)fd );
	
	if ( completion && args && PyArg_ParseTuple(args, "|i:setMaxPacketSize", &size) && (size != -1) )
	{
		oldSize = completion->maxPacketSize;
		completion->maxPacketSize = size;
	}

	return oldSize;
}

//------------------------------------------------------------------------------
int CarbonIO::getAddrInfo( const char *nodename,
						   const char *servname,
						   struct addrinfo *hints,
						   struct addrinfo **res )
{
	SJob *job = getJob();

	// must copy all data in the job, since our stack is about to
	// get swapped out
	job->name1p = job->name2p = NULL;
	if ( nodename )
	{
		strncpy( job->name1, nodename, sizeof(job->name1) - 1 );
		job->name1[ sizeof(job->name1) - 1 ] = 0;
		job->name1p = job->name1;
	}
	if ( servname )
	{
		strncpy( job->name2, servname, sizeof(job->name2) - 1 );
		job->name2[ sizeof(job->name2) - 1 ] = 0;
		job->name2p = job->name2;
	}
	
	D_GETADDRINFO(ciolog("getAddrInfo for [%s]:[%s]",
						  job->name1p ? job->name1p : "<NULL>",
						  job->name2p ? job->name2p : "<NULL>"));

	if ( hints )
	{
		memcpy( &(job->hints), hints, sizeof(struct addrinfo) );
		job->param1p = &(job->hints);
	}
	else
	{
		job->param1p = 0;
	}
	
	int ret = -1;
	job->ret = -1;
	
	PostQueuedCompletionStatus( m_completionHandle, ceJobGetAddrInfo, (ULONG_PTR)job, 0 );
	*res = NULL;
	if ( blockCurrentTasklet(job) )
	{
		ret = job->ret;
		if ( job->ret == 0 )
		{
			*res = (addrinfo*)job->result;
		}
		D_GETADDRINFO(ciolog("getAddrInfo returning [%s]", *res ? inet_ntoa(((sockaddr_in *)((*res)->ai_addr))->sin_addr) : "<NULL>"));
	}
	else
	{
		D_GETADDRINFO(ciolog("getAddrInfo block failed"));
		ret = -1; // signal a Python exception
	}

	releaseJob( job );
	return ret;	
}

//------------------------------------------------------------------------------
hostent *CarbonIO::getHostByName( const char* name )
{
	SJob *job = getJob();
	strncpy( job->name1, name, sizeof(job->name1) - 1 );
	job->name1[sizeof(job->name1) - 1 ] = 0;
	job->result = 0;
	hostent *ret;
	
	PostQueuedCompletionStatus( m_completionHandle, ceJobGetHostByName, (ULONG_PTR)job, 0 );
	if ( blockCurrentTasklet(job) )
	{
		ret = (hostent*)job->result;
		if ( !ret )
		{
			WSASetLastError( job->err );
		}
	}
	else
	{
		ret =(hostent*)-1;
	}
	releaseJob( job );
	return ret;
}

//------------------------------------------------------------------------------
hostent *CarbonIO::getHostByAddr(const char *addr, int len, int type )
{
	SJob *job = getJob();
	job->len = (int)std::min( sizeof( job->name1 ) - 1, (unsigned long long)len );
	memcpy( job->name1, addr, job->len );
	job->param1 = type;
	job->result = 0;
	hostent *ret;
	
	PostQueuedCompletionStatus( m_completionHandle, ceJobGetHostByAddr, (ULONG_PTR)job, 0 );
	if ( blockCurrentTasklet(job) )
	{
		ret = (hostent*)job->result;
		if ( !ret )
		{
			WSASetLastError(job->err);
		}
	}
	else
	{
		ret = (hostent*)-1;
	}
	releaseJob( job );
	return ret;	
}

//------------------------------------------------------------------------------
void CarbonIO::enableSSL( SOCKET fd )
{
	EnterCriticalSection( &m_sslEnableLock );
	if ( !m_sslEnabled.get( (long long)fd ) )
	{
		emitStatusMessage( "enabling ssl for [%d]", (int)fd );
		*(m_sslEnabled.add( (long long)fd )) = true;
	}
	LeaveCriticalSection( &m_sslEnableLock );
}

//------------------------------------------------------------------------------
void CarbonIO::setSSLClientCertificate( PyObject *obj )
{
	char* cert = PyString_AsString( obj );
	if ( cert )
	{
	}
}

//------------------------------------------------------------------------------
void CarbonIO::setSSLServerCertificate( PyObject *obj )
{
	char* cert = PyString_AsString( obj );
	if ( cert )
	{
	}
}

//------------------------------------------------------------------------------
void CarbonIO::setSSLPrivateClientKey( PyObject *obj )
{
	char* key = PyString_AsString( obj );
	if ( key )
	{
	}
}

//------------------------------------------------------------------------------
void CarbonIO::setSSLPrivateServerKey( PyObject *obj )
{
	char* key = PyString_AsString( obj );
	if ( key )
	{
	}
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::setWakeupMethod( int method )
{
	if ( (method > 0) && (method <= WAKEUP_LAST) )
	{
		m_wakeupMethod = method;
		Py_RETURN_NONE;
	}

	setPyError( "wakeup method out of range", method );
	return 0;
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::getWakeupMethod()
{
	switch ( m_wakeupMethod )
	{
		case WAKEUP_DYNAMIC_CONTEXT:
		{
			return PyString_FromString( "Dynamic Context" );
		}
		
		case WAKEUP_PENDING_CALL:
		{
			return PyString_FromString( "Pending Call" );
		}

		default:
		{
			return PyString_FromString( "UNDEFINED! RUN!" );
		}
	}
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::getBytesWaiting( SOCKET fd )
{
	long bytes = 0;

	m_completionListLock.readLock();
	SCompletionUnit *completion = m_completionList.getItem( (long long)fd );
	if ( completion )
	{
		INC_COMPLETION_REF( completion );
		m_completionListLock.unlock();

		Ccp::CriticalLockScoped ulock( completion->unitLock );

		for( SPacket *P = completion->packetListHead ; P ; P=P->next )
		{
			bytes += P->packetLen;
		}
		
		ulock.release();
		
		DEC_COMPLETION_REF( completion );
	}
	else
	{
		m_completionListLock.unlock();
	}

	return PyInt_FromLong( bytes );
}

//------------------------------------------------------------------------------
bool CarbonIO::send( SOCKET fd, char *data, int len, int flags )
{
	if ( !data || !len )
	{
		D_SEND(ciolog("Sending 0-len data"));
		return true; // trivial but legal
	}

	m_completionListLock.readLock();
	SCompletionUnit *completion =  m_completionList.getItem( (long long)fd );
	if ( !completion )
	{
		m_completionListLock.unlock();
		setPyError( "descriptor not found" );
		return false;
	}

	INC_COMPLETION_REF( completion );
	m_completionListLock.unlock();

	if ( completion->dead ) // NOTE: theoretically this could allow sends on a socket that has been shutdown with SD_RECIEVE
	{
		DEC_COMPLETION_REF( completion );
		setPyError( "descriptor not alive", completion->dead == -1 ? WSAECONNRESET : completion->dead );
		return false;
	}

	Ccp::CriticalLockScoped slock( completion->sendLock );

	// if there is already a packet with the same send flags,
	// then groovy. Otherwise add ourselves to the list

	if ( !completion->rawQueuedDataHead ) // no list? start one
	{
		completion->rawQueuedDataHead = getPacket();
		completion->rawQueuedDataHead->next = 0;
		completion->rawQueuedDataHead->streamSend = true;
		completion->rawQueuedDataHead->flags = flags;
		completion->rawQueuedDataTail = completion->rawQueuedDataHead;
	}

	if ( completion->rawQueuedDataTail->flags != flags ) // existing stream have different flags?
	{
		D_SEND(ciolog("rawQueuedDataHead flag mismatch for [%d] sending [%d]", (int)fd, len));
		completion->rawQueuedDataTail->next = getPacket();
		completion->rawQueuedDataTail = completion->rawQueuedDataTail->next;
		completion->rawQueuedDataTail->next = 0;
		completion->rawQueuedDataTail->flags = flags;
		completion->rawQueuedDataTail->streamSend = true;
	}

	// at this point we know that we belong on the end of the tail
	appendDataToPacket( completion->rawQueuedDataTail, data, len );
	completion->queuedBytes += len;

	emitStatusMessage( "initiated send of [%d] to [%d]", len, (int)fd );

	// if a send is not pending then inject a 'blank' completion
	// which will do the work (presumably SSL encryption)
	startSendRunning( completion );
	slock.release();

	bool result = true;
	
	if ( completion->blockingSend )
	{
		D_SEND(ciolog("blocking on send of[%d] for [%d]", len, (int)fd, len));
		result = blockOnSend( completion );
	}
	else
	{
		D_SEND(ciolog("asyc send of[%d] for[%d]", len, (int)fd));
	}

	DEC_COMPLETION_REF( completion );

	return result;
}

//------------------------------------------------------------------------------
int CarbonIO::sendTo( SOCKET fd, char *data, int len, int flags, struct sockaddr* to, int* tolen )
{
	Ccp::RWSpinlockReadScoped rlock( m_completionListLock );
	SCompletionUnit *completion = m_completionList.getItem( (long long)fd );
	if ( !completion )
	{
		return SOCKET_ERROR;
	}

	if ( !completion->udpConnection )
	{
		return SOCKET_ERROR;
	}
	
	INC_COMPLETION_REF( completion );

	rlock.release();

	Ccp::CriticalLockScoped slock( completion->sendLock );

	SPacket *packet = 0;
	if ( !completion->rawQueuedDataHead ) // no list? start one
	{
		completion->rawQueuedDataHead = getPacket();
		completion->rawQueuedDataTail = completion->rawQueuedDataHead;
		packet = completion->rawQueuedDataHead;
	}
	else
	{
		completion->rawQueuedDataTail->next = getPacket();
		completion->rawQueuedDataTail = completion->rawQueuedDataTail->next;
		packet = completion->rawQueuedDataTail;
	}

	memcpy( &packet->addr, to, *tolen );
	packet->addrlen = *tolen;
	packet->flags = flags;
	appendDataToPacket( packet, data, len );

	startSendRunning( completion );
	slock.release();
	
	DEC_COMPLETION_REF( completion );

	return len;
}

//------------------------------------------------------------------------------
int CarbonIO::sendSequence( SOCKET fd, PyObject *obj, int flags )
{
	SPacket *packet = getPacket();

	if ( !prepareSequence(obj, packet) )
	{
		D_SEQUENCE(ciolog("prepare sequence failed"));
		freePacket( packet );
		return -1;
	}

	int ret = send( fd, packet->data, packet->packetLen, flags ) ? packet->packetLen : -1;
	
	D_SEQUENCE(ciolog("prepare sequence found [%d] bytes, send returned[%d]", packet->packetLen, ret));

	freePacket( packet );
	return ret;
}

//------------------------------------------------------------------------------
int CarbonIO::sendSequenceTo( SOCKET fd, PyObject *obj, int flags, struct sockaddr* from, int* fromlen )
{
	SPacket *packet = getPacket();

	int ret = 0;
	if ( prepareSequence(obj, packet) )
	{
		ret = sendTo( fd, packet->data, packet->packetLen, flags, from, fromlen );
	}

	freePacket( packet );
	return ret;
}

//------------------------------------------------------------------------------
bool CarbonIO::sendPacket( SOCKET fd, PyObject *args )
{
	char *data;
	unsigned int len;
	char *OOBData = 0;
	unsigned int OOBLen =0;

	if ( !PyArg_ParseTuple(args, "s#|z#:sendPacket", &data, &len, &OOBData, &OOBLen) )
	{
		return false;
	}

	D_SENDPACKET(ciolog("send packet queing data [%d:%s][%d:%s]", len, data ? data : "<null>", OOBLen, OOBData ? OOBData : "<null>"));

	SPacket *packet = createPacketFromOutgoingData( data, len, OOBData, OOBLen );

	return sendPacketEx( fd, packet, true );
}

//------------------------------------------------------------------------------
int CarbonIO::setBlockingSend( SOCKET fd, int block )
{
	Ccp::RWSpinlockReadScoped rlock( m_completionListLock );
	SCompletionUnit *completion = m_completionList.getItem( (long long)fd );
	if ( !completion )
	{
		return block;
	}

	int oldvalue = completion->blockingSend;
	completion->blockingSend = block;
	return oldvalue;
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::setCompressionThreshold( SOCKET fd, PyObject *args )
{
	int threshold = -1;

	Ccp::RWSpinlockReadScoped rlock( m_completionListLock );

	SCompletionUnit *completion = m_completionList.getItem( (long long)fd );
	if ( completion )
	{
		if ( !PyArg_ParseTuple(args, "i", &completion->compressionThreshold) )
		{
			return 0;
		}
	}

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::setGlobalCompressionThreshold( PyObject *args )
{
	if ( !PyArg_ParseTuple(args, "i", &m_compressionThreshold) )
	{
		return 0;
	}
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::setCompressionMinRatio( PyObject *args )
{
	int ratio = 90;
	if ( !PyArg_ParseTuple(args, "i", &ratio) )
	{
		return 0;
	}
	if ( ratio > 0 && ratio < 100 )
	{
		m_compressionMinRatio = ratio;	
	}
		
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::setCompressionType( PyObject *args )
{
	char *type;
	if ( !PyArg_ParseTuple(args, "z", &type) )
	{
		return nullptr;
	}

	if ( !_strnicmp(type, "snappy", 6) )
	{
		m_compressionType = ceHeaderBitSnappyCompressed;
		PyErr_SetString( PyExc_NotImplementedError, "Snappy compression is no longer supported" );
		return nullptr;
	}
	else if ( !_strnicmp(type, "zlib", 4) )
	{
		m_compressionType = ceHeaderBitZlibCompressed;
	}

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* CarbonIO::setCompressionLevel( PyObject *args )
{
	int level = 6;
	if ( !PyArg_ParseTuple(args, "i", &level) )
	{
		return 0;
	}
	
	if ( level >= 0 && level <= 9 )
	{
		m_compressionLevel = level;
	}

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
CarbonIO::SPacket* CarbonIO::getPacket()
{
	SPacket* ret;
	if ( !m_packetFreeList )
	{
		ret = new(std::nothrow) SPacket;
		memset( ret, 0, sizeof(SPacket) );
	}
	else
	{
		Ccp::CriticalLockScoped plock( m_packetFreeListLock );
		
		if ( !m_packetFreeList )
		{
			plock.release();
			
			// lost a race
			ret = new(std::nothrow) SPacket;
			memset( ret, 0, sizeof(SPacket) );
		}
		else
		{
			ret = m_packetFreeList;
			m_packetFreeList = m_packetFreeList->next;
			ret->flags = 0;
			ret->packetLen = 0;
			ret->streamSend = false;
			ret->next = 0;
			m_stats.packetFreeListSize = --m_packetFreeListSize;
		}
	}

	m_stats.packetsAllocated = InterlockedIncrement( &m_packetsAllocated );

	return ret;
}

//------------------------------------------------------------------------------
void CarbonIO::freePacket( SPacket *packet )
{
	m_stats.packetsAllocated = InterlockedDecrement( &m_packetsAllocated );
	
	// don't allow a free-list packetsize to grow past our page size,
	// but if its less, just leave it alone
	if ( packet->bufferLen > c_minPacketBufferSize )
	{
		delete[] packet->data;
		packet->data = 0;
		packet->bufferLen = 0;
	}

	packet->packetLen = 0;
	packet->payloadOffset = 0;

	freeAuxData( *packet );

	EnterCriticalSection( &m_packetFreeListLock );
	packet->next = m_packetFreeList;
	m_packetFreeList = packet;
	m_stats.packetFreeListSize = ++m_packetFreeListSize;
	LeaveCriticalSection( &m_packetFreeListLock );
}

//------------------------------------------------------------------------------
void CarbonIO::appendDataToPacket( SPacket *packet, const char *data, const unsigned int len )
{
	// note- no sanity checks are made here since this is a
	// private method with strict calling requirements

	if ( !len )
	{
		return;
	}
	
	if ( !packet->data ) // easy case, just add it
	{
		packet->data = new(std::nothrow) char[len];
		memcpy( packet->data, data, len );
		packet->bufferLen = len;
		packet->packetLen = len;
	}
	else if ( (packet->packetLen + len) <= packet->bufferLen ) // slightly harder, can append
	{
		memcpy( packet->data + packet->packetLen, data, len );
		packet->packetLen += len;
	}
	else // hardest case, buffer replacement, but still straightforward
	{
		packet->bufferLen = packet->packetLen + len;
		packet->bufferLen += packet->bufferLen / 2; // it is possible we will be back, add some breathing room
		char *newBuf = new(std::nothrow) char[ packet->bufferLen ];
		memcpy( newBuf, packet->data, packet->packetLen );
		memcpy( newBuf + packet->packetLen, data, len );
		packet->packetLen += len;
		delete[] packet->data;
		packet->data = newBuf;
	}
}

//------------------------------------------------------------------------------x
bool CarbonIO::sendPacketEx( SOCKET fd, SPacket *packet, bool GILOwned )
{
	m_completionListLock.readLock();
	SCompletionUnit *completion =  m_completionList.getItem( (long long)fd );
	if ( !completion )
	{
		m_completionListLock.unlock();
		if ( GILOwned )
		{
			setPyError( "descriptor not found" );
		}
		freePacket( packet );
		return false;
	}

	INC_COMPLETION_REF( completion );
	m_completionListLock.unlock();

	if ( packet->packetLen > completion->maxPacketSize )
	{
		emitErrorMessage( "[%p] tried to send a packet that is too large [%d]>[%d]", completion->workHandle, packet->packetLen, completion->maxPacketSize );
		if ( GILOwned )
		{
			setPyError( "tried to send packet that was too large", completion->dead );
		}
		DEC_COMPLETION_REF( completion );
		freePacket( packet );
		return false;
	}

	if ( completion->dead ) // NOTE: theoretically this could allow sends on a socket that has been shutdown with SD_RECIEVE
	{
		if ( GILOwned )
		{
			setPyError( "descriptor not alive", completion->dead == -1 ? WSAECONNRESET : completion->dead );
		}
		DEC_COMPLETION_REF( completion );
		freePacket( packet );
		return false;
	}

	Ccp::CriticalLockScoped slock( completion->sendLock );

	m_stats.packetsSent++;
	completion->stats.packetsSent++;
	completion->queuedBytes += packet->packetLen;

	if ( !completion->rawQueuedDataHead )
	{
		completion->rawQueuedDataHead = packet;
		completion->rawQueuedDataTail = packet;
	}
	else
	{
		completion->rawQueuedDataTail->next = packet;
		completion->rawQueuedDataTail = packet;
	}

	emitStatusMessage( "initiated packet send of [%d] to [%p]", packet->packetLen, completion->workHandle );

	startSendRunning( completion );
	slock.release();

	bool result = true;
	if ( GILOwned && completion->blockingSend )
	{
		D_SENDPACKET(ciolog("blocking on send for [%d]", (int)fd, packet->packetLen));
		result = blockOnSend( completion );
	}

	DEC_COMPLETION_REF( completion );

	return result;
}

//------------------------------------------------------------------------------
void CarbonIO::workerThread( void *arg )
{
	int size;
	SCompletionUnit *completion = 0;
	SJob* job;

	m_singleton.m_stats.busyThreads = InterlockedIncrement( &m_singleton.m_busyWorkerThreads ); // preload

	long long serviceTime = 0;

	for(;;)
	{
		if ( serviceTime )
		{
			m_singleton.m_events.inc( "worker::serviceTime", m_singleton.getPerformanceCounter() - serviceTime );
		}
		
		if ( !m_singleton.m_running )
		{
			break;
		}
		
		m_singleton.m_stats.busyThreads = InterlockedDecrement( &m_singleton.m_busyWorkerThreads );

		// TODO- idle thread reap semantics? or just leave the thread count at
		// whatever the peak was? they are all sleeping cozy in the
		// following call, so no real need to other than freeing kernel
		// resources

		D_WORKER(ciolog( "worker thread entering [%d/%d]", m_singleton.m_stats.busyThreads, m_singleton.m_spawnedWorkerThreads ));

		int status = GetQueuedCompletionStatus( m_singleton.m_completionHandle,
												(DWORD*)&size,
												(PULONG_PTR)&job,
												(OVERLAPPED **)&completion,
												INFINITE);

		serviceTime = m_singleton.getPerformanceCounter();
		
		if ( m_singleton.m_enableTimers )
		{
			m_singleton.m_events.incevt( "worker::event" );
		}

		// for checking status on breakpoints
		int err = WSAGetLastError();

		if ( !m_singleton.m_running )
		{
			break;
		}

		m_singleton.m_stats.busyThreads = InterlockedIncrement( &m_singleton.m_busyWorkerThreads );

		// if all threads are busy spawn another one -- TODO, thread reaping semantics?
		if ( m_singleton.m_busyWorkerThreads >= m_singleton.m_spawnedWorkerThreads
			 && m_singleton.m_spawnedWorkerThreads < (m_singleton.m_blockedThreads + c_dynamicWorkerThreadCap) )
		{
			m_singleton.m_stats.workerThreads = InterlockedIncrement( &m_singleton.m_spawnedWorkerThreads );
			_beginthread( workerThread, 0, 0 );
		}

		// indicates that we just got an error relating to this
		// completion operation
		if ( !status )
		{
			D_WORKER(ciolog("worker thread err completion[%p] lasterr[%d] send[%s]", completion, WSAGetLastError(), completion->send ? "true":"false" ));

			if ( completion )
			{
				SCompletionUnit *fail = completion->send ? ((SWriteOverlap *)completion)->parent : completion;

				D_REAPSOCKET(ciolog("err case on [%d]", (int)fail->workHandle));

				if ( completion->send )
				{
					// interesting but ultimately we don't care, just
					// make sure the 'sendRunning' flag is cleared and
					// any tasklets waiting on completion get poked

					Ccp::CriticalLockScoped ulock( fail->unitLock );

					fail->sendRunning = false;
					SJob *senders = fail->jobsToWakeOnSend;
					fail->jobsToWakeOnSend = 0;

					ulock.release();

					DEC_COMPLETION_REF( fail );
					
					reverseJobList( senders ); //this list was in reverse order
					m_singleton.wakeUpTaskletChain( senders );
				}
				else
				{
					D_REAPSOCKET(ciolog("read death for[%d]", (int)fail->workHandle));

					// death of triggerRead()
					Ccp::CriticalLockScoped ulock( fail->unitLock );

					fail->dead = WSAECONNRESET;
					SJob* list = fail->readJobsWaiting;
					fail->readJobsWaiting = 0;
					fail->readJobsWaitingTail = 0;
					for( SJob* link = list ; link ; link = link->next )
					{
						completion->readJobsUnscheduled++;
						linkSequenceJob( fail, link );
					}
							
					ulock.release();
					
					DEC_COMPLETION_REF( fail );

					m_singleton.wakeUpTaskletChain( list );
				}
			}

			continue;
		}

		// A send has completed, check to see if any more are
		// pending, this allows the send queue to drain off-core and no
		// sending operations to block while holding the GIL
		if ( (size >= 0) && completion && completion->send )
		{
			if ( size == 0 )
			{
				emitStatusMessage( "send loop kicked off for [%p]", ((SWriteOverlap *)completion)->parent->workHandle );
			}
			else
			{
				emitStatusMessage( "send completion size [%d] to [%p]", size, ((SWriteOverlap *)completion)->parent->workHandle );
			}
			
			if ( ((SWriteOverlap *)completion)->parent->sendRunning )
			{
				m_singleton.handleSendCompletion( (SWriteOverlap *)completion );
			}
			DEC_COMPLETION_REF( ((SWriteOverlap *)completion)->parent );
			continue;
		}
	
		// fast detect the most common case: reads
		if ( size >= 0 )
		{
			completion->stats.bytesReceived += size;
			m_singleton.m_stats.bytesReceived += size;

			if ( completion->reap || completion->dead ) // reaping or dead, we're done
			{
				emitStatusMessage( "[%d] bytes contained in completion message to [%p], but it is dead or reaping, ignored", size, completion->workHandle );

				DEC_COMPLETION_REF( completion );
				continue;
			}

			// it was a zero-byte read, pull the data off that we are
			// expecting, this should always succeed but handle error anyway
			if ( completion->wasZeroByteRead )
			{
				completion->wasZeroByteRead = false;
				WSABUF buffer;
				buffer.buf = completion->buf;
				buffer.len = c_systemPageSize; 
				ULONG flags = 0;
				if ( WSARecv((SOCKET)completion->workHandle, &buffer, 1, (DWORD*)&size, &flags, completion, 0)
					 &&  WSAGetLastError() != WSA_IO_PENDING )
				{
					completion->dead = WSAGetLastError();
					Ccp::CriticalLockScoped ulock( completion->unitLock );

					SJob *list = getWakeupList( completion );

					ulock.release();

					DEC_COMPLETION_REF( completion );
					
					m_singleton.failAllTasklets( list );
				}

				// else just inherit the refcount
				continue;
			}

			if ( size == 0 )
			{
				// zero signals EOF

				emitStatusMessage( "0-byte read for [%p], interpreting as graceful EOF", completion->workHandle );

				Ccp::CriticalLockScoped ulock( completion->unitLock );

				completion->dead = -1;
				SJob* list = completion->readJobsWaiting;
				completion->readJobsWaiting = 0;
				completion->readJobsWaitingTail = 0;
				for( SJob* link = list ; link ; link = link->next )
				{
					completion->readJobsUnscheduled++;
					linkSequenceJob( completion, link );
				}

				ulock.release();

				DEC_COMPLETION_REF( completion );

				m_singleton.wakeUpTaskletChain( list );

				continue;
			}

			emitStatusMessage( "[%d] bytes delivered by completion message for [%p]", size, completion->workHandle );

			if ( completion->udpConnection )
			{
				m_singleton.handleUDPRead( size, completion );
			}
			else
			{
				m_singleton.handleRead( size, completion );
			}
		}
		else
		{
			// far less common jobs, there are injected artificially
			// via PostQueuedCompletionStatus()

			m_singleton.m_stats.blockedThreads = InterlockedIncrement( &m_singleton.m_blockedThreads );
					
			switch( size )
			{
				case ceJobGetAddrInfo:
				{
					D_GETADDRINFO(ciolog("getAddrInfo job[%s]:[%s] in",
										  job->name1p ? job->name1p : "<NULL>",
										  job->name2p ? job->name2p : "<NULL>"));
					
					job->ret = getaddrinfo( job->name1p,
											job->name2p,
											(struct addrinfo*)job->param1p,
											(addrinfo**)&job->result );
					
					D_GETADDRINFO(ciolog("getAddrInfo OUT ret[%d]", job->ret));

					job->next = 0;
					m_singleton.wakeUpTaskletChain( job, true );
					break;
				}

				case ceJobGetHostByName:
				{
					job->result = socket_hostent_dup( gethostbyname( job->name1 ) );
					job->err = WSAGetLastError();
					job->next = 0;
					m_singleton.wakeUpTaskletChain( job, true );
					break;
				}
				
				case ceJobGetHostByAddr:
				{
					job->result = socket_hostent_dup( gethostbyaddr( job->name1, job->len, job->param1 ) );
					job->err = WSAGetLastError();
					job->next = 0;
					m_singleton.wakeUpTaskletChain( job, true );
					break;
				}

				case ceJobReapSocket:
				{
					// one and only one reap request can be sent out,
					// so this thread can assume exclusivity

					D_REAPSOCKET(ciolog("reap job entry for socket[%d]", (int)completion->workHandle));

					// get the list of any outstanding tasklets waiting
					// on this completion unit, at this point this list
					// SHOULD be blank, this is a precaution against
					// races
					Ccp::CriticalLockScoped ulock( completion->unitLock );
					SJob *list = getWakeupList( completion );
					ulock.release();
					m_singleton.failAllTasklets( list );

					// then blow it up.
					m_singleton.destroyCompletionUnit( completion );

					break;
				}

				case ceJobAccept:
				{
					serviceTime = 0;
					m_singleton.queueAccept( job );
					break;
				}

				case ceJobConnect:
				{
					serviceTime = 0;
					m_singleton.connectEx( job );
					break;
				}

				default:
				{
					// TODO- this ought to be impossible, gracefully handle it
					// but should probably let someone know its happened
					break;
				}
			}

			m_singleton.m_stats.blockedThreads = InterlockedDecrement( &m_singleton.m_blockedThreads );
		}
	}

	// until thread-reaping semantics are installed, this code never
	// atually executes.
	
	m_singleton.m_stats.workerThreads = InterlockedDecrement( &m_singleton.m_spawnedWorkerThreads );
}

//------------------------------------------------------------------------------
void CarbonIO::queueAccept( SJob* job )
{
	long long key = job->socket;

	job->next = 0;

	EnterCriticalSection( &m_acceptQueueLock );
	SAcceptQueue *entry = m_acceptQueueList.get( key );
	if ( entry )
	{
		// a thread is already servicing accepts on
		// this descriptor, add ourselves to the list
		// and be done
		if ( entry->tail )
		{
			D_ACCEPT(ciolog("entry existed, adding to tail [%d]", (int)key));
			entry->tail->next = job;
		}
		else
		{
			D_ACCEPT(ciolog("entry existed, installing [%d]", (int)key));
			entry->head = job;
		}

		entry->tail = job;
	}
	else
	{
		// there is no servicing entry, add one and
		// start processing
		entry = m_acceptQueueList.add( job->socket );
		entry->head = 0; // list existance signlals this thread has been entered
		entry->tail = 0;
		SJob *acceptJob = job;

		for(;;)
		{
			D_ACCEPT(ciolog("entering accept queue for [%d]",(int)key));

			LeaveCriticalSection( &m_acceptQueueLock );
			acceptEx( acceptJob );
			EnterCriticalSection( &m_acceptQueueLock );

			if ( !(acceptJob = entry->head) )
			{
				D_ACCEPT(ciolog("last job processed, removing entry for [%d]",(int)key));
				m_acceptQueueList.remove( key );
				break;
			}

			entry->head = entry->head->next;
			if ( !entry->head )
			{
				D_ACCEPT(ciolog("looping around to last accept for [%d]",(int)key));
				entry->tail = 0;
			}
			else
			{
				D_ACCEPT(ciolog("looping around to another accept for [%d]",(int)key));
			}
		}
	}

	LeaveCriticalSection( &m_acceptQueueLock );
}

//------------------------------------------------------------------------------
void CarbonIO::acceptEx( SJob* job )
{
	D_ACCEPT(ciolog("asked to accept on socket [%d]", (int)job->socket ));

	job->ret = 0;
	job->next = 0;

	m_completionListLock.readLock();
	SCompletionUnit *completion = m_completionList.getItem( (long long)job->socket );
	m_completionListLock.unlock();
	if ( completion && completion->udpConnection )
	{
		D_ACCEPT(ciolog("can't accept on UDP socket [%d]", (int)job->socket ));
		job->ret = WSAENOTSOCK;
		job->socket = INVALID_SOCKET;
		wakeUpTaskletChain( job );
		return;
	}
	
	for(;;) // failure to negotiate SSL is a soft error, so keep trying until that works
	{
		D_ACCEPT(ciolog("accept entering for [%d]", (int)job->socket ));

		if ( job->param1 ) // timeout value set?
		{
			fd_set fds;
			struct timeval tv;
			tv.tv_sec = job->param1 / 1000000;
			tv.tv_usec = job->param1 % 1000000;
			FD_ZERO( &fds );
			FD_SET( job->socket, &fds );

			if ( select((int)job->socket + 1, &fds, NULL, NULL, &tv) <= 0 )
			{
				job->socket = INVALID_SOCKET;
				job->param1 = 1;
				wakeUpTaskletChain( job );
				return;
			}
		}
		
		job->param1 = 0; // whatever else happens, there was not a timeout
		job->addrlen = sizeof(job->address);
		
		SOCKET newfd = ::accept( job->socket, (sockaddr *)&job->address, &job->addrlen );

		D_ACCEPT(ciolog("accept returned [%d]", (int)newfd ));

		if ( newfd == INVALID_SOCKET )
		{
			D_ACCEPT(ciolog("accept fail <1> on [%d]", (int)newfd));
			job->ret = WSAGetLastError();
			job->socket = INVALID_SOCKET;
			break;
		}

		// wire it in
		completion = addToCio( (HANDLE)newfd );
		if ( !completion )
		{
			::closesocket( newfd );
			job->ret = WSAGetLastError();
			job->socket = INVALID_SOCKET;
			D_ACCEPT(ciolog("accept fail <3> on [%d] err[%d]", (int)newfd, job->ret));
			break;
		}

		// what follows are non-fatal (to the accept call) errors which
		// silently trigger a retry
		
		EnterCriticalSection( &m_sslEnableLock );
		if ( m_sslEnabled.get((long long)job->socket) )
		{
			D_ACCEPT(ciolog("accept enabling SSL on [%d]", (int)newfd));
			completion->ssl = new(std::nothrow) SslPipe();
			completion->ssl->initAsServer();

			if ( m_SSLHandshakeNegotiationSeconds )
			{
				completion->handshakeTimeout = time(0) + m_SSLHandshakeNegotiationSeconds;
			}
		}
		LeaveCriticalSection( &m_sslEnableLock );

		int err = triggerRead(completion);
		if ( err )
		{
			D_ACCEPT(ciolog("accept trigger failed (looping) on [%d]", (int)newfd));

			completion->reap = true;
			completion->dead = err;
			completion->refCount = 1;

			destroyCompletionUnit( completion );
		}
		else
		{
			InterlockedIncrement( &m_stats.accepts );
			INC_COMPLETION_REF( completion );

			job->socket = newfd;

			// do this at the most interesting time: when we are adding
			// a new descriptor.
			if ( m_lastStaleCheck != time(0) ) // limit to one check per second
			{
				m_lastStaleCheck = time(0);
				checkForStaleSSLHandshakes();
			}

			break;
		}
	}

	D_ACCEPT(ciolog("accept broken with[%d] err[%d]", (int)job->socket, job->ret));

	wakeUpTaskletChain( job );
}

//------------------------------------------------------------------------------
void CarbonIO::connectEx( SJob *job )
{
	job->next = 0;

	long long jobSerial = job->serial;
	
	m_completionListLock.readLock();
	SCompletionUnit *completion = m_completionList.getItem( (long long)job->socket );
	if ( completion )
	{
		m_completionListLock.unlock();

		ciolog( "tried to connect a socket we already have a record for [%d]", (int)job->socket );
		
		// okay so trying to re-connect a socket we already have a
		// record for. now what? fail and log, see if it ever comes up
		job->socket = INVALID_SOCKET;
		job->ret = WSASYSCALLFAILURE; // just pick something
		wakeUpTaskletChain( job );
		return;
	}

	m_completionListLock.unlock();

	SOCKADDR_STORAGE address;
	memcpy( &address, &job->address, job->addrlen );
	SOCKET socket= job->socket;
	int addrlen = job->addrlen;
	int ret;
	
	if ( (ret = ::connect(socket, (sockaddr *)&address, addrlen)) )
	{
		if ( job->serial != jobSerial )
		{
			return; // tasklet died while we were waiting, abort
		}
		
		job->ret = WSAGetLastError();
		D_CONNECT(ciolog("connect failed at first w/[%d]", job->ret ));

		if ( job->ret == WSAEWOULDBLOCK && job->param1 )
		{
			// if it would block and we've been asked to connect with a
			// timeout

			fd_set fds;
			fd_set fds_exc;
			struct timeval tv;
			tv.tv_sec = job->param1 / 1000000;
			tv.tv_usec = job->param1 % 1000000;
			FD_ZERO( &fds );
			FD_SET( job->socket, &fds );
			FD_ZERO( &fds_exc );
			FD_SET( job->socket, &fds_exc);
			
			int err = select( (int)job->socket + 1, 0, &fds, &fds_exc, &tv );

			if ( job->serial != jobSerial )
			{
				return; // tasklet died while we were waiting, abort
			}
			
			job->err = err;
			job->ret = WSAGetLastError();

			D_CONNECT(ciolog("select out w/[%d]:[%d]", job->ret, job->err ));

			if ( (job->err <= 0) || !FD_ISSET(job->socket, &fds) )
			{
				D_CONNECT(ciolog("timeout"));
				
				job->param1b = true;
				wakeUpTaskletChain( job );
				return;
			}

			// otherwise we're connected

			D_CONNECT(ciolog("connected"));
			job->ret = 0;
		}
		else
		{
			D_CONNECT(ciolog("other error"));
			wakeUpTaskletChain( job );
			return;
		}
	}
	else
	{
		if ( job->serial != jobSerial )
		{
			return; // tasklet died while we were waiting, abort
		}

		job->ret = ret;
		D_CONNECT(ciolog("connect succeeded"));
	}


	// was shut down, all data structures are basically invalid, just die
	if ( !m_running ) 
	{
		return;
	}
	
	if ( job->ret )
	{
		job->ret = WSAGetLastError();
		D_CONNECT(ciolog("connect for [%d] failed with lasterr[%d]", job->ret, (int)job->socket, job->ret));
		wakeUpTaskletChain( job );
		return;
	}

	D_CONNECT(ciolog("connect returned[%d] for [%d]", job->ret, (int)job->socket));

	// if the connect succeeds but the add fails, shut it all down
	completion = addToCio( (HANDLE)job->socket );
	if ( !completion )
	{
		D_CONNECT(ciolog("CarbonIO Add failed <2>, returning err[%d]", WSAGetLastError()));
		job->ret = WSAGetLastError();
		wakeUpTaskletChain( job );
		return;
	}

	EnterCriticalSection( &m_sslEnableLock );
	if ( m_sslEnabled.get((long long)job->socket) )
	{
		LeaveCriticalSection( &m_sslEnableLock );

		D_CONNECT(ciolog("connect enabling SSL on [%d]", (int)job->socket));
		completion->ssl = new(std::nothrow) SslPipe();

		// install client semantics since this is a connect
		completion->ssl->initAsClient();

		if ( m_SSLHandshakeNegotiationSeconds )
		{
			completion->handshakeTimeout = time(0) + m_SSLHandshakeNegotiationSeconds;
		}

		// dispatch the callup
		char callup[16384];
		int callupSize = sizeof(callup);
		completion->wsaBuf.buf = callup;

		if( completion->ssl->getPendingTransmitData(callup, &callupSize) )
		{
			D_CONNECT(ciolog("Sending initial callup [%d] bytes", callupSize));
			completion->wsaBuf.len = callupSize;
			completion->sendRunning = true;
			INC_COMPLETION_REF( completion );

			completion->stats.bytesSent += callupSize;
			m_stats.bytesSent += callupSize;
					
			DWORD dummy; // so winXP does not crash, it needs this pointer to be valid
			memset( &(completion->writeCompletion), 0, sizeof(OVERLAPPED) );
			if ( WSASend( (SOCKET)completion->workHandle, &completion->wsaBuf, 1, &dummy, 0, &(completion->writeCompletion), 0)
				 && (WSAGetLastError() != WSA_IO_PENDING) )
			{
				completion->sendRunning = false;
				completion->reap = true;
				completion->dead = WSAGetLastError();
				DEC_COMPLETION_REF( completion );

				D_CONNECT(ciolog("initial callup to[%d] failed with [%d]", (int)completion->workHandle, WSAGetLastError()));

				destroyCompletionUnit( completion );

				job->ret = WSAGetLastError();
				LeaveCriticalSection( &m_sslEnableLock );

				wakeUpTaskletChain( job );
				return;
			}
		}
		else
		{
			// this is really impossible, no initial callup from OpenSSL? its borked
			completion->reap = true;
			completion->dead = -1;
			destroyCompletionUnit( completion );

			job->ret = WSAGetLastError();
			LeaveCriticalSection( &m_sslEnableLock );

			wakeUpTaskletChain( job );
			return;
		}
	}
	else
	{
		LeaveCriticalSection( &m_sslEnableLock );
	}

	int err = triggerRead( completion );
	if ( err )
	{
		completion->reap = true;
		completion->dead = err;
		job->ret = err;

		D_CONNECT(ciolog("connect succeeded but read trigger failed for[%d] with[%d]", (int)completion->workHandle, job->ret));

		destroyCompletionUnit( completion );
	}
	else
	{
		D_CONNECT(ciolog("connect succeeded, read triggered"));
		InterlockedIncrement( &m_stats.connects );
		INC_COMPLETION_REF( completion );
	}

	wakeUpTaskletChain( job );
}

//------------------------------------------------------------------------------
VOID WINAPI CarbonIO::timerRoutine( PVOID param, unsigned char timerOrWaitFired )
{
	STimerTrackParam* track = (STimerTrackParam *)param;
		
	Ccp::CriticalLockScoped ulock( track->completion->unitLock );

	// if the job is not found in the list then it was most likely
	// popped off JUST in time and is now being serviced with its packet
	
	SJob* previous = 0;
	for( SJob* J = track->completion->readJobsWaiting ; J ; J = J->next )
	{
		if ( J == track->job )
		{
			// unlink this job
			if ( previous )
			{
				if ( !(previous->next = J->next) )
				{
					track->completion->readJobsWaitingTail = previous;
				}
			}
			else
			{
				if ( !(track->completion->readJobsWaiting = J->next) )
				{
					track->completion->readJobsWaitingTail = 0;
				}
			}

			ulock.release();

			// we now own the job, wake it up
			J->param1b = true;
			J->next = 0;

			// get a GIL threading context and perform the wakeup
			PyGILState_STATE pyGILState = PyGILState_Ensure();
			if ( m_singleton.wakeupEx(J) )
			{
				m_singleton.onTaskletScheduled( false );
			}
			PyGILState_Release( pyGILState );

			break;
		}

		previous = J;
	}
}

//------------------------------------------------------------------------------
int CarbonIO::recvEx( SOCKET fd,
					  char *buf,
					  PyObject **obj,
					  bool includeOOB,
					  int len,
					  int flags,
					  struct sockaddr* from,
					  int* fromlen,
					  float timeout )
{
	if ( obj ) // default return is null, if the python object was supplied
	{
		*obj = NULL;
	}

	if ( flags & MSG_PEEK )
	{
		setPyError( "MSG_PEEK not supported", WSAECONNRESET );
		return -1;
	}

	D_RECVEX(ciolog( "recvEx for [%d] want [%d]", (int)fd, len ));

	m_completionListLock.readLock();
	
	SCompletionUnit *completion = m_completionList.getItem( (long long)fd );
	if ( !completion )
	{
		D_RECVEX(ciolog( "recvEx <1> %d <1> %p %d", fd, completion, completion->reap ? 1:0));
		
		m_completionListLock.unlock();
		setPyError( "socket not found in manged list", WSAECONNRESET );
		return -1;
	}

	// hold a reference so we can block safely without holding a read lock
	INC_COMPLETION_REF( completion );
	m_completionListLock.unlock();

	emitStatusMessage( "RecvEx[%p] acquiring lock [%d]", completion->workHandle, completion->readJobsUnscheduled );
	
	Ccp::CriticalLockScoped ulock( completion->unitLock );

	int thread = ++completion->threadCallSerial;

	// the very first read to a UDP connection will require this since
	// the bind() is not followed by an accept() and thus CarbonIO has
	// no idea it should have issued a recv() on it
	if ( completion->udpConnection && !completion->readTriggered )
	{
		int ret = triggerRead( completion );
		if ( ret )
		{
			DEC_COMPLETION_REF( completion );
			setPyError( "UDP read-trigger failed", ret );
			return -1;
		}

		INC_COMPLETION_REF( completion ); // the 'trigger read' ref
	}

	// convert this completion unit to a packetizer, if it isn't, and
	// we have been directed to receive a packet
	if ( !completion->udpConnection )
	{
		if ( (len == -1) && !completion->isPacketConnection )
		{
			D_RECVEX(ciolog( "installing packetizer for[%d]", (int)fd));
			installPacketizer( completion );
		}
		else if ( (len != -1) && completion->isPacketConnection )
		{
			D_RECVEX(ciolog( "recvEx <3> [%d] asking for [%d] on a packet connection", (int)fd, len));
			// once a stream is defined as "packet" it cannot go back, fail loudly
			ulock.release();
			DEC_COMPLETION_REF( completion );
			setPyError( "cannot read stream data on packet socket", ERROR_INVALID_HANDLE );
			return -1;
		}
	}

	// any data waiting for us? this is only if no reads are waiting
	SPacket *P = 0;

	// TODO: How does Brian produce a breakage when I comment this in?
	// it should cover the race where data has just arrived but has not
	// yet been packetized and there ARE readers wiating for it, this
	// would theoretically bypass lal the ordering logic.
	if ( !completion->readJobsUnscheduled )//&& !completion->readJobsWaiting )
	{
		// no other reads are pending try popping a packet directly off the queue to avoid yielding
		P = popHeadPacket( completion, len );
		emitStatusMessage( "RecvEx tried to pop[%p] for[%p] {%d}", P, completion->workHandle, thread );
	}
	else
	{
		emitStatusMessage( "RecvEx chain entry [%d] for[%p] {%d}", completion->readJobsUnscheduled, completion->workHandle, thread );
	}

	int ret;

	if ( !P )
	{
		if ( completion->dead )
		{
			int result;
			D_RECVEX(ciolog( "recvEx dead, nothing here and nothing coming for [%d]", (int)completion->workHandle));
			emitStatusMessage( "RecvEx found dead[%d] for[%p] {%d}", completion->dead, completion->workHandle, thread );

			if ( completion->dead == -1 )
			{
                if ( obj )
				{
					if ( completion->isPacketConnection )
					{
						*obj = Py_None;
						Py_INCREF(Py_None);
					}
					else
					{
						ulock.release();
						spinUntilNoReadJobsScheduled( completion );
//						setPyError( "socket has been closed", WSAECONNRESET );
						*obj = PyString_FromString( "" ); // stream connections expect a blank (zero) read to signify EOF
					}
				}
				result = 0;
			}
			else
			{
				ulock.release();
				spinUntilNoReadJobsScheduled( completion );
				setPyError( "data requested from dead socket", completion->dead );
				result = -1;
			}

			ulock.release(); // if it hasn't been already

			DEC_COMPLETION_REF( completion ); 
			return result;
		}

		// no data yet, wait for some
		
		SJob *job = getJob();
		job->param1p = 0;
		job->len = len;
		job->param1b = false;
		
		D_JOBFIFO(ciolog("[%p] in", job));

		linkReadJob( completion, job );

		completion->consecutiveNoYieldReads = 0;

		ulock.release();

		STimerTrackParam *track = 0;
		if ( timeout > 0 )
		{
			track = new STimerTrackParam;
			track->completion = completion;
			track->job = job;
			track->timer = INVALID_HANDLE_VALUE;

			if ( !CreateTimerQueueTimer( &track->timer,
										 0,
										 timerRoutine,
										 track,
										 (int)(timeout * 1000.f),
										 0,
										 WT_EXECUTEONLYONCE|WT_EXECUTELONGFUNCTION ) )
			{
				setPyError( "could not execute a recv w/timeout [%d]", GetLastError() );
				delete track;
				return -1;
			}
		}

		emitStatusMessage( "RecvEx[%p] block IN[%d] {%d}", completion->workHandle, completion->readJobsUnscheduled, thread );
		job->addrlen = thread;

		// Sleep.  Count the number of jobs unresumed (out of sleep mode) so that we know when
		// it is safe to return a ready incoming packet without queuing up
		bool ok = blockCurrentTasklet( job );

		emitStatusMessage( "RecvEx[%p] block [%d]OUT {%d}", completion->workHandle, completion->readJobsUnscheduled, thread );

		if ( track )
		{
			// according to docs, this will either succeed and remove
			// the timer, or wait for the callback to complete and then
			// return, either way we are safe to delete it
			DeleteTimerQueueTimer( NULL, track->timer, INVALID_HANDLE_VALUE );
			delete track;
		}
		
		if ( job->param1b )
		{
			setPyError( "recv timed out", WSAETIMEDOUT );
			return -1;
		}

		if ( !ok ) // task was unblocked by something OTHER than CarbonIO
		{
			D_RECVEX(ciolog( "recvEx block failed for [%d]", (int)completion->workHandle));

			EnterCriticalSection( &completion->unitLock ); // just in case it got linked in before it was killed, rare race.
			unlinkSequenceJob( completion, job );
			LeaveCriticalSection( &completion->unitLock );
			
			DEC_COMPLETION_REF( completion );

			setPyError( "recv tasklet block failed", WSAESHUTDOWN );
			releaseJob( job );
			return -1;
		}

		// a packet has been recieved and placed in our job, now
		// enforce the FIFO set up with jobSequence. schedule-spin
		// until we are the head of the list, then (and only then)
		// unlink it and continue. In the usual case of a single packet
		// ready to be recieved (or available) this falls through
		int tries = 1000; // must be some way to break the infinite loop
		if ( completion->jobSequence == job )
		{
			EnterCriticalSection( &completion->unitLock ); // unlink and dec counter
			completion->readJobsUnscheduled--;
			unlinkSequenceJob( completion, job );
			LeaveCriticalSection( &completion->unitLock );
		}
		else
		{
			EnterCriticalSection( &completion->unitLock ); // knock off the counter here
			completion->readJobsUnscheduled--;
			LeaveCriticalSection( &completion->unitLock );

			while( completion->jobSequence != job ) // as long as everything is in order this falls through
			{
				if ( !job->oorReported )
				{
					job->oorReported = true;
					InterlockedIncrement( &m_stats.oorRecieved );
					D_JOBFIFO(ciolog("FIRST [%p] not matching [%p]", completion->jobSequence, job));
				}
				else
				{
					if ( --tries == 0 )
					{
						D_JOBFIFO(ciolog("[%p] GIVING UP [%p]", completion->jobSequence, job));
						m_stats.oorGaveUp++;
						break;
					}
					D_JOBFIFO(ciolog("[%p] not matching [%p]", completion->jobSequence, job));
				}
				emitStatusMessage( "Spining[%d] RecvEx[%p] block OUT[%d] want[%p] i am[%p] {%d}", tries, completion->workHandle, completion->readJobsUnscheduled, completion->jobSequence, job, thread );

				PyObject* ret = PyStackless_Schedule( Py_None, 0 );
				Py_XDECREF( ret );
			}

			EnterCriticalSection( &completion->unitLock ); // NOW unlink it
			unlinkSequenceJob( completion, job );
			LeaveCriticalSection( &completion->unitLock );
		}

		D_JOBFIFO(ciolog("[%p] unlinked", job));
		// if we got here, its because we were woken up by a worker
		// thread (or the block failed)
		P = (SPacket *)job->param1p;
		D_RECVEX(ciolog( "recvEx out of recv block with [%d] bytes for [%d]", P ? P->auxData ? P->auxDataLen : P->packetLen - P->payloadOffset : -1, (int)completion->workHandle));

		ret = job->ret;

		if ( m_singleton.m_enableTimers )
		{
			m_singleton.m_events.inc( "request::recv::block", m_singleton.getPerformanceCounter() - job->t0 );
		}

		releaseJob( job );

		if ( !P )
		{
			if ( completion->dead == -1 )
			{
				emitStatusMessage( "Graceful 1 RecvEx[%p] block OUT[%d] {%d}", completion->workHandle, completion->readJobsUnscheduled, thread );
				D_RECVEX(ciolog("null P: graceful shutdown for [%d]", (int)completion->workHandle));
                if ( obj )
                {
					if ( completion->isPacketConnection )
					{
						*obj = Py_None;
						Py_INCREF(Py_None);
					}
					else
					{
						spinUntilNoReadJobsScheduled( completion );
//						setPyError( "socket has been closed", WSAECONNRESET );
						*obj = PyString_FromString( "" ); // stream connections expect a blank (zero) read to signify EOF
					}
				}

				DEC_COMPLETION_REF( completion );
				return 0;
			}
			else
			{
				D_RECVEX(ciolog("null P: ERROR shutdown[%d] for [%d]", completion->dead, (int)completion->workHandle));
				spinUntilNoReadJobsScheduled( completion );
				setPyError( "recv operation failed", completion->dead ); // use what the underlyhing layer is expecting

				DEC_COMPLETION_REF( completion );
				return -1;
			}
		}
		emitStatusMessage( "RecvEx[%p] block OUT[%d] len[%d] {%d}", completion->workHandle, completion->readJobsUnscheduled, P->packetLen, thread );
	}
	else
	{
		// there was already a packet in Q, return
		// without blocking. If the read had been stalled restart it
		if ( m_singleton.m_enableTimers )
		{
			m_singleton.m_events.inc( "request::recv::noblock", 0 );
		}

		if ( !completion->readTriggered
			 && (c_incomingQueueHaltLevel && (completion->queuedIncomingBytes < c_incomingQueueHaltLevel)) )
		{
			if ( !triggerRead(completion) )
			{
				INC_COMPLETION_REF( completion );
			}
			else
			{
				// okay.. we tried to re-issue the read and it borked..
				// this could be bad for the case of a remote host which
				// sent a bunch of data and then disconnected thinking it
				// had completed successfully, when in fact on this end it
				// all got queued and is about to be thrown out because the
				// socket just got marked dead. may need to revisit.
			}
		}

		const int c_maximumNoYieldReads = 2; // enforce a yield every other read
		bool yield = false;
		if ( ++completion->consecutiveNoYieldReads >= c_maximumNoYieldReads )
		{
			// we need to force a yield, but do so AFTER the
			// completion-lock has been released
			yield = true;
			completion->consecutiveNoYieldReads = 0;
		}

		D_RECVEX(ciolog( "<2>recvEx out of recv block with [%d] bytes for [%d]", P->packetLen, (int)completion->workHandle));

		if ( yield )
		{
			SJob *job = getJob(); // blank job as a placeholder
			job->tasklet = (PyTaskletObject *)PyStackless_GetCurrent();
			linkSequenceJob( completion, job ); // prevent any other packets from being delivered
			completion->readJobsUnscheduled++;
			ulock.release();

			PyObject* ret = PyStackless_Schedule( Py_None, 0 ); // force a yield with a single spin
			Py_XDECREF( ret );

			EnterCriticalSection( &completion->unitLock );
			completion->readJobsUnscheduled--;
			unlinkSequenceJob( completion, job );
			LeaveCriticalSection( &completion->unitLock );

			if ( job->tasklet ) // expected, we woke up on our own
			{
				Py_CLEAR( job->tasklet );
			}
			// else carbonIO actually thought it needed to wake us up
			// (probably a connection failure clearing all connections)
			// and the tasklet was already cleared, but it is still up
			// to this thread to free the job
			
			freeJob( job );
		}
		else
		{
			ulock.release();
		}
	}

	if ( from && fromlen ) // fill in if requested
	{
		*fromlen = std::min(*fromlen, P->addrlen);
		memcpy( from, &P->addr, *fromlen );
	}

	if ( completion->udpConnection )
	{
		// popHeadPacket() guarantees us <= len data
		ret = P->packetLen;

		emitStatusMessage( "UDP bytes[%d] received for [%d] {%d}", ret, (int)fd, thread );

		// if the buffer is not big enough truncate the return value..
		// for UDP you either provide enough space or its gone
		int truncLen = P->packetLen;
		if ( (len > 0) && (P->packetLen > (unsigned int)len) )
		{
			truncLen = len;
		}
		if ( obj )
		{
			*obj = PyString_FromStringAndSize( P->data, truncLen );
		}
		else if ( buf )
		{
			memcpy( buf, P->data, truncLen );
		}
	}
	else if ( len == -1 ) // wanted a packet, be sure to take into account payload
	{
		char *realdata = P->auxData ? P->auxData : P->data + P->payloadOffset;
		int realLen = P->auxData ? P->auxDataLen : P->packetLen - P->payloadOffset;

		emitStatusMessage( "Packet bytes[%d] received for [%d] {%d}", realLen + P->oobLen + sizeof(int), (int)fd,thread );

		if ( obj )
		{
			*obj = PyString_FromStringAndSize( realdata, realLen );

			if ( includeOOB )
			{
				D_RECVEX(ciolog("OOB called for on recieve for[%d], packing a tuple", (int)completion->workHandle));
				PyObject *tuple = PyTuple_New( 3 );
				PyTuple_SET_ITEM( tuple, 0, *obj );
				PyTuple_SET_ITEM( tuple, 1, PyString_FromStringAndSize(P->oobData, P->oobLen) );
				PyTuple_SET_ITEM( tuple, 2, PyLong_FromLongLong(P->serialNumber)  );
				*obj = tuple;
			}
		}
		else if ( buf )
		{
			memcpy( buf, realdata, realLen );
		}
		ret = realLen;
	}
	else
	{
		// popHeadPacket() guarantees us <= len data
		ret = P->packetLen;

		emitStatusMessage( "TCP bytes[%d] received for [%d] {%d}", ret, (int)fd, thread );

		if ( obj )
		{
			*obj = PyString_FromStringAndSize( P->data, P->packetLen );
		}
		else if ( buf )
		{
			memcpy( buf, P->data, P->packetLen );
		}
	}

	freePacket( P ); // done with it

	D_RECVEX(ciolog( "recvEx got object[%p] size[%d]\n", obj?*obj:0, obj?PyString_Size(*obj):0 ));

	DEC_COMPLETION_REF( completion );
	
	return ret;
}

//------------------------------------------------------------------------------
CarbonIO::SCompletionUnit* CarbonIO::addToCio( HANDLE fd )
{
	D_ADDTOCIO(ciolog("adding handle [%lld]:[0x%016X]", (long long)fd, (long long)fd ));
	
	// link into our user-space awareness, before clearing the system to send events

	m_completionListLock.readLock();
	if ( m_completionList.get( (long long)fd ) )
	{
		m_completionListLock.unlock();

		D_ADDTOCIO(ciolog("fd already exists for[%d] with[%d]", (int)fd, WSAGetLastError()));
		// the fd already exists? This should be impossible, something
		// has gone horribly wrong

		return 0;
	}
	m_completionListLock.unlock();

	if ( CreateIoCompletionPort(fd, m_completionHandle, 0, c_concurrentCioThreads) != m_completionHandle )
	{
		D_ADDTOCIO(ciolog("iocompletion port add failed for[%d] with[%d]", (int)fd, WSAGetLastError()));

		// couldn't add to our completion port? very bad magic.
		return 0;
	}

	// now that all the operations are known to succeed, add the record
	
	SCompletionUnit *completion = new(std::nothrow) SCompletionUnit;

	D_ADDTOCIO(ciolog("fd[%d] added", (int)fd));

	memset( completion, 0, sizeof(SCompletionUnit) ); // burn to the water line
	
	InitializeCriticalSection( &completion->unitLock );
	InitializeCriticalSection( &completion->sendLock );
	completion->workHandle = fd;
	completion->writeCompletion.send = true;
	completion->writeCompletion.parent = completion;

	completion->refCount = 1; // Python's reference. released only by a close()
#ifndef DESTROY_ON_DEC
	completion->refReachedZero = CreateEvent( 0, true, false, 0 );
#endif

	m_completionListLock.writeLock();
	m_completionList.addItem( completion, (long long)fd );
	m_completionListLock.unlock();

	m_stats.totalSockets = m_completionList.count();
	m_stats.activeSockets = m_completionList.count();

	return completion;
}

//------------------------------------------------------------------------------
// The job chain provided is in the expected order of wakeup, i.e. first
// wakeup job if at the head of the chain.
void CarbonIO::wakeUpTaskletChain( SJob* job, bool force /*=false*/ )
{
	if ( !job || !m_running )
	{
		return;
	}

	// Mark the submission time for this job.  This is the time that the job is 'ready'
	// from our point of view.  The rest is delay in waking up Python
	{
		long long time = m_singleton.getPerformanceCounter();
		SJob *j = job;
		while( j )
		{
			j->t0 = time;
			j = j->next;
		}
	}

	EnterCriticalSection( &m_wakeUpQueueLock );
	if ( m_wakeUpQueue )
	{
		// Insert the jobs into the queue.  The queue is maintained in reverse order.
		while( job )
		{
			SJob *next = job->next;
			
			m_stats.GILCoalesced++;

			// a thread is already waiting for the GIL, put our request in
			job->next = m_wakeUpQueue;
			m_wakeUpQueue = job;
			job = next;
		}

		LeaveCriticalSection( &m_wakeUpQueueLock );
	}
	else
	{
		// we are the first to be here, start a line and grab the GIL
		m_wakeUpQueue = job;

		LeaveCriticalSection( &m_wakeUpQueueLock );
		
		m_gilAcquireTimeTemp = getPerformanceCounter(); // time how long this takes

		if ( m_singleton.m_wakeupMethod == WAKEUP_PENDING_CALL )
		{
			if ( Py_AddPendingCall( &wakeUpTaskletsEx, (void *)(long long)force ) == -1 )
			{
				// fallback to dynamic context (extra CPU but safe)
				InterlockedIncrement( &m_singleton.m_stats.pendingCallQueueFailure );

				PyGILState_STATE pyGILState = PyGILState_Ensure();
				wakeUpTaskletsEx( force ? (void*)1 : (void*)0 ); // wake up everyone who was waiting
				PyGILState_Release( pyGILState );
			}
			else
			{
				m_singleton.onTaskletScheduled( false ); // only downside of the above is we need to make sure the scheduler is running
			}
		}
		else // WAKEUP_DYNAMIC_CONTEXT
		{
			PyGILState_STATE pyGILState = PyGILState_Ensure();
			wakeUpTaskletsEx( force ? (void*)1 : (void*)0 ); // wake up everyone who was waiting
			PyGILState_Release( pyGILState );
		}
	}
}

//------------------------------------------------------------------------------
void CarbonIO::dataReceived( SCompletionUnit *completion, const char* data, const unsigned int len )
{
	D_HANDLEREAD(ciolog("into handle read for[%d] with [%d]", (int)completion->workHandle, len ));
	
	if ( !completion->packetListHead ) // any packet containers ready to recieve data?
	{
		D_HANDLEREAD(ciolog("not working packet [%d] recieving [%d]", (int)completion->workHandle, len ));
		completion->packetListHead = m_singleton.getPacket();
		completion->packetListTail = completion->packetListHead;
	}

	completion->queuedIncomingBytes += len;

	if ( !completion->isPacketConnection )
	{
		D_HANDLEREAD(ciolog("[%d] not a packet connection, just copying[%d] to the tail now[%d]", (int)completion->workHandle, len, completion->packetListTail->packetLen + len));
		emitStatusMessage( "[%p] not a packet connection, just copying[%d] to the tail now[%d]", completion->workHandle, len, completion->packetListTail->packetLen + len);

		// receiving stream data, just copy it in and we're done
		appendDataToPacket( completion->packetListTail, data, len );
		return;
	}

	if ( isPacketValid(completion->packetListTail) )
	{
		// a valid, complete packet is sitting on the tail (has not
		// been consumed yet) create another one to fill in
		
		D_HANDLEREAD(ciolog("valid packet on the tail, adding for [%d] recieving [%d]", (int)completion->workHandle, len ));
		emitStatusMessage( "valid packet on the tail, adding for [%p] recieving [%d]", completion->workHandle, len );
		completion->packetListTail->next = m_singleton.getPacket();
		completion->packetListTail = completion->packetListTail->next;
	}

	const char *indat = data;
	unsigned int available = len;
	unsigned int packetSize;
	unsigned int need;

	// de-packetize
	do
	{
		// not enough to have a complete size? copy in what we've got
		if ( (completion->packetListTail->packetLen + available) < sizeof(int) )
		{
			D_HANDLEREAD(ciolog("not enough[%d]+[%d] to get packet size on [%d]", completion->packetListTail->packetLen, available, (int)completion->workHandle ));
			emitStatusMessage( "not enough[%d]+[%d] to get packet size on [%p]", completion->packetListTail->packetLen, available, completion->workHandle );
			appendDataToPacket( completion->packetListTail, indat, available );
			break;
		}

		if ( completion->packetListTail->packetLen )
		{
			// do we have fewer than an int in here? better copy enough
			// in to get a header at least
			if ( completion->packetListTail->packetLen < sizeof(int) )
			{
				int deficit = sizeof(int) - completion->packetListTail->packetLen;

				D_HANDLEREAD(ciolog("not enough for header[%d] appending[%d] to[%d]", completion->packetListTail->packetLen, deficit, (int)completion->workHandle ));
				emitStatusMessage( "not enough for header[%d] appending[%d] to[%p]", completion->packetListTail->packetLen, deficit, completion->workHandle );

				appendDataToPacket( completion->packetListTail, indat, deficit );
				indat += deficit;
				available -= deficit;
			}

			// a partial packet has been recieved, call for completion
			packetSize = (ceHeaderSizeMask & *(int *)completion->packetListTail->data) + sizeof(int);
			need = packetSize - completion->packetListTail->packetLen;

			D_HANDLEREAD(ciolog("working packet[%d:%d] need[%d] avail[%d] for [%d]", packetSize, completion->packetListTail->packetLen, need, available, (int)completion->workHandle ));
			emitStatusMessage( "working packet[%d:%d] need[%d] avail[%d] for [%p]", packetSize, completion->packetListTail->packetLen, need, available, completion->workHandle );
		}
		else
		{
			// packet is empty, call for the full amount
			packetSize = (ceHeaderSizeMask & *(int *)indat) + sizeof(int);
			need = packetSize;

			D_HANDLEREAD(ciolog("not working packet, needing full amount[%d] for[%d] [0x%08X]:[0x%08X]", need, (int)completion->workHandle, *(unsigned int *)indat, ceHeaderSizeMask & *(int *)indat));
			emitStatusMessage( "not working packet, needing full amount[%d] for[%p] [0x%08X]:[0x%08X]", need, completion->workHandle, *(unsigned int *)indat, ceHeaderSizeMask & *(int *)indat );
		}

		// don't even try to receive a packet that claims to be too large, just flush
		if ( packetSize > completion->maxPacketSize )
		{
			emitStatusMessage( "packet too large on [%p] [%d>%d]", completion->workHandle, packetSize, completion->maxPacketSize );
			completion->packetListTail->packetLen = 0; // eat it
			return;
		}

		if ( need <= available ) // enough data is available to complete a packet
		{
			completion->stats.packetsReceived++;
			m_singleton.m_stats.packetsReceived++;

			appendDataToPacket( completion->packetListTail, indat, need );
			available -= need;
			indat += need;

			unsigned int header = *(int *)completion->packetListTail->data;
			if ( header & ceHeaderExpectPayloadOffset )
			{
				completion->packetListTail->oobData = sizeof(int)*2 + completion->packetListTail->data;
				completion->packetListTail->oobLen = *(int *)(completion->packetListTail->data + sizeof(int));
				completion->packetListTail->payloadOffset = sizeof(int)*2 + completion->packetListTail->oobLen;

				if ( completion->packetListTail->payloadOffset > completion->packetListTail->packetLen )
				{
					// impossible, packet is corrupt
					emitErrorMessage( "Corrupt message received on [%p] [%d>%d]", completion->workHandle, completion->packetListTail->payloadOffset, completion->packetListTail->packetLen );
					completion->packetListTail->packetLen = 0; // eat it
					return;
				}
			}
			else
			{
				completion->packetListTail->oobData = 0;
				completion->packetListTail->oobLen = 0;
				completion->packetListTail->payloadOffset = sizeof(int);
			}

			if ( m_singleton.decompress(completion->packetListTail) )
			{
				completion->stats.bytesReceivedDecompressed += completion->packetListTail->auxDataLen;
				completion->stats.bytesReceived -= completion->packetListTail->auxDataLen;
			}
			if ( !completion->packetListTail->packetLen ) // compress fail?
			{
				emitErrorMessage( "Corrupt packet, unable to decompress for[%p] [%d:%d]", completion->workHandle, completion->packetListTail->payloadOffset, completion->packetListTail->packetLen );
				completion->packetListTail->packetLen = 0; // eat it
				return;
			}

			for( SCallbackEntry *callback = m_singleton.m_packetCallbackChainPostDecompress; callback ; callback = callback->next )
			{
				D_POSTDECOMPRESS(ciolog("checking post callback"));
				bool stop = true;
				if ( completion->packetListTail->auxData )
				{
					stop = callback->callback((long long)completion->workHandle,
											  completion->packetListTail->auxData,
											  completion->packetListTail->auxDataLen,
											  completion->packetListTail->oobData,
											  completion->packetListTail->oobLen );
				}
				else
				{
					stop = callback->callback((long long)completion->workHandle,
											  completion->packetListTail->data + completion->packetListTail->payloadOffset,
											  completion->packetListTail->packetLen - completion->packetListTail->payloadOffset,
											  completion->packetListTail->oobData,
											  completion->packetListTail->oobLen );
				}

				if ( stop )
				{
					emitStatusMessage( "bluenet wanted the packet for[%p]", completion->workHandle );
					freeAuxData( *completion->packetListTail );
					completion->packetListTail->packetLen = 0;
					break;
				}
			}
			if ( !completion->packetListTail->packetLen )
			{
				// callback was triggered, loop back
				continue;
			}

			completion->packetListTail->serialNumber = ++completion->packetSerialNumber; // tag this packet with a serial number if anyone cares

			emitStatusMessage( "packet #%d parsed: aux[%d] plen[%d] payload[%d]", completion->packetListTail->serialNumber, completion->packetListTail->auxDataLen, completion->packetListTail->packetLen, completion->packetListTail->packetLen - completion->packetListTail->payloadOffset );

			if ( available )
			{
				// full packet now exists on tail, link another one to it
				completion->packetListTail->next = m_singleton.getPacket();
				completion->packetListTail = completion->packetListTail->next;
			}
		}
		else
		{
			// have part of a packet, copy it all in and bail
			appendDataToPacket( completion->packetListTail, indat, available );
			
			D_PACKET(ciolog("partial packet [%d] partial[0x%08X] total[0x%08X]", available, Ccp::Hash(indat, available), Ccp::Hash(completion->packetListTail->data, completion->packetListTail->packetLen)));
			emitStatusMessage( "partial packet [%d] partial[0x%08X] total[0x%08X]", available, Ccp::Hash(indat, available), Ccp::Hash(completion->packetListTail->data, completion->packetListTail->packetLen));
			available = 0;
		}

	} while( available > 0 );
}

//------------------------------------------------------------------------------
// Reverse a chain of jobs
void CarbonIO::reverseJobList( SJob* &job )
{
	if ( job == NULL )
	{
		return;
	}
	
	SJob *prev = NULL;
	for( ; ; )
	{
		SJob *next = job->next;
		job->next = prev;
		if ( !next )
		{
			return;
		}
		prev = job;
		job = next;
	}
}

//------------------------------------------------------------------------------
// This is called from outside, such as when the application wakes up, to make
// sure that any pending wakeups are performed.
void CarbonIO::wakeupTasklets()
{
	// the force=false here means that we don't break wait of the application.
	// It is an external call from the application to CarbonIO, not an internal
	// one requiring the application to be alerted.
	wakeUpTaskletsEx( (void*)0 );
}

//------------------------------------------------------------------------------
int CarbonIO::wakeUpTaskletsEx( void* arg )
{
	if ( !m_singleton.m_wakeUpQueue ) // fast return of the trivial case
	{
		return 0;
	}

	bool force = arg ? true : false;
	// do the actual work of running through the wakeupQueue and waking
	// tasks up
	if ( m_singleton.m_running )
	{
		m_singleton.m_stats.GILAcquiredNum++;
		if ( m_singleton.m_gilAcquireTimeTemp )
		{
			m_singleton.m_gilAcquireTimeTemp = m_singleton.getPerformanceCounter() - m_singleton.m_gilAcquireTimeTemp;
			if ( m_singleton.m_gilAcquireTimeTemp > 0 )
			{
				m_singleton.m_stats.GILAcquireTime += m_singleton.m_gilAcquireTimeTemp;

				if ( m_singleton.m_gilAcquireTimeTemp > m_singleton.m_stats.longestGILAcquireTime )
				{
					m_singleton.m_stats.longestGILAcquireTime = (long)m_singleton.m_gilAcquireTimeTemp;
				}
			}

			m_singleton.m_gilAcquireTimeTemp = 0;
		}

		Ccp::CriticalLockScoped lock( m_singleton.m_wakeUpQueueLock );

		if ( m_singleton.m_wakeUpQueue )
		{
			// Reverse, so that jobs are woken up in the order of insertion.
			reverseJobList( m_singleton.m_wakeUpQueue );
			SJob *next;
			do
			{
				next = m_singleton.m_wakeUpQueue->next; // as soon as it is woken up we no longer own it, pre-fetch next
				D_WAKEUP(ciolog( "Waking up <1> [%p]", m_singleton.m_wakeUpQueue->tasklet ));
				emitStatusMessage( "Waking up {%d} [%p]", m_singleton.m_wakeUpQueue->addrlen, m_singleton.m_wakeUpQueue->tasklet );
				m_singleton.wakeupEx( m_singleton.m_wakeUpQueue );
				
			} while( (m_singleton.m_wakeUpQueue = next) );
		}

		m_singleton.onTaskletScheduled( force );
	}

	return 0;
}

//------------------------------------------------------------------------------
CarbonIO::SJob* CarbonIO::getJob()
{
	SJob* job;

	static long long s_serial = 1;

	if ( !m_jobFreeList )
	{
		m_singleton.m_stats.jobPoolSize = InterlockedIncrement( &m_singleton.m_jobPoolSize );
		job = new(std::nothrow) SJob;
	}
	else
	{
		EnterCriticalSection( &m_jobFreeLock );
		job = m_jobFreeList;
		m_jobFreeList = m_jobFreeList->next;
		LeaveCriticalSection( &m_jobFreeLock );
	}

	job->oorReported = false;
	job->serial = s_serial++;
	
	m_stats.outstandingJobs = InterlockedIncrement( &m_outstandingJobs );

	return job;
}

//------------------------------------------------------------------------------
void CarbonIO::freeJob( SJob* job )
{
	m_stats.outstandingJobs = InterlockedDecrement( &m_outstandingJobs );
	D_JOBFIFO(ciolog("Freeing job[%p]", job));

	EnterCriticalSection( &m_jobFreeLock );
	job->next = m_jobFreeList;
	job->serial = 0;
	m_jobFreeList = job;
	LeaveCriticalSection( &m_jobFreeLock );
}

//------------------------------------------------------------------------------
void CarbonIO::releaseJob( SJob* job )
{
	// If job->tasklet is non-zero, then this job was not woken up by CarbonIO
	// We will then clear job->tasklet (to indicate to CarbonIO that we have woken
	// up early, and leave the job alone (so that the job doesn't disappear
	// from underneath the workerthread's feet.
	if ( job->tasklet )
	{
		job->serial = 0;
		Py_CLEAR( job->tasklet );
	}
	else
	{
		// A successful wakeup; The job was ours and we must reap it.
		freeJob( job );
	}
}

//------------------------------------------------------------------------------
void CarbonIO::installPacketizer( SCompletionUnit* completion )
{
	if ( completion->isPacketConnection )
	{
		return;
	}
	completion->isPacketConnection = true;

	if ( !completion->maxPacketSize )
	{
		completion->maxPacketSize = c_defaultMaxPacketSize;
	}

	// Any data already been received before we discovered it was
	// suppose to be packetized? Run it through now as if it were just
	// received, and the right thing will happen
	SPacket *tempHead = completion->packetListHead;
	completion->packetListHead = 0;
	completion->packetListTail = 0;
	while( tempHead )
	{
		dataReceived( completion, tempHead->data, tempHead->packetLen );
		SPacket *N = tempHead->next;
		freePacket( tempHead );
		tempHead = N;
	}
}

//------------------------------------------------------------------------------
bool CarbonIO::prepareSequence( PyObject *obj, SPacket *packet )
{
	PyObject *fast = PySequence_Fast(obj, "sequence required");
	for (int i = 0; i<PySequence_Fast_GET_SIZE(fast); i++)
	{
		D_SEQUENCE(ciolog("preparing fast items [%d]", i));
		
		char *buf;
		int len;
		PyObject *objInner = PySequence_Fast_GET_ITEM( fast, i );
		if ( !PyArg_Parse(objInner, "s#", &buf, &len) )
		{
			if ( PySequence_Check(objInner) )
			{
				D_SEQUENCE(ciolog("parse failed, but it was a sequence"));
				PyErr_Clear();
				prepareSequence(objInner, packet);
				continue;
			}
			else
			{
				D_SEQUENCE(ciolog("parse failed, and it wasn't a sequence"));
				return false;
			}
		}	

		D_SEQUENCE(ciolog("appending [%d] bytes", len ));
		appendDataToPacket( packet, buf, len );
	}

	D_SEQUENCE(ciolog("returning [%d]", packet->packetLen));
	return true;
}

//------------------------------------------------------------------------------
int CarbonIO::triggerRead( SCompletionUnit* completion )
{
	// Don't wait to be asked to receive data before shifting it off
	// the TCP/IP stack into user space. This may be called once and
	// ONLY ONCE until a completion event with data is processed, since
	// it presumes exclusive ownership of the OVERLAPPED portion of the
	// CompletionUnit

	if ( completion->dead || completion->reap )
	{
		D_TRIGGERREAD(ciolog("tried to trigger read on a dead[%d:%s] descriptor[%d]", completion->dead, completion->reap ? "reap":"active", (int)completion->workHandle));;
		return WSAENOTSOCK;
	}
	
	WSABUF buffer;
	buffer.buf = completion->buf;
	buffer.len = c_systemPageSize; 
	ULONG flags = 0;

	memset( completion, 0, sizeof(OVERLAPPED) ); // make sure the overlapped portion is nulled out
	completion->fromlen = sizeof(completion->from);
	DWORD dummy; // so winXP does not crash, it needs this pointer to be valid
	if ( completion->udpConnection ?
		 WSARecvFrom((SOCKET)completion->workHandle, &buffer, 1, &dummy, &flags, (sockaddr*)&completion->from, &completion->fromlen, completion, 0)
								   : WSARecv((SOCKET)completion->workHandle, &buffer, 1, &dummy, &flags, completion, 0) )
	{
		// it is possible that the error was 'nobuffs' indicating that
		// the kernel can't lock c_systemPageSize memory for the read,
		// re-issue the read with a 0 length to allieviate this
		
		int err = WSAGetLastError();

		if ( err == WSAENOBUFS )
		{
			completion->wasZeroByteRead = true;
			buffer.len = 0;
			if ( WSARecv((SOCKET)completion->workHandle, &buffer, 1, &dummy, &flags, completion, 0)
				 && WSAGetLastError() != WSA_IO_PENDING )
			{
				D_TRIGGERREAD(ciolog("WSARecv failed for[%d] w/[%d]", (int)completion->workHandle, err));

				completion->dead = err;
				return err;
			}
			else
			{
				m_singleton.m_stats.zeroByteReadsRequired++;
			}
		}
		else if ( err != WSA_IO_PENDING )
		{
			D_TRIGGERREAD(ciolog("WSARecv failed for trigger with [%d] on [%d]", err, (int)completion->workHandle));
			completion->dead = err;
			return err;
		}

		D_TRIGGERREAD(ciolog("triggered read for [%d] got err[%d]", (int)completion->workHandle, err));
	}
	else
	{
		D_TRIGGERREAD(ciolog("triggered read for [%d]", (int)completion->workHandle));
	}

	completion->readTriggered = true;
	return 0;
}

//------------------------------------------------------------------------------
void CarbonIO::destroyCompletionUnit( SCompletionUnit *completion )
{
	// wait for any sends to complete
	while( completion->sendRunning )
	{
		m_stats.shutdownSpinWaitCount++;
		D_REAPSOCKET(ciolog("reap waiting on pending send for[%d]", (int)completion->workHandle));
		Sleep( 10 ); 
	}

	// pull it out of the list
	m_completionListLock.writeLock();
	m_completionList.remove( (long long)completion->workHandle );
	m_completionListLock.unlock();

	m_stats.totalSockets = m_completionList.count();
	m_stats.activeSockets = m_completionList.count();

	emitStatusMessage( "[%p] removed from completion list, moving on to final destruction", completion->workHandle );

	// set to blocking so the connection is closed cleanly
	unsigned long noblock = 0;
	ioctlsocket( (SOCKET)completion->workHandle, FIONBIO, &noblock );
	::shutdown( (SOCKET)completion->workHandle, SD_BOTH );
	::closesocket( (SOCKET)completion->workHandle ); // do the actual close now since the below might wait a LONG time for GC to fixup
	D_REAPSOCKET(ciolog("[%d] closed", (int)completion->workHandle));

	DEC_COMPLETION_REF( completion ); // should be the last reference, and clean up memory but if its not, wherever it IS dec'ed will do it

	// the object has been removed from the list (it can no longer be
	// found) but we must wait for anyone using it to return their
	// reference before we can demolish the internals

#ifndef DESTROY_ON_DEC
	if ( completion->refCount ) // anyone?
	{
		D_REF(ciolog("waiting for [%d] ref count to reach zero from [%d]", (int)completion->workHandle, completion->refCount));
		while( WaitForSingleObject(completion->refReachedZero, 3000) == WAIT_TIMEOUT )
		{
			ciolog("[%p] taking longer than normal to free its last reference", completion->workHandle );
		}
	}
	
	deleteCompletionUnit( completion ); // wipe it out
#endif
}

//------------------------------------------------------------------------------
bool CarbonIO::processRawQueuedData( SCompletionUnit *completion )
{
	Ccp::CriticalLockScoped slock( completion->sendLock );

	// udp connections shift out one packet at a time
	if ( completion->udpConnection )
	{
		completion->queuedData = completion->rawQueuedDataHead;
		if ( completion->rawQueuedDataHead )
		{
			completion->rawQueuedDataHead = completion->rawQueuedDataHead->next;
			if ( !completion->rawQueuedDataHead )
			{
				completion->rawQueuedDataTail = 0;
			}

			return true;
		}
		else
		{
			completion->sendRunning = false;
			return false;
		}
	}

	// do not queue up data unless the initial ssl handshake has completed (or there is no ssl at all)
	if ( completion->ssl && !completion->ssl->handshakeComplete() )
	{
		completion->sendRunning = false;
		return false;
	}

	// attach list to the queue
	if ( !(completion->queuedData = completion->rawQueuedDataHead) )
	{
		D_PROCESSRAW(ciolog("processRawQueuedData: dataHead is null"));
		completion->sendRunning = false;
		return false;
	}

	completion->sendRunning = true; // explicity set it, even though it should be unnecessary
	completion->rawQueuedDataTail = 0;
	completion->rawQueuedDataHead = 0;

	slock.release();

	SPacket *packet = completion->queuedData;

	// traverse the list and queue up the data into a single packet
	// which can be sent out
	while( packet )
	{
		SPacket *next = packet->next;
		completion->queuedBytes -= packet->packetLen;

		// find the offset to the body (the part we compress)
		unsigned int offset = sizeof(unsigned int); // the header itself
		if ( *(unsigned int *)packet->data & ceHeaderExpectPayloadOffset ) // was there out-of-band data?
		{
			offset += sizeof(unsigned int); // the size of the len param
			offset += *(unsigned int *)(packet->data + sizeof(unsigned int)); // the size of the data
		}

		char *outbuf = 0;
		unsigned int outlen;
		unsigned int bodyLen = packet->packetLen - offset;
		
		if ( !packet->streamSend
			 && (bodyLen >= (completion->compressionThreshold ? completion->compressionThreshold : m_compressionThreshold))
			 && checkAndCompress( packet->data + offset, bodyLen, &outbuf, &outlen) )
		{
			completion->stats.bytesSentCompressed += outlen + offset;

			*(unsigned int *)packet->data &= ceHeaderBitsMask; // knock off old size
			*(unsigned int *)packet->data |= (outlen + offset) - sizeof(unsigned int); // plug in NEW size
			*(unsigned int *)packet->data |= m_compressionType;

			// if this is not the root packet, append the data TO the
			// root packet
			if ( packet != completion->queuedData )
			{
				ensurePacketSpace( completion->queuedData, completion->queuedData->packetLen + offset + outlen );
				appendDataToPacket( completion->queuedData, packet->data, offset );
				appendDataToPacket( completion->queuedData, outbuf, outlen );
				freePacket( packet );
			}
			else
			{
				packet->packetLen = offset;
				appendDataToPacket( packet, outbuf, outlen );
			}

			delete[] outbuf;
		}
		else if ( packet != completion->queuedData )
		{
			appendDataToPacket( completion->queuedData, packet->data, packet->packetLen );
			freePacket( packet );
		}

		packet = next;
	}

	completion->queuedData->next = 0;
	
	return true;
}

//------------------------------------------------------------------------------
bool CarbonIO::checkAndCompress( const char* data, const unsigned int len, char **outbuf, unsigned int *outlen )
{
	// compress the data and make sure it meets the minimum compression
	// qualifications
	
	if ( !compress(data, len, outbuf, outlen) )
	{
		D_COMPRESS(ciolog("compression failed"));
		delete[] *outbuf;
		return false;
	}

	if ( *outlen >= ((len * m_compressionMinRatio) / 100) )
	{
		D_COMPRESS(ciolog("compression not enough [%d]>=[%d]", *outlen, ((len * m_compressionMinRatio) / 100)));
		delete[] *outbuf;

		if ( len >= m_compressionThreshold )
		{
			m_stats.compressedRejected += *outlen;
			m_stats.compressedRejectedIn += len;
		}

		m_stats.bytesSent += len;

		return false;
	}

	m_stats.compressedAccepted += *outlen;
	m_stats.compressedAcceptedIn += len;
	m_stats.bytesSentCompressed += *outlen;

	D_COMPRESS(ciolog("compressed in:[%d] out[%d]", len, *outlen));

	return true;
}

//------------------------------------------------------------------------------
void CarbonIO::checkForStaleSSLHandshakes()
{
	struct SReapEntry
	{
		SCompletionUnit *completion;
		SReapEntry *next;
	};
	SReapEntry* reapList = 0;

	// get a list of candidates holding the read lock, but do NOT try
	// and acquire a unitLock until the read lock is released
	EnterCriticalSection( &m_staleSSLHandshakeLock );
	m_completionListLock.readLock();
	for( SCompletionUnit **pcompletion = m_completionList.getFirst() ; pcompletion ; pcompletion = m_completionList.getNext() )
	{
		SCompletionUnit *completion = *pcompletion;

		if ( completion->ssl
			 && completion->handshakeTimeout
			 && (completion->handshakeTimeout < time(0)) )
		{
			SReapEntry *entry = new(std::nothrow) SReapEntry;
			entry->completion = completion;
			entry->next = reapList;
			reapList = entry;
			INC_COMPLETION_REF( completion );
		}
	}
	m_completionListLock.unlock();
	LeaveCriticalSection( &m_staleSSLHandshakeLock );

	while( reapList )
	{
		SReapEntry *next = reapList->next;
		
		reapList->completion->dead = WSAENOTCONN;

		Ccp::CriticalLockScoped ulock( reapList->completion->unitLock );
		SJob *list = getWakeupList( reapList->completion );
		ulock.release();
		
		failAllTasklets( list, WSAENOTCONN );

		DEC_COMPLETION_REF( reapList->completion );
		delete reapList;

		reapList = next;
	}
}

//------------------------------------------------------------------------------
void CarbonIO::handleRead( int size, SCompletionUnit *completion )
{
	CPerformanceTime timeCompress( "worker::handleRead" );
	D_HANDLEREAD(ciolog("reading [%d] in from [%d]", size, (int)completion->workHandle));
	
	// a temp pointer for transparant handling of SSL vs
	// in-place data reads, default to in-place
	char *data = completion->buf;

	char sslBuffer[16384];

	bool handshakeNegotiated = false;

	// if SSL has been enabled then we don't read data directly
	// from the buffer, we inject it to the SSL cipher and read
	// out whatever it tells us is available
	if ( completion->ssl )
	{
		bool waitingForHandshake = !completion->ssl->handshakeComplete();

		D_SSL(ciolog( "injecting [%d] bytes from read [%d][%c]", size, (int)completion->workHandle, completion->ssl->handshakeComplete()?'t':'f'));

		{
			CPerformanceTime timeCompress( "ssl-inject" );
			completion->ssl->injectReceivedData( completion->buf, size ); // inject the raw data
		}
		
		data = sslBuffer; // set up pointers
		size = sizeof(sslBuffer); // size is now a bidirctional parameter (tells how much space in, reports how much is avail out)

		if ( (handshakeNegotiated = (waitingForHandshake && completion->ssl->handshakeComplete())) )
		{
			completion->handshakeTimeout = 0;
		}
	}

	Ccp::CriticalLockScoped ulock( completion->unitLock );

	// SSL processes data in maximum of 16384 sized frames, so a
	// looping structure is required around this part (this will
	// collapse to a single-pass for non-SSL sockets)

	if ( !completion->ssl )
	{
		dataReceived( completion, data, size );
	}
	else
	{
		int dataRead;
		for(;;)
		{
			size = sizeof(sslBuffer); // reset size for next pass

			{
				CPerformanceTime timeCompress( "ssl-read" );
				dataRead = completion->ssl->read( data, &size );
			}

			if ( dataRead )
			{
				dataReceived( completion, data, size );
			}
			else
			{
				break;
			}
		}
	}

	// might have data to deliver and wake a task up with, check and drain
	SJob *list = 0;
	SJob *tail = 0;
	SPacket *packet;
	while( completion->readJobsWaiting )
	{
		packet = popHeadPacket( completion, completion->readJobsWaiting->len );
		if ( !packet )
		{
			break;
		}

		if ( tail )
		{
			tail->next = popHeadReadJob( completion );
			tail = tail->next;
		}
		else
		{
			list = popHeadReadJob( completion );
			tail = list;
		}

		// link at the last minute to prevent lost references
		tail->param1p = packet;
		linkSequenceJob( completion, tail );
		completion->readJobsUnscheduled++;
	}

	if ( list )
	{
		emitStatusMessage( "[%p] readJobsWaiting[%p:%p] packets[%p:%p]",
						   completion->workHandle,
						   completion->readJobsWaiting,
						   completion->readJobsWaitingTail,
						   completion->packetListHead,
						   completion->packetListTail );

		for( SJob *J = list ; J ; J = J->next )
		{
			emitStatusMessage( "packet len[%d] waking up[%p] {%d} job[%p]->[%p]",
							   ((SPacket *)J->param1p)->packetLen,
							   completion->workHandle, J->addrlen, J, J->next );
		}
	}
	else
	{
		emitStatusMessage( "list was null[%p] readJobsWaiting[%p:%p] packets[%p:%p]",
						   completion->workHandle,
						   completion->readJobsWaiting,
						   completion->readJobsWaitingTail,
						   completion->packetListHead,
						   completion->packetListTail );
	}

	// defer actual wakeups for after the lock is released at the
	// bottom of this function
	
	// a receive might have triggered the need for the SSL
	// layer to issue a send (handshake negotiation).
	if ( completion->ssl )
	{
		completion->wsaBuf.buf = sslBuffer;

		size = sizeof(sslBuffer);
		for(;;)
		{
			int cont;
			{
				CPerformanceTime timeCompress( "ssl-getPendingTransmitData" );
				cont = completion->ssl->getPendingTransmitData(sslBuffer, &size);
			}
			if ( !cont )
			{
				break;
			}
			
			D_SSL(ciolog( "cipher wanted to transmit [%d] bytes on [%d] [%c]", size, (int)completion->workHandle, completion->ssl->handshakeComplete()?'t':'f'));

			completion->wsaBuf.len = size;
			completion->sendRunning = true;
			INC_COMPLETION_REF( completion );

			completion->stats.bytesSent += size;
			m_stats.bytesSent += size;

			memset( &(completion->writeCompletion), 0, sizeof(OVERLAPPED) );
			DWORD dummy; // so winXP does not crash, it needs this pointer to be valid
			if ( WSASend( (SOCKET)completion->workHandle, &completion->wsaBuf, 1, &dummy, 0, &(completion->writeCompletion), 0) 
				 && (WSAGetLastError() != WSA_IO_PENDING) )
			{
				completion->sendRunning = false;
				int err = WSAGetLastError();

				SJob *failList = getWakeupList( completion );
				ulock.release();

				emitStatusMessage("err<1>");
					
				wakeUpTaskletChain( list );
				m_singleton.failAllTasklets( failList, err );

				DEC_COMPLETION_REF( completion );
				
				return;
			}

			size = sizeof(sslBuffer);
		}
	}
	
	if ( handshakeNegotiated ) // true once and only once per session (todo-- connection re-negotiation?)
	{
		Ccp::CriticalLockScoped slock( completion->sendLock );
		startSendRunning( completion );
	}

	if ( okayToTriggerRead(completion) )
	{
		// as long as the socket is still alive, ask for more data
		int err = triggerRead( completion );
		if ( err )
		{
			D_HANDLEREAD(ciolog("<1>hande read failed trigger for [%d]", (int)completion->workHandle));

			SJob *failList = getWakeupList( completion );

			ulock.release();
			
			wakeUpTaskletChain( list );
			m_singleton.failAllTasklets( failList, err );

			emitStatusMessage("err<2>");

			DEC_COMPLETION_REF( completion );

			return;
		}
	}
	else // we are not triggering another read at this time (throttling)
	{
		D_HANDLEREAD(ciolog("hande read stalling reads for [%d] [%d]>=[%d]", (int)completion->workHandle, completion->queuedIncomingBytes, c_incomingQueueHaltLevel ));

		completion->readTriggered = false;
		ulock.release();
		DEC_COMPLETION_REF( completion );

		emitStatusMessage("err<3>");

		wakeUpTaskletChain( list );
		return;
	}

	ulock.release();
	wakeUpTaskletChain( list );
}

//------------------------------------------------------------------------------
void CarbonIO::handleUDPRead( int size, SCompletionUnit *completion )
{
	CPerformanceTime timeCompress( "worker::handleUdpRead" );
	D_HANDLEREAD(ciolog("reading [%d] in from [%d] on UDP", size, (int)completion->workHandle));

	Ccp::CriticalLockScoped ulock( completion->unitLock );
	
	SPacket* packet = getPacket();
	appendDataToPacket( packet, completion->buf, size );
	
	SJob *list = 0;

	if ( !completion->packetListHead && completion->readJobsWaiting )
	{
		// no need to link just to unlink below, dispatch the single
		// packet
		D_HANDLEUDPREAD(ciolog( "UDP packet available for[%d]", (int)completion->workHandle));
		SJob *job = popHeadReadJob( completion );
		job->param1p = packet;
		job->next = 0;
		list = job;
	}
	else
	{
		if ( completion->packetListTail )
		{
			completion->packetListTail->next = packet;
		}
		else
		{
			completion->packetListHead = packet;
		}

		completion->packetListTail = packet;

		while( completion->readJobsWaiting && completion->packetListHead )
		{
			SJob *job = popHeadReadJob( completion );
			job->param1p = popHeadPacket( completion, job->len );
			job->next = list;
			list = job;

			linkSequenceJob( completion, job );
			completion->readJobsUnscheduled++;
		}
	}

	if ( triggerRead(completion) )
	{
		D_HANDLEUDPREAD(ciolog("handeUDPread failed trigger for [%d]", (int)completion->workHandle));
		
		int err = WSAGetLastError();
		
		SJob *failList = getWakeupList( completion );
		
		ulock.release();
		
		reverseJobList( list ); //because we built it in reverse order
		wakeUpTaskletChain( list );
		m_singleton.failAllTasklets( failList,  err );

		DEC_COMPLETION_REF( completion );
		return;
	}

	ulock.release();
	
	reverseJobList( list ); //because we built it in reverse order
	wakeUpTaskletChain( list );
}

//------------------------------------------------------------------------------
void CarbonIO::handleSendCompletion( SWriteOverlap *writeCompletion )
{
	CPerformanceTime timeCompress( "worker::handleSendCompletion" );

	SCompletionUnit *completion = writeCompletion->parent;

	Ccp::CriticalLockScoped ulock( completion->unitLock );

	D_HANDLESEND(ciolog("Send completion for [%d]", (int)completion->workHandle));

	// this was the data that was being shifted out, release it
	if( completion->queuedData )
	{
		D_HANDLESEND(ciolog("freeing previous outgoing data [%d]", completion->queuedData->packetLen));
		freePacket( completion->queuedData );
	}

	D_HANDLESEND(ciolog("[%d] bytes prepared to send for [%d]", completion->queuedData ? completion->queuedData->packetLen : 0, (int)completion->workHandle));

	if ( !processRawQueuedData(completion) ) // no data? we're done
	{
		D_HANDLESEND(ciolog("null q"));
							 
		SJob *list = completion->jobsToWakeOnSend;
		completion->jobsToWakeOnSend = 0;

		ulock.release();
		
		reverseJobList( list ); // This list was in reverse order
		wakeUpTaskletChain( list );

		return;
	}

	// check on any inline SSL
	if ( completion->ssl )
	{
		// normal operaton- the SSL has finished negotiating
		// and is ready to be fed data, so inject it

		{
			CPerformanceTime timeCompress( "ssl-write" );
			completion->ssl->write( completion->queuedData->data, completion->queuedData->packetLen );
		}

		// anything squirt out the other side? queue it up into the
		// same packet we just queued it out of
		
		int ret;
		char scratch[16384];
		int scratchSize = sizeof(scratch);
		completion->queuedData->packetLen = 0;
		completion->queuedData->next = 0;

		// can only encode 16384 bytes at a time, loop over full size
		// of the input and q all the data up
		for(;;)
		{
			{
				CPerformanceTime ssl( "ssl-getPendingTransmitData" );
				ret = completion->ssl->getPendingTransmitData( scratch, &scratchSize );
			}
			if ( ret <= 0 )
			{
				break;
			}
			appendDataToPacket( completion->queuedData, scratch, scratchSize );
			scratchSize = sizeof(scratch);
		}

		D_SSL(ciolog("sendSSL cipher queued up[%d] bytes on[%d] [%c]", completion->queuedData->packetLen, (int)completion->workHandle, completion->ssl->handshakeComplete()?'t':'f'));
	}

	// the data has aleady been collected, massaged and encrypted. The
	// result is a single packet sitting on the queuedData pointer,
	// send it on its way
	completion->wsaBuf.buf = completion->queuedData->data;
	completion->wsaBuf.len = completion->queuedData->packetLen;
	completion->stats.bytesSent += completion->wsaBuf.len;
	m_stats.bytesSent += completion->wsaBuf.len;
	memset( writeCompletion, 0, sizeof(OVERLAPPED) );
	INC_COMPLETION_REF( completion );

	int ret = 0;
	DWORD dummy; // so winXP does not crash, it needs this pointer to be valid
	if ( completion->udpConnection )
	{
		ret = WSASendTo( (SOCKET)completion->workHandle,
						 &completion->wsaBuf,
						 1,
						 &dummy,
						 completion->queuedData->flags,
						 &completion->queuedData->addr,
						 completion->queuedData->addrlen,
						 writeCompletion,
						 0 );
	}
	else
	{
		ret = WSASend( (SOCKET)completion->workHandle,
					   &completion->wsaBuf,
					   1,
					   &dummy,
					   completion->queuedData->flags,
					   writeCompletion, 0 );
	}

	if ( ret && (WSAGetLastError() != WSA_IO_PENDING) )
	{
		completion->sendRunning = false;
		int err = WSAGetLastError();

		D_HANDLESEND(ciolog("[%d] bytes ERROR[%d] SEND for [%d]", completion->queuedData->packetLen, err, (int)completion->workHandle));

		completion->dead = err;
		freePacket( completion->queuedData );
		completion->queuedData = 0;

		SJob *list = getWakeupList( completion );
		ulock.release();

		DEC_COMPLETION_REF( completion );

		m_singleton.failAllTasklets( list, err );
	}
	else
	{
		D_HANDLESEND(ciolog("[%d] bytes sent for [%d]", completion->queuedData->packetLen, (int)completion->workHandle));

		reverseJobList( completion->jobsToWakeOnSend );
		SJob* onSendCompletion = completion->jobsToWakeOnSend;
		completion->jobsToWakeOnSend = 0;

		ulock.release();

		wakeUpTaskletChain( onSendCompletion );
	}
}

//------------------------------------------------------------------------------
bool CarbonIO::compress( const char *in, unsigned int inLen, char **out, unsigned int *outLen )
{
	if ( !in || !inLen || !out || !outLen || (inLen > c_maxCompressibleSize) )
	{
		return false;
	}

	if ( m_compressionType == ceHeaderBitZlibCompressed )
	{
		CPerformanceTime timeCompress( "zlib-compress" );

		// Zlib requires 100.1% of in, plus 12 bytes, available in the
		// output, add a little more for safety
		unsigned int maxlen = inLen + (inLen / 1000) + 12 + 1 + sizeof(unsigned int);
		
		// embed the decompressed size for faster decompression (easier allocation)
		*out = new(std::nothrow) char[ maxlen ];
		*(unsigned int *)(*out) = inLen;
		
		z_stream c_data;

		c_data.next_in = reinterpret_cast<unsigned char *>(const_cast<char *>(in));
		c_data.avail_in = inLen;
		c_data.next_out = reinterpret_cast<unsigned char *>(*out + sizeof(unsigned int));
		c_data.avail_out = maxlen - sizeof(unsigned int);

		c_data.zalloc = Z_NULL;
		c_data.zfree = Z_NULL;
		c_data.opaque = Z_NULL;

		if ( deflateInit(&c_data, m_compressionLevel) )
		{      
			delete [] *out;
			*out = 0;
			return false;
		}

		if ( deflate(&c_data, Z_FINISH) != Z_STREAM_END )
		{      
			deflateEnd(&c_data);
			delete [] *out;
			*out = 0;
			return false;
		}

		if ( deflateEnd(&c_data) != Z_OK )
		{      
			delete [] *out;
			*out = 0;
			return false;
		}

		*outLen = c_data.total_out + sizeof(unsigned int);
		return true;
	}
	else if ( m_compressionType == ceHeaderBitSnappyCompressed )
	{
		// no longer supported
		return false;
	}

	return false;
}

//------------------------------------------------------------------------------
bool CarbonIO::decompress( SPacket* packet )
{
	if ( *(unsigned int *)packet->data & ceHeaderBitZlibCompressed )
	{
		CPerformanceTime timeCompress( "decompress" );

		// pull out the embedded size
		char* in = packet->data + packet->payloadOffset;
		unsigned int inLen = packet->packetLen - packet->payloadOffset;

		packet->auxDataLen = *(unsigned int *)in;
		D_DECOMPRESS(ciolog("found size [%d]", packet->auxDataLen));
		if ( packet->auxDataLen > c_maxCompressibleSize ) // sane?
		{
			D_DECOMPRESS(ciolog("decompress size out of range [%d]", packet->auxDataLen ));
			packet->auxData = 0;
			packet->auxDataLen = 0;
			packet->packetLen = 0;
			return false;
		}

		packet->auxData = new(std::nothrow) char[ packet->auxDataLen ];
		
		z_stream c_data;

		c_data.next_in = reinterpret_cast<unsigned char *>(const_cast<char *>(in + sizeof(unsigned int)));
		c_data.avail_in = inLen - sizeof(unsigned int);
		c_data.next_out = reinterpret_cast<unsigned char *>(packet->auxData);
		c_data.avail_out = packet->auxDataLen;
		c_data.zalloc = Z_NULL;
		c_data.zfree = Z_NULL;
		c_data.opaque = Z_NULL;

		if ( inflateInit(&c_data) )
		{
			D_DECOMPRESS(ciolog("fail<1>"));
			delete[] packet->auxData;
			packet->auxData = 0;
			packet->auxDataLen = 0;
			packet->packetLen = 0;
			return false;
		}

		int ret;
		if ( ( ret = inflate(&c_data, Z_NO_FLUSH)) != Z_STREAM_END )
		{
			D_DECOMPRESS(ciolog("fail<2>"));
			inflateEnd( &c_data );
			delete[] packet->auxData;
			packet->auxData = 0;
			packet->auxDataLen = 0;
			packet->packetLen = 0;
			return false;
		}

		if ( packet->auxDataLen != c_data.total_out )
		{
			D_DECOMPRESS(ciolog("fail<3>"));
			delete[] packet->auxData;
			packet->auxData = 0;
			packet->auxDataLen = 0;
			packet->packetLen = 0;
			return false;
		}

		if ( inflateEnd(&c_data) != Z_OK )
		{
			D_DECOMPRESS(ciolog("fail<4> [%d]", inflateEnd(&c_data)));
			delete[] packet->auxData;
			packet->auxData = 0;
			packet->auxDataLen = 0;
			packet->packetLen = 0;
			return false;
		}

		D_DECOMPRESS(ciolog("zlib decompressed in[%d:0x%08X] out[%d:%.2f%%]",
							 packet->packetLen - packet->payloadOffset, Ccp::Hash(packet->data + packet->payloadOffset, packet->packetLen - packet->payloadOffset), packet->auxDataLen, (100.f * (float)packet->auxDataLen / (float)(packet->packetLen - packet->payloadOffset))));
		m_stats.bytesReceivedDecompressed += packet->auxDataLen;
		m_stats.bytesReceived -= packet->auxDataLen;
		return true;
	}
	else if ( *(unsigned int *)packet->data & ceHeaderBitSnappyCompressed )
	{
		// no longer supported
		return false;
	}

	return false; // data was not compressed
}

//------------------------------------------------------------------------------
void CarbonIO::emitErrorMessage( const char* format, ... )
{
	if ( !m_singleton.m_errorMessageCallback || !format || !format[0] )
	{
		return;
	}

	char temp[8000];

	va_list arg;
	va_start( arg, format );
	int len = vsprintf( temp, format, arg );
	va_end ( arg );

	m_singleton.m_errorMessageCallback( temp );
}

//------------------------------------------------------------------------------
void CarbonIO::emitStatusMessage( const char* format, ... )
{
	if ( !m_singleton.m_statusMessageCallback || !format || !format[0] )
	{
		return;
	}

	char temp[8000];

	va_list arg;
	va_start( arg, format );
	int len = vsprintf( temp, format, arg );
	va_end ( arg );
	
	m_singleton.m_statusMessageCallback( temp );
}

#ifdef DISABLE_CIO_OPTIMIZATIONS
#pragma optimize( "",  on )
#endif
