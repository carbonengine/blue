/*
 * slsocket_posix.cpp
 * This file defines the classes and functions needed to implement stacklessio.socket on top
 * of the stacklessio frameworks.  Its functions are exposed in slsocket.h and called
 * from the modifiled stackless/socketmodule.c
 * Also implements slsocket_select() for stakclessio.select and stackless/selectmodule.c
 */
#define PySocket_BUILDING_SOCKET
#include "socket_semantics.h"
#ifdef SLSOCKET


#include "stacklessio.h"
#include "slsocket.h"
#include "slsocketmodule.h"

#include "pyport.h" // This includes sys/select and sets the FD_SETSIZE and all.

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> // for getaddrinfo
#include <sys/event.h> // for kqueue
#include <sys/time.h> // for kqueue

#if defined __APPLE__
#include <ext/hash_set> // for the weakref manager
#include <dispatch/dispatch.h> // for grand central dispatch
#endif

#include "CarbonIO/protocol.h"

#include <algorithm>

using Ccp::Mutex;
using Ccp::MutexOwner;
using Ccp::PyObjectPtr;
using Ccp::SystemError;
using Ccp::PyCheck;
using Ccp::PyError;
using Ccp::PyAllowThreads;
using Ccp::Atomic32;
using Ccp::Atomic64;

extern "C" PyObject *socket_timeout = NULL;

// Temporary debugging features
static int slsockVersion = 1;
static Py_ssize_t slsockAllocChunkSize = 1024*1024;
static int defaultRCVBUF = -1;
static int defaultSNDBUF = -1;

// global socket behaviour
static bool defaultBlockingSend = true;


//A socket holding thingie.
typedef Ccp::Handle Socket;

// For explicitness and windows-like-ness
typedef int SOCKET;
#define INVALID_SOCKET -1


// Useful for storing bufer addresses.  Probably we will remove this
typedef struct WSABUF {
    size_t len;
    char * buf;
} WSABUF;

// some more typedefs for windows pseudo-compatibility
typedef ssize_t SSIZE_T;
typedef size_t ULONG;
typedef int DWORD;

// Microsoft's useful _countof macro
template <typename T, size_t N>
char (* &_ArraySizeHelper( T (&arr)[N]))[N];
#define _countof( arr ) (sizeof( _ArraySizeHelper( arr ) ))

// Microsoft's downcast macro without the downcast
template <typename T, typename V >
T downcast(V v) { return (T)(v); }


//first, an exception translator.  These APIs are all supposed to return
//with a python exception set.

static void TranslateExc(const std::exception &e) throw()
{
	if (dynamic_cast<const SystemError *>(&e)) {
        const SystemError &se = dynamic_cast<const SystemError &>(e);
        PyObjectPtr info(PyString_FromString(se.what()));
		if (info)
			socket_set_extra_error_info(info);
        else
            PyErr_Clear();
        // Set the socket error
        socket_set_error(se.GetCode());
    } else
        Ccp::PyErrFromException(e);
}
static void TranslateExc() throw()
{
    Ccp::PyErrFromException();
}

// A convenience function to set WSAError and also extra error info.
// Use this when you don't want to return a socket error but an error
// code.
static void SetSystemError(const SystemError &se) {
	PyObjectPtr info(PyString_FromString(se.what()));
	if (info)
		socket_set_extra_error_info(info);
	else
		PyErr_Clear();
	//se.SetErrno();
}


//Modify the buffer size of new socket according to defaults
static void SetBuffers(SOCKET s)
{
	if (defaultRCVBUF >= 0) {
		int err = setsockopt( s, SOL_SOCKET, SO_RCVBUF, (char *)&defaultRCVBUF, sizeof(defaultRCVBUF) );
		if (err == SOCKET_ERROR)
			throw SystemError("setsockopt");
	}
	if (defaultSNDBUF >= 0) {
		int err = setsockopt( s, SOL_SOCKET, SO_SNDBUF, (char *)&defaultSNDBUF, sizeof(defaultSNDBUF) );
		if (err == SOCKET_ERROR)
			throw SystemError("setsockopt");
	}
}


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
        other->obj = NULL;
	}
	Py_bufferKeeper(const Py_bufferKeeper &o){
		/* actually not a const constructor, see below */
		*this = 0;
	}
    Py_bufferKeeper &operator=(Py_buffer *buf) {
        Release();
        memcpy(static_cast<Py_buffer *>(this), buf, sizeof(*buf));
        buf->obj = NULL;
        return *this;
    }
    Py_bufferKeeper &operator=(const Py_bufferKeeper &o) {
		//we need the const assignment for containers.  OTOH, we need
		//to clear the source, for our semantics to work!  Do this
		//via this const casting trick.
		Py_bufferKeeper &a = const_cast<Py_bufferKeeper &>(o);
        Release();
		memcpy(this, &a, sizeof(a));
        a.obj = NULL;
		return *this;
	}
	~Py_bufferKeeper() {Release();}
	void Release() {
        if (obj)
            PyBuffer_Release(this);
    }
	operator bool () const {return obj != NULL;}
};

// A storage for Bufferkeepers.
class Py_bufferKeeperStore
{
public:
	Py_bufferKeeperStore() {}
private:
	// no copying
	Py_bufferKeeperStore(const Py_bufferKeeperStore &);
	Py_bufferKeeperStore &operator=(const Py_bufferKeeperStore &);
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

    // convenience when there is only one buffer
    const void *Data() const {
        assert(mNBuffers == 1);
        return (void *)mBuffers[0].buf;
    }
    const size_t DataLen() const {
        assert(mNBuffers == 1);
        return mBuffers[0].len;
    }

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
	SockStats(bool ignore) : mParent(0) { (void)ignore; Clear(); }
	SockStats() : mParent(&sGlobalStats) { Clear(); }
	void Clear() {
		mBytesReceived = mBytesSent = 0;
        mPacketsReceived = mPacketsSent = 0;
		mActiveSockets = mTotalSockets = 0 ;
		mAccepts = mConnects = mNoBufs = 0;
	}

	void BytesReceived(int bytes) {
		mBytesReceived.Add(bytes, false);
		if (mParent)
			mParent->BytesReceived(bytes);
	}

	void BytesSent(int bytes) {
		mBytesSent.Add(bytes, false);
		if (mParent)
			mParent->BytesSent(bytes);
	}

	void PacketReceived() {
		mPacketsReceived.Increment(false);
		if (mParent)
			mParent->PacketReceived();
	}

	void PacketSent() {
		mPacketsSent.Increment(false);
		if (mParent)
			mParent->PacketSent();
	}

	void AddSocket() {
		mActiveSockets.Increment(false);
		mTotalSockets.Increment(false);
	}
	void DeleteSocket() {
		mActiveSockets.Decrement(false);
	}

	void Connect() {
		mConnects.Increment(false);
		if (mParent)
			mParent->Connect();
	}

	void Accept() {
		mAccepts.Increment(false);
		if (mParent)
			mParent->Accept();
	}

	void Nobuf() {
		mNoBufs.Increment(false);
		if (mParent)
			mParent->Nobuf();
	}

	PyObject *GetStats() {
		return Py_BuildValue("{sL sL si si si si si si si}",
			"BytesReceived", mBytesReceived.value(),
			"BytesSent", mBytesSent.value(),
			"PacketsReceived", mPacketsReceived.value(),
			"PacketsSent", mPacketsSent.value(),
			"ActiveSockets", mActiveSockets.value(),
			"TotalSockets", mTotalSockets.value(),
			"Accepts", mAccepts.value(),
			"Connects", mConnects.value(),
			 "Nobuferr", mNoBufs.value()
		);
	}

private:


    Ccp::Atomic64 mBytesReceived;
    Ccp::Atomic64 mBytesSent;
	Ccp::Atomic32 mPacketsReceived;
	Ccp::Atomic32 mPacketsSent;
	Ccp::Atomic32 mActiveSockets;
	Ccp::Atomic32 mTotalSockets;
	Ccp::Atomic32 mAccepts;
	Ccp::Atomic32 mConnects;
	Ccp::Atomic32 mNoBufs;
	SockStats * const mParent;
public:
	static SockStats sGlobalStats;
};


SockStats SockStats::sGlobalStats(true);


// The KQueue class!
class KQueue
{
public:
    KQueue(){
        mHandle = kqueue();
        StartThread();
    }
    ~KQueue() {
        StopThread();
        close(mHandle);
    }

    void RegisterHandle(SOCKET handle, class SocketXtra *obj)
    {
        struct kevent evts[2];
        EV_SET(&evts[0], handle, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, (void*)obj);
        EV_SET(&evts[1], handle, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, (void*)obj);
        int ok = kevent(mHandle, evts, 2, NULL, NULL, NULL);
        if (ok < 0)
            throw SystemError("kevent");
    }

	void UnRegisterHandle(SOCKET handle, class SocketXtra *obj)
	{
		struct kevent evts[2];
		EV_SET(&evts[0], handle, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
		EV_SET(&evts[1], handle, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
		kevent(mHandle, evts, 2, NULL, NULL, NULL);
	}

    void Pump(float timeout); // defined later

    void PumpForever() {
        while (mRunning)
            Pump(0.1f);
        mThreadExited = true;
    }

    void StartThread() {
        mRunning = true;
        mThreadExited = false;
        // start the thread
        dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
        dispatch_async_f(queue, (void*)this, ThreadFunc);
    }

    void StopThread() {
        mRunning = false;
        while (mThreadExited == false)
            usleep(1000);
    }

    static void ThreadFunc(void *ctxt) {
        KQueue *self = (KQueue*)ctxt;
        self->PumpForever();
    }

    static KQueue &GetSingleton() {
        static KQueue kq;
        return kq;
    }

private:
    int mHandle;
    bool mRunning;
    bool mThreadExited;
};

//Extra data that we construct for a socket.  This holds reference count
//and members for the read queue, a critical section, and so on.
//It also keeps the SOCKET for worker threads to use.  This is only
//closed when a special reference count goes to zero, to avoid race
//condition with worker threads when the main thread decides to close it.

class SocketResult;
typedef IOPtr<class SocketResult> SocketResultPtr_t;
typedef std::deque<SocketResultPtr_t> SocketResultQueue_t;
typedef IOPtr<class SocketXtra> SocketXtraPtr_t;

// A class to keep an SocketExtra alive weakly alive while the client is
// waiting for it.
// Add SocketXtra* hashing
namespace __gnu_cxx
{
    template<>
    struct hash<SocketXtra *>
    {
        size_t operator() (const SocketXtra *s) const {
            return (size_t)s;
        }
    };
}

class XtraLookup
{
    friend class SocketXtra;

public:
    static XtraLookup &GetSingleton() {
        static XtraLookup sLookup;
        return sLookup;
    }

    inline void Lookup(SocketXtra *obj, SocketXtra **result);

protected:
    void Insert(SocketXtra *obj)
    {
        Ccp::MutexOwner own(mMutex);
        assert(mSet.find(obj) == mSet.end());
        mSet.insert(obj);
        assert(mSet.find(obj) != mSet.end());
    }

    void Remove(SocketXtra *obj)
    {
        Ccp::MutexOwner own(mMutex);
        assert(mSet.find(obj) != mSet.end());
        mSet.erase(obj);
    }

private:
    typedef __gnu_cxx::hash_set<SocketXtra *> set_t;
    typedef set_t::iterator set_i;
    Ccp::Mutex mMutex;
    set_t mSet;
};

class SocketXtra
{
private:
	SocketXtra(Socket _s) :
		mRefcount(1),
        mOurSockRef(false),
		mRecvPacketSequence(0),
		mMaxPacketSize(1024*1024), //one megabyte
        mBlockingSend(defaultBlockingSend)
    {
		mSocket.Swap(_s);
		if (mSocket.Valid()) {
			SockStats::sGlobalStats.AddSocket();
			mSockRef.Increment();
            mOurSockRef = true;
		}
        XtraLookup::GetSingleton().Insert(this);
        KQueue::GetSingleton().RegisterHandle(mSocket.Peek(), this);
    }
	~SocketXtra() {
    	_ASSERT(mRefcount.value() == -10000000);
	    XtraLookup::GetSingleton().Remove(this);
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
		SetBuffers(result);
        SocketXtra *xtra = new SocketXtra(_s);
		*xtradata = (void*)xtra;
        return result;
	}

	//reference management of SocketXtra lifetime.  Use smart reference counting
    // to help with the weak references.  Because a pointer may exist in the
    // xtralookup store after the reference reaches zero, we must be careful to
    // make these methods so that an object cannot be resurrected.
    Atomic32::value_t AddRef() {
        return mRefcount.Increment();
    }
	Atomic32::value_t Release() {
		Atomic32::value_t r = mRefcount.Decrement();
		if (r == 0) {
            // we reached zero. Claim deletion by setting it to a large negative
            // value.  This protects ous from other threads that may do an AddRef
            // where one of them gets a positive value and deems the object alive.
            if (mRefcount.CompareExchange(0, -10000000)) {
                // yes, we caught it.  Any increments racing with us will now return a negative value.
                r = -10000000;
                delete this;
            } // otherwise, someone increfed it in the mean time and resurrected it.
        }
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
        Atomic32::value_t ref = mSockRef.Increment();
		if (ref > 0) {
			return mSocket.Peek();
		} else {
            mSockRef.Decrement();
        	return INVALID_SOCKET;
        }
	}

	//Return a socket previously got by GetSocket.  If the socket refcount goes down
	//to zero, the socket is closed, _but_only_ if no one else manages to grab
	//it in the mean time.
	int ReturnSocket() {
		::Socket old;
		{
			Atomic32::value_t ref = mSockRef.Decrement();
			if (ref == 0) {
				// try to close the socket. It it is zero, set it to a very
				// negative value
				if (mSockRef.CompareExchange(0, -10000000)) {
					// Yes, success, no one else tried to GetSocket in between
					// our InterlockedDecrement and InterlockedCompareExchange.
					// Now we can close it.
					old.Swap(mSocket);
					_ASSERT(old.Valid());
					int result = close(old.Detach());
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

public:
    // Event handling stuff

    bool InitiateOrLinkRequest(SocketResult *request, bool isWriteOperation)
    {
        std::deque<IOPtr<SocketResult> > &queue = isWriteOperation ? mSendQueue : mRecvQueue;
        Ccp::MutexOwner own(mCS);
        queue.push_back(request);
        if (queue.size() == 1)
            return TryOperationAndPump(queue, true, false);
        else
            return false;
    }


    // An event has been routed to this SocketExtra. Find someone to notify.
    static void
    OnKQueueEvent(SocketXtra *xtra, bool isWriteOperation); // safe thunker

    static void
    CancelEvent(SocketXtra *xtra, SocketResult *request, bool isWriteOperation); // safe thunker

private:
    void OnKQueueEvent(bool isWriteOperation)
    {
        SocketResultQueue_t &queue = isWriteOperation ? mSendQueue : mRecvQueue;
        Ccp::MutexOwner own(mCS);
        if (queue.size()) {
            TryOperationAndPump(queue, false, true);
        }
    }

    void CancelEvent(SocketResult *event, bool isWriteOperation)
    {
        IOPtr<SocketResult> me(event);
        std::deque<IOPtr<SocketResult> > &queue = isWriteOperation ? mSendQueue : mRecvQueue;
        Ccp::MutexOwner own(mCS);
        std::deque<IOPtr<SocketResult> >::iterator it = queue.begin();
        while(it != queue.end() && *it != me)
            ++it;
        if (it != queue.end()) {
            if (it == queue.begin())
                PopAndPump(queue); // start subsequent requests
            else
                queue.erase(it);  // just erase it
        }
    }

    // Pump both queueues.  Done e.g. when the socket has been closed.
    void PumpQueues()
    {
        Ccp::MutexOwner own(mCS);
        if (mSendQueue.size())
            TryOperationAndPump(mSendQueue, false, true);
        if (mRecvQueue.size())
            TryOperationAndPump(mRecvQueue, false, true);
    }

    bool TryOperationAndPump(SocketResultQueue_t &queue, bool initialTry, bool sendToQueue)
    {
        try {
            // try the operation at the head of queue
            bool result = TryOperation(queue.front().get(), this, initialTry, sendToQueue);
            if (result)
                PopAndPump(queue);
            return result;
        } catch (...) {
            PopAndPump(queue);
            throw;
        }
    }

    void PopAndPump(SocketResultQueue_t &queue) throw()
    {
        // pop this operation off the queue head and continue.
        queue.pop_front();
        while(queue.size()) {
            try {
                // Kick off the new head
                if (!TryOperation(queue.front().get(), this, true, true))
                    return; // operation did not complete
            } catch (const std::exception &e) {
                IOEventQueue::GetSingleton().WriteUnraisable(e, "in Pump");
            } catch (...) {
                IOEventQueue::GetSingleton().WriteUnraisable("in Pump");
            }
            // head succeeded or unexpectedly threw an error
            queue.pop_front();
        }
    }

    // route the retry operation to the request.  forward declaration of function.
    static bool TryOperation(class SocketResult *request, SocketXtra *xtra, bool initialTry, bool sendToQueue);


public:
	// API for the blockingsend property.
	void SetBlockingSend(bool b) {mBlockingSend = b;}
	bool GetBlockingSend() const {return mBlockingSend;}

	//An API for the max packet size
	void SetMaxPacketSize(int s) {mMaxPacketSize = s;}
	int GetMaxPacketSize() const {return mMaxPacketSize;}

	//Close the socket.  We use the reference counting system
	//for this, so that we don't pull the rug out from underneath someone
	//just about to perform a socket operation.
	int CloseSocket()
	{
		KQueue::GetSingleton().UnRegisterHandle( mSocket.Peek(), this );
        int result = 0;
        if (mOurSockRef) {
            mOurSockRef = false;
            result = ReturnSocket();
        }
        // must pump both queues now
        PumpQueues();
        return 0;
	}

	// Get a the next index for receive packets
	unsigned int RecvPacketSequence()
	{
		return mRecvPacketSequence++;
	}

private:

	SocketXtra(const SocketXtra &);	//no copying
	const SocketXtra &operator=(const SocketXtra &); //no copying

public:
	SockStats mStats;

private:
    mutable Mutex mCS;	//critical section for stuff
	// The queues of requests
    SocketResultQueue_t mRecvQueue;
    SocketResultQueue_t mSendQueue;


private:
	Atomic32 mRefcount; //reference count for keeping the socketex alive.

    ::Socket mSocket; //the actual socket
	Atomic32 mSockRef; //for threadsafe socket close
    bool mOurSockRef;

	unsigned int mRecvPacketSequence;
	unsigned int mMaxPacketSize;
	bool mBlockingSend;
};


// The weak lookup here.  This is where an weak pointer is turned into a real one.
// We need to be careful not to resurrect an object that reference counting has
// already deemed dead.  This is why the SocketXtra Addref and Release methods
// are written the way they are.  An Addref that returns zero or negative is considered
// to point to a dying object and must be undone.
inline void XtraLookup::Lookup(SocketXtra *obj, SocketXtra **result)
{
    Ccp::MutexOwner own(mMutex);
    set_i it = mSet.find(obj);
    if (it != mSet.end()) {
        if (obj->AddRef() > 0) {
            *result = obj;
            return;
        }
        obj->Release();  // no, we lost a race to a Release and deletion.
    }
    *result = NULL;
}

static void IOPtr_AddRef(SocketXtra *x) {x->AddRef();}
static void IOPtr_Release(SocketXtra *x) {x->Release();}


void
SocketXtra::OnKQueueEvent(SocketXtra *xtra, bool isWriteOperation) {
    // An event occurred on the SocketExtra.  Get the strong ptr to the Xtra
    SocketXtraPtr_t ptr;
    XtraLookup::GetSingleton().Lookup(xtra, &ptr);
    if (!ptr)
        return;
    ptr->OnKQueueEvent(isWriteOperation);
}

void
SocketXtra::CancelEvent(SocketXtra *xtra, SocketResult *result, bool isWriteOperation) {
    // An event occurred on the SocketExtra.  Get the strong ptr to the Xtra
    SocketXtraPtr_t ptr;
    XtraLookup::GetSingleton().Lookup(xtra, &ptr);
    if (!ptr)
        return;
    ptr->CancelEvent(result, isWriteOperation);
}


//A specialization of IOWorker, for use in this file
class EIOWorker : public IOWorker
{
public:
	EIOWorker() : mError(false), mErrorCode(0) {}
	void ExecuteAndWait()
	{
		IOWorker::ExecuteAndWait();
        ReRaise();
	}
	std::string GetErrorMsg() const {return "thread::"+mErrorMsg;}
    virtual void WorkFunc(void) = 0;
private:
    void ThreadFunc(void)
    {
        try {
            WorkFunc();
        } catch (const SystemError &e) {
            mError = true;
            mErrorCode = e.code();
            mErrorMsg = e.what();
        } catch (const std::exception &e) {
            mError = true;
            mErrorMsg = e.what();
        } catch (...) {
            mError = true;
        }
    }


    void ReRaise()
    {
        if (mError) {
            if (mErrorCode)
                throw SystemError(mErrorCode, GetErrorMsg());
            if (mErrorMsg.size())
                throw std::runtime_error(GetErrorMsg());
            throw std::runtime_error(GetErrorMsg() + "unknown exception");
        }
    }

private:
    bool mError;
	int mErrorCode;
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


class AddrInfoResult : public EIOWorker
{
public:
	AddrInfoResult(const char *nodename, const char *servname, const struct addrinfo *hints)
		: mHints(*hints), mGAIError(0), mRes(0)
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
	void WorkFunc()
	{
		mGAIError = getaddrinfo(mN?mNode.c_str():0, mS?mServ.c_str():0, &mHints, &mRes);
		if (mGAIError == EAI_SYSTEM)
            throw SystemError("getaddrinfo");
	}

#if __APPLE__
#if __MAC_OS_X_VERSION_MAX_ALLOWED < __MAC_14_0
    typedef std::basic_string<char, std::char_traits<char>, Ccp::PyAllocator<char> > pystring;
#else
    typedef std::string pystring;
#endif
#endif
    pystring mServ, mNode;
	bool mS, mN;
	struct addrinfo mHints;
	struct addrinfo *GetRes() { //take ownership of the res
		struct addrinfo *r = mRes;
		mRes = 0;
		return r;
	}
    int GAIError() const {return mGAIError;}
private:
    int mGAIError;
	struct addrinfo *mRes;
};


class HostByAddrNameResult : public EIOWorker
{
public:
	HostByAddrNameResult(const char *name) : mName(name), mRes(0), mHerrno(0)
	{}
	HostByAddrNameResult(const char *addr, int len, int type) : mAddr(addr), mLen(len), mType(type), mRes(0), mHerrno(0)
	{}

	void WorkFunc()
	{
		struct hostent *e;
		if (mName.size())
			e = gethostbyname(mName.c_str());
		else
			e = gethostbyaddr(mAddr.c_str(), mLen, mType);
		if (!e)
            mHerrno = h_errno;
        else
            mRes = socket_hostent_dup(e);
	}

#if __APPLE__
#if __MAC_OS_X_VERSION_MAX_ALLOWED < __MAC_14_0
    typedef std::basic_string<char, std::char_traits<char>, Ccp::PyAllocator<char> > pystring;
#else
    typedef std::string pystring;
#endif
#endif
	pystring mName, mAddr;
	int mLen, mType;
	struct hostent *mRes;
    int mHerrno;
};



// Our external functions

//Create an Overlapped socket, along with a reference count.  Return
//the new refcount with a value of 1
int slsock_socket(void **xtradata, int family, int type, int proto)
{
	*xtradata = 0;
	try {
		Socket sock;
		{
			PyAllowThreads _threads;
			Socket(socket(family, type, proto)).Swap(sock);
		}
		if (!sock.Valid()) {
			socket_set_error(errno);
			return INVALID_SOCKET;
		}
		return SocketXtra::New(xtradata, sock);
	} catch (const std::exception &e) {
		TranslateExc(e);
		return (SOCKET_T) -2;
	}
}

int slsock_socket_from_fd(void **xtradata, int fd)
{
	*xtradata = 0;
	try {
        Socket sock(fd);
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


class ErrorTransfer
{
public:
    ErrorTransfer() : mErrorCode(0) {}

    void SetErrorCode(int code) { mErrorCode = code; }
    int GetErrorCode() const { return mErrorCode; }

    void SetError(const Ccp::SystemError &e){
		SetError(e.what(), e.code());
	}
	void SetError(const char *msg, int code){
		SetErrorCode(code);
		mErrorMsg = std::string("IOCompletion: ") + msg;
	}
    void SetError(const char *msg = "") {
        SetError(msg, errno);
    }
    void SetError(const std::exception &e) {
		if (dynamic_cast<const SystemError*>(&e)) {
			SetError(dynamic_cast<const SystemError&>(e));
		} else if (dynamic_cast<const std::bad_alloc*>(&e)) {
			SetError(e.what(), -2);
		} else {
			SetErrorCode(-1);
			mErrorMsg = std::string("IOCompletion: ") + typeid(e).name()+ " : " + e.what();
		}
	}

protected:
    // Re-raise the error on the target thread.
	void ReRaise() const {
		if (!GetErrorCode())
			return;
		if (GetErrorCode() == -2)
			throw std::bad_alloc();
		if (GetErrorCode() == -1)
			throw std::runtime_error(mErrorMsg);
		throw SystemError(GetErrorCode(), mErrorMsg.c_str());
	}


private:
    int mErrorCode; // the error code.
    std::string mErrorMsg; //the error message to go with the error code
};


class SocketResult : public IORequest, public ErrorTransfer
{
    friend class SocketXtra;
public:
    SocketResult(SocketXtra *xtra) : mXtra(xtra) {}

    SocketXtra *GetXtra() const { return mXtra; }

    // If we timeout, initiate next request in the queue
    void OnTimeout() {
        IORequest::OnTimeout();
        SocketXtra::CancelEvent(mXtra, this, IsWriteOperation());
    }

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
			if (e.GetCode() == ETIMEDOUT)
				return false;
			throw;
		}
		if (GetNoWait())
			return true;
		if (IsTimeout())
			return false;
        ReRaise();  // Re-raise error from thread
		return true;
	}

    void Request(double timeout)
    {
        // Initiate request.  This may link us in a queue, or just start kqueue wait
        if (mXtra->InitiateOrLinkRequest(this, IsWriteOperation()))
            return; // it finished on the first try
        if (timeout >= 0.0)
            SetTimeout(timeout);
        WaitForCompletion(GetOpStr());
    }


protected:
    // Try operation, called from SocketXtra
    bool TryOperation(SocketXtra *xtra, bool initialTry, bool sendToQueue)
    {
        bool done;
        SocketXtra::SockRef sockRef(xtra->Socket());

        if (! sendToQueue) {
            // The client is the immediate caller
            done = Operation(xtra, sockRef, initialTry);
        } else {

            // The client is waiting for a result in the event queue
            try {
                done = Operation(xtra, sockRef, initialTry);
            } catch (const std::exception &e) {
                SetError(e);
                done = true;
            } catch (...) {
                SetError("unknown exception");
                done = true;
            }

            if (done && OnNonTimeout()) {
                // we are ready with an error or success and we didn't lose to a timeout.
                try {
                    SubmitToQueue();
                } catch (const std::exception &e) {
                    IOEventQueue::GetSingleton().WriteUnraisable(e, "SubmitToQueue");
                }
            }
        }
        return done;
    }

    // The operation to perform
protected:
    virtual bool IsWriteOperation() const = 0;
    virtual const char *GetOpStr() const = 0;
    virtual bool Operation(SocketXtra *xtra, SOCKET handle, bool initialTry) = 0;

private:
	SocketXtra *mXtra;  // The socket which originate the IORequest
};

// This function defined here to resolve circular dependencies.
bool
SocketXtra::TryOperation(SocketResult *request, SocketXtra *xtra, bool initialTry, bool sendToQueue)
{
    return request->TryOperation(xtra, initialTry, sendToQueue);
}


void KQueue::Pump(float timeout)
{
    struct kevent events[10];
    struct timespec ts= {0};

    if (timeout > 0.0f) {
        ts.tv_sec = (time_t)timeout;
        ts.tv_nsec = (long)((timeout  - (float)ts.tv_sec) * 1e9f);
    }

    for(;;) {
        int nr = kevent(mHandle, NULL, 0, events, sizeof(events)/sizeof(*events), &ts);
        if (nr < 0)
            throw SystemError("kevent");
        if (nr == 0)
            return;
        for (int i = 0; i<nr; i++) {
            // Get the weak xtra data pointer
            SocketXtra *xtra = (SocketXtra*)events[i].udata;
            SocketXtra::OnKQueueEvent(xtra, events[i].filter == EVFILT_WRITE);
        }
        // try again, but just poll
        {
            struct timespec z = {0};
            ts = z;
        }
    }
}


class ConnectResult : public SocketResult
{
public:

    ConnectResult(SocketXtra *xtra) : SocketResult(xtra) {}

    void Connect(const sock_addr_t *addr, socklen_t addrlen, double timeout)
    {
        // Copy address
        mAddrLen = std::min((socklen_t)sizeof(mAddr), addrlen);
        memcpy(&mAddr, addr, mAddrLen);
        Request(timeout);
    }

    bool IsWriteOperation() const { return true; }
    const char *GetOpStr() const { return "connect"; }

    bool Operation(SocketXtra *xtra, SOCKET handle, bool first) {
        if (first) {
            // Initial attempt
            int ok = connect(handle, (struct sockaddr *)&mAddr, mAddrLen);
            if (ok == 0) {
                xtra->mStats.Connect();
                return true; // immediate success
            }
            // Initial connect cal should result in EINPROGRESS
            if (errno == EINPROGRESS)
                return false;
        } else {
            // We are being called because an IO event was signalled.
            // See if there is an error
            int error;
            socklen_t len = (socklen_t)sizeof(error);
            int ok = getsockopt(handle, SOL_SOCKET, SO_ERROR, &error, &len);
            if (ok < 0)
                throw SystemError("getsockopt");
            if (error)
                throw SystemError(error, "connect");

            // Verify that all is well.  We should be connected now
            ok = connect(handle, (struct sockaddr *)&mAddr, mAddrLen);
            if (ok == 0 || errno == EISCONN) {
                xtra->mStats.Connect();
                return true;
            }
            if (errno == EALREADY)
                return false; // false signal, must continue
        }
        throw SystemError("connect");
    }

private:
    struct sockaddr_storage mAddr;
    socklen_t mAddrLen;
};

//returns 0, or error code with WSAGetLastError() ready.
//-1 used to signal a python exception.
int	slsock_connect(PySocketSockObject *s, const sock_addr_t *addr, socklen_t addrlen, int raise_timeout )
{
    // a non-blocking connect
    if (s->sock_timeout == 0.0)
        return connect(s->sock_fd, (struct sockaddr *)addr, addrlen);

    // Create a IOResult
    try {
        IOPtr<ConnectResult> cr(new ConnectResult(static_cast<SocketXtra*>(s->sock_xtradata)));
        cr->Connect(addr, addrlen, s->sock_timeout);
        return 0;
    } catch (const SystemError &e) {
        if (raise_timeout && e.code() == ETIMEDOUT) {
			SetPyTimeout("connect");
			return 	-1;
		}
		SetSystemError(e);
		return e.code();
    } catch (const PyError &e) {
		if (!raise_timeout && e.Matches(socket_timeout)) {
			// for connect_ex, we want to just return the code
			errno = ETIMEDOUT;
			return ETIMEDOUT;
		}
		TranslateExc(e);
		return -1;
	} catch (const std::exception &e) {
		TranslateExc(e);
		return -1; //special Python error handling
	} catch (...) {
        TranslateExc();
        return -1; //
    }
}


int slsock_getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)
{
	try {
		*res = 0;
		IOPtr<AddrInfoResult> result(new AddrInfoResult(nodename, servname, hints));
		result->ExecuteAndWait();
        if (result->GAIError() == 0)
            *res = result->GetRes();
		return result->GAIError();
	} catch (const SystemError &e) {
		SetSystemError(e);
		return EAI_SYSTEM;;
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
        if (!result->mRes)
            h_errno = result->mHerrno;
		return result->mRes;
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
        if (!result->mRes)
            h_errno = result->mHerrno;
		return result->mRes;
	} catch (const std::exception &e) {
		TranslateExc(e);
		return (hostent*)-1; //python exception
	}
	return 0;
}


class AcceptResult : public SocketResult
{
public:

    AcceptResult(SocketXtra *xtra) : SocketResult(xtra) {}

    SOCKET_T Accept(void **xtradata, PySocketSockObject *s, struct sockaddr *addr, socklen_t *addrlen)
    {
        // Copy address
        // TODO: Support timed out xtradata stuff
        Request(s->sock_timeout);
        *addrlen = std::min(*addrlen, mAddrLen);
        memcpy(addr, &mAddr, *addrlen);
        return SocketXtra::New(xtradata, mTarget);
    }

    bool IsWriteOperation() const { return false; }
    const char *GetOpStr() const { return "accept"; }

    bool Operation(SocketXtra *xtra, SOCKET handle, bool first) {
        (void)first;
        mAddrLen = (socklen_t)sizeof(mAddr);
        Ccp::Handle target(accept(handle, (struct sockaddr *) &mAddr, &mAddrLen));
        if (target.Valid()) {
            mTarget.Swap(target);
            xtra->mStats.Accept();
            return true;
        }
        if (errno == EWOULDBLOCK)
            return false;
        throw SystemError("accept");
    }


private:
    struct sockaddr_storage mAddr;
    socklen_t mAddrLen;
    Ccp::Handle mTarget;
};


SOCKET_T slsock_accept(void **xtradata, PySocketSockObject *s, struct sockaddr *addr, socklen_t *addrlen)
{
	try {
		*xtradata = 0;
        if (s->sock_timeout == 0.0)
        {
            Socket r;
            {
                PyAllowThreads _threads;
                Socket(accept(s->sock_fd, addr, addrlen)).Swap(r);
		    }
            if (r.Valid())
                return SocketXtra::New(xtradata, r);
            return INVALID_SOCKET;
        }

        SocketXtra *ex = static_cast<SocketXtra*>(s->sock_xtradata);
        IOPtr<AcceptResult> result(new AcceptResult(ex));
        return result->Accept(xtradata, s, addr, addrlen);
    } catch (const std::exception &e) {
		TranslateExc(e);
	} catch (...) {
        TranslateExc();
	}
    return INVALID_SOCKET - 1; // indicate a python exception set.
}


class SendBase : public SocketResult
{
public:
    SendBase(SocketXtra *xtra) : SocketResult(xtra), mResult(0) {}
    bool IsWriteOperation() const {return true;}
    const char *GetOpStr() const {return "send";}

protected:
    void StealBuffer(Py_buffer *buf)
    {
        mKeeper = buf;
    }

    void Request(double timeout)
    {
        if (!GetXtra()->GetBlockingSend())
            SetNoWait();
        SocketResult::Request(timeout);
    }

protected:
    Py_bufferKeeper mKeeper;
    ssize_t mResult;
};


class SendResult : public SendBase
{
public:

    SendResult(SocketXtra *xtra) : SendBase(xtra) {}

    ssize_t SendTo(PySocketSockObject *s, Py_buffer *buf, int flags, struct sockaddr *addr, socklen_t addrlen)
    {
        // Copy address
        mAddrLen = std::min(addrlen, (socklen_t)sizeof(mAddr));
        if (mAddrLen)
            memcpy(&mAddr, addr, mAddrLen);
        // steal the buffer
        StealBuffer(buf);
        mFlags = flags;

        Request(s->sock_timeout);
        return mResult;
    }

    bool Operation(SocketXtra *xtra, SOCKET handle, bool first) {
        (void)first;
        if (mAddrLen)
            mResult = sendto(handle, (const void *)mKeeper.buf, (size_t)mKeeper.len, mFlags, (struct sockaddr*)&mAddr, mAddrLen);
        else
            mResult = send(handle, (const void *)mKeeper.buf, (size_t)mKeeper.len, mFlags);
        if (mResult >= 0) {
            xtra->mStats.BytesSent(mResult);
            return true;
        }
        if (errno == EWOULDBLOCK)
            return false;
        throw SystemError(GetOpStr());
    }

protected:
    int mFlags;
    struct sockaddr_storage mAddr;
    socklen_t mAddrLen;
};


class SendAllResult : public SendResult
{
public:

    SendAllResult(SocketXtra *xtra) : SendResult(xtra) {}

    bool Operation(SocketXtra *xtra, SOCKET handle, bool first) {
        (void) first;
        while (mResult < mKeeper.len) {
            if (!SendChunk(handle))
                return false;
        }
        xtra->mStats.BytesSent(mResult);
        return true;
    }

    bool SendChunk(SOCKET handle)
    {
        assert(mResult >= 0);
        char *buf = (char*)mKeeper.buf + mResult;
        ssize_t len = mKeeper.len - mResult;
        if (len > 0) {
            ssize_t sent = send(handle, (const void *)buf, len, mFlags);
            if (sent >= 0) {
                mResult += sent;
                return true;
            }
            if (errno == EWOULDBLOCK)
                return false;
            throw SystemError(GetOpStr());
        }
        return true;
    }
};

static void FlipHeader(uint32_t &header)
{
    if (htons(1) == 1) {
        // Big endian machine.  Our protocol uses little endian notation, so we must cheat
        uint32_t h = header;
        uint32_t tmp = (h & 0xff ) << 24;
        tmp |= ((h & 0xff00) << 8);
        tmp |= ((h & 0xff0000) >> 8);
        tmp |= ((h & 0xff000000) >> 24);
        header = tmp;
    }
}

class SendPacketResult : public SendBase
{
public:

    SendPacketResult(SocketXtra *xtra) : SendBase(xtra) {}

    ssize_t SendPacket(PySocketSockObject *s, Py_buffer *buf)
    {
        // Copy address
        // steal the buffer
        StealBuffer(buf);
        mHeader = mKeeper.len;
#ifdef PY3_COMPATIBILITY_MODE
    	mHeader = htonl(mHeader);
#endif
        FlipHeader(mHeader);
        Request(s->sock_timeout);
        return mResult;
    }

protected:
    bool Operation(SocketXtra *xtra, SOCKET handle, bool first) {
        (void) first;
        while (mResult < mKeeper.len + 4) {
            if (!SendChunk(handle))
                return false;
        }
        xtra->mStats.BytesSent(mResult);
        xtra->mStats.PacketSent();
        return true;
    }

    bool SendChunk(SOCKET handle)
    {
        assert(mResult >= 0);
        ssize_t sent;
        if (mResult < 4) {
            // sending header
            assert(sizeof(mHeader) == 4);
            sent = send(handle, (const char*)&mHeader + mResult, 4-mResult, 0);
        } else {
            // sending cargo
            ssize_t datasent = mResult - 4;
            char *buf = (char*)mKeeper.buf + datasent;
            ssize_t len = mKeeper.len - datasent;
            if (len > 0)
                sent = send(handle, (const void *)buf, len, 0);
            else
                sent = 0;
        }

        if (sent >= 0) {
            mResult += sent;
            return true;
        }
        if (errno == EWOULDBLOCK)
            return false;
        throw SystemError(GetOpStr());
    }

private:
    uint32_t mHeader;
};



//Helper function to parse arguments to send function, taking into account that we may accept
//a sequence instead of a string or buffer type thingy.
//fmt1 is expected to start with "s*" while fmt2 should start with "O"
// Update:  On unix we don't accept a sequence, only buffers.
int slsock_parse_send(PyObject *args, const char *fmt1, const char *fmt2, PyObject **obj, Py_buffer *buf, void *flags, void *addro)
{
	_ASSERT(fmt1[0] == 's' && fmt1[1] == '*');
	_ASSERT(fmt2[0] == 'O');
	if (!PyArg_ParseTuple(args, fmt1, buf, flags, addro)) {
#if 1
        return 0;  // no sequence support
#else
		PyErr_Clear();
		if (!PyArg_ParseTuple(args, fmt2, obj, flags, addro))
			return 0; //something weird

		//ok, found an object
		if (!PySequence_Check(*obj)) {
			PyErr_SetString(PyExc_TypeError, "expected a string or sequence");
			return 0;
		}
		//found a sequence
#endif
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
int slsock_sendto(PySocketSockObject *s, PyObject *obj, Py_buffer *pbuf, int flags, sock_addr_t *addr, socklen_t addrlen)
{
    assert(obj == NULL);
	try {
		if (s->sock_timeout == 0.0) {
			/* must support non-blocking send without timeout, since proper timeout causes race. */
			ssize_t err;
			if (addrlen)
				err = sendto(s->sock_fd, (const void *)pbuf->buf, (size_t)pbuf->len, flags, (struct sockaddr*)addr, (socklen_t)addrlen);
			else
				err = send(s->sock_fd, (const void *)pbuf->buf, (size_t)pbuf->len, flags);
			/* for non-blocking send, all errors are reported */
			return (int)err;
		}

		IOPtr<SendResult> result(new SendResult(static_cast<SocketXtra*>(s->sock_xtradata)));
		return result->SendTo(s, pbuf, flags, (sockaddr*)addr, addrlen);
	} catch (const std::exception &e) {
		TranslateExc(e);
		return -2;
	} catch (...) {
        TranslateExc();
    }
    return -2; // python error set
}

//return -1 on generail WSAGetLastError failure, and -2 to indicate python exception
int slsock_sendall(PySocketSockObject *s, PyObject *obj, Py_buffer *pbuf, int flags)
{
    assert(obj == NULL);
	try {
		if (s->sock_timeout == 0.0) {
			ssize_t sent = send(s->sock_fd, (const void *)pbuf->buf, (size_t)pbuf->len, flags);
            if (sent >= 0 && sent != pbuf->len) {
                SetPyTimeout("send");
                return -2;
            }
			return sent;
		}

		IOPtr<SendAllResult> result(new SendAllResult(static_cast<SocketXtra*>(s->sock_xtradata)));
		return result->SendTo(s, pbuf, flags, 0, 0);
	} catch (const std::exception &e) {
		TranslateExc(e);
	} catch (...) {
        TranslateExc();
    }
    return -2; // python error set
}

//returns -1 on a python error, 0 otherwise.
int slsock_sendpacket(PySocketSockObject *s, PyObject *obj, Py_buffer *pbuf)
{
	try {
        // non-blocking makes no sense here
		IOPtr<SendPacketResult> result(new SendPacketResult(static_cast<SocketXtra*>(s->sock_xtradata)));
		result->SendPacket(s, pbuf);
        return 0;
	} catch (const std::exception &e) {
		TranslateExc(e);
    } catch (...) {
        TranslateExc();
    }
    return -1;
}


class RecvPacketResult : public SocketResult
{
public:
    RecvPacketResult(SocketXtra *xtra) : SocketResult(xtra),
        mSequence(0),
        mBytesRead(0),
        mHeader(0),
        mPacketSize(0),
        mData(0),
        mDataLen(0),
        mEOF(false)
    {}

    ~RecvPacketResult() {
        if (mData)
            free(mData);
    }


    PyObject *RecvPacket(PySocketSockObject *s)
    {
        Request(s->sock_timeout);
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

protected:

    bool IsWriteOperation() const {return false;}
    const char *GetOpStr() const {return "recvpacket";}

    bool HasOobData() const { return mHeader & ceHeaderExpectPayloadOffset; }

    void HandleOobData(SOCKET handle) {
        if (!mData)
            return;
        // Ensure that we have enough data for the "out-of-band" data header
        if (mBytesRead < (sizeof(mHeader) + sizeof(uint32_t)))
            return;

#ifdef PY3_COMPATIBILITY_MODE
        uint32_t oobDataLen = ntohl(*(uint32_t *)(mData));
#else
        uint32_t oobDataLen = *(uint32_t *)(mData);
#endif
        // sanity check the out-of-band data length; mPacketSize was sanity checked already
        if (oobDataLen > mPacketSize) {
            char tmp[128] = {'\0'};
            snprintf(tmp, 127, "corrupted out-of-band data in packet detected at %d bytes, max is %d",
                     oobDataLen, mPacketSize);
            throw std::length_error(tmp);
        }

        // Ensure that we have received all "out-of-band" data
        if (mBytesRead < (sizeof(mHeader) + sizeof(uint32_t) + oobDataLen))
            return;

        // TODO here we can add support for the compression bits, if we'd ever want to

        char* oobData = sizeof(uint32_t) + mData;
        for (SCallbackEntry *callback = g_packetCallbackChainPostDecompress; callback; callback = callback->next) {
            bool stop = callback->callback(
                (long long)handle,
                mData + oobDataLen + sizeof(uint32_t),
                mPacketSize - oobDataLen,
                oobData,
                oobDataLen
            );
            if (stop) {
                // BlueNet ate the packet, so reset our internal state
                if (mData)
                    free(mData);
                mBytesRead = mDataLen = 0;
                mData = 0;
                return;
            }
        }
    }

    bool Operation(SocketXtra *xtra, SOCKET handle, bool first) {
        (void) first;
        while (NeedMore()) {
            if (!RecvChunk(handle))
                return false;

            if (HasOobData())
                HandleOobData(handle); // may reset the internal state, so just keep looping
        }
        if (!mEOF) {
            // do not increase mSequence when we received a closing packet, otherwise the bookkeeping on higher levels
            // can get confused in the event of a forced disconnect.
            mSequence = xtra->RecvPacketSequence();
        }
        xtra->mStats.BytesReceived(mBytesRead);
        xtra->mStats.PacketReceived();
        return true;
    }

    bool NeedMore() const {
        if (mEOF)
            return false;
        if (mBytesRead < sizeof(mHeader))
            return true;
        if (mBytesRead - sizeof(mHeader) < mPacketSize)
            return true;
        return false;
    }

    bool RecvChunk(SOCKET handle)
    {
        assert(mBytesRead >= 0);
        ssize_t rcvd;
        if (mBytesRead < sizeof(mHeader)) {
            // receiving header
            assert(sizeof(mHeader) == 4);
            rcvd = recv(handle, (char*)&mHeader + mBytesRead, sizeof(mHeader) - mBytesRead, 0);
            if (rcvd > 0 && mBytesRead+rcvd == 4) {
                // We completed reading the header, do stuff!
#ifdef PY3_COMPATIBILITY_MODE
            	mHeader = htonl(mHeader);
#endif
                FlipHeader(mHeader);
                mPacketSize = (mHeader & ceHeaderSizeMask);
                if (mPacketSize > (uint32_t)GetXtra()->GetMaxPacketSize()) {
                    char tmp[128] = {'\0'};
                    snprintf(tmp, 127, "too large a packet detected at %d bytes, max is %d",
                              mPacketSize, GetXtra()->GetMaxPacketSize());
                    throw std::length_error(tmp);
                }
            }
        } else {
            // Receiving cargo
            GrowBuffer();
            ssize_t datarecvd = mBytesRead - sizeof(mHeader);
            rcvd = recv(handle, mData + datarecvd, mDataLen - datarecvd, 0);
        }
        if (rcvd >= 0) {
            mBytesRead += rcvd;
            if (rcvd == 0)
                mEOF = true;
            return true;
        }
        if (errno == EWOULDBLOCK)
            return false;
        throw SystemError(GetOpStr());
    }


    //If there is no more space in the input buffer, grow it.
    void GrowBuffer()
    {
        ssize_t dataRead = mBytesRead - sizeof(mHeader);
        if (dataRead == mDataLen) {
            // We have filled the buffer with data and need to grow it
            // because there is more to come.
            ssize_t chunk = slsockAllocChunkSize;
            ssize_t newlen;
            if (chunk <= 0)
                //allocate to the max
                newlen = mPacketSize;
            else
                // Allocate at most chunk bytes
                newlen = std::min(mDataLen + chunk, (ssize_t)mPacketSize);
            char *data = (char*)realloc(mData, newlen);
            if (!data)
                throw std::bad_alloc();
            mData = data;
            mDataLen = newlen;
        }
    }

private:
    unsigned int mSequence;
    ssize_t mBytesRead;
    uint32_t mHeader;
    uint32_t mPacketSize;
    char *mData;
    ssize_t mDataLen;
    bool mEOF;
};


PyObject *slsock_recvpacket(PySocketSockObject *s)
{
	try {
		IOPtr<RecvPacketResult> result(new RecvPacketResult(static_cast<SocketXtra*>(s->sock_xtradata)));
		return result->RecvPacket(s);
	}catch (const std::exception &e) {
		TranslateExc(e);
    }catch (...) {
        TranslateExc();
    }
    return NULL;
}


PyObject *slsock_recvpacket_oob(PySocketSockObject *s)
{
    try {
		IOPtr<RecvPacketResult> result(new RecvPacketResult(static_cast<SocketXtra*>(s->sock_xtradata)));
        return Py_BuildValue("NON", result->RecvPacket(s), Py_None, result->GetSequence());
	}catch (const std::exception &e) {
		TranslateExc(e);
    }catch (...) {
        TranslateExc();
    }
    return NULL;
}


class RecvResult : public SocketResult
{
public:

    RecvResult(SocketXtra *xtra) : SocketResult(xtra), mResult(0) {}

    ssize_t RecvFrom(PySocketSockObject *s, Py_buffer *buf, int len, int flags, struct sockaddr *addr, socklen_t *addrlen)
    {
        // prepare to copy address
        if (addr)
            mAddrLen = std::min(*addrlen, (socklen_t)sizeof(mAddr));
        else
            mAddrLen = 0;

        // steal the buffer
        mKeeper = buf;
        mLen = len;
        mFlags = flags;

        // TODO: Support timed out xtradata stuff
        Request(s->sock_timeout);
        mKeeper.Release();

        // copy address
        if (addr) {
            *addrlen = mAddrLen;
            memcpy(addr, &mAddr, mAddrLen);
        }
        return mResult;
    }

    bool IsWriteOperation() const { return false; }
    const char *GetOpStr() const { return mAddrLen ? "recvfrom" : "recv"; }

    bool Operation(SocketXtra *xtra, SOCKET handle, bool first) {
        (void)first;
        if (mAddrLen)
            mResult = recvfrom(handle, (void *)mKeeper.buf, mLen, mFlags, (struct sockaddr*)&mAddr, &mAddrLen);
        else
            mResult = recv(handle, (void *)mKeeper.buf, mLen, mFlags);
        if (mResult >= 0) {
            if (mResult > 0)
                xtra->mStats.BytesReceived(mResult);
            return true;
        }
        if (errno == EWOULDBLOCK)
            return false;
        throw SystemError(GetOpStr());
    }

protected:
    Py_bufferKeeper mKeeper;
    int mLen;
    int mFlags;
    struct sockaddr_storage mAddr;
    socklen_t mAddrLen;
    ssize_t mResult;
};


//return negative with error, with a python error set.
ssize_t slsock_recvfrom(PySocketSockObject *s, Py_buffer *pbuf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	try {
		if (s->sock_timeout == 0.0) {
			/* must support non-blocking send without timeout, since proper timeout causes race. */
			ssize_t err;
            if (fromlen) {
                socklen_t addrlen = *fromlen;
				err = recvfrom(s->sock_fd, (void *)pbuf->buf, len, flags, (struct sockaddr*)from, &addrlen);
                *fromlen = addrlen;
			} else
				err = recv(s->sock_fd, (void *)pbuf->buf, len, flags);
			/* for non-blocking recv, all errors are reported */
            if (err < 0)
                socket_set_error(errno);
			return (int)err;
		}

		IOPtr<RecvResult> result(new RecvResult(static_cast<SocketXtra*>(s->sock_xtradata)));
        if (fromlen) {
            socklen_t addrlen = *fromlen;
            ssize_t r = result->RecvFrom(s, pbuf, len, flags, (sockaddr*)from, &addrlen);
            *fromlen = addrlen;
            return r;
        } else {
            return result->RecvFrom(s, pbuf, len, flags, 0, 0);
        }
	} catch (const std::exception &e) {
		TranslateExc(e);
		return -2;
	} catch (...) {
        TranslateExc();
    }
    return -2; // python error set
}


/* Select support.  Although it is a silly programming paradigm to use with stackless, we still want to support it to aid
 * in migrating apps that use select, such as SocketServer.
 */

class Fdsets
{
public:
	Fdsets(const fd_set *_rd, const fd_set *_wr, const fd_set *_ex) :
	  rd(*_rd), wr(*_wr), ex(*_ex)
	{}

	void Get(fd_set *_rd, fd_set *_wr, fd_set *_ex)
	{
		copy(_rd, &rd);
		copy(_wr, &wr);
		copy(_ex, &ex);
	}

private:
	static void copy(fd_set *d, const fd_set *s)
	{
        *d = *s;
    }

public:
	fd_set rd, wr, ex;
};

// The WSIOWorker thread based subclass, that performs the actual select on a
// long lived worker thread.
class SelectResult : public EIOWorker
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
	void WorkFunc()
	{
		mResult = select(mNfds, &mSets.rd, &mSets.wr, &mSets.ex, mTimeoutP);
		if (mResult == SOCKET_ERROR)
			throw SystemError("select");
	}
	int GetResult(fd_set * readfds, fd_set * writefds, fd_set *exceptfds)
	{
		mSets.Get(readfds, writefds, exceptfds);
		return mResult;
	}

private:
	int mNfds;
	Fdsets mSets;
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
		 SetSystemError(e);
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

//Set the zero byte reads attribute.  Not supported on linux
int slsock_setzerobytereads(PySocketSockObject *s, int on){
    (void)s;
    (void)on;
	return 0;
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
	if (!PyDict_Check(dict)) {
		PyErr_SetString(PyExc_TypeError, "");
        return NULL;
    }
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
