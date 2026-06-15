// Copyright © 2011 CCP ehf.

#pragma once
#ifndef BlueNet_H
#define BlueNet_H


#pragma warning (disable : 4996) // remove Windows nagging about using _strincmp() etc

#include <LinkHash.h>
#include <RWSpinLock.h>
#include <ScopedLocks.h>

#if __APPLE__
#include <dispatch/dispatch.h>
#endif

//------------------------------------------------------------------------------
//#define BN_DEBUG_LOG
#define BN_CCP_DEBUG_LOG
#define BN_ENUM(a) //a
#define BN_CLIENTTRACKING(a) //a
#define BN_PURGETRANSPORT(a) //a
#define BN_USERID(a) //a
#define BN_NODEID(a) //a
#define BN_XNODEID(a) //a
#define BN_CREATE_HEADER(a) //a
#define BN_CONST_IMPORT(a) //a
#define BN_DELIVER(a) //a
#define BN_TONODE(a) //a
#define BN_TOPROXY(a) //a
#define BN_TOCLIENT(a) //a
#define BN_ROUTE(a) //a
#define BN_SEND_LIST(a) //a
#define BN_AGG(a) //a
#define BN_PING(a) //a

#define BN_ADD_MESSAGE(a) //a
#define BN_SERIALIZE(a) //a
#define BN_SERIALIZE_DICT(a) //a
#define BN_UNSERIALIZE(a) //a
#define BN_BLUE_CALLBACK(a) //a
#define BN_BATCH(a) //a
#define BN_PACKSTRING(a) //a

// how small the data can be that will fit into a static job allocation,
// larger than this will require a new/delete
const int BLUE_NET_SMALL_BUFFER_SIZE = 4096;
const int DEFAULT_PING_FREQUENCY = 30;
const unsigned int MIN_SCHEDULED_IO_INTERVAL = 15;

// Keep this in sync with the identically named constants in carbon/common/lib/const.py
enum AddressType {
	ADDRESS_TYPE_NODE = 1,
	ADDRESS_TYPE_CLIENT = 2,
	ADDRESS_TYPE_BROADCAST = 4,
	ADDRESS_TYPE_ANY = 8
};

// the amount of data that can accumulate on a transport before
// transmission is forced
const int BLUE_TRANSPORT_PACKET_AGGREGATOR_SIZE = 1500;

struct BlueNetHeader;
class BitPacker;
class BitPackerManaged;
struct DataCallbackJob;
struct TransportRepr;
struct BlueNetPacket;
class BlueNetPacketBatch;
class Marshal;

//------------------------------------------------------------------------------
class BlueNet
{
public:
	static bool Init( PyObject* blueModule, PySocketModule_APIObject* socketAPI );

	//---------------------------------------
	struct PacketInfo
	{
		union
		{
			unsigned long long fromNodeID;
			unsigned long long fromClientID;
		};

		unsigned int accumulatedLatency; // how many milliseconds ago this packet was sent
		int blueNetKey;
		long long TransportIdArrivedOn; // this is NOT necessarily the source, just what's on the first hop back
	};

	// register interest in a packet type. If the callback returns TRUE
	// then the packet is consumed, otherwise it will traverse the
	// chain and ultimately be handed back to Python
	typedef void(*DataCallback)( const unsigned long long fromID, const int blueNetKey, const char* data, const int len );
	typedef void(*ExtendedDataCallback)( const BlueNet::PacketInfo* info, const char* data, const int len );

	// RegisterCallback flavors:
	//
	// Async - Issue the callback immediately on the thread that read
	//         the message, fastest, but messages may come in at any time on
	//         any thread
	// Sync - Issue callbacks serially from the single thread that
	//        calls DeliverCPackets();
	// FromPython - Issue callbacks from a valid Python context
	//              using Py_AddPendingCall(...)
	void RegisterCallbackAsync( DataCallback callback, const int blueNetKey );
	void RegisterCallbackSync( DataCallback callback, const int blueNetKey );
	void RegisterCallbackFromPython( DataCallback callback, const int blueNetKey );
	void RegisterCallbackAsync( ExtendedDataCallback callback, const int blueNetKey );
	void RegisterCallbackSync( ExtendedDataCallback callback, const int blueNetKey );
	void RegisterCallbackFromPython( ExtendedDataCallback callback, const int blueNetKey );

	void UnregisterCallback( const int blueNetKey );

	//---------------------
	enum Priority
	{
		PRIORITY_NORMAL =0, // ~200ms worst case (or aggregating for a flush)
		PRIORITY_HIGH, // is sent out immediately
	};

	void DeliverCPackets();

	// used from the server to route a packet to this client
	bool SendPacketToClient( const unsigned long long clientID,
							 const int blueNetKey,
							 const char *data,
							 const int len,
							 const int priority =PRIORITY_NORMAL );
	bool SendPacketToClientList( const unsigned long long* clientList,
								 const int listLen,
								 const int blueNetKey,
								 const char *data,
								 const int len,
								 const int priority =PRIORITY_NORMAL );
	bool SendPacketToAllClients( const int blueNetKey,
								 const char *data,
								 const int len,
								 const int priority =PRIORITY_NORMAL );
	bool SendPacketToChar( const unsigned long long charID,
						   const int blueNetKey,
						   const char *data,
						   const int len,
						   const int priority =PRIORITY_NORMAL );
	bool SendPacketToCharList( const unsigned long long* charList,
							   const int listLen,
							   const int blueNetKey,
							   const char *data,
							   const int len,
							   const int priority =PRIORITY_NORMAL );

	unsigned long long ClientIDFromCharID( const unsigned long long charID );
	unsigned long long CharIDFromClientID( const unsigned long long charID );
	unsigned long long GetProxyNodeIDByClientID( const unsigned long long clientID );

	static inline int BlueNetKeyFromName( const char* name );

	bool SendPacketToNode( const unsigned long long nodeID,
						   const int blueNetKey,
						   const char *data,
						   const int len,
						   const int priority =PRIORITY_NORMAL );


	unsigned int Flush();
	unsigned int FlushToClientList( const unsigned long long* clientList, const int listLen );
	unsigned int FlushToCharList( const unsigned long long* charList, const int listLen );

	//---------------------
	enum Mode
	{
		MODE_NONE,
		MODE_PROXY,
		MODE_SERVER,
		MODE_CLIENT,
	};
	int GetRoutingMode() const { return m_routingMode; }

	// only MODE_PROXY can sensibly ask these questions
	long long GetClientIDFromTransportID( long long transportID );
	bool IsClient( long long transportID );

	int GetProxyNodeList( long long **returnList ); // returns a new'ed list of Node Id's and how many are contained
	long long GetTransportIDFromClientID( long long clientId );
	long long GetTransportIDFromNodeID( long long nodeID );
	long long GetNodeIDFromTransportID( long long transportID );
	long long PickRandomProxyNodeId( bool readLockHeld );
	long long GetMyNodeID() const { return m_myNodeID; }
	long long GetMyClientID() const { return m_myClientID; }
	unsigned int GetWatchdogInterval() const { return m_watchdogInterval; }
	unsigned int GetMinScheduledIOInterval() const { return m_minScheduledIOInterval; }
	unsigned int GetLatencyToFirstHop();

	enum SessionStatus
	{
		STATUS_LOGIN = -1,
		STATUS_LOGOUT = -2,
	};
	typedef void(*ClientSessionCallback)( const long long charId, const long long clientId, int status );
	// the GIL will be held when these callbacks are issued, but they will come in ASYNChronously
	void RegisterClientSessionCallback( ClientSessionCallback callback );
	void UnRegisterClientSessionCallback( ClientSessionCallback callback );

	static bool SerializeObject( PyObject* object, BitPackerManaged& packet );
	static PyObject* UnserializeObject( BitPacker& packer );

private:

	BlueNet();

	// python interface
	static PyObject* PySetMyNodeID( PyObject *self, PyObject *args );
	static PyObject* PySetTransportNodeID( PyObject *self, PyObject *args );
	static PyObject* PySetMinPingFrequency( PyObject *self, PyObject *args );
	static PyObject* PySetAggregateThreadFrequency( PyObject *self, PyObject *args );
	static PyObject* PySetMinScheduledIOInterval( PyObject *self, PyObject *args ); // Minimum time the communications engine will wake up Blue with a packet event <default 15ms>
	static PyObject* PySetWatchdogInterval( PyObject *self, PyObject *args ); //SetWatchdogInterval(ms)\n\nInterval the watchdog wakes up to check for unserviced packets <default 23ms>
	static PyObject* PySetWakeupMethod( PyObject *self, PyObject *unused ); // control how CarbonIO wakes stuff up
	static PyObject* PyEnumerateTransport( PyObject *self, PyObject *args ); // expose routing table
	static PyObject* PyPurgeTransport( PyObject *self, PyObject *args ); // only internal cleanup, remove it
	static PyObject* PySendBlueNetPacket( PyObject *self, PyObject *args ); // given a message create a bitpacked header
	static PyObject* PySetMode( PyObject *self, PyObject *args ); // am I a duck, a chair, or a suspension bridge?
	static PyObject* PyAddClient( PyObject *self, PyObject *args ); // add a user to a transport
	static PyObject* PyMapCharToClient( PyObject *self, PyObject *args ); // associate a client with a userID
	static PyObject* PyPurgeClient( PyObject *self, PyObject *args ); // remove user from transport
	static PyObject* PyGetLatencyToFirstHop( PyObject *self, PyObject *unused ); // Valid for client only, reprots latency to the proxy
	static PyObject* PyGetWakeupMethod( PyObject *self, PyObject *unused ); // view how CarbonIO wakes stuff up
	static PyObject* PyGetRoutingMode( PyObject *self, PyObject *unused ); // what mode am I in?
	static PyObject* PyAddProxyNode( PyObject *self, PyObject *unused ); // tell bluenet about a proxy node
	static PyObject* PyDelProxyNode( PyObject *self, PyObject *unused ); // remove proxy node from bluenet
	static PyObject* PyClearProxyNodes( PyObject *self, PyObject *unused ); // clear them all

	static PyObject* PyAddSendEvent( PyObject *self, PyObject *args );
	static PyObject* PyTransmitBatch( PyObject* module, PyObject* args );
	static PyObject* PyIsBatchingEnabled( PyObject* module, PyObject* args );
	static PyObject* PySetBatchingEnabled( PyObject* module, PyObject* args );
	static PyObject* PyTestSerializer( PyObject* module, PyObject* args );
	static PyObject* PyGetStats( PyObject* module, PyObject* args );
	static PyObject* PySetNewStringMaxLen( PyObject* module, PyObject* args );
	static PyObject* PyGetNewStringMaxLen( PyObject* module, PyObject* args );
	static PyObject* PyDumpStringTable( PyObject* module, PyObject* args );
	static PyObject* PyDumpNewStringTable( PyObject* module, PyObject* args );
	static PyObject* PyDumpTransportTable( PyObject* module, PyObject* args );

	//---------------------
	enum CallbackStyle
	{
		CALLBACK_SYNC, // make the callback in C outside the normal Python tick, but synchronous with the main thread
		CALLBACK_PYTHON, // synchronous, but deliver in a valid Python context
		CALLBACK_ASYNC, // you get it when we get it, on whatever thread it arrived on (here there be dragons)
	};
	void RegisterCallback( DataCallback callback, const int blueNetKey, int style =CALLBACK_SYNC );
	void RegisterCallback( ExtendedDataCallback callback, const int blueNetKey, int style =CALLBACK_SYNC );

	// check for sub-macho delivery
	static int OnPostDecompress( long long descriptor, const char* data, const int len, const char* OobData, const int OobLen );

	bool ParseHeader( const char* data, const int len, BlueNetHeader *header );
	void PurgeTransport( long long transportID );

	static PyMethodDef m_methods[];
	static BlueNet m_singleton;
	static int m_constNoneKey;
	static int m_constPongKey;
	static int m_constMinPingFrequency;
	time_t m_fastTime; // updated once per frame rather than for every packet

	// Using Ccp::LinkHash instead of STL HashSet because we need the speed here
	Ccp::LinkHash<TransportRepr*> m_transportsBySocket;
	Ccp::LinkHash<TransportRepr*> m_transportsByTransportID;
	Ccp::LinkHash<TransportRepr*> m_transportsByNodeID;
	Ccp::LinkHash<TransportRepr*> m_transportsByClientID;
	Ccp::LinkHash<TransportRepr*> m_transportsByProxyID;

	//---------------------
	struct ClientMapping
	{
		TransportRepr* transport;
		long long clientID;
	};
	Ccp::LinkHash<ClientMapping> m_clientMapByCharID;

	Ccp::RWSpinlock m_transportLock; // used for transport tables and mappings

	//---------------------
	struct DataCallbackRepr
	{
		int style; // callback style
		DataCallback callback;
		ExtendedDataCallback exCallback;
	};
	Ccp::LinkHash<DataCallbackRepr> m_callbackList;
	Ccp::RWSpinlock m_callbackListLock;

	Ccp::LinkHash<ClientSessionCallback> m_clientSessionCallbackList;
	Ccp::RWSpinlock m_clientSessionCallbackListLock;

	int m_routingMode;
	long long m_myNodeID;
	long long m_myClientID;
	unsigned int m_watchdogInterval;
	unsigned int m_minScheduledIOInterval;

	bool m_pendingCallPending;
	DataCallbackJob* m_callbackHead;
	DataCallbackJob* m_callbackTail;
	DataCallbackJob* m_callbackHead_C;
	DataCallbackJob* m_callbackTail_C;
	CcpMutex m_callbackJobLock;
	DataCallbackJob* m_callbackPool;
	CcpMutex m_callbackPoolLock;

	DataCallbackJob* GetDataCallbackJob();
	void FreeDataCallbackJob( DataCallbackJob* callback );

	bool SendClientPacket( const int blueNetKey,
						   const char* data,
						   const int len,
						   TransportRepr* transport,
						   const long long clientID,
						   const int priority );
	unsigned long long CheckForPing( TransportRepr* transport );
	void SequenceTransports();
	void Route( BlueNetHeader* header, BitPacker* packer, TransportRepr* source, const char* data, const int len );
	bool Deliver( BlueNetHeader* header, const char* data, const int len, long long TransportIdArrivedOn );
	static int DeliverEx( void* arg ); // does the work while holding a vliad Python context (pending call)
	static void DeliverJobs( DataCallbackJob *jobs );

	// queue if called for
	bool SendPacketEx( TransportRepr *transport,
					   const char* data,
					   unsigned int len,
					   const char* headerData,
					   unsigned int headerLen,
					   const int priority );

	inline unsigned int FlushTransport( TransportRepr* transport );
	void AnswerPing( TransportRepr* transport, unsigned long long timestamp );

	static void BlueNetCallback( const BlueNet::PacketInfo* info, const char* data, const int len );

	int m_blueNetHandlerPacketKey;

	void PackString( const char* string, const unsigned int len, BitPackerManaged& packer );
	bool UnpackString( char** string, unsigned int* len, BitPacker& packer );

	void PackMarshalStream( PyObject* object, BitPackerManaged& packer );

	Marshal* m_marshal;

	struct Stats
	{
		long long numberFellback;
		long long numberTotal;
		long long bytes;
		long long bytesCompare;
		long long stringHits;
		long long stringMisses;
	};
	Stats m_stats;

	struct StringEntry
	{
		long long frequency; // for stat collection
		unsigned int index;
		char* newString; // strings that were not found in the main table are stored dynamically here
	};
	Ccp::LinkHash<StringEntry> m_stringTable; // existing strings, preloaded and static
	Ccp::LinkHash<StringEntry> m_newStrings; // any new string get plugged in here for dump/add offline
	unsigned int m_NewStringMaxLen;

	static void LogStatus( const char* msg );
	static void LogError( const char* msg );

	PySocketModule_APIObject *m_socketAPI;
};

extern BLUEIMPORT BlueNet* BeNet; // for external access to the singleton

//------------------------------------------------------------------------------
int BlueNet::BlueNetKeyFromName( const char* name )
{
	if ( !name || !name[0] )
	{
		return 0;
	}

	unsigned int ret = 0x811C9DC5; // NOT random, this is careful magic
	do
	{
		ret ^= (unsigned int)*name;
		ret *= 0x1000193; // NOT random, this is careful magic
	} while( *(name++) );

	return (int)ret;
}

#endif
