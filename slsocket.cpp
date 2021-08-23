/*
 * slsocket.cpp
 * This file defines the classes and functions needed to implement stacklessio.socket on top
 * of the stacklessio frameworks.  Its functions are exposed in slsocket.h and called
 * from the modifiled stackless/socketmodule.c
 * Also implements slsocket_select() for stakclessio.select and stackless/selectmodule.c
 */
#include "StdAfx.h"
#include "socket_semantics.h"
#ifdef SLSOCKET

#ifdef _WIN64
#pragma pack(push, 16)
#else
#pragma pack(push, 8)
#endif
#include <winsock2.h> // this overrides subsequent includes of winsock.h
#include <mstcpip.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#pragma pack(pop)

#include "stacklessio.h"
#include "slsocket.h"
#include <crtdbg.h>

#include <intrin.h>
#pragma intrinsic(_InterlockedIncrement, _InterlockedDecrement, _InterlockedCompareExchange64)


using Ccp::Mutex;
using Ccp::MutexOwner;
using Ccp::PyObjectPtr;
using Ccp::SystemError;
using Ccp::PyCheck;
using Ccp::PyError;
using Ccp::PyAllowThreads;

extern "C" PyObject *socket_timeout = NULL;

// Temporary debugging features
static int slsockVersion = 1;
static Py_ssize_t slsockAllocChunkSize = 1024*1024;
static int defaultRCVBUF = -1;
static int defaultSNDBUF = -1;

// global socket behaviour
static bool defaultBlockingSend = true;


//A socket holding thingie.
class _Socket
{
public:
	typedef SOCKET _type;
	static const SOCKET Invalid() {return INVALID_SOCKET;}
    static const bool IsInvalid(SOCKET s) {return s == INVALID_SOCKET; }
	static void Close(SOCKET s) {closesocket(s);}
};
typedef Ccp::Singlestore<_Socket> Socket;


//A specialization of SystemError, which uses WSAGetLastError
class WSAError : public SystemError
{
public:
	WSAError(const char *msg = "") : SystemError(WSAGetLastError(), msg)	{}
	WSAError(DWORD err) : SystemError(err) {}
	WSAError(DWORD err, const char *msg) : SystemError(err, msg) {}
};


//A helper class that "steals" a Py_buffer and keeps it.  We need this
//because the IO Requests must own the buffer.  They can outlive the tasklet
//that issued the IO and thus they must be responsible for releasing the
//buffer when appropriate.
class Py_bufferKeeper : public Py_buffer
{
public:
	//Initialization and copying "steals" from the other.
	Py_bufferKeeper() {
		memset(this, 0, sizeof(*this));
	}
	Py_bufferKeeper(Py_buffer *other) {
		*static_cast<Py_buffer*>(this) = *other;
		memset(other, 0, sizeof(*other));
	}
	Py_bufferKeeper(const Py_bufferKeeper &o){
		/* actually not a const constructor, see below */
		*this = 0;
	}
	Py_bufferKeeper &operator=(const Py_bufferKeeper &o) {
		//we need the const assignment for containers.  OTOH, we need
		//to clear the source, for our semantics to work!  Do this
		//via this const casting trick.
		Py_bufferKeeper &a = const_cast<Py_bufferKeeper &>(o);
		memcpy(this, &a, sizeof(a));
		memset(&a, 0, sizeof(a));
		return *this;
	}
	~Py_bufferKeeper() {Release();}
	void Release() {PyBuffer_Release(this);}
	operator bool () const {return obj != NULL;}
};

// A storage for Bufferkeepers.
class Py_bufferKeeperStore
{
public:
	Py_bufferKeeperStore() {}
private:
	// no copying
	Py_bufferKeeperStore(const Py_bufferKeeperStore &) {}
	Py_bufferKeeperStore &operator=(const Py_bufferKeeperStore &) {}
public:
	void Push(const Py_bufferKeeper &k) {
		if (mKeeper) {
			if (!mKeepers.get())
				mKeepers = std::unique_ptr<std::vector<Py_bufferKeeper> >(new std::vector<Py_bufferKeeper>);
			mKeepers->push_back(k);
		}
	}
	void Release() {
		mKeeper.Release();
		mKeepers.reset();
	}

private:
	Py_bufferKeeper mKeeper;
	std::unique_ptr<std::vector<Py_bufferKeeper> > mKeepers;
};

// A utility class to store WSABuffers
// This is helpful when building stuff to WSASend over the wire.
class WSABUFStore
{
public:
	WSABUFStore() : mNBuffers(0){}
	void Push(const WSABUF &b){
		if (mNBuffers < _countof(mBuffers)) {
			mBuffers[mNBuffers++] = b;
			return;
		}
		if (mNBuffers++ == _countof(mBuffers))
			for (DWORD i=0; i < _countof(mBuffers); ++i)
				mVBuffers.push_back(mBuffers[i]);
		mVBuffers.push_back(b);
	}
	void Push(char *data, ULONG len) {
		WSABUF b;
		b.len = len;
		b.buf = data;
		Push(b);
	}
	operator WSABUF * () {
		if (mVBuffers.size())
			return &mVBuffers[0];
		return mBuffers;
	}
	DWORD Size() const {return mNBuffers;}
	operator DWORD () const {return mNBuffers;}
    
    private:
	// Auto space for two buffers, a vector for more.
	// since this WSABUFStore is typically an auto variable,
	// space isn't so important and we can have the vector a direct
	// member.  It won't allocate until we push to it.
	WSABUF mBuffers[2];
	DWORD mNBuffers;
	std::vector<WSABUF> mVBuffers;
};

struct SockStats
{
	SockStats(bool ignore) : mParent(0) { ignore; Clear(); }
	SockStats() : mParent(&sGlobalStats) { Clear(); }
	void Clear() {
		mBytesReceived = mBytesSent = mPacketsReceived = mPacketsSent = 0;
		mActiveSockets = mTotalSockets = 0 ;
		mAccepts = mConnects = mNoBufs = 0;
	}

	void BytesReceived(LONG bytes) {
		InterlockedAdd(&mBytesReceived, bytes);
		if (mParent)
			mParent->BytesReceived(bytes);
	}

	void BytesSent(LONG bytes) {
		InterlockedAdd(&mBytesSent, bytes);
		if (mParent)
			mParent->BytesSent(bytes);
	}

	void PacketReceived() {
		_InterlockedIncrement(&mPacketsReceived);
		if (mParent)
			mParent->PacketReceived();
	}
	
	void PacketSent() {
		_InterlockedIncrement(&mPacketsSent);
		if (mParent)
			mParent->PacketSent();
	}

	void AddSocket() {
		_InterlockedIncrement(&mActiveSockets);
		_InterlockedIncrement(&mTotalSockets);
	}
	void DeleteSocket() {
		_InterlockedDecrement(&mActiveSockets);
	}

	void Connect() {
		_InterlockedIncrement(&mConnects);
		if (mParent)
			mParent->Connect();
	}
	
	void Accept() {
		_InterlockedIncrement(&mAccepts);
		if (mParent)
			mParent->Accept();
	}

	void Nobuf() {
		_InterlockedIncrement(&mNoBufs);
		if (mParent)
			mParent->Nobuf();
	}

	PyObject *GetStats() {
		return Py_BuildValue("{sL sL si si si si si si si}",
			"BytesReceived", mBytesReceived,
			"BytesSent", mBytesSent,
			"PacketsReceived", mPacketsReceived,
			"PacketsSent", mPacketsSent,
			"ActiveSockets", mActiveSockets,
			"TotalSockets", mTotalSockets,
			"Accepts", mAccepts,
			"Connects", mConnects,
			 "Nobuferr", mNoBufs
		);
	}

private:
	static void InterlockedAdd(__int64 volatile *addend, LONG add) {
		for(;;) {
			__int64 oldval = *addend;
			__int64 newval = oldval + add;
			__int64 result = _InterlockedCompareExchange64(addend, newval, oldval);
			if (result == oldval)
				break;
		}
	}

	__int64 mBytesReceived;
	__int64 mBytesSent;
	LONG mPacketsReceived;
	LONG mPacketsSent;
	LONG mActiveSockets;
	LONG mTotalSockets;
	LONG mAccepts;
	LONG mConnects;
	LONG mNoBufs;
	SockStats * const mParent;
public:
	static SockStats sGlobalStats;
};

		
SockStats SockStats::sGlobalStats(true);


//Extra data that we construct for a socket.  This holds reference count
//and members for the read queue, a critical section, and so on.
//It also keeps the SOCKET for worker threads to use.  This is only
//closed when a special reference count goes to zero, to avoid race
//condition with worker threads when the main thread decides to close it.
class SocketXtra
{
public:
	SocketXtra(Socket _s) :
		mRefcount(0), mSockRef(0),
		mBlockingSend(defaultBlockingSend), mZerobyteReads(false),
		mRecvPacketSequence(0),
		mMaxPacketSize(1024*1024) //one megabyte
	{
		mSocket.Swap(_s);
		if (mSocket.Valid()) {
			SockStats::sGlobalStats.AddSocket();
			++mSockRef;
		}
	}
private:
	~SocketXtra() {
		_ASSERT(mRefcount == 0);
		_ASSERT(!mRecvQueue.size());
		CloseSocket();
	}

public:
	//a convenience function for returning new sockets, while creating a SocketXtra
	//and storing it.
	static SOCKET New(void **xtradata, Socket &_s)
	{
		_ASSERT(xtradata);
		*xtradata = 0;
		SOCKET result = _s.Peek();
		SocketXtra *xtra = new SocketXtra(_s);
		xtra->AddRef();
		*xtradata = (void*)xtra;
		return result;
	}

	//reference management of SOCKET lifetime
	LONG AddRef() {return InterlockedIncrement(&mRefcount);}
	LONG Release() {
		LONG r = InterlockedDecrement(&mRefcount);
		if (!r)
			delete this;
		return r;
	}

private:
	//Socket closing system.  We must be threadsafe when closing
	//sockets, because a new socket with the same fd may be reopened
	//just subsequently, and any worker thread that, for example, queries IOCompletionStatus
	//may get confused.
	//So, we use refcounting for the socket close.
	//This function returns a socket which is either valid or INVALID_SOCKET, and
	//the socket remains valid until it is returned with ReturnSocket()
	//It is used primarily on worker threads, that are in danger of race by the main thread.
	SOCKET GetSocket() {
		LONG ref = InterlockedIncrement(&mSockRef);
		if (ref > 0)
			return mSocket.Peek();
		else
			return INVALID_SOCKET;
	}
	
	//Return a socket previously got by GetSocket.  If the socket refcount goes down
	//to zero, the socket is closed, _but_only_ if no one else manages to grab
	//it in the mean time.
	int ReturnSocket() {
		::Socket old;
		{
			DWORD ref = InterlockedDecrement(&mSockRef);
			if (ref == 0) {
				// try to close the socket. It it is zero, set it to a very
				// negative value
				if (InterlockedCompareExchange(&mSockRef, -10000000, 0)) {
					// Yes, success, no one else tried to GetSocket in between
					// our InterlockedDecrement and InterlockedCompareExchange.
					// Now we can close it.
					old.Swap(mSocket);
					_ASSERT(old.Valid());
					int result = closesocket(old.Detach());
					SockStats::sGlobalStats.DeleteSocket();
					return result;
				}
			}
		}
		return 0;
	}

public:

	//Peek at the socket without going through the socket reference counting hoops.
	//Use only where we know that the socket is valid.
	SOCKET PeekSocket() const {
		return mSocket.Peek();
	}

	//A temporary socket holder.  The socket won't be closed while this object
	//is in existence.
	class SockRef {
	public:
		SockRef(SocketXtra *parent) : mParent(parent), mSock(parent->GetSocket())
		{}
		SockRef(const SockRef &o) : mParent(o.mParent), mSock(o.mParent->GetSocket())
		{}
		~SockRef() {
			mParent->ReturnSocket();
		}
		operator SOCKET () const {return mSock;}
		bool Valid() const {return mSock != INVALID_SOCKET;}
	private:
		SockRef &operator=(const SockRef &o);
		IOPtr<SocketXtra> mParent;
		const SOCKET mSock;
	};

	//Atomically get a socket object.  It is either the valid (unclosed) socket or INVALID_SOCKET.
	SockRef Socket() {return SockRef(this);}

	//Recv queue management

	//link entry into the tail of the recv queue.  Returns true if at head
	bool PushRecv(class RecvPacketResult *r)
	{
		MutexOwner own(mCS);
		mRecvQueue.push_back(r);
		return mRecvQueue.size() == 1;
	}

	//unlink from head of recv queue.  Returns the new head, or NULL
	typedef IOPtr<class RecvPacketResult> RecvPacketResultPtr_t;
	RecvPacketResultPtr_t PopRecv(RecvPacketResult *head) {
		MutexOwner own(mCS);
		_ASSERT(mRecvQueue.size() > 0 && head == mRecvQueue.front().get());
		mRecvQueue.pop_front();
		if (!mRecvQueue.size())
			return 0;
		return mRecvQueue.front();
	}

	//Get the next guy in the recv queue, or 0
	//head must be the head of the queue.
	class RecvPacketResult *NextRecv(RecvPacketResult *head) const {
		MutexOwner own(mCS);
		_ASSERT(mRecvQueue.size() > 0 && head == mRecvQueue.front().get());
		if (mRecvQueue.size()<2)
			return 0;
		class RecvPacketResult *result = mRecvQueue[1].get();
		return result;
	}
	
	// API for the blockingsend property.
	void SetBlockingSend(bool b) {mBlockingSend = b;}
	bool GetBlockingSend() const {return mBlockingSend;}

	//A API for the zerobytereads property
	void SetZerobyteReads(bool on) {mZerobyteReads = on;}
	bool GetZerobyteReads() const {return mZerobyteReads;}

	//An API for the max packet size
	void SetMaxPacketSize(int s) {mMaxPacketSize = s;}
	int GetMaxPacketSize() const {return mMaxPacketSize;}

	//Close the socket.  We use the reference counting system
	//for this, so that we don't pull the rug out from underneath someone
	//just about to perform a socket operation.
	int CloseSocket()
	{
		if (mAcceptQueue.get())
			mAcceptQueue->clear();
		return ReturnSocket();
	}

	// Get a the next index for receive packets
	unsigned int RecvPacketSequence()
	{
		return mRecvPacketSequence++;
	}

	// Accept queue.  We put accepted sockets here where the wating party
	// had timed out.  This is to resolve the race between accepting a
	// connection and timing out.  If we just time out, clients may get
	// accepted, but the connection is then discarded.
	void PushAccept(class AcceptExResult *s)
	{
		MutexOwner own(mCS);
		if (!mAcceptQueue.get())
			mAcceptQueue = std::unique_ptr<acceptQueue_t >(new acceptQueue_t);
		mAcceptQueue->push_back(s);
	}
	IOPtr<class AcceptExResult> PopAccept(){
		if (mAcceptQueue.get() && mAcceptQueue->size()) {
			MutexOwner own(mCS);
			if (mAcceptQueue.get() && mAcceptQueue->size()) {
				IOPtr<class AcceptExResult> result = mAcceptQueue->front();
				mAcceptQueue->pop_front();
				return result;
			}
		}
		return IOPtr<class AcceptExResult>();
	}
	
private:
	
	SocketXtra(const SocketXtra &);	//no copying
	const SocketXtra &operator=(const SocketXtra &); //no copying

public:
	SockStats mStats;
	
private:
	mutable Mutex mCS;	//critical section for stuff
	LONG mRefcount; //reference count for keeping the socketex alive.
	
	::Socket mSocket; //the actual socket
	LONG mSockRef; //for threadsafe socket close
	
	typedef IOPtr<class RecvPacketResult> RecvPacketResultPtr_t;
	std::deque<RecvPacketResultPtr_t> mRecvQueue;
	unsigned int mRecvPacketSequence;
	unsigned int mMaxPacketSize;
	bool mBlockingSend;
	bool mZerobyteReads;
	typedef std::deque<IOPtr<class AcceptExResult> > acceptQueue_t;
	std::unique_ptr<acceptQueue_t > mAcceptQueue;
};

static void IOPtr_AddRef(SocketXtra *x) {x->AddRef();}
static void IOPtr_Release(SocketXtra *x) {x->Release();}
typedef IOPtr<SocketXtra> SocketXtraPtr_t;


//A specialization of IOWorker, for use in this file
class WSIOWorker : public IOWorker
{
public:
	WSIOWorker() : mErrorCode(0) {}
	DWORD GetExecuteFlag() const {return WT_EXECUTELONGFUNCTION;}
	void ExecuteAndWait()
	{
		IOWorker::ExecuteAndWait();
		if (GetErrorCode())
			throw SystemError(GetErrorCode(), GetErrorMsg().c_str());
	}
	void SetErrorCode(DWORD c) {mErrorCode = c;}
	DWORD GetErrorCode() const {return mErrorCode;}
	std::string GetErrorMsg() const {return "thread::"+mErrorMsg;}

protected:
	void SetError(const char *msg, DWORD code){
		SetErrorCode(code);
		mErrorMsg = msg;
	}
private:
	DWORD mErrorCode;
	std::string mErrorMsg;
};


// A helper function to generate  python timeout error messages and throw them
static void SetPyTimeout(const char *why=NULL)
{
	if (why == NULL)
		PyErr_SetString(socket_timeout, "timed out");
	else
		PyErr_Format(socket_timeout, "timed out during %s", why);
}

static void ThrowTimeout(const char *why=NULL)
{
	SetPyTimeout(why);
	throw Ccp::PyError();
}


// A special subclass of IOOverlapped which handles the special case
// of winsock overlapped operations.
// It contains an mXtra pointer that holds the socket which operation
// is running.  It will get the Socket handle on completion in
// an atomic manner, returning INVALID_HANDLE if the handle was
// closed during this time.
// It also contains error information, to propagate to the
// receiving tasklet.
class IOWSAOverlapped : public IOOverlapped
{
public:
	IOWSAOverlapped() : mOverlappedFlags(0) 
	{}
	IOWSAOverlapped(SocketXtra *xtra) : mOverlappedFlags(0) 
	{
		SetXtra(xtra);
	}
	void PreDelete()
	{
		ClearXtra();
	}

	// WSAOverlapped IO needs a special translation to get the proper error code.
	// The error code that was signalled by winsock.
	void HandleCompletion() {
		if (!OnNonTimeout())
			// We were preempted by a timout callback. Just quit.
			return;

		// Get a reference counted socket, or INVALID_SOCKET.  The mechanics
		// involved will resolve the race that can occur between closing a socket
		// and getting it.
		_ASSERT(mXtra);
		SocketXtra::SockRef sockRef(mXtra.get());
		try {
			// Get extended info including error state
			GetOverlappedResult(sockRef);
			// Handle the completion
			HandleWSACompletion(sockRef);
		} catch (std::exception &e) {
			HandleWSAError(sockRef, e);
		}
	}

	// The virtual base for the Winsock completion.  Gets the valid SOCKET
	// or INVALID_SOCKET
	// The default implementation just submits to the queue
	virtual void HandleWSACompletion(SOCKET socket) {
		SubmitToQueue();
	}

	// An error was encountered, and must be handled.
	// The default action is to transfer the error to the waiting thread.
	virtual void HandleWSAError(SOCKET socket, const std::exception &e) {
		SubmitErrorToQueue(e);
	}

	// Transfer exceptions to the waiting thread
	void SubmitErrorToQueue(const std::exception &e)
	{
		SetError(e);
		SubmitToQueue();
	}

	DWORD GetFlags() const {return mOverlappedFlags;}
	void SetFlags(DWORD f) {mOverlappedFlags = f;}


	// A special WaitForCompletion that propagates any errors.
	// A timeout is considered a python error
	void WaitForCompletion(const char *why_timeout=NULL)
	{
		if (!WaitForCompletionOrTimeout())
			ThrowTimeout(why_timeout);
	}

	// The same, but returns false when timout occurs.
	bool WaitForCompletionOrTimeout()
	{
		try {
			IORequest::WaitForCompletion();
		} catch (const Ccp::SystemError &e) {
			// Handle the case when WSAETIMEDOUT is returned
			// which means that internal timers in send/receive/connect/accept timed out.
			// Treat this the same as our manual timeout detection.
			if (e.GetCode() == WSAETIMEDOUT)
				return false;
			throw;
		}
		if (GetNoWait())
			return true;
		if (IsTimeout())
			return false;
		if (GetErrorCode())
			ReRaise();  // Re-raise error from thread
		return true;
	}

	//a utility function to check the success of initialization of an 
	//overlapped function, such as WSARecv and WSASend
	static void OLCheck(const char *errstr, bool success) {
		if (success)
			return;
		DWORD err = WSAGetLastError();
		if (err == WSA_IO_PENDING)
			return;
		throw WSAError(errstr);
	}

public:
	// Set the error code from caught exceptions.
	// In the future, it will be possible to transfer exceptions between threads,
	// but we have to use this poor-man's approach for now.
	void SetError(const Ccp::SystemError &e){
		SetError(e.what(), e.code());
	}
	void SetError(const char *msg, DWORD code){
		SetErrorCode(code);
		mErrorMsg = std::string("IOCompletion: ") + msg;
	}
	void SetError(const std::exception &e) {
		if (dynamic_cast<const SystemError*>(&e)) {
			SetError(dynamic_cast<const SystemError&>(e));
		} else if (dynamic_cast<const std::bad_alloc*>(&e)) {
			SetError(e.what(), ERROR_OUTOFMEMORY);
		} else {
			SetErrorCode((DWORD)-1);
			mErrorMsg = std::string("IOCompletion: ") + typeid(e).name()+ " : " + e.what();
		}
	}
protected:
	void ClearError() {
		SetErrorCode(0);
	}

	// Re-raise the error on the target thread.
	void ReRaise() const {
		if (!GetErrorCode())
			return;
		if (GetErrorCode() == ERROR_OUTOFMEMORY)
			throw std::bad_alloc();
		if (GetErrorCode() == -1)
			throw std::exception(mErrorMsg.c_str());
		throw SystemError(GetErrorCode(), mErrorMsg.c_str());
	}

	// Xtra pointer management.  It is necessary only while the overlapped IO
	// is in flight, since we need it to get the socket when we get the completion
	// callback.  But hanging on to it for too long can cause circular
	// dependencies in some cases, so we can clear it if we want.
	void SetXtra(SocketXtra *xtra)
	{
		mXtra = SocketXtraPtr_t(xtra);
	}
	void ClearXtra()
	{
		mXtra = 0;
	}

private:
	// We must always call GetOverlappedResult() to get the proper socket info after
	// IOCompletion
	void GetOverlappedResult(SOCKET sock) {
		if (sock == INVALID_SOCKET) {
			// this means that the socket was closed before we were woken up. (the last
			// reference went away.  Normally this would result in a WSA_OPERATION_ABORTED
			// but this is not available when we use BindIOCompletionCallback.  It probably
			// would be returned from a blocking WSAGetOverlappedResult...
			// anyway, we just simulate that			
			throw WSAError(WSA_OPERATION_ABORTED, "WsaGetOverlappedResult");
		}
		DWORD transfered;
		BOOL ok = WSAGetOverlappedResult(sock,
			Overlapped(), 
			&transfered, FALSE, &mOverlappedFlags);
		if (!ok)
			throw WSAError("WsaGetOverlappedResult");
		SetBytesTransfered(transfered);
	}


protected:
	SocketXtraPtr_t mXtra;  // The socket which originate the IORequest
private:
	std::string mErrorMsg; //the error message to go with the error code.
	DWORD mOverlappedFlags;// The "flags" from GetOverlappedResult
};


// A special worker template class that adds initial worker capability to
// a WSAOverlapped dude
template <class T>
class WorkerAdapter
{
public:
	void WorkerStart()
	{
		T* final = static_cast<T*>(this);
		IOHandoffHelper handoff(final);
		BOOL r = QueueUserWorkItem(ThreadProc_Thunk, static_cast<void*>(final), GetExecuteFlag());
		if (!r)
			throw Ccp::SystemError("QueueUserWorkItem");
		handoff.Success();
	}

	// The function that defines the work done by the thread.
	// The defaultaction is just to submit it to the queue
	virtual void ThreadFunc() {
		static_cast<T*>(this)->SubmitToQueue();
	}

	virtual ULONG GetExecuteFlag() const {
		return WT_EXECUTEDEFAULT;
	}
	
private:
	// Thread start thunker
	static DWORD WINAPI ThreadProc_Thunk(LPVOID lpParameter)
	{
		IOPtr<T> self = IOHandoffHelper::Complete(static_cast<T*>(lpParameter));
		return self->ThreadProc();
	}
	DWORD ThreadProc()
	{
		try {
			ThreadFunc();
		} catch (const std::exception &e) {
			try {
				static_cast<T*>(this)->SetError(e);
				static_cast<T*>(this)->SubmitToQueue();
			} catch (const std::exception &e) {
				// PyStacklessIOEventQueue.WriteUnraisable(e, "")
				e;
			}
		}
		return 0;
	}
};



//An IOResult subclass for the WSARecv function.
//It automatically handles doing ZeroByteRecv operations in case we
//get the WSAENOBUF error, which can happen if kernel memory is tight.
class IOOverlappedRecv : public IOWSAOverlapped
{
public:
    IOOverlappedRecv(SocketXtra *xtra) : IOWSAOverlapped(xtra) {}

    void Recv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufCount, DWORD _flags, bool zbRecv = false)
    {
        //Store these here, in case a zero byte read needs to be scheduled.
        _ASSERT(dwBufCount > 0 && dwBufCount <= _countof(mBuffers));
        memcpy(&mBuffers, lpBuffers, dwBufCount*sizeof(WSABUF));
        mNumBuffers = dwBufCount;
        mFlags = _flags;
        if (!zbRecv)
            // Post a proper read request
            PostActualRecv(s);
        else
            // Do an immediate zerobyte Recv
            PostZerobyteRecv(s);
    }

    //We override WSACompletion, since we want to support zero byte reads
    //transparently at this point
    void HandleWSACompletion(SOCKET socket) {
        if (IsPendingZerobyte()) {
            // This was a zero byte read request
            SetPendingZerobyte(false);
            PostActualRecv(socket);
            return;
        }
        DWORD bytesReceived = GetBytesTransfered();
        mXtra->mStats.BytesReceived(bytesReceived);
        OnRecv(socket, bytesReceived);
    }

    //Handle the error case that we know how to handle
    void HandleWSAError(SOCKET socket, const std::exception &e)
    {
        const Ccp::SystemError *e32 = dynamic_cast<const Ccp::SystemError*>(&e);
        if (e32 && e32->GetCode() == WSAENOBUFS && !IsPendingZerobyte()) {
            // schedule a zero byte read!
            mXtra->mStats.Nobuf();
            PostZerobyteRecv(socket);
        } else
            IOWSAOverlapped::HandleWSAError(socket, e);
    }

    // Called when actual data has been received.  By default, just submits to queue.
    virtual void OnRecv(SOCKET sock, DWORD bytesReceived)
    {
        SubmitToQueue();
    }

protected:
    //This virtual function is so that we can override it in the child class, the RecvFrom
    virtual int ActualRecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufCount,
        LPDWORD nRecvd, LPDWORD lpFlags,
        LPWSAOVERLAPPED lpOverlapped)
    {
        //Must use WSARecvFrom here, even if we don't want the "from" value, because
        //WSARecv works only with connected sockets, so we couldn't use this
        //class with UDP sockets.
        return WSARecvFrom(s, lpBuffers, dwBufCount, nRecvd, lpFlags,0, 0, lpOverlapped, 0);
    }

private:	
    // Post a real read request.
    void PostActualRecv(SOCKET s)
    {
        // Must use WSARecvFrom here, even if we don't want the "from" value, because
        // WSARecv works only with connected sockets.
        // Note: must provide a non-zero lpNumberOfBytesReceived because it is required
        // on Windows XP, in spite of what the documentation says.
        _ASSERT(!IsPendingZerobyte());
        IOHandoffHelper handoff(this);
        DWORD dummy, flags = mFlags;
        int res = ActualRecv(s, mBuffers, mNumBuffers, &dummy, &flags, Overlapped());
        OLCheck("WSARecvFrom", res!=SOCKET_ERROR);
        handoff.Success();
    }

    // Post a zero byte read request.  We do this in response to getting the WSAENOBUFS error
    // since that indicates that the OS can't lock pages for the receive operation.
    void PostZerobyteRecv(SOCKET s)
    {
        _ASSERT(!IsPendingZerobyte());
        IOHandoffHelper handoff(this);
        WSABUF buf = {0,0};
        DWORD nBytes, flags = mFlags;
        SetPendingZerobyte(true);
        try {
            // Note: must provide a non-zero lpNumberOfBytesReceived because it is required
            // on Windows XP, in spite of what the documentation says.
            int err = WSARecv(s, &buf, 1,&nBytes, &flags, Overlapped(), 0);
            OLCheck("WSARecvFrom", err!=SOCKET_ERROR);
        } catch (...) {
            SetPendingZerobyte(false);
            throw;
        }
        handoff.Success();
    }

    // use the mNumbuffers as a flag for pending zero byte read
    // Hacky? yes.  If we ever support dynamic mNumBuffers, we
    // might as well splunge on a whole bool value.
    bool IsPendingZerobyte() const {
        _ASSERT(mNumBuffers != 0);
        return mNumBuffers < 0;
    }
    void SetPendingZerobyte(bool flag) {
        if ((flag && mNumBuffers > 0) || (!flag && mNumBuffers < 0))
            mNumBuffers = -mNumBuffers;
    }

private:
    // We need to store the buffers here, because we automatically take care
    // of rescheduling the Recv with the same buffer.  Meanwhile, the old buffers
    // can be invalid.  We only have space for two buffers.
    // dynamic buffers can be added as required.
    WSABUF mBuffers[2];
    int mNumBuffers;
    DWORD mFlags;
};


//An IOResult subclass for the WSARecvFrom functions.
class IOOverlappedRecvFrom : public IOOverlappedRecv
{
public:
    IOOverlappedRecvFrom(SocketXtra *xtra) : IOOverlappedRecv(xtra) {}

    void GetFrom(struct sockaddr *from, int *len)
    {
        *len = std::min(*len, mSockaddrLen);
        memcpy(from, &mSockaddr, *len);
    }

protected:
    // Override the virtual function, providing a place to store the from address.
    virtual int ActualRecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufCount,
        LPDWORD nRecvd, LPDWORD lpFlags,
        LPWSAOVERLAPPED lpOverlapped)
    {
        mSockaddrLen = sizeof(mSockaddr);
        return WSARecvFrom(s, lpBuffers, dwBufCount, nRecvd, lpFlags,
            (sockaddr *)&mSockaddr, &mSockaddrLen,
            lpOverlapped, 0);
    }

private:
    // Must hold onto these here.
    sock_addr_t mSockaddr; //for WSARecvFrom
    int mSockaddrLen;
};


//An IOResult subclass for the WSASend function
class IOOverlappedSend : public IOWSAOverlapped
{
public:
	IOOverlappedSend(SocketXtra *xtra) : IOWSAOverlapped(xtra) {}

	void Send(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufCount, DWORD flags)
	{
		// Note: must provide a non-zero lpNumberOfBytesSent because it is required
		// on Windows XP, in spite of what the documentation says.
		DWORD dummy;
		IOHandoffHelper handoff(this);
		int res = WSASend(s, lpBuffers, dwBufCount, &dummy, flags, Overlapped(), 0);
		OLCheck("WSASend", res!=SOCKET_ERROR);
		handoff.Success();
	}

	void HandleWSACompletion(SOCKET socket) {
		mXtra->mStats.BytesSent(GetBytesTransfered());
		IOWSAOverlapped::HandleWSACompletion(socket);
	}
};


class IOOverlappedSendTo : public IOOverlappedSend
{
public:
	IOOverlappedSendTo(SocketXtra *xtra) : IOOverlappedSend(xtra) {}

	void SendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufCount, DWORD flags,
				const sockaddr *to, int toLen)
	{
		if (toLen == 0) {
			//WSASend is faster than WSASendTo!
			Send(s, lpBuffers, dwBufCount, flags);
			return;
		}
		toLen = std::min((unsigned long long)toLen, sizeof(mSockaddr));
		memcpy(&mSockaddr, to, toLen);
		// Note: must provide a non-zero lpNumberOfBytesSent because it is required
		// on Windows XP, in spite of what the documentation says.
		DWORD dummy;
		IOHandoffHelper handoff(this);
		int res = WSASendTo(s, lpBuffers, dwBufCount, &dummy, flags, (sockaddr*)&mSockaddr, toLen, Overlapped(), 0);
		OLCheck("WSASendTo", res!=SOCKET_ERROR);
		handoff.Success();
	}

private:
	SOCKADDR_STORAGE mSockaddr; //for WSASendTo
};


//An IOResult subclass for the WSARecv and WSARecvFrom functions.
class RecvResult : public IOOverlappedRecvFrom
{
public:
	RecvResult(SocketXtra *xtra, Py_buffer *dest) : IOOverlappedRecvFrom(xtra), mKeeper(dest) {}
	
	//Don't keep the extra buffer reference for longer than necessary.
	void HandleWSACompletion(SOCKET socket) {
		mKeeper.Release();
		IOOverlappedRecvFrom::HandleWSACompletion(socket);
	}

private:
	//the destination object, must keep a reference to it during execution
	Py_bufferKeeper mKeeper; 
};

// A safe downcasting function that throws an exception if the value is too big.
template<typename To, typename From>
inline To downcast(From f) {
	To t = (To)f;
	if ((From)t != f)
		throw std::overflow_error("buffer too long");
	return t;
}


// This class helps with preparing data for send.  It will store and keep
// the Py_buffers as required.
class SendPrepare
{
public:
	//Add the send data
	SSIZE_T  Prepare(WSABUFStore &buffers, Py_buffer *pbuf)
	{
		WSABUF buf;
		buf.buf = (char*)pbuf->buf;
		buf.len = downcast<u_long>(pbuf->len);
		AddRef(pbuf);
		buffers.Push(buf);
		return buf.len;
	}
	//The same, but for a sequence of data
	SSIZE_T Prepare(WSABUFStore &buffers,
		PyObject *seq)
	{
		PyObjectPtr fast(PyCheck(PySequence_Fast(seq, "sequence required")));
		SSIZE_T sum = 0;
		for (SSIZE_T i = 0; i<PySequence_Fast_GET_SIZE(fast.get()); i++) {
			Py_buffer buf;
			PyObject *obj = PySequence_Fast_GET_ITEM(fast.get(), i);
			if (!PyArg_Parse(obj, "s*", &buf)) {
				// it could be another sequence, handle it recursively
				if (PySequence_Check(obj)) {
					PyErr_Clear();
					SSIZE_T s = Prepare(buffers, obj);
					SSIZE_T old = sum;
					sum += s;
					if (sum < old) {
						OutputDebugString("message too long");
						throw std::length_error("message too long");
					}
					continue;
				} else
					throw PyError();
			}
			//BUG WORKAROUND!: We must incref the object if it wasn't already done!
			//When python does this properly, don't bother anymore.
			if (!buf.obj) {
				buf.obj = obj;
				Py_INCREF(buf.obj);
			}
			SSIZE_T old = sum;
			sum += Prepare(buffers, &buf);
			if (sum < old) {
				OutputDebugString("message too long");
				throw std::length_error("message too long");
			}
		}
		if (!sum && !buffers.Size()) {
			//create one empty buffer
			WSABUF tmp = {0};
			buffers.Push(tmp);
		}
		return sum;
	}

	// And the universal dude
	SSIZE_T Prepare(WSABUFStore &buffers, PyObject *seq, Py_buffer *buf)
	{
		if (seq)
			return Prepare(buffers, seq);
		else 
			return Prepare(buffers, buf);
	}

	void ClearRefs()
	{
		mKeeper.Release();
	}

private:
	void AddRef(Py_buffer *buf) {
		mKeeper.Push(buf);
	}
	Py_bufferKeeperStore mKeeper;
};

class SendResult : public SendPrepare, public IOOverlappedSendTo
{
public:
	SendResult(SocketXtra *xtra) : IOOverlappedSendTo(xtra) {}

	void WaitForCompletion(const char *why_timeout=NULL)
	{
		//override this to clear the refs early if we waited for the
		//request to finish. (otherwise they would be cleared when
		//the dustbin is cleared
		IOOverlappedSendTo::WaitForCompletion(why_timeout);
		if (!GetNoWait())
			ClearRefs(); //we waited
	}

	void WaitForCompletion(bool &timeout)
	{
		timeout = !IOOverlappedSendTo::WaitForCompletionOrTimeout();
		if (!GetNoWait())
			ClearRefs();
	}
};


class SendPacketResult : public SendResult
{
public:
	SendPacketResult(PySocketSockObject *s) :
		SendResult(static_cast<SocketXtra*>(s->sock_xtradata))
	{}

	SSIZE_T Prepare(WSABUFStore &buffers, PyObject *obj, Py_buffer *pbuf)
	{
		WSABUF header;
		header.buf = (char*)&mHeader;
		header.len = sizeof(mHeader);
		buffers.Push(header);
		SSIZE_T result = header.len;
		result += SendResult::Prepare(buffers, obj, pbuf);
		mHeader = (DWORD)(result-sizeof(mHeader)); //number of cargo bytes
		if (mHeader > (DWORD)mXtra->GetMaxPacketSize()) {
			char tmp[128];
			sprintf_s(tmp, "packet too long at %d bytes, max size is %d",
				mHeader, mXtra->GetMaxPacketSize());
			OutputDebugString(tmp);
			throw std::length_error(tmp);
		}
		return result;
	}
	
	void HandleWSACompletion(SOCKET socket) {
		mXtra->mStats.PacketSent();
		SendResult::HandleWSACompletion(socket);
	}

private:
	DWORD mHeader;
};

// A new recvpacketresult class.
// It resiles on the IOOverlappedRecv to do all of the zerobyte read things
// and focuses solely on the chunking logic.
class RecvPacketResult : public IOOverlappedRecv
{
public:
    RecvPacketResult(PySocketSockObject *s) : 
      IOOverlappedRecv(static_cast<SocketXtra*>(s->sock_xtradata)),
          mSequence(0),
          mBytesRead(0),
          mData(0),
          mDataLen(0),
          mEOF(false)
      {
          mZerobyteReads = static_cast<SocketXtra*>(s->sock_xtradata)->GetZerobyteReads();
      }

      ~RecvPacketResult() {
          if (mData)
              free(mData);
      }

      //The thread handler function to process data and request more.  It must be careful
      //to catch exceptions and turn into error codes for the request
      void OnRecv(SOCKET sock, DWORD bytesTransfered)
      {
          bool readScheduled = false;
          if (bytesTransfered)
              readScheduled = ReadMore(sock, bytesTransfered);
          else
              mEOF = true; //signal a socket close

          if (!readScheduled)
              Finish(sock, true); // no more io pending for us.
      }

      //There was some error during a Recv, just finish this.
      void OnRevFail(SOCKET sock, const std::exception &e)
      {
          Finish(sock, true);
      }

      //Set up the read buffers for the next read request
      void SetupReadBuffers(WSABUF *buf, DWORD &nbuf)
      {
          if (mBytesRead < sizeof(mHeader)) {
              //continue reading header
              buf[0].buf = ((char*)&mHeader)+mBytesRead;
              buf[0].len = (u_long)(sizeof(mHeader) - mBytesRead);
              nbuf = 1;
          } else {
              GrowBuffer();
              int dataRead = mBytesRead-sizeof(mHeader);
              buf[0].buf = mData + dataRead;
              buf[0].len = mDataLen - dataRead;
              nbuf = 1;
              //see if we should attempt to read the header of the next guy
              //we can do this if we are reading the whole rest of the package and there is another
              //request waiting.
              if (mDataLen == mHeader) {
                  RecvPacketResult *next = mXtra->NextRecv(this);
                  if (next) {
                      buf[1] = next->GetHeaderBuf();
                      nbuf = 2;
                  }
              }
          }
      }

      //If there is no more space in the input buffer, grow it.
      void GrowBuffer()
      {
          int dataRead = mBytesRead-sizeof(mHeader);
          if (dataRead == mDataLen) {
              // We have filled the buffer with data and need to grow it
              // because there is more to come.
              Py_ssize_t chunk = slsockAllocChunkSize;
              size_t newlen;
              if (chunk <= 0)
                  //allocate to the max
                  newlen = mHeader;
              else
                  // Allocate at most chunk bytes
				  newlen = std::min( (Py_ssize_t)mDataLen + chunk, (Py_ssize_t)mHeader );
              char *data = (char*)realloc(mData, newlen);
              if (!data)
                  throw std::bad_alloc();
              mData = data;
              mDataLen = (DWORD)newlen;
          }
      }

      void InitiateRead(SOCKET sock)
      {
          WSABUF buf[2];
          DWORD nbuf;
          SetupReadBuffers(buf, nbuf);
          Recv(sock, buf, nbuf, 0, mZerobyteReads);
      }

      //process read data and ask for more.  Returns true if a new read request was posted,
      //false, if it is complete.  This function may raise exceptions
      bool ReadMore(SOCKET sock, DWORD bytesTransfered)
      {
          mBytesRead += bytesTransfered;
          bool more;
          if (mBytesRead < sizeof(mHeader)) {
              //continue reading header
              more = true;
          } else if (mBytesRead == sizeof(mHeader)) {
              //just finished reading header, allocate buffer.
              if (mHeader > (DWORD)mXtra->GetMaxPacketSize()) {
                  char tmp[128];
                  sprintf_s(tmp, "too large a packet detected at %d bytes, max is %d",
                      mHeader, mXtra->GetMaxPacketSize());
                  throw std::length_error(tmp);
              }
              more = mHeader!=0;
          } else if (mBytesRead < (int)(sizeof(mHeader)+mHeader)) {
              //header here but packet not finished
              more = true;
          } else {
              //packet finished.
              //did we read any of the next guy's header? (we requested next header's amount of data :)
              DWORD rest = mBytesRead - (sizeof(mHeader) + mHeader);
              if (rest) {
                  RecvPacketResult *next = mXtra->NextRecv(this);
                  _ASSERT(next);
                  next->OnHeaderBytesRead(rest);
                  mBytesRead = (int)(sizeof(mHeader)+mHeader);
              }
              mXtra->mStats.PacketReceived();
              more = false;
          }
          if (!more) {
              return more;
          }
          InitiateRead(sock);
          return more;
      }

      //Two methods, used to help us with linking these requests into a list on the
      //socket.
      void Initiate(SOCKET sock) {
          bool atHead = mXtra->PushRecv(this);
          mSequence = mXtra->RecvPacketSequence();
          if (atHead) {
              try {
                  bool read_posted = ReadMore(sock, 0);
                  _ASSERT(read_posted);
              } catch (...) {
                  //Remove from queue and initiate others
                  Finish(sock, false);
                  throw;
              }
          }
      }

      void Finish(SOCKET sock, bool toIoQueue) {
          // We are at the head, pop us.
          // Note the necessary use of a smart pointer, the ReadMore()
          // may cause a queue pop and so a borrowed pointer off the queue
          // is not a good idea.
          SocketXtra::RecvPacketResultPtr_t next = mXtra->PopRecv(this);

          if (next) {
              // There is a "next" request to start.  We start it off as soon as possible
              // Since the SubmitToQueue might take some time.
              // But we must be careful with our recursion, we cannot "Finish" it until
              // after we Finish ourselves, so that everything is in the right order.
              try {
                  //start his read ops as soon as possible
                  if (next->ReadMore(sock, 0))
                      next = 0; //A read was posted and another thread will finish
              } catch (const std::exception &e) {
                  next->SetError(e);
              }
          }

          // Submit ourselves, then tail-recursively call Finish on the next dude.
          // Even if submit fails, we still must call Finish
          try {
              if (toIoQueue)
                  SubmitToQueue();
          } catch (...) {
              // Must never fail to call Finish on the next dude.
              if (next)
                  next->Finish(sock, true);  // An exception here will override the current
              throw;  // Re-raise;
          }
          if (next)
              next->Finish(sock, true); //since it is "next" someone is also waiting for it.		
      }

      //For optimization, we can have the guy next in front of us in the line
      //read our header as part of his read operation!
      WSABUF GetHeaderBuf() {
          WSABUF buf;
          buf.buf = (char*)&mHeader;
          buf.len = sizeof(mHeader);
          return buf;
      }

      //Called by the predecessor in the queue, to inform us that part or all of the
      //header has been read.
      void OnHeaderBytesRead(DWORD nBytes) {
          _ASSERT(nBytes > 0 && nBytes <= sizeof(mHeader));
          _ASSERT(mBytesRead == 0);
          mBytesRead = nBytes;
      }

      PyObject *GetPacket() {
          PyObject *result = Py_None;
          if (mEOF)
              Py_INCREF(result);
          else
              //Here we can later create a buffer object and avoid copying the data
              result = PyString_FromStringAndSize(mData, mBytesRead-sizeof(mHeader));
          if (mData)
              free(mData);
          mData = 0;
          return result;
      }

      PyObject *GetSequence() {
          return PyInt_FromLong(mSequence);
      }

private:
    unsigned int mSequence;
    int mBytesRead;
    DWORD mHeader;
    char *mData;
    DWORD mDataLen;
    bool mEOF;
    bool mZerobyteReads;
};

	

class ConnectResult : public WSIOWorker
{
public:
	ConnectResult(SocketXtra *xtra, const sock_addr_t *addr, int addrlen, double timeout) :
		mXtra(xtra), mAddrLen(std::min(sizeof(mAddr), (unsigned long long)addrlen)), mTimeout(timeout), mTimedout(false)
	{
		memcpy(&mAddr, addr, mAddrLen);
	}

	bool IsTimeout() const {return mTimedout;}

private:
	void PreDelete() {mXtra = 0;}

	void ThreadFunc()
	{
		int err = connect(mXtra->Socket(), (sockaddr *)&mAddr, mAddrLen);
		if (err == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				fd_set set;
				FD_ZERO(&set);
				FD_SET(mXtra->Socket(), &set);
				/* we are in non-blocking mode.  If timeout is positive, start a select call here */
				if (mTimeout > 0.0) {
					// timeout mode
					timeval tv;
					tv.tv_sec = (long)mTimeout;
					tv.tv_usec = (long)((mTimeout-(double)tv.tv_sec)*1000000.0);
					err = select(1, 0, &set, 0, &tv);
				} else if (mTimeout < 0.0) {
					// blocking mode
					err = select(1, 0, &set, 0, NULL);
				} else
					// non-blocking mode
					err = 0;
				if (err == 0) {
					mTimedout = true;
				} else {
					err = connect(mXtra->Socket(), (sockaddr *)&mAddr, mAddrLen);
					if (err && WSAGetLastError() == WSAEISCONN)
						err = 0;  // Connection succeeded
				}
			}
		}
		if (err == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAETIMEDOUT)
				mTimedout = true;
			else
				SetError("connect", WSAGetLastError());
		} else
			if (!mTimedout)
				mXtra->mStats.Connect();
	}
	
private:
	SocketXtraPtr_t mXtra;
	SOCKET_T mSocket;
	SOCKADDR_STORAGE mAddr;
	int mAddrLen;
	double mTimeout;
	bool mTimedout;
};


class AddrInfoResult : public WSIOWorker
{
public:
	AddrInfoResult(const char *nodename, const char *servname, const struct addrinfo *hints)
		: mHints(*hints), mRes(0)
	{
		//copy strings and keep null flags
		if (nodename) mNode = nodename;
		if (servname) mServ = servname;
		mS = servname!=0;
		mN = nodename!=0;
	}
	~AddrInfoResult() {
		if (mRes)
			freeaddrinfo(mRes);
	}
	void ThreadFunc()
	{
		DWORD err = getaddrinfo(mN?mNode.c_str():0, mS?mServ.c_str():0, &mHints, &mRes);
		if (err)
			SetError("getaddrinfo", err);
	}
	typedef std::basic_string<char, std::char_traits<char>, Ccp::PyAllocator<char> > pystring;
	pystring mServ, mNode;
	bool mS, mN;
	struct addrinfo mHints;
	struct addrinfo *GetRes() { //take ownership of the res
		struct addrinfo *r = mRes;
		mRes = 0;
		return r;
	}
private:
	struct addrinfo *mRes;
};


class HostByAddrNameResult : public WSIOWorker
{
public:
	HostByAddrNameResult(const char *name) : mName(name), mRes(0)
	{}
	HostByAddrNameResult(const char *addr, int len, int type) : mAddr(addr), mLen(len), mType(type), mRes(0)
	{}

	void ThreadFunc()
	{
		struct hostent *e;
		if (mName.size())
			e = gethostbyname(mName.c_str());
		else
			e = gethostbyaddr(mAddr.c_str(), mLen, mType);
		if (!e)
			SetError(mName.size()?"gethostbyname":"gethostbyaddr", WSAGetLastError());
		else
			mRes = socket_hostent_dup(e);
	}

	typedef std::basic_string<char, std::char_traits<char>, Ccp::PyAllocator<char> > pystring;
	pystring mName, mAddr;
	int mLen, mType;
	struct hostent *mRes;
};


// An accept request.  It uses a hybrid of thread and io completion,
// with the thread started up first to create the target socket, then
// doing a proper AcceptEx.  The use of a thread minimizes the work
// done on the calling thread.
class AcceptExResult : public IOWSAOverlapped , public WorkerAdapter<AcceptExResult>
{
public:
	AcceptExResult(SocketXtra *xtra) : IOWSAOverlapped(xtra), mReady(false)
	{}

	void Accept()
	{
		WorkerStart();
	}

	void Wait(SocketXtra *xtra, double timeout)
	{
		if (mReady)
			return;
		WaitReset();
		if (timeout > 0.0)
			SetTimeout(timeout);
		bool success = WaitForCompletionOrTimeout();
		if (!success) {
			//Timed out.  Store this accept request for the next dude
			xtra->PushAccept(this);
			ThrowTimeout("accept");
		}
	}

	// Runs on a worker thread.  Create target socket, starts AcceptEx
	void ThreadFunc() {
		SOCKADDR_STORAGE sockname;
		int namelen = sizeof(sockname);
		getsockname(mXtra->PeekSocket(), (sockaddr*)&sockname, &namelen);
		Socket(socket(sockname.ss_family, SOCK_STREAM, IPPROTO_TCP)).Swap(mTarget);
		if (!mTarget.Valid())
			throw WSAError("socket");	
		StartAccept();
	}
	
	void StartAccept()
	{
		DWORD dwBytes;
		IOHandoffHelper handoff(this);
		SOCKET sock = mXtra->PeekSocket();
		BOOL ok = GetAcceptEx(sock)(sock,
			mTarget.Peek(),
			mBuffer, 
			0,
			sizeof(SOCKADDR_STORAGE) + 16, 
			sizeof(SOCKADDR_STORAGE) + 16, 
			&dwBytes, 
			Overlapped());
		OLCheck("AcceptEx", ok==TRUE);
		handoff.Success();
	}

	void HandleWSACompletion(SOCKET sock)
	{
		FinishAccept(sock);
		mReady = true;
		mXtra->mStats.Accept(); //add statistics
		ClearXtra();
		return IOWSAOverlapped::HandleWSACompletion(sock);
	}

	//Finish the accept stuff
	void FinishAccept(SOCKET sock)
	{
		_ASSERT(mTarget.Valid());
		//bind the accepted socket to the io result
		IOOverlapped::Bind((HANDLE)mTarget.Peek());
		//now we have the result here.  Update the socket properties
		SOCKET oldsock = sock;
		int err = setsockopt( mTarget.Peek(), 
			SOL_SOCKET, 
			SO_UPDATE_ACCEPT_CONTEXT, 
			(char *)&oldsock, sizeof(oldsock) );
		if (err == SOCKET_ERROR)
			throw WSAError("setsockopt");
	}

	//accept has succeeded.  Get the result
	SOCKET_T GetSock(void **xtradata, sockaddr *a, int *addrlen, SocketXtra *xtra) {
		sockaddr *localaddr, *remoteaddr;
		int locallen, remotelen;
		GetGetAcceptExSockaddrs(xtra->PeekSocket())(
			mBuffer, 
			0,
			sizeof(SOCKADDR_STORAGE) + 16, 
			sizeof(SOCKADDR_STORAGE) + 16, 
			&localaddr, &locallen,
			&remoteaddr, &remotelen);
		*addrlen = std::min(*addrlen, remotelen);
		memcpy(a, remoteaddr, *addrlen);

		*xtradata = 0;
		return SocketXtra::New(xtradata, mTarget);
	}

private:
	//helper functions for AcceptExRequest.
	LPFN_ACCEPTEX GetAcceptEx(SOCKET sock)
	{
		static LPFN_ACCEPTEX lpfnAcceptEx = 0;
		if (!lpfnAcceptEx) {
			GUID GuidAcceptEx = WSAID_ACCEPTEX;
			DWORD dwBytes;
			WSAIoctl(sock, 
					SIO_GET_EXTENSION_FUNCTION_POINTER, 
					&GuidAcceptEx, sizeof(GuidAcceptEx),
					&lpfnAcceptEx, sizeof(lpfnAcceptEx), 
					&dwBytes, NULL, NULL);
			if (!lpfnAcceptEx) {
				OutputDebugString("AcceptEx missing");
				throw std::runtime_error("AcceptEx missing");
			}
		}
		return lpfnAcceptEx;
	}
	LPFN_GETACCEPTEXSOCKADDRS GetGetAcceptExSockaddrs(SOCKET sock)
	{
		//also the GetAcceptExSockaddrs function
		static LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = NULL;
		DWORD dwBytes;
		if (!lpfnGetAcceptExSockaddrs) {	
			GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
			WSAIoctl(sock, 
					SIO_GET_EXTENSION_FUNCTION_POINTER, 
					&GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs),
					&lpfnGetAcceptExSockaddrs, sizeof(lpfnGetAcceptExSockaddrs), 
					&dwBytes, NULL, NULL);
			if (!lpfnGetAcceptExSockaddrs) {
				OutputDebugString("GetAcceptExSockaddrs missing");
				throw(std::runtime_error("GetAcceptExSockaddrs missing"));
			}
		}
		return lpfnGetAcceptExSockaddrs;
	}

private:
	Socket mTarget;
	char mBuffer[2*(sizeof(SOCKADDR_STORAGE)+16)];
	bool mReady;
};


class ConnectExResult : public IOWSAOverlapped
{
public:
	ConnectExResult(SocketXtra *xtra) : IOWSAOverlapped(xtra)
	{}
	
	void Connect(u_short family, const sock_addr_t *addr, int addrlen, double timeout)
	{
		int err;
		IOHandoffHelper handoff(this);
		BOOL ok = GetConnectEx(mXtra->Socket())(
			mXtra->Socket(), (sockaddr*)addr, addrlen,
			0, 0, 0,
			Overlapped());

		// ConnectEx works only on bound sockets.  If it is unbound, bind it to "any"
		// and try again
		if (!ok && (err = WSAGetLastError()) == WSAEINVAL) {
			if (family == AF_INET6) {
				struct sockaddr_in6 sa = {family, 0, 0, in6addr_any};
				err = bind(mXtra->Socket(), (struct sockaddr*)&sa, sizeof(sa));
			} else {
				struct sockaddr_in sa = {family, 0, INADDR_ANY};
				err = bind(mXtra->Socket(), (struct sockaddr*)&sa, sizeof(sa));
			}
			if (err == SOCKET_ERROR)
				throw WSAError("bind");
			ok = GetConnectEx(mXtra->Socket())(
				mXtra->Socket(), (sockaddr*)addr, addrlen,
				0, 0, 0,
				Overlapped());
		}
		// ConnectEx works only for connection oriented sockets
		if (!ok && (err = WSAGetLastError()) == WSAEADDRNOTAVAIL) {
			//fall back to regular connect
			handoff.Fail(); // just to be explicit, there is no handoff, just connect
			ok = connect(mXtra->Socket(),  (sockaddr*)addr, addrlen);
			if (!ok)
				throw WSAError("connect");
			mXtra->mStats.Connect(); //add statistics
			return;
		}
		OLCheck("ConnectEx", ok==TRUE);
		handoff.Success();

		//now, wait for completion
		if (timeout >= 0.0)
			SetTimeout(timeout);
		WaitForCompletion("connect");

		// okay.  Now finalize the ConnectEx
		err = setsockopt(mXtra->Socket(), 
						SOL_SOCKET, 
						SO_UPDATE_CONNECT_CONTEXT, 
						NULL, 
						0 );
		if (err == SOCKET_ERROR)
			throw WSAError("setsockopt");
		mXtra->mStats.Connect(); //add statistics
	}
	static bool IsSupported(SOCKET sock) {
		return (GetConnectEx_int(sock) != 0);
	}

private:
	//helper functions for AcceptExRequest.
	static LPFN_CONNECTEX GetConnectEx(SOCKET sock)
	{
		LPFN_CONNECTEX lpfnConnectEx = GetConnectEx_int(sock);
		if (!lpfnConnectEx) {
			OutputDebugString("ConnectEx missing");
			throw std::runtime_error("ConnecEx missing");
		}
		return lpfnConnectEx;
	}
	static LPFN_CONNECTEX GetConnectEx_int(SOCKET sock)
	{
		static LPFN_CONNECTEX lpfnConnectEx = 0;
		if (!lpfnConnectEx) {
			GUID GuidConnectEx = WSAID_CONNECTEX;
			DWORD dwBytes;
			WSAIoctl(sock, 
					SIO_GET_EXTENSION_FUNCTION_POINTER, 
					&GuidConnectEx, sizeof(GuidConnectEx),
					&lpfnConnectEx, sizeof(lpfnConnectEx), 
					&dwBytes, NULL, NULL);
		}
		return lpfnConnectEx;
	}
};

//Our external functions

//first, an exception translator.  These APIs are all supposed to return
//with a python exception set.
static void TranslateExc(const std::exception &e) throw()
{
	if (dynamic_cast<const SystemError *>(&e)) {
		PyObjectPtr info(PyString_FromString(dynamic_cast<const SystemError *>(&e)->what()));
		if (info)
			socket_set_extra_error_info(info);
		else
			PyErr_Clear();
		socket_set_error(dynamic_cast<const SystemError *>(&e)->GetCode());
	} else
		Ccp::PyErrFromException(e);
}
static void TranslateExc() throw()
{
	Ccp::PyErrFromException();
}

//A conveinence function to set WSAError and also extra error info
static void SetWSAError(const SystemError &e) {
	PyObjectPtr info(PyString_FromString(e.what()));
	if (info)
		socket_set_extra_error_info(info);
	else
		PyErr_Clear();
	WSASetLastError(e.GetCode());
}

//Modify the buffer size of new socket according to defaults
static void SetBuffers(SOCKET s)
{
	if (defaultRCVBUF >= 0) {
		int err = setsockopt( s, SOL_SOCKET, SO_RCVBUF, (char *)&defaultRCVBUF, sizeof(defaultRCVBUF) );
		if (err == SOCKET_ERROR)
			throw WSAError("setsockopt");
	}
	if (defaultSNDBUF >= 0) {
		int err = setsockopt( s, SOL_SOCKET, SO_SNDBUF, (char *)&defaultSNDBUF, sizeof(defaultSNDBUF) );
		if (err == SOCKET_ERROR)
			throw WSAError("setsockopt");
	}
}

//Create an Overlapped socket, along with a reference count.  Return
//the new refcount with a value of 1
SOCKET_T slsock_socket(void **xtradata, int family, int type, int proto)
{
	*xtradata = 0;
	try {
		Socket sock;
		{
			PyAllowThreads _threads;
			Socket(WSASocket(family, type, proto, 0, 0, WSA_FLAG_OVERLAPPED)).Swap(sock);
		}
		if (!sock.Valid()) {
			socket_set_error(WSAGetLastError());
			return INVALID_SOCKET;
		}
		IOOverlapped::Bind((HANDLE)sock.Peek());
		SetBuffers(sock.Peek());
		return SocketXtra::New(xtradata, sock);
	} catch (const std::exception &e) {
		TranslateExc(e);
		return (SOCKET_T) -2;
	}
}


//Close the socket
int slsock_closesocket(SOCKET_T s, void *xtradata)
{
	SocketXtra *xtra = static_cast<SocketXtra*>(xtradata);
	_ASSERT(xtra->Socket() == s);
	int result = xtra->CloseSocket();
	return result;
}

//Release the socket extra data
void slsock_releasesocket(PySocketSockObject *s)
{
	SocketXtra *xtra = static_cast<SocketXtra*>(s->sock_xtradata);
	if (xtra)
		xtra->Release();
	s->sock_xtradata = 0;
	return;
}


//Replacement for socket receive.  Will have called s->errorhandler() on error.
ssize_t slsock_recvfrom(PySocketSockObject *s, Py_buffer *dest, int len, int flags,
						struct sockaddr *from, int *fromlen)
{
	try {
		int res;
		DWORD bytesRecvd = 0;
		DWORD dwFlags=flags;
		WSABUF buf;
		buf.buf = (char *)dest->buf;
		buf.len = len;

		// We always try with an initial non-blocking receive.  This saves us from the event loop
		// in many cases.
		if (from)
			memset(from, 0, *fromlen); //clear it.  It is only set if the socket is connectionless.
		res = WSARecvFrom(s->sock_fd, &buf, 1, &bytesRecvd, &dwFlags, from, fromlen, 0, 0);
		if (res != SOCKET_ERROR) {
			// Success! best performance
			static_cast<SocketXtra*>(s->sock_xtradata)->mStats.BytesReceived(bytesRecvd);
			return bytesRecvd;
		}

		// do we fail if the NB receive failed?
		bool fail = false;
		if (s->sock_timeout == 0.0) {
			// non blocking receive.  All errors, including WSAEWOULDBLOCK, are reported.
			fail = true;
		} else {
			// this was the initial try.  EWOULDBLOCK and ENOBUFS errors prompt us to
			// perform an overlapped operation.
			DWORD err = WSAGetLastError();
			if (err != WSAEWOULDBLOCK && err != WSAENOBUFS)
				fail = true;
		}
		if (fail) {
			s->errorhandler();
			return -1;
		}

		// Otherwise, continue towards an IOCOMPLETION
		IOPtr<RecvResult> result(new RecvResult(static_cast<SocketXtra*>(s->sock_xtradata), dest));
		
		result->Recv(s->sock_fd, &buf, 1, flags);
		if (s->sock_timeout > 0.0)
			result->SetTimeout(s->sock_timeout);
		result->WaitForCompletion("recv");
		if (from)
			result->GetFrom(from, fromlen);

		return result->GetBytesTransfered();
	} catch (const SystemError &e) {
		SetWSAError(e);
		s->errorhandler();
	} catch (const std::exception &e) {
		TranslateExc(e);
	}

	return -1;
}


//Helper function to parse arguments to send function, taking into account that we may accept
//a sequence instead of a string or buffer type thingy.
//fmt1 is expected to start with "s*" while fmt2 should start with "O"
int slsock_parse_send(PyObject *args, const char *fmt1, const char *fmt2, PyObject **obj, Py_buffer *buf, void *flags, void *addro)
{
	_ASSERT(fmt1[0] == 's' && fmt1[1] == '*');
	_ASSERT(fmt2[0] == 'O');
	if (!PyArg_ParseTuple(args, fmt1, buf, flags, addro)) {
		PyErr_Clear();
		if (!PyArg_ParseTuple(args, fmt2, obj, flags, addro))
			return 0; //something weird

		//ok, found an object
		if (!PySequence_Check(*obj)) {
			PyErr_SetString(PyExc_TypeError, "expected a string or sequence");
			return 0;
		}
		//found a sequence
	} else {
		//found a Py_buffer object
		*obj = 0;
		// BUG WORKAROUND: Python doesn't incref the object if it didn't support the buffer interface!  We must do it here
		// Remove the following three lines when python has been fixed.
		if (!buf->obj) {
			buf->obj = PyTuple_GET_ITEM(args, 0);
			Py_INCREF(buf->obj);
		}
	}
	return 1;
}


//return -1 on generail WSAGetLastError failure, and -2 to indicate python exception
static int slsock_sendto_impl(PySocketSockObject *s, PyObject *obj, Py_buffer *pbuf, int flags, sock_addr_t *addr, int addrlen, SSIZE_T *prep_size)
{
	try {
		WSABUFStore wsabufs;
		if (s->sock_timeout == 0.0) {
			/* must support non-blocking send without timeout, since proper timeout causes race. */
			SendPrepare prep;
			SSIZE_T prepped = prep.Prepare(wsabufs, obj, pbuf);
			if (prep_size)
				*prep_size = prepped;
			DWORD bytesSent;
			int err;
			if (addrlen)
				err = WSASendTo(s->sock_fd, wsabufs, wsabufs, &bytesSent, flags, (const sockaddr*)addr, addrlen, 0, 0);
			else
				err = WSASend(s->sock_fd, wsabufs, wsabufs, &bytesSent, flags, 0, 0);
			/* for non-blocking send, all errors are reported */
			return err ? -1 : bytesSent;
		}

		IOPtr<SendResult> result(new SendResult(static_cast<SocketXtra*>(s->sock_xtradata)));
		if (!reinterpret_cast<SocketXtra*>(s->sock_xtradata)->GetBlockingSend())
			result->SetNoWait();
		SSIZE_T preparedBytes = result->Prepare(wsabufs, obj, pbuf);
		result->SendTo(s->sock_fd, wsabufs, wsabufs, flags, (sockaddr*)addr, addrlen);
		if (s->sock_timeout >= 0.0 && !result->GetNoWait())
			result->SetTimeout(s->sock_timeout);
		result->WaitForCompletion("send");
		if (!reinterpret_cast<SocketXtra*>(s->sock_xtradata)->GetBlockingSend())
			return downcast<int>(preparedBytes); //Winsock will send everything regardless.
		return result->GetBytesTransfered();

	} catch (const SystemError &e) {
		SetWSAError(e);
		return -1;
	} catch (const std::exception &e) {
		TranslateExc(e);
		return -2;
	}
}


int slsock_sendto(PySocketSockObject *s, PyObject *obj, Py_buffer *pbuf, int flags, sock_addr_t *addr, int addrlen)
{
	return slsock_sendto_impl(s, obj, pbuf, flags, addr, addrlen, 0);
}


int slsock_sendall(PySocketSockObject *s, PyObject *obj, Py_buffer *pbuf, int flags)
{
	// Sendto will send all the data, since it is using overlapped IO.  On Windows,
	// this is guaranteed to send everything or return an error, so no need for specials
	// sendall semantics
	SSIZE_T prep_size;
	int result = slsock_sendto_impl(s, obj, pbuf, flags, 0, 0, &prep_size);
	if (s->sock_timeout == 0) {
		// with non-blocking, we have failed to send all if we send less than requested.
		if (result >= 0 && result != prep_size ) {
			SetPyTimeout("send");
			return -2;
		}
	}
	return result;
}


//returns -1 on a python error, 0 otherwise.
int slsock_sendpacket(PySocketSockObject *s, PyObject *obj, Py_buffer *pbuf)
{
	try {
		IOPtr<SendPacketResult> result(new SendPacketResult(s));
		if (!reinterpret_cast<SocketXtra*>(s->sock_xtradata)->GetBlockingSend())
			result->SetNoWait();
		WSABUFStore wsabufs;
		result->Prepare(wsabufs, obj, pbuf);
		result->Send(s->sock_fd, wsabufs, wsabufs, 0);
		if (s->sock_timeout > 0.0 && !result->GetNoWait())
			result->SetTimeout(s->sock_timeout);
		result->WaitForCompletion("send packet");
		return 0;
	} catch (const std::exception &e) {
		TranslateExc(e);
		return -1;
	}
}


static IOPtr<RecvPacketResult>
slsock_recvpacket_int(PySocketSockObject *s)
{
	IOPtr<RecvPacketResult> result;
	result = IOPtr<RecvPacketResult>(new RecvPacketResult(s));
	result->Initiate(s->sock_fd);
	if (s->sock_timeout > 0.0)
		result->SetTimeout(s->sock_timeout);
	result->WaitForCompletion("recv packet");
	return result;
}


PyObject *slsock_recvpacket(PySocketSockObject *s)
{
	try {
		IOPtr<RecvPacketResult> result(slsock_recvpacket_int(s));
		return result->GetPacket();
	}catch (const std::exception &e) {
		TranslateExc(e);
		return 0;
	}
}


PyObject *slsock_recvpacket_oob(PySocketSockObject *s)
{
	try {
		IOPtr<RecvPacketResult> result(slsock_recvpacket_int(s));
		return Py_BuildValue("NON", result->GetPacket(), Py_None, result->GetSequence());
	}catch (const std::exception &e) {
		TranslateExc(e);
		return 0;
	}
}


//returns 0, or error code with WSAGetLastError() ready.
//-1 used to signal a python exception.
int	slsock_connect(PySocketSockObject *s, const sock_addr_t *addr, int addrlen, int raise_timeout)
{
    if (s->sock_timeout == 0) {
        /* non-blocking connect */
        int r = connect(static_cast<SocketXtra*>(s->sock_xtradata)->Socket(),
            reinterpret_cast<const sockaddr*>(addr), addrlen);
        if (r == SOCKET_ERROR)
            r = WSAGetLastError();
        return r;
    }

	try {
		if (ConnectExResult::IsSupported(s->sock_fd)) {
			IOPtr<ConnectExResult> result(new ConnectExResult(
								static_cast<SocketXtra*>(s->sock_xtradata)));
			result->Connect(s->sock_family, addr, addrlen, s->sock_timeout);
		} else {
			IOPtr<ConnectResult> result(new ConnectResult(
				static_cast<SocketXtra*>(s->sock_xtradata), addr, addrlen, s->sock_timeout));
			result->ExecuteAndWait();
			if (result->IsTimeout())
				ThrowTimeout("connect");
		}
		return 0;
	} catch (const SystemError &e) {
		if (raise_timeout && e.GetCode() == WSAETIMEDOUT) {
			SetPyTimeout("connect");
			return -1;
		}
		SetWSAError(e);
		return e.GetCode();
	} catch (const PyError &e) {
		if (!raise_timeout && PyErr_ExceptionMatches(socket_timeout)) {
			// for connect_ex, we want to just return the code
			WSASetLastError(WSAETIMEDOUT);
			return WSAETIMEDOUT;
		}
		TranslateExc(e);
		return -1;
	}catch (const std::exception &e) {
		TranslateExc(e);
		return -1; //special Python error handling
	}
}


int slsock_getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	try {
		*res = 0;
		IOPtr<AddrInfoResult> result(new AddrInfoResult(nodename, servname, hints));
		result->ExecuteAndWait();
		*res = result->GetRes();
		return 0;
	} catch (const SystemError &e) {
		SetWSAError(e);
		return e.GetCode();
	}catch (const std::exception &e) {
		TranslateExc(e);
		return -1; //a Python exception
	}
}


hostent *slsock_gethostbyname(const char *name)
{
	try {
		IOPtr<HostByAddrNameResult> result(new HostByAddrNameResult(name));
		result->ExecuteAndWait();	
		return result->mRes;
	} catch (const SystemError &e) {
		SetWSAError(e);
	} catch (const std::exception &e) {
		TranslateExc(e);
		return (hostent*)-1; //python exception
	}
	return 0;
}


hostent *slsock_gethostbyaddr(const char *addr, int len, int type)
{
	try {
		IOPtr<HostByAddrNameResult> result(new HostByAddrNameResult(addr, len, type));
		result->ExecuteAndWait();	
		return result->mRes;
	} catch (const SystemError &e) {
		SetWSAError(e);
	} catch (const std::exception &e) {
		TranslateExc(e);
		return (hostent*)-1; //python exception
	}
	return 0;
}


SOCKET_T slsock_accept(void **xtradata, PySocketSockObject *s, struct sockaddr *addr, int *addrlen)
{
	try {
		*xtradata = 0;
		// First try a non-blocking accept
		Socket r;
		{
			PyAllowThreads _threads;
			Socket(accept(s->sock_fd, addr, addrlen)).Swap(r);
		}
		if (r.Valid()) {
			IOOverlapped::Bind((HANDLE)r.Peek());
			return SocketXtra::New(xtradata, r);
		}
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			return INVALID_SOCKET;
		if (s->sock_timeout == 0.0)
			return INVALID_SOCKET; // non-blocking ops return EWOULDBLOCK, not timeout.

		// See if there is a queued socket from a timed-out accept:
		SocketXtra *ex = static_cast<SocketXtra*>(s->sock_xtradata);
		IOPtr<AcceptExResult> result(ex->PopAccept());
		if (!result) {
			result = IOPtr<AcceptExResult>(new AcceptExResult(ex));
			result->Accept();
		}
		result->Wait(ex, s->sock_timeout);
		return result->GetSock(xtradata, addr, addrlen, ex);
	} catch (const SystemError &e) {
		SetWSAError(e);
		return INVALID_SOCKET;
	} catch (const std::exception &e) {
		TranslateExc(e);
		return INVALID_SOCKET-1; //python exception
	}
}

/* Select support.  Although it is a silly programming paradigm to use with stackless, we still want to support it to aid
 * in migrating apps that use select, such as SocketServer.
 */

//This class stores fd_set objects dynamically thus freeing us of the
//weird precompiler decided sizes of a fd_set
class DynamicFdsets
{
public:
	DynamicFdsets(const fd_set *_rd, const fd_set *_wr, const fd_set *_ex) :
	  rd(0), wr(0), ex(0)
	{
		// compute the size needed for the fd sets.
		const size_t a = _alignof(fd_set) - 1; //make sure the sets are aligned.
		size_t s1;
		size_t s2;
		size_t s3;

		if( _rd )
		{
			s1 = (offsetof( fd_set, fd_array[_rd->fd_count] ) + a) & ~a;
		}
		else
		{
			s1 = (offsetof( fd_set, fd_array[0] ) + a) & ~a;
		}
		if( _wr )
		{
			s2 = (offsetof( fd_set, fd_array[_wr->fd_count] ) + a) & ~a;
		}
		else
		{
			s2 = (offsetof( fd_set, fd_array[0] ) + a) & ~a;
		}
		if( _ex )
		{
			s3 = (offsetof( fd_set, fd_array[_ex->fd_count] ) + a) & ~a;
		}
		else
		{
			s3 = (offsetof( fd_set, fd_array[0] ) + a) & ~a;
		}

		mem.resize(s1+s2+s3);
		rd = (fd_set*) &mem[0];
		wr = (fd_set*) &mem[s1];
		ex = (fd_set*) &mem[s1 + s2];
		copy(rd, _rd);
		copy(wr, _wr);
		copy(ex, _ex);
	}

	void Get(fd_set *_rd, fd_set *_wr, fd_set *_ex)
	{
		copy(_rd, rd);
		copy(_wr, wr);
		copy(_ex, ex);
	}

private:
	static void copy(fd_set *d, const fd_set *s)
	{
		if (!d)
			return;
		if (s) {
			d->fd_count = s->fd_count;
			memcpy(d->fd_array, s->fd_array, s->fd_count * sizeof(s->fd_array[0]));
		} else
			d->fd_count = 0;
	}

private:
	std::vector<char, Ccp::PyAllocator<char> > mem;

public:
	fd_set *rd, *wr, *ex;
};

// The WSIOWorker thread based subclass, that performs the actual select on a
// long lived worker thread.
class SelectResult : public WSIOWorker
{
public:
	SelectResult(int nfds, fd_set * readfds, fd_set * writefds, fd_set *exceptfds, const struct timeval * timeout) :
		mNfds(nfds),
		mSets(readfds, writefds, exceptfds)
	{
		if (timeout) {
			mTimeout = *timeout;
			mTimeoutP = &mTimeout;
		} else
			mTimeoutP = 0;
	}

	/* note that there is a window between SelectResult() and ThreadFunc where the main thread
	 * may have woken up or errored and closed on of the handles.  We don't worry about it
	 * because "select" is benign.  At most it will report on an invalid handle or something,
	 * so we omit all that tedious socket locking stuff from SocketExtra
	 */
	void ThreadFunc()
	{
		mResult = select(mNfds, mSets.rd, mSets.wr, mSets.ex, mTimeoutP);
		if (mResult == SOCKET_ERROR)
			SetError("select",WSAGetLastError());
	}
	int GetResult(fd_set * readfds, fd_set * writefds, fd_set *exceptfds)
	{
		mSets.Get(readfds, writefds, exceptfds);
		return mResult;
	}

private:
	int mNfds;
	DynamicFdsets mSets;
	struct timeval mTimeout;
	struct timeval *mTimeoutP;
	int mResult;
};

int slsock_select(int nfds, fd_set * readfds, fd_set * writefds, fd_set *exceptfds, const struct timeval * timeout)
{
	try {
		IOPtr<SelectResult> request(new SelectResult(nfds, readfds, writefds, exceptfds, timeout));
		request->ExecuteAndWait();
		return request->GetResult(readfds, writefds, exceptfds);
	} catch (const SystemError &e) {
		 SetWSAError(e);
	} catch (const std::exception &e) {
		TranslateExc(e);
		return (SOCKET_ERROR)-1; //python exception
	}
	return SOCKET_ERROR;
}

// Settings

//Set the blockingsend attribute on the socket
int slsock_setblockingsend(PySocketSockObject *s, int blocking){
	SocketXtra *xtra = static_cast<SocketXtra*>(s->sock_xtradata);
	int old = (int)xtra->GetBlockingSend();
	xtra->SetBlockingSend(blocking!=0);
	return old;
}

//Set the zero byte reads attribute
int slsock_setzerobytereads(PySocketSockObject *s, int on){
	SocketXtra *xtra = static_cast<SocketXtra*>(s->sock_xtradata);
	int old = (int)xtra->GetZerobyteReads();
	xtra->SetZerobyteReads(on!=0);
	return old;
}

//Set the packetsize attribute
int slsock_setmaxpacketsize(PySocketSockObject *s, int size) {
	SocketXtra *xtra = static_cast<SocketXtra*>(s->sock_xtradata);
	int old = xtra->GetMaxPacketSize();
	xtra->SetMaxPacketSize(size);
	return old;
}	

//Retrieve the socket statistics
PyObject *slsock_getstats(PySocketSockObject *s)
{
	if (s)
		return static_cast<SocketXtra *>(s->sock_xtradata)->mStats.GetStats();
	else
		return SockStats::sGlobalStats.GetStats();
}

// Flexible generic methods to get and apply settings to the stacklessio.socket module
PyObject *
slsock_get_settings(PyObject *self)
{
	return Py_BuildValue("{sisnsisisi}",
		"version", slsockVersion,
		"allocChunkSize", slsockAllocChunkSize,
		"defaultRCVBUF", defaultRCVBUF,
		"defaultSNDBUF", defaultSNDBUF,
		"defaultBlockingSend", int(defaultBlockingSend)
		);
}

PyObject *
slsock_apply_settings(PyObject *self, PyObject *dict)
{
	PyObject *v;
	if (!PyDict_Check(dict))
		return PyErr_SetString(PyExc_TypeError, ""), NULL;
	v = PyDict_GetItemString(dict, "version");
	if (v) {
		long d = PyInt_AsLong(v);
		if (d == -1 && PyErr_Occurred()) return NULL;
		slsockVersion = d;
	}
	v = PyDict_GetItemString(dict, "allocChunkSize");
	if (v) {
		Py_ssize_t d = PyInt_AsSsize_t(v);
		if (d == -1 && PyErr_Occurred()) return NULL;
		slsockAllocChunkSize = d;
	}
	v = PyDict_GetItemString(dict, "defaultRCVBUF");
	if (v) {
		long d = PyInt_AsLong(v);
		if (d == -1 && PyErr_Occurred()) return NULL;
		defaultRCVBUF = d;
	}
	v = PyDict_GetItemString(dict, "defaultSNDBUF");
	if (v) {
		long d = PyInt_AsLong(v);
		if (d == -1 && PyErr_Occurred()) return NULL;
		defaultSNDBUF = d;
	}
	v = PyDict_GetItemString(dict, "defaultBlockingSend");
	if (v) {
		int d = PyObject_IsTrue(v);
		if (d == -1) return NULL;
		defaultBlockingSend = !!d;
	}
	Py_RETURN_NONE;
}

#endif /*SLSOCKET*/
