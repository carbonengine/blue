// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#include <socketmodule.h>

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#if CCP_STACKLESS

#include "BlueNet.h"
#include "StructPacker.h"
#include "IBluePython.h"
#include "Marshal.h"

#include "stdlib.h"
#include "stdio.h"

#include <SimpleLog.h>

BlueNet BlueNet::m_singleton; // so the static singleton can exist
int BlueNet::m_constNoneKey = 0;
int BlueNet::m_constPongKey;
int BlueNet::m_constMinPingFrequency = DEFAULT_PING_FREQUENCY; // in seconds.. at least this much time must elapse

BlueNet* BeNet = 0;
static CcpLogChannel_t s_blueNetChannel = CCP_LOG_DEFINE_CHANNEL("BlueNet");

extern const char* MARSHAL_STRINGS[];
extern int MARSHAL_STRING_ARRAY_LEN;

//------------------------------------------------------------------------------
SP_START( BlueNetHeader )
	SP_TYPE( int64_t, sourceID );
	SP_TYPE( int64_t, destinationType );
	SP_TYPE( int64_t, destinationID );
	SP_TYPE( int32_t, blueNetKey );
	SP_TYPE( uint32_t, forkedAddresses );
	SP_TYPE( uint32_t, serialNumber );
	SP_TYPE( uint32_t, latencyAccumulator );
	SP_TYPE( bool, priority ); // true == high
	SP_TYPE( uint64_t, timestamp ); // if non-zero, implied ping request
SP_END;

const unsigned long long TIMESTAMP_PINGBACK_BIT = 0x8000000000000000ULL;

// enough room to absorb any overhead a worst-case bitpack might cause, plus the CRC
const int BLUE_HEADER_SCRATCH_SIZE = ((sizeof(BlueNetHeader) * 3) / 2) + sizeof(unsigned short);

//------------------------------------------------------------------------------
struct DataCallbackJob
{
	BlueNet::DataCallback callback;
	BlueNet::ExtendedDataCallback exCallback;
	long long fromID;
	char* data;
	char smallData[BLUE_NET_SMALL_BUFFER_SIZE];
	int len;
	BlueNet::PacketInfo info;
	DataCallbackJob *next;
};

//------------------------------------------------------------------------------
struct TransportRepr
{
	long long descriptor = 0; // the actual socket
	long long ID = 0; // transportID
	char name[128] = {'\0'}; // transport name
	long long nodeID = 0;

	time_t nextLatencyTimeCheck = 0;
	int latency = 0; // approximate lag-time to this transport

	bool isClientTransport = false; // proxy marks a client connection
	long long clientID = 0; // for proxies, what client is connected

	int sequence = 0; // numerically continuous and contiguous numbering imposed on the hash list (forking optimization)

	CcpMutex packetAggregateLock = {"BlueNet/TransportRepr", "packetAggregateLock"};
	char packetAggregator[ BLUE_TRANSPORT_PACKET_AGGREGATOR_SIZE * 2 ] = {'\0'};
	int bytesAggregated = 0;
};

//------------------------------------------------------------------------------
struct BlueNetPacket
{
	BitPackerManaged payload;
	unsigned long long *recipientList;
	int listSize;
	int listEntries;

	BlueNetPacket() { listSize = 0; recipientList = 0; }
	~BlueNetPacket() { CCP_DELETE[] recipientList; }
};

//------------------------------------------------------------------------------
class BlueNetPacketBatch
{
public:
	int m_listSize;
	int m_numberOfEntries;
	BlueNetPacket** m_list;

	void Grow()
	{
		// grow by doubling, migrate all the entries into the new list,
		// fill in the new ones, then destroy the old reference
		int newSize = m_listSize * 2;
		BlueNetPacket** newList = CCP_NEW("BlueNet/DestinyPacketBatch") BlueNetPacket*[ newSize ];

		memcpy( newList, m_list, m_listSize * sizeof(BlueNetPacket*) );
		for( int i=m_listSize; i<newSize; i++ )
		{
			newList[i] = CCP_NEW("BlueNet/DestinyPacket-grow") BlueNetPacket;
		}
		CCP_DELETE[] m_list;
		m_list = newList;
		m_listSize = newSize;
	}

	BlueNetPacketBatch()
	{
		m_listSize = 4;
		m_numberOfEntries = 0;
		m_list = CCP_NEW("BlueNet/BlueNetPacket list") BlueNetPacket*[ m_listSize ];
		for( int i=0; i<m_listSize; i++ )
		{
			m_list[i] = CCP_NEW("BlueNet/DestinyPacket") BlueNetPacket;
		}
	}

	~BlueNetPacketBatch()
	{
		for( int i=0; i<m_listSize; i++ )
		{
			CCP_DELETE m_list[i];
		}
		CCP_DELETE[] m_list;
	}
};

//------------------------------------------------------------------------------
enum
{
	PYOBJECT_NONE =0,
	PYOBJECT_STRING,
	PYOBJECT_INT,
	PYOBJECT_LONG,
	PYOBJECT_DOUBLE,
	PYOBJECT_LIST,
	PYOBJECT_TUPLE,
	PYOBJECT_TRUE,
	PYOBJECT_FALSE,
	PYOBJECT_DICT,
	PYOBJECT_MARSHALED_EMBEDDED,
	PYOBJECT_MARSHALED,
};

static Ccp::SimpleLog s_log;
//------------------------------------------------------------------------------
#include <stdarg.h>
static CcpMutex s_logLock = {"BlueNet", "s_logLock"};

//------------------------------------------------------------------------------
void bnlog( const char* format, ... )
{
#if defined(BN_DEBUG_LOG) || defined(BN_CCP_DEBUG_LOG)
	char temp[64000];
	if ( !format || !format[0] )
	{
		return;
	}
	va_list arg;
	va_start( arg, format );
	int len = vsprintf( temp, format, arg );
	va_end ( arg );
#endif
#ifdef BN_DEBUG_LOG
	s_logLock.Acquire();
	s_log.out( "%s", temp );
	s_logLock.Release();
#endif
#ifdef BN_CCP_DEBUG_LOG
	CCP_LOGWARN_CH( s_blueNetChannel, "%s", temp );
#endif
}

//------------------------------------------------------------------------------
PyMethodDef BlueNet::m_methods[] =
{
	{ "SetMyNodeID", PySetMyNodeID, METH_VARARGS, "SetMyNodeID(id)\n\nIdentify this node" },
	{ "SetTransportNodeID", PySetTransportNodeID, METH_VARARGS, "SetTransportNodeID(transport,id)\n\nset the nodeID of this transport" },
	{ "SetMode", PySetMode, METH_VARARGS, "SetMode(<proxy/server/client>)\n\nHow low-level routing should act, must be set once and only once" },
	{ "SetMinPingFrequency", PySetMinPingFrequency, METH_VARARGS, "SetMinPingFrequency(seconds)\n\nSets the minimum time that must elapse before latency is determined" },
	{ "SetAggregateThreadFrequency", PySetAggregateThreadFrequency, METH_VARARGS, "SetAggregateThreadFrequency(ms)\n\nSets the maximum time all blue-aggregate transports must be visited" },
	{ "SetMinScheduledIOInterval", PySetMinScheduledIOInterval, METH_VARARGS, "SetMinScheduledIOInterval(ms)\n\nMinimum time the communications engine will wake up Blue with a packet event <default 15ms>" },
	{ "SetWatchdogInterval", PySetWatchdogInterval, METH_VARARGS, "SetWatchdogInterval(ms)\n\nInterval the watchdog wakes up to check for unserviced packets <default 23ms>" },
	{ "SetWakeupMethod", PySetWakeupMethod, METH_VARARGS, "SetWakeupMethod(['static'|'dynamic'|'pending'|'hangpending'])\nSet the underlying CarbonIO tasklet wakeup methodology" },
	{ "GetLatencyToFirstHop", PyGetLatencyToFirstHop, METH_NOARGS, "GetLatencyToFirstHop()\n\nValid for client only, reprots latency to the proxy" },
	{ "GetWakeupMethod", PyGetWakeupMethod, METH_VARARGS, "GetWakeupMethod()\nread what underlying taklet wakeup methodology CarbonIO is using" },
	{ "GetRoutingMode", PyGetRoutingMode, METH_NOARGS, "GetRoutingMode()\n\nreturns 'proxy', 'server', 'client' or 'none'" },

	{ "EnumerateTransport", PyEnumerateTransport, METH_VARARGS, "EnumerateTransports(descriptor, id, name, userID, nodeID)\n\nExpose to blue"
																"the transport 'routing table' which will be used for auto-routing"
																"if this is a proxy node (see setType())" },
	{ "PurgeTransport", PyPurgeTransport, METH_VARARGS, "PurgeTransport(transportID)\n\nremove from the BlueNet tables" },
	{ "SendBlueNetPacket", PySendBlueNetPacket, METH_VARARGS, "SendBlueNetPacket(...)\n\ncreate a ready-for-wire header given a machonet message" },
	{ "AddClient", PyAddClient, METH_VARARGS, "AddClient(clientID, transportID)\n\nadd a client to a transport" },
	{ "MapCharToClient", PyMapCharToClient, METH_VARARGS, "MapCharToClient(charID, clientID)\n\nassociate this client ID with this char ID" },
	{ "PurgeClient", PyPurgeClient, METH_VARARGS, "PurgeClient(clientID)\n\nremove client from transport" },
	{ "AddProxyNode", PyAddProxyNode, METH_VARARGS, "AddProxyNodet(transportId, nodeId)\nIdentify this [existing!] transport Id as a proxy" },
	{ "DelProxyNode", PyDelProxyNode, METH_VARARGS, "DelProxyNode(nodeId)\nRemove this nodeId from the list of recognized proxies" },
	{ "ClearProxyNodes", PyClearProxyNodes, METH_VARARGS, "ClearProxyNodes()\nclear all proxy nodes" },

	{ "AddSendEvent", PyAddSendEvent, METH_VARARGS, "AddSendEvent(<client list>, <message type>, <payload>)\nBlueNet version of forward notify down" },
	{ "TransmitBatch", PyTransmitBatch, METH_VARARGS, "TransmitBatch()\nTransmit all currently batched BlueNet messages" },
	{ "IsBatchingEnabled", PyIsBatchingEnabled, METH_NOARGS,  "IsBatchingEnabled()\nreturns True/False" },
	{ "SetBatchingEnabled", PySetBatchingEnabled, METH_VARARGS, "SetBatchingEnabled(<True/False>\nEnable/Disable BlueNet batching" },
	{ "TestSerializer", PyTestSerializer, METH_VARARGS, "TestSerializer(<object>)\nInternally check the serializer with the given payload" },
	{ "GetStats", PyGetStats, METH_VARARGS, "GetStats()\nReturn a dict of internal stats" },
	{ "SetNewStringMaxLen", PySetNewStringMaxLen, METH_VARARGS, "SetNewStringMaxLen(#)\nSet longest string BlueNet will catalog for future add, set to 0 to disable (default 0)" },
	{ "GetNewStringMaxLen", PyGetNewStringMaxLen, METH_NOARGS, "GetNewStringMaxLen()\nGet longest string BlueNet will catalog for future add (0 means disabled)" },
	{ "DumpStringTable", PyDumpStringTable, METH_NOARGS, "DumpStringTable()\nReturn list of cacheable strings and their stats" },
	{ "DumpNewStringTable", PyDumpNewStringTable, METH_NOARGS, "DumpNewStringTable()\nReturn list of new strings stats" },
	{ "DumpTransportTable", PyDumpTransportTable, METH_NOARGS, "DumpTransportTable()\nEmit the transport routing table to the WARN log" },


	{ 0, 0, 0, 0 }
};

//------------------------------------------------------------------------------
BlueNet::BlueNet() :
	m_callbackJobLock( "BlueNet", "m_callbackJobLock" ), m_callbackPoolLock( "BlueNet", "m_callbackPoolLock" ),
	m_transportLock( "BlueNet", "m_transportLock" ),
	m_callbackListLock( "BlueNet", "m_callbackListLock" ), m_clientSessionCallbackListLock( "BlueNet", "m_clientSessionCallbackListLock" )
{
	BeNet = this;

	m_routingMode = MODE_NONE;
	m_myNodeID = 0;
	m_myClientID = 0;

	m_callbackHead = 0;
	m_callbackTail = 0;
	m_callbackHead_C = 0;
	m_callbackTail_C = 0;

	m_callbackPool = 0;

	m_constPongKey = BlueNetKeyFromName( "BlueNet::pong" );
	m_fastTime = time(0);

	m_minScheduledIOInterval = MIN_SCHEDULED_IO_INTERVAL;
	m_watchdogInterval = MIN_SCHEDULED_IO_INTERVAL / 2;

	m_NewStringMaxLen = 0;

	m_marshal = 0;

	m_pendingCallPending = false;

	m_blueNetHandlerPacketKey = BlueNetKeyFromName( "BlueNet::BatchPacket" );
}

//------------------------------------------------------------------------------
bool BlueNet::Init( PyObject* blueModule, PySocketModule_APIObject* socketAPI )
{
#ifdef BN_DEBUG_LOG
	char buf[256];
	sprintf( buf, "\\dev\\bn%d_log.txt", _getpid() );
	s_log.init( buf );
	printf("BlueNet logging as [%s]\n", buf );
#endif
	static struct PyModuleDef moduleDef = {
		PyModuleDef_HEAD_INIT,
		"blue.net",
		"",
		-1,
		m_methods,
	};
	auto module = PyModule_Create(&moduleDef);
	if ( ! module ) {
		CCP_LOGERR("Failed to create blue.net module");
		return false;
	}

	srand( (unsigned int)time(0) );

	// index the string table
	for( int i=0; i<MARSHAL_STRING_ARRAY_LEN; i++ )
	{
		unsigned long long key = (unsigned long long)Ccp::HashString(MARSHAL_STRINGS[i]);
		StringEntry *entry = m_singleton.m_stringTable.add( key );
		entry->index = i;
		entry->frequency = 0;
		entry->newString = 0;
	}

	memset( &m_singleton.m_stats, 0, sizeof(BlueNet::Stats) );

	m_singleton.m_marshal = Marshal::New();

	m_singleton.RegisterCallbackFromPython( BlueNetCallback, m_singleton.m_blueNetHandlerPacketKey );

	if ( PyModule_AddObject(blueModule, "net", module) ) {
		CCP_LOGERR("Failed adding net submodule to blue");
		return false;
	}

	m_singleton.m_socketAPI = socketAPI;

	return true;
}

//------------------------------------------------------------------------------
void BlueNet::RegisterCallbackAsync( DataCallback callback, const int blueNetKey )
{
	RegisterCallback( callback, blueNetKey, CALLBACK_ASYNC );
}

//------------------------------------------------------------------------------
void BlueNet::RegisterCallbackSync( DataCallback callback, const int blueNetKey )
{
	RegisterCallback( callback, blueNetKey, CALLBACK_SYNC );
}

//------------------------------------------------------------------------------
void BlueNet::RegisterCallbackFromPython( DataCallback callback, const int blueNetKey )
{
	RegisterCallback( callback, blueNetKey, CALLBACK_PYTHON );
}

//------------------------------------------------------------------------------
void BlueNet::RegisterCallbackAsync( ExtendedDataCallback callback, const int blueNetKey )
{
	RegisterCallback( callback, blueNetKey, CALLBACK_ASYNC );
}

//------------------------------------------------------------------------------
void BlueNet::RegisterCallbackSync( ExtendedDataCallback callback, const int blueNetKey )
{
	RegisterCallback( callback, blueNetKey, CALLBACK_SYNC );
}

//------------------------------------------------------------------------------
void BlueNet::RegisterCallbackFromPython( ExtendedDataCallback callback, const int blueNetKey )
{
	RegisterCallback( callback, blueNetKey, CALLBACK_PYTHON );
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PySetMyNodeID( PyObject *self, PyObject *args )
{
	if ( !PyArg_ParseTuple(args, "L", &m_singleton.m_myNodeID) )
	{
		m_singleton.m_myNodeID = 0;
		return NULL;
	}

	BN_NODEID(bnlog("My NodeID set to [%lld]", m_singleton.m_myNodeID));
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PySetTransportNodeID( PyObject *self, PyObject *args )
{
	long long transportID;
	long long nodeID;
	if ( !PyArg_ParseTuple(args, "LL", &transportID, &nodeID) )
	{
		return NULL;
	}

	Ccp::RWSpinlockWriteScoped wlock( m_singleton.m_transportLock );

	TransportRepr *transport = m_singleton.m_transportsByTransportID.getItem( transportID );
	if ( !transport )
	{
		BN_XNODEID(bnlog("could not locate transport[%lld] to set nodeID[%lld]", transportID, nodeID));
		Py_RETURN_NONE;
	}

	BN_XNODEID(bnlog("set transport[%lld] to nodeID[%lld]", transportID, nodeID));

	m_singleton.m_transportsByNodeID.remove( transport->nodeID ); // delete old ref if it existed
	m_singleton.m_transportsByNodeID.addItem( transport, nodeID ); // and re-index

	transport->nodeID = nodeID;

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PySetMinPingFrequency( PyObject *self, PyObject *args )
{
	if ( !PyArg_ParseTuple(args, "i", &m_singleton.m_constMinPingFrequency) )
	{
		return NULL;
	}

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PySetAggregateThreadFrequency( PyObject *self, PyObject *args )
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "SetAggregateThreadFrequency no longer serves any purpose" , 1 );
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PySetMinScheduledIOInterval( PyObject *self, PyObject *args )
{
	if ( !PyArg_ParseTuple(args, "i", &m_singleton.m_minScheduledIOInterval) )
	{
		return NULL;
	}

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyGetWakeupMethod( PyObject *self, PyObject *args )
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "GetWakeupMethod no longer serves any purpose" , 1 );
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PySetWatchdogInterval( PyObject *self, PyObject *args )
{
	if ( !PyArg_ParseTuple(args, "i", &m_singleton.m_watchdogInterval) )
	{
		return NULL;
	}

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PySetWakeupMethod( PyObject *self, PyObject *args )
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "SetWakeupMethod serves no purpose any more", 1 );
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
void BlueNet::RegisterCallback( DataCallback callback, const int blueNetKey, int style /*CALLBACK_SYNC*/ )
{
	Ccp::RWSpinlockWriteScoped wlock( m_callbackListLock );

	m_callbackList.remove( blueNetKey );

	DataCallbackRepr *repr = m_callbackList.add( blueNetKey );

	memset( repr, 0, sizeof(DataCallbackRepr) );
	repr->callback = callback;
	repr->style = style;
}

//------------------------------------------------------------------------------
void BlueNet::RegisterCallback( ExtendedDataCallback callback, const int blueNetKey, int style /*CALLBACK_SYNC*/ )
{
	Ccp::RWSpinlockWriteScoped wlock( m_callbackListLock );

	m_callbackList.remove( blueNetKey );

	DataCallbackRepr *repr = m_callbackList.add( blueNetKey );

	memset( repr, 0, sizeof(DataCallbackRepr) );
	repr->exCallback = callback;
	repr->style = style;
}

//------------------------------------------------------------------------------
void BlueNet::UnregisterCallback( const int blueNetKey )
{
	Ccp::RWSpinlockWriteScoped wlock( m_callbackListLock );
	m_callbackList.remove( blueNetKey );
}

//------------------------------------------------------------------------------
void BlueNet::DeliverCPackets()
{
	// check without grabbing the lock to trivialize idle usage
	if ( !m_callbackHead_C )
	{
		return;
	}

	// get in get the list and get out, so none of the async activity is
	// blocked by long callbacks
	m_callbackJobLock.Acquire();
	DataCallbackJob *jobs = m_callbackHead_C;
	m_callbackHead_C = 0;
	m_callbackTail_C = 0;
	m_callbackJobLock.Release();

	DeliverJobs( jobs );
}

//------------------------------------------------------------------------------
bool BlueNet::SendPacketToClient( const unsigned long long clientID,
								  const int blueNetKey,
								  const char *data,
								  const int len,
								  const int priority )
{
	if ( m_routingMode == MODE_CLIENT )
	{
		CCP_LOGERR_CH( s_blueNetChannel, "BlueNet mode [%d] trying to sendPacketToClient", m_routingMode );
		return false;
	}

	Ccp::RWSpinlockReadScoped rlock( m_transportLock );

	TransportRepr *target = m_singleton.m_transportsByClientID.getItem( clientID );
	if ( !target )
	{
		BN_TOCLIENT(bnlog("could not locate clientID[%lld]", clientID));
		return false;
	}

	BN_TOCLIENT(bnlog("packet type[0x%08X] size[%d] send to client[%lld]", blueNetKey, len, clientID));
	return SendClientPacket( blueNetKey, data, len, target, clientID, priority );
}

//------------------------------------------------------------------------------
bool BlueNet::SendPacketToClientList( const unsigned long long* clientList,
									  const int listLen,
									  const int blueNetKey,
									  const char *data,
									  const int len,
									  const int priority )
{
	if ( m_routingMode == MODE_CLIENT )
	{
		CCP_LOGERR_CH( s_blueNetChannel, "char list send attempt in mode[%d]", m_routingMode );
		return false;
	}

	if ( !clientList || listLen <= 0 )
	{
		CCP_LOGERR_CH( s_blueNetChannel, "bad input len SendPacketToClientList [%p][%d]", clientList, listLen );
		return false;
	}

	if ( listLen == 1 ) // trivial "fork"
	{
		return SendPacketToClient( clientList[0], blueNetKey, data, len );
	}

	const int c_maxListLen = 1024; // max that can be sent to a single proxy
	struct TempListNode
	{
		TransportRepr *transport;
		int num;
		unsigned long long recipients[c_maxListLen];
	};

	char headerData[sizeof(BlueNetHeader) + (sizeof(TempListNode) * 5) / 4];
	BitPacker packer( headerData, sizeof(headerData) );

	BlueNetHeader header;
	memset( &header, 0, sizeof(BlueNetHeader) );
	header.blueNetKey = blueNetKey;
	header.destinationType = ADDRESS_TYPE_CLIENT;

	Ccp::RWSpinlockReadScoped rlock( m_transportLock );

	TempListNode* list = CCP_NEW("BlueNet/TempListNode") TempListNode[ m_transportsByTransportID.count() ];

	// single look-up pass to compute everything necessary
	bool ret = true;

	int listIndex = 0;
	while( listIndex < listLen )
	{
		// prepare list
		TransportRepr **ttarget;
		for( ttarget = m_transportsByTransportID.getFirst(); ttarget; ttarget = m_transportsByTransportID.getNext() )
		{
			list[(*ttarget)->sequence].num = 0;
			list[(*ttarget)->sequence].transport = *ttarget;

			BN_SEND_LIST(bnlog("sequence[%d] setup target[%lld] ", (*ttarget)->sequence, (*ttarget)->ID));
		}

		// keep going until we are done, or a single node hits max size
		for( ;listIndex < listLen ;listIndex++ )
		{
			TransportRepr *transport = m_transportsByClientID.getItem( clientList[listIndex] );
			if ( transport )
			{
				BN_SEND_LIST(bnlog("client[%d:%lld] to transport seq[%d] entry[%d]", listIndex, clientList[listIndex], transport->sequence, list[transport->sequence].num));
				list[transport->sequence].recipients[list[transport->sequence].num++] = clientList[listIndex];

				if ( list[transport->sequence].num >= c_maxListLen )
				{
					CCP_LOGWARN_CH( s_blueNetChannel, "large multicast list actually reached [%d] for a single proxy", listLen );
					break;
				}
			}
			else
			{
				BN_SEND_LIST(bnlog("client[%d:%lld] not found", listIndex, clientList[listIndex]));
			}
		}

		for( unsigned int i=0; i<m_transportsByTransportID.count(); i++ )
		{
			// for each proxy, append the recipient list computed above and send it out
			if ( list[i].num )
			{
				packer.Reset();
				header.timestamp = CheckForPing( list[i].transport );
                header.latencyAccumulator = list[i].transport->latency;
				header.forkedAddresses = list[i].num;
				header.priority = priority == PRIORITY_HIGH ? true : false;
				header.Pack( packer );

				BN_SEND_LIST(bnlog("%d: packing[%d] addresses to transport[%lld]", i, list[i].num, list[i].transport->ID));

				for( int j=0; j<list[i].num; j++ ) // append in the targets
				{
					packer.Pack( list[i].recipients[j] );
				}

				ret = SendPacketEx( list[i].transport, data, len, headerData, packer.Finalize(), PRIORITY_HIGH ) ? ret : false;
			}
		}
	}

	CCP_DELETE[] list;

	return ret;
}

//------------------------------------------------------------------------------
bool BlueNet::SendPacketToAllClients( const int blueNetKey,
									  const char *data,
									  const int len,
									  const int priority )
{
	if ( m_routingMode == MODE_CLIENT )
	{
		CCP_LOGERR_CH( s_blueNetChannel, "send to all char list send attempt in mode[%d]", m_routingMode );
		return false;
	}

	if ( !data || len <= 0 )
	{
		CCP_LOGERR_CH( s_blueNetChannel, "bad input len SendPacketToAllClients [%p][%d]", data, len );
		return false;
	}

	Ccp::RWSpinlockReadScoped rlock( m_transportLock );

	Ccp::LinkHash<TransportRepr*>::CIterator iter( m_transportsByClientID );

	unsigned long long* list = (unsigned long long *)(CCP_MALLOC("BlueNet/AllCliList", m_transportsByClientID.count() * sizeof(unsigned long long)));
	int onEntry = 0;
	for( TransportRepr** TT = iter.getFirst(); TT; TT = iter.getNext() )
	{
		list[onEntry++] = (*TT)->clientID;
	}

	rlock.release();

	bool retval = SendPacketToClientList( list, onEntry, blueNetKey, data, len, priority );

	CCP_FREE(list);

	return retval;
}

//------------------------------------------------------------------------------
bool BlueNet::SendPacketToChar( const unsigned long long charID,
								const int blueNetKey,
								const char *data,
								const int len,
								const int priority )
{
	if ( m_routingMode != MODE_SERVER )
	{
		CCP_LOGERR_CH( s_blueNetChannel, "BlueNet mode [%d] trying to sendPacketToClient", m_routingMode );
		return false;
	}

	Ccp::RWSpinlockReadScoped rlock( m_transportLock );

	ClientMapping *mapping = m_singleton.m_clientMapByCharID.get( charID );
	if ( !mapping )
	{
		BN_TOCLIENT(bnlog("could not locate mapping for charID[%lld] to send [0x%08X]", charID, blueNetKey));
		return false;
	}

	BN_TOCLIENT(bnlog("sending packet type[0x%08X] size[%d] to char[%lld]", blueNetKey, len, charID));
	return SendClientPacket( blueNetKey, data, len, mapping->transport, mapping->clientID, priority );
}

//------------------------------------------------------------------------------
bool BlueNet::SendPacketToCharList( const unsigned long long* charList,
									const int listLen,
									const int blueNetKey,
									const char *data,
									const int len,
									const int priority )
{
	// map to client IDs then invoke SendPacketToClientList()

	if ( !charList || !listLen )
	{
		return false;
	}

	unsigned long long* clientList = (unsigned long long *)(CCP_MALLOC("BlueNet/CharListList", listLen * sizeof(unsigned long long)));
	int j = 0;
	for( int i=0; i<listLen; i++ )
	{
		Ccp::RWSpinlockReadScoped rlock( m_transportLock );
		ClientMapping *mapping = m_clientMapByCharID.get( charList[i] );
		if ( mapping )
		{
			clientList[j++] = mapping->clientID;
		}
	}

	bool retval = SendPacketToClientList( clientList, j, blueNetKey, data, len, priority );

	CCP_FREE(clientList);

	return retval;
}

//------------------------------------------------------------------------------
unsigned long long BlueNet::ClientIDFromCharID( unsigned const long long charID )
{
	Ccp::RWSpinlockReadScoped rlock( m_transportLock );

	ClientMapping *map = m_clientMapByCharID.get( charID );
	return map ? map->clientID : 0;
}

//------------------------------------------------------------------------------
unsigned long long BlueNet::CharIDFromClientID( const unsigned long long clientID )
{
	Ccp::RWSpinlockReadScoped rlock( m_transportLock );

	Ccp::LinkHash<ClientMapping>::CIterator iter(m_clientMapByCharID);
	for( ClientMapping *map = iter.getFirst() ; map ; map = iter.getNext() )
	{
		if ( map->clientID == clientID )
		{
			return iter.getCurrentKey();
		}
	}

	return 0;
}

//------------------------------------------------------------------------------
bool BlueNet::SendPacketToNode( const unsigned long long nodeID,
								const int blueNetKey,
								const char *data,
								const int len,
								const int priority )
{
	BlueNetHeader header;
	memset( &header, 0, sizeof(BlueNetHeader) );

	TransportRepr *target;

	Ccp::RWSpinlockReadScoped rlock( m_transportLock );

	if ( m_routingMode == MODE_CLIENT )
	{
		int count = m_singleton.m_transportsByProxyID.count();
		if ( count == 0 )
		{
			CCP_LOGERR_CH( s_blueNetChannel, "client has no valid proxies on Node send" );
			return false;
		}
		else if ( count > 1 )
		{
			CCP_LOGERR_CH( s_blueNetChannel, "client too many[%d] proxies on Node send", count );
			return false;
		}

		TransportRepr **T = m_transportsByProxyID.getFirst();
		if ( !T )
		{
			CCP_LOGERR_CH( s_blueNetChannel, "proxy list claims to have 1 entry but did not fetch it" );
			return false;
		}

		target = *T;
		header.destinationID = nodeID;
	}
	else
	{
		// I am a proxy or sol node and mean to be sending to a
		// specific node that is NOT a client

		if ( nodeID == m_myNodeID )
		{
			CCP_LOGERR_CH( s_blueNetChannel, "cannot send packet to myself[%lld]", nodeID );
			return false;
		}

		target = m_transportsByNodeID.getItem( nodeID );
		if ( !target )
		{
			CCP_LOGERR_CH( s_blueNetChannel, "NodeID[%lld] NOT found in node list", nodeID );
			return false;
		}

		header.destinationID = nodeID;
	}

	if ( !target )
	{
		return false;
	}

	header.timestamp = CheckForPing( target );
    header.latencyAccumulator = target->latency;
	header.destinationType = ADDRESS_TYPE_NODE;
	header.blueNetKey = blueNetKey;

	char headerData[ BLUE_HEADER_SCRATCH_SIZE ];
	BitPacker packer( headerData, BLUE_HEADER_SCRATCH_SIZE );
	header.Pack( packer );

	BN_TONODE(bnlog("packet type[0x%08X] size[%d] send to node[%lld]", blueNetKey, len, nodeID));

	return SendPacketEx( target, data, len, headerData, packer.Finalize(), header.timestamp ? PRIORITY_HIGH : priority );
}

//------------------------------------------------------------------------------
unsigned long long BlueNet::GetProxyNodeIDByClientID( const unsigned long long clientID )
{
	Ccp::RWSpinlockReadScoped rlock( m_transportLock );

	TransportRepr *T = 0;

	if ( m_routingMode == MODE_CLIENT )
	{
		int count = m_transportsByProxyID.count();
		if ( count == 0 )
		{
			CCP_LOGERR_CH( s_blueNetChannel, "client has no valid proxies on GetProxyNodeIDByClientID" );
			return 0;
		}
		else if ( count > 1 )
		{
			CCP_LOGERR_CH( s_blueNetChannel, "client too many[%d] proxies on GetProxyNodeIDByClientID", count );
			return 0;
		}

		TransportRepr **transport = m_transportsByProxyID.getFirst();
		if ( !transport )
		{
			CCP_LOGERR_CH( s_blueNetChannel, "proxy list claims client has 1 entry but did not fetch it for GetProxyNodeIDByClientID" );
			return 0;
		}

		T = *transport;
	}
	else
	{
		T = m_transportsByClientID.getItem( clientID );
	}

	return T ? T->nodeID : 0;
}

//------------------------------------------------------------------------------
long long BlueNet::GetClientIDFromTransportID( long long transportID )
{
	if ( m_routingMode != MODE_PROXY )
	{
		CCP_LOGERR_CH( s_blueNetChannel, "non-proxy [%d] asked for a clientID by descriptor", m_routingMode );
		return 0;
	}

	Ccp::RWSpinlockReadScoped rlock( m_transportLock );
	TransportRepr *repr = m_transportsByTransportID.getItem( transportID );
	if ( !repr )
	{
		CCP_LOGWARN_CH( s_blueNetChannel, "transportID[%lld] not found in GetClientIDFromDescriptor", transportID );
		return 0;
	}

	if ( repr->isClientTransport )
	{
		return repr->clientID;
	}

	CCP_LOGERR_CH( s_blueNetChannel, "transportID[%lld] not a client", transportID );

	return 0;
}

//------------------------------------------------------------------------------
bool BlueNet::IsClient( long long ID )
{
	if ( (m_routingMode == MODE_SERVER) || (m_routingMode == MODE_CLIENT) )
	{
		// by definition
		return false;
	}

	Ccp::RWSpinlockReadScoped rlock( m_transportLock );
	TransportRepr *repr = m_transportsByTransportID.getItem( ID );
	if ( !repr )
	{
		return false;
	}

	return repr->isClientTransport;
}

//------------------------------------------------------------------------------
int BlueNet::GetProxyNodeList( long long **returnList )
{
	if ( !returnList || !*returnList )
	{
		return 0;
	}

	if ( !m_transportsByProxyID.count() ) // fast fail
	{
		return 0;
	}

	Ccp::RWSpinlockReadScoped rlock( m_singleton.m_transportLock );

	if ( !m_transportsByProxyID.count() ) // guard against race
	{
		return 0;
	}

	*returnList = CCP_NEW("BlueNet/returnList") long long[ m_transportsByProxyID.count() ];

	int index = 0;
	for( TransportRepr** T = m_transportsByProxyID.getFirst(); T; T = m_transportsByProxyID.getNext() )
	{
		(*returnList)[index++] = (*T)->nodeID;
	}

	return index;
}

//------------------------------------------------------------------------------
long long BlueNet::GetTransportIDFromClientID( long long clientId )
{
	Ccp::RWSpinlockReadScoped rlock( m_transportLock );
	TransportRepr* T = m_transportsByClientID.getItem( clientId );
	return T ? T->ID : 0;
}

//------------------------------------------------------------------------------
long long BlueNet::GetTransportIDFromNodeID( long long nodeID )
{
	Ccp::RWSpinlockReadScoped rlock( m_transportLock );
	TransportRepr* T = m_transportsByNodeID.getItem( nodeID );
	return T ? T->ID : 0;
}

//------------------------------------------------------------------------------
long long BlueNet::GetNodeIDFromTransportID( long long transportID )
{
	Ccp::RWSpinlockReadScoped rlock( m_transportLock );
	TransportRepr* T = m_transportsByTransportID.getItem( transportID );
	return T ? T->nodeID : 0;
}

//------------------------------------------------------------------------------
long long BlueNet::PickRandomProxyNodeId( bool readLockHeld )
{
	// clients connect to only proxies, and machonet does not
	// specifically enumerate them, so pick from all available
	// transports, otherwise look only in the proxy list
	if ( !m_transportsByProxyID.count() ) // fast fail
	{
		return 0;
	}

	if ( !readLockHeld )
	{
		m_singleton.m_transportLock.readLock();
	}

	// optimize for client case of 1
	int count = m_transportsByProxyID.count();
	if ( count == 1 )
	{
		if ( !readLockHeld )
		{
			m_singleton.m_transportLock.unlock();
		}

		return (*m_transportsByProxyID.getFirst())->nodeID;
	}

	int proxy = rand() % count;

	int index = 0;
	Ccp::LinkHash<TransportRepr*>::CIterator iter( m_transportsByProxyID );
	for( TransportRepr** T = iter.getFirst(); T; T = iter.getNext() )
	{
		if ( proxy == index++ )
		{
			if ( !readLockHeld )
			{
				m_singleton.m_transportLock.unlock();
			}

			return (*T)->nodeID;
		}
	}

	if ( !readLockHeld )
	{
		m_singleton.m_transportLock.unlock();
	}

	return 0;
}

//------------------------------------------------------------------------------
unsigned int BlueNet::GetLatencyToFirstHop()
{
	if ( m_singleton.m_routingMode != MODE_CLIENT )
	{
		return 0;
	}

	Ccp::RWSpinlockReadScoped rlock( m_singleton.m_transportLock );

	// clients should only have one transport, always pick it
	TransportRepr **ttarget = m_singleton.m_transportsByTransportID.getFirst();

	return ttarget ? (*ttarget)->latency : 0;
}

typedef void(*ClientSessionCallback)( const long long charId, const long long clientId, int status );

//------------------------------------------------------------------------------
void BlueNet::RegisterClientSessionCallback( ClientSessionCallback callback )
{
	Ccp::RWSpinlockWriteScoped wlock( m_clientSessionCallbackListLock );
	m_clientSessionCallbackList.remove( (long long)callback );
	ClientSessionCallback *repr = m_clientSessionCallbackList.add( (long long)callback );
	*repr = callback;
}

//------------------------------------------------------------------------------
void BlueNet::UnRegisterClientSessionCallback( ClientSessionCallback callback )
{
	Ccp::RWSpinlockWriteScoped wlock( m_clientSessionCallbackListLock );
	m_clientSessionCallbackList.remove( (long long)callback );
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyAddProxyNode( PyObject *self, PyObject *args )
{
	long long transportID;
	long long nodeID;
	if ( !PyArg_ParseTuple(args, "LL", &transportID, &nodeID) )
	{
		return NULL;
	}

	Ccp::RWSpinlockWriteScoped wlock( m_singleton.m_transportLock );

	TransportRepr *transport = m_singleton.m_transportsByTransportID.getItem( transportID );
	if ( !transport )
	{
		Py_RETURN_NONE;
	}

	m_singleton.m_transportsByProxyID.remove( nodeID ); // delete old ref if it existed
	m_singleton.m_transportsByProxyID.addItem( transport, nodeID ); // and re-index

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyDelProxyNode( PyObject *self, PyObject *args )
{
	long long nodeID;
	if ( !PyArg_ParseTuple(args, "L", &nodeID) )
	{
		return NULL;
	}

	Ccp::RWSpinlockWriteScoped wlock( m_singleton.m_transportLock );
	m_singleton.m_transportsByProxyID.remove( nodeID );

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyClearProxyNodes( PyObject *self, PyObject *unused )
{
	Ccp::RWSpinlockWriteScoped wlock( m_singleton.m_transportLock );
	m_singleton.m_transportsByProxyID.clear();
	Py_RETURN_NONE;
}

//----------------------------------------------------------------------------------------------------------
PyObject* BlueNet::PyAddSendEvent( PyObject* module, PyObject* args )
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "AddSendEvent no longer serves any purpose" , 1 );
	Py_RETURN_NONE;
}

//----------------------------------------------------------------------------------------------------------
PyObject* BlueNet::PyTransmitBatch( PyObject* module, PyObject* args )
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "TransmitBatch no longer serves any purpose" , 1 );
	Py_RETURN_NONE;
}

//----------------------------------------------------------------------------------------------------------
PyObject* BlueNet::PyIsBatchingEnabled( PyObject* module, PyObject* args )
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "IsBatchingEnabled no longer serves any purpose" , 1 );
	Py_RETURN_FALSE;
}

//----------------------------------------------------------------------------------------------------------
PyObject* BlueNet::PySetBatchingEnabled( PyObject* module, PyObject* args )
{
	PyErr_WarnEx( PyExc_DeprecationWarning, "SetBatchingEnabled no longer serves any purpose" , 1 );
	Py_RETURN_NONE;
}

//----------------------------------------------------------------------------------------------------------
PyObject* BlueNet::PyTestSerializer( PyObject* module, PyObject* args )
{
	PyObject* obj;
	if ( !PyArg_ParseTuple(args, "O", &obj) )
	{
		BN_SERIALIZE(bnlog("serialize tester FAIL<1>"));
		return NULL;
	}

	BitPackerManaged packer;
	if ( !SerializeObject(obj, packer) )
	{
		BN_SERIALIZE(bnlog("serialize tester FAIL<2>"));
		Py_RETURN_FALSE;
	}

	char* data;
	unsigned int len = packer.Finalize( &data );

	BitPacker unpacker( data, len );

	obj = UnserializeObject( unpacker );
	if ( !obj )
	{
		BN_SERIALIZE(bnlog("serialize tester FAIL<3>"));
		Py_RETURN_FALSE;
	}

	BN_SERIALIZE(bnlog("serialize tester PASS"));

	Py_DECREF( obj );
	Py_RETURN_TRUE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyGetStats( PyObject* module, PyObject* args )
{
	BlueNet::Stats *stats = &m_singleton.m_stats;
	return Py_BuildValue("{sL "
						 "sL "
						 "sL "
						 "sL "
						 "sL " // 5
						 "sL}",
						 "numberFellback", stats->numberFellback,
						 "numberTotal", stats->numberTotal,
						 "bytes", stats->bytes,
						 "bytesCompare", stats->bytesCompare,
						 "stringHits", stats->stringHits, // 5
						 "stringMisses", stats->stringMisses
						);
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PySetNewStringMaxLen( PyObject* module, PyObject* args )
{
	if ( !PyArg_ParseTuple(args, "|I", &m_singleton.m_NewStringMaxLen) )
	{
		return 0;
	}

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyGetNewStringMaxLen( PyObject* module, PyObject* args )
{
	return PyLong_FromLong( m_singleton.m_NewStringMaxLen );
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyDumpStringTable( PyObject* module, PyObject* args )
{
	PyObject* stats = PyDict_New();

	for( StringEntry* entry = m_singleton.m_stringTable.getFirst(); entry; entry = m_singleton.m_stringTable.getNext() )
	{
		PyObject* val = PyLong_FromLongLong( entry->frequency );
		PyDict_SetItemString( stats, MARSHAL_STRINGS[entry->index], val );
		Py_DECREF( val );
	}

	return stats;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyDumpNewStringTable( PyObject* module, PyObject* args )
{
	PyObject* stats = PyDict_New();

	for( StringEntry* entry = m_singleton.m_newStrings.getFirst(); entry; entry = m_singleton.m_newStrings.getNext() )
	{
		PyObject* val = PyLong_FromLongLong( entry->frequency );
		PyDict_SetItemString( stats, entry->newString, val );
		Py_DECREF( val );
	}

	return stats;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyDumpTransportTable( PyObject* module, PyObject* args )
{
	for( TransportRepr** T = m_singleton.m_transportsByTransportID.getFirst(); T; T = m_singleton.m_transportsByTransportID.getNext() )
	{
		char buf[256];
		sprintf( buf, "%s:%lld - ID[%lld] : node[%lld] clientID[%lld]", (*T)->name, (*T)->descriptor, (*T)->ID, (*T)->nodeID, (*T)->clientID );
		CCP_LOGWARN_CH( s_blueNetChannel, "%s", buf );
		printf( "%s\n", buf );
	}

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
unsigned int BlueNet::Flush()
{
	unsigned int bytesFlushed = 0;
	Ccp::RWSpinlockReadScoped rlock( m_transportLock );

	Ccp::LinkHash<TransportRepr*>::CIterator iter( m_transportsBySocket );

	for( TransportRepr** TT = iter.getFirst(); TT; TT = iter.getNext() )
	{
		TransportRepr *transport = *TT;
		if ( transport->bytesAggregated )
		{
			BN_AGG(bnlog("transport[%lld] had[%d] bytes scheduled for transmit", transport->ID, transport->bytesAggregated));

			CcpAutoMutex tlock( transport->packetAggregateLock );
			m_socketAPI->send_formatted_packet( transport->descriptor, transport->packetAggregator, transport->bytesAggregated );
			bytesFlushed += transport->bytesAggregated;
			transport->bytesAggregated = 0;
		}
	}

	return bytesFlushed;
}

//------------------------------------------------------------------------------
unsigned int BlueNet::FlushToClientList( const unsigned long long* clientList, const int listLen )
{
	if ( !clientList || !listLen )
	{
		return 0;
	}

	unsigned int bytesSent = 0;

	Ccp::RWSpinlockReadScoped rlock( m_transportLock );
	for( int i=0; i<listLen; i++ )
	{
		TransportRepr* transport = m_transportsByClientID.getItem( clientList[i] );
		if ( transport )
		{
			bytesSent += FlushTransport( transport );
		}
	}

	return bytesSent;
}

//------------------------------------------------------------------------------
unsigned int BlueNet::FlushToCharList( const unsigned long long* charList, const int listLen )
{
	if ( !charList || !listLen )
	{
		return 0;
	}

	unsigned int bytesSent = 0;

	Ccp::RWSpinlockReadScoped rlock( m_transportLock );
	for( int i=0; i<listLen; i++ )
	{
		ClientMapping* mapping = m_clientMapByCharID.get( charList[i] );
		if ( mapping )
		{
			TransportRepr* transport = m_transportsByClientID.getItem( mapping->clientID );
			if ( transport )
			{
				bytesSent += FlushTransport( transport );
			}
		}
	}

	return bytesSent;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyEnumerateTransport( PyObject *self, PyObject *args )
{
	TransportRepr *transport = CCP_NEW("BllueNet/TransportRepr") TransportRepr;

	char *name;
	long long userID;
	if ( !PyArg_ParseTuple(args, "LLzLLL",
						   &transport->descriptor,
						   &transport->ID,
						   &name,
						   &userID,
						   &transport->clientID,
						   &transport->nodeID) )
	{
		CCP_DELETE transport;
		return NULL;
	}

	strncpy( transport->name, name, sizeof(transport->name) - 1 );
	if ( !strncmp(name, "tcp:packet:client", 17) && transport->clientID )
	{
		transport->isClientTransport = true;
	}

	m_singleton.m_transportLock.readLock();

	bool added = true;

	TransportRepr *T = m_singleton.m_transportsBySocket.getItem( transport->descriptor );
	if ( T )
	{
		added = false;
		m_singleton.m_transportLock.unlock();

		CCP_LOGERR_CH( s_blueNetChannel, "Attempting to enumerate a transport more than once [%d]/[%lld]", T->ID, transport->descriptor );

		// this should be extremely impossible, gracefully plod on..
		BN_ENUM(bnlog("key [%lld] already found, clobbering", transport->descriptor));
		m_singleton.PurgeTransport( T->ID );
	}
	else
	{
		m_singleton.m_transportLock.unlock();
	}

	m_singleton.m_transportLock.writeLock();

	m_singleton.m_transportsBySocket.addItem( transport, transport->descriptor );
	m_singleton.m_transportsByTransportID.addItem( transport, transport->ID );

	if ( transport->nodeID )
	{
		m_singleton.m_transportsByNodeID.addItem( transport, transport->nodeID );
	}

	m_singleton.SequenceTransports(); // make sure they are numbered

	m_singleton.m_transportLock.unlock();

	BN_ENUM(bnlog("added transport ID[%lld] name[%s] client[%lld] node[%lld]", transport->ID, transport->name, transport->clientID, transport->nodeID));

	if ( added )
	{
		Py_RETURN_TRUE;
	}
	else
	{
		Py_RETURN_FALSE;
	}
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyPurgeTransport( PyObject *self, PyObject *args )
{
	long long transportID;
	if ( !PyArg_ParseTuple(args, "L:transportID", &transportID) )
	{
		return NULL;
	}

	m_singleton.PurgeTransport( transportID );

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PySendBlueNetPacket( PyObject *self, PyObject *args )
{
	char *key;
	PyObject *forkTargets;
	PyObject *pickle;
	long long destinationType;
	long long destinationID;
	int priority;

	// pull args out into the header
	if ( !PyArg_ParseTuple(args, "SLLzi|O",
						   &pickle,
						   &destinationType,
						   &destinationID,
						   &key,
						   &priority,
						   &forkTargets) )
	{
		return NULL;
	}

	char *data = PyBytes_AsString( pickle );
	int size = (int)PyBytes_GET_SIZE( pickle );
	if ( !data || !size )
	{
		return NULL; // pyerror will already be set
	}

	if ( destinationType == ADDRESS_TYPE_NODE )
	{
		m_singleton.SendPacketToNode( destinationID, BlueNetKeyFromName(key), data, size );
	}
	else if ( destinationType == ADDRESS_TYPE_CLIENT )
	{
		if ( destinationID )
		{
			m_singleton.SendPacketToClient( destinationID, BlueNetKeyFromName(key), data, size, priority ? PRIORITY_HIGH : PRIORITY_NORMAL );
		}
		else if ( forkTargets && PyTuple_Check(forkTargets) )
		{
			int elements = (int)PyTuple_GET_SIZE( forkTargets );
			unsigned long long *list = (unsigned long long *)CCP_MALLOC("BlueNet/forkTargetsList", elements * sizeof(unsigned long long) );

			for( Py_ssize_t i=0; i<elements ; i++ )
			{
				list[i] = PyLong_AsLongLong( PyTuple_GET_ITEM(forkTargets, i) );
				if ( list[i] == -1 )
				{
					CCP_FREE(list);
					return NULL;
				}
			}

			CCP_FREE(list);
			m_singleton.SendPacketToClientList( list, elements, BlueNetKeyFromName(key), data, size, priority ? PRIORITY_HIGH : PRIORITY_NORMAL );
		}
	}

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PySetMode( PyObject *self, PyObject *args )
{
	if ( m_singleton.m_routingMode != MODE_NONE )
	{
		CCP_LOGERR_CH( s_blueNetChannel, "Tried to set the mode again, but its already [%d]", m_singleton.m_routingMode );
		Py_RETURN_NONE;
	}

	char *mode;
	if ( !PyArg_ParseTuple(args, "s", &mode) )
	{
		return NULL;
	}

	if ( !_strnicmp(mode, "proxy", 5) )
	{
		CCP_LOG_CH( s_blueNetChannel, "BlueNet personality set to PROXY");
		m_singleton.m_routingMode = MODE_PROXY;
	}
	else if ( !_strnicmp(mode, "server", 6) )
	{
		CCP_LOG_CH( s_blueNetChannel, "BlueNet personality set to SERVER");
		m_singleton.m_routingMode = MODE_SERVER;
	}
	else if ( !_strnicmp(mode, "client", 6) )
	{
		CCP_LOG_CH( s_blueNetChannel, "BlueNet personality set to CLIENT");
		m_singleton.m_routingMode = MODE_CLIENT;
	}
	else
	{
		CCP_LOGWARN_CH( s_blueNetChannel, "BlueNet personality set to 'other'");

		// let this be legal for orchestrator etc.. but don't install our hook
		Py_RETURN_NONE;
	}

	// install our routing
	m_singleton.m_socketAPI->add_oob_data_callback( OnPostDecompress );

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyAddClient( PyObject *self, PyObject *args )
{
	long long transportID;
	long long clientID;

	// pull args out into the header
	if ( !PyArg_ParseTuple(args, "LL", &clientID, &transportID) )
	{
		return NULL;
	}

	Ccp::RWSpinlockWriteScoped wlock( m_singleton.m_transportLock );

	TransportRepr* transport = m_singleton.m_transportsByTransportID.getItem( transportID );
	if ( !transport )
	{
		BN_CLIENTTRACKING(bnlog("transport[%lld] was not found to add client[%lld]", transportID, clientID));
		Py_RETURN_FALSE;
	}

	m_singleton.m_transportsByClientID.remove( clientID );
	m_singleton.m_transportsByClientID.addItem( transport, clientID );

	BN_CLIENTTRACKING(bnlog("transport[%lld] adding client[%lld]", transportID, clientID));

	Py_RETURN_TRUE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyMapCharToClient( PyObject *self, PyObject *args )
{
	long long charID = 0;
	long long clientID;

	// pull args out into the header
	if ( !PyArg_ParseTuple(args, "LL", &charID, &clientID) )
	{
		return NULL;
	}

	if ( !charID ) // fast-fail the trivial case
	{
		Py_RETURN_NONE;
	}

	Ccp::RWSpinlockWriteScoped wlock( m_singleton.m_transportLock );

	TransportRepr* transport = m_singleton.m_transportsByClientID.getItem( clientID );
	if ( !transport )
	{
		Py_RETURN_NONE;
	}

	m_singleton.m_clientMapByCharID.remove( charID );
	ClientMapping *map = m_singleton.m_clientMapByCharID.add( charID );
	map->transport = transport;
	map->clientID = clientID;
	wlock.release();

	if ( m_singleton.m_clientSessionCallbackList.count() )
	{
		Ccp::RWSpinlockReadScoped rlock( m_singleton.m_clientSessionCallbackListLock );
		Ccp::LinkHash<ClientSessionCallback>::CIterator iter(m_singleton.m_clientSessionCallbackList);
		for( ClientSessionCallback *C = iter.getFirst(); C; C = iter.getNext() )
		{
			(*C)( charID, clientID, STATUS_LOGIN );
		}
		rlock.release();
	}

	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyPurgeClient( PyObject *self, PyObject *args )
{
	long long clientID;

	// pull args out into the header
	if ( !PyArg_ParseTuple(args, "L", &clientID) )
	{
		return NULL;
	}

	Ccp::RWSpinlockWriteScoped wlock( m_singleton.m_transportLock );

	m_singleton.m_transportsByClientID.remove( clientID );

	// remove any charID association
	long long charID = 0;
	for( ClientMapping *map = m_singleton.m_clientMapByCharID.getFirst() ;
		 map ;
		 map = m_singleton.m_clientMapByCharID.getNext() )
	{
		if ( map->clientID == clientID )
		{
			charID = m_singleton.m_clientMapByCharID.getCurrentKey();
			BN_PURGETRANSPORT(bnlog( "charId[%lld] found for[%lld], removing mapping", charID, clientID ));
			m_singleton.m_clientMapByCharID.remove( charID );
			// let it continue in case something wierd happened, but in
			// practice there should only be one occurance
		}
	}

	wlock.release();

	if ( charID && m_singleton.m_clientSessionCallbackList.count() )
	{
		Ccp::RWSpinlockReadScoped rlock( m_singleton.m_clientSessionCallbackListLock );
		Ccp::LinkHash<ClientSessionCallback>::CIterator iter(m_singleton.m_clientSessionCallbackList);
		for( ClientSessionCallback *C = iter.getFirst(); C; C = iter.getNext() )
		{
			(*C)( charID, clientID, STATUS_LOGOUT );
		}
		rlock.release();
	}

	BN_CLIENTTRACKING(bnlog("purged client[%lld]", clientID));
	Py_RETURN_TRUE;
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyGetLatencyToFirstHop( PyObject *self, PyObject *unused )
{
	if ( m_singleton.m_routingMode != MODE_CLIENT )
	{
		Py_RETURN_NONE; // chew static
	}

	return PyLong_FromLong( m_singleton.GetLatencyToFirstHop() );
}

//------------------------------------------------------------------------------
PyObject* BlueNet::PyGetRoutingMode( PyObject *self, PyObject *unused )
{
	switch ( m_singleton.m_routingMode )
	{
		case MODE_CLIENT:
		{
			return PyUnicode_FromString( "client" );
		}

		case MODE_PROXY:
		{
			return PyUnicode_FromString( "proxy" );
		}

		case MODE_SERVER:
		{
			return PyUnicode_FromString( "server" );
		}

		default:
		{
			return PyUnicode_FromString( "none" );
		}
	}
}

//------------------------------------------------------------------------------
int BlueNet::OnPostDecompress( long long descriptor, const char* data, const int len, const char* OobData, const int OobLen )
{
	if ( !OobData || (OobLen <= sizeof(unsigned short)) )
	{
		return false;
	}

	BlueNetHeader header;
	BitPacker packer( OobData, OobLen );
	if ( !header.Unpack( packer ) )
	{
		BN_DELIVER(bnlog("failed to unpack"));
		return false;
	}

	if ( header.blueNetKey == m_singleton.m_constNoneKey )
	{
		// by definition we are not interested in this
		BN_DELIVER(bnlog("header had 'NONE' type"));
		return false;
	}

	BN_DELIVER(bnlog("header type[0x%08X] len[%d] OobLen[%d]", header.blueNetKey, len, OobLen));

	Ccp::RWSpinlockReadScoped rlock( m_singleton.m_transportLock );
	TransportRepr *source = m_singleton.m_transportsBySocket.getItem( descriptor );
	if ( !source )
	{
		BN_DELIVER(bnlog("OOB data but transport[%lld] not located", descriptor));
		return false;
	}

	if ( header.blueNetKey == m_constPongKey )
	{
		// dedicated ping answer

		unsigned long long timestamp = header.timestamp & ~TIMESTAMP_PINGBACK_BIT;
		int newLatency = (int)(CcpGetTickCount() - timestamp) / 2; // assume symetric ping times
		BN_PING(bnlog("computing newlatency[%d] = ([%d] - [0x%016llX]) / 2", newLatency, GetTickCount(), header.timestamp));

		if ( !source->latency )
		{
			source->latency = newLatency;
			BN_PING(bnlog("pong received from [%lld] (first) lag now[%d]", source->ID, source->latency));
		}
		else
		{
			int diff = newLatency - source->latency;

			BN_PING(bnlog("pong received from [%lld], old[%d] diff[%d] now[%d]", source->ID, source->latency, diff, source->latency + diff / 3));

			source->latency += diff / 3;

			// if the delta is a spike then schedule another ping immediately.
			if ( (diff > 75) || (diff < -75) )
			{
				CCP_LOG_CH( s_blueNetChannel, "Latency change treshold triggered. Old latency[%d] new[%d] diff[%d] re-scheduling a check", newLatency, source->latency - (diff / 3), diff );
				source->nextLatencyTimeCheck = 0;
			}
		}

		if ( header.latencyAccumulator ) // did the pong also request a ping?
		{
			BN_PING(bnlog("latency accumulator non-zero, answering[%lld] as ping", source->ID));
			m_singleton.AnswerPing( source, header.latencyAccumulator );
		}

		// by definition this packet exists atomically from any payload, consume it
		return true;
	}

	if ( header.timestamp ) // was there a piggybacked ping request?
	{
		BN_DELIVER(bnlog("timestamp piggybacked"));
		m_singleton.AnswerPing( source, header.timestamp );
        header.timestamp = 0; // do not propogate ping request!
	}

	if ( m_singleton.m_routingMode == MODE_PROXY )
	{
		BN_DELIVER(bnlog("IDS node[%lld] client[%lld]", source->nodeID, source->clientID));

		if ( source->isClientTransport )
		{
			BN_DELIVER(bnlog("<1> plugging in id[%lld] was[%lld]", source->clientID, header.sourceID));
			header.sourceID = source->clientID;
		}
		else
		{
			BN_DELIVER(bnlog("<2> plugging in id[%lld] was[%lld]", source->nodeID, header.sourceID));
			header.sourceID = source->nodeID;
		}

		if ( header.destinationID != m_singleton.m_myNodeID )
		{
			BN_DELIVER(bnlog("id[%lld] was NOT my id[%lld] so I'm routing it"));
			m_singleton.Route( &header, &packer, source, data, len );
			return true; // it was a bluenet packet for sure, whether we routed it or not, it was not meant for macho
		}

		BN_DELIVER(bnlog("id[%lld] was me:[%lld] delivering"));
	}

    // latencyAccumulator should have been properly accumulated when the packet reaches the end hop!!
	long long ID = source->ID;
	rlock.release(); // 'deliver' might invoke a callback, hold no locks!

	return m_singleton.Deliver( &header, data, len, ID ); // now THIS one might punt to Macho to alert a developer something is unhandled!
}

//------------------------------------------------------------------------------
void BlueNet::PurgeTransport( long long transportID )
{
	Ccp::RWSpinlockWriteScoped wlock( m_singleton.m_transportLock );

	BN_PURGETRANSPORT(bnlog("Purging transportID[%lld]", transportID));

	TransportRepr *T = m_transportsByTransportID.getItem( transportID );
	if ( !T )
	{
		return;
	}

	m_transportsByTransportID.remove( transportID );
	m_transportsBySocket.remove( T->descriptor );
	m_transportsByNodeID.remove( T->nodeID );
	m_transportsByProxyID.remove( T->nodeID );

	// remove this transport from any users who have declared it as their home
	for( TransportRepr **I = m_transportsByClientID.getFirst() ;
		 I ;
		 I = m_transportsByClientID.getNext() )
	{
		if ( (*I) == T )
		{
			m_transportsByClientID.remove( m_transportsByClientID.getCurrentKey() );
		}
	}

	// clean off any character IDs that refer to it
	long long charId = 0;
	long long clientId = 0;
	for( ClientMapping *map = m_singleton.m_clientMapByCharID.getFirst() ;
		 map ;
		 map = m_singleton.m_clientMapByCharID.getNext() )
	{
		if ( map->transport == T )
		{
			BN_PURGETRANSPORT(bnlog( "remoivng charid[%lld] via purge-transport", m_singleton.m_clientMapByCharID.getCurrentKey() ));

			charId = m_singleton.m_clientMapByCharID.getCurrentKey();
			clientId = map->clientID;
			m_singleton.m_clientMapByCharID.remove( charId );
		}
	}

	CCP_DELETE T;

	m_singleton.SequenceTransports();

	wlock.release();

	if ( charId && m_singleton.m_clientSessionCallbackList.count() )
	{
		Ccp::RWSpinlockReadScoped rlock( m_singleton.m_clientSessionCallbackListLock );
		Ccp::LinkHash<ClientSessionCallback>::CIterator iter(m_singleton.m_clientSessionCallbackList);
		for( ClientSessionCallback *C = iter.getFirst(); C; C = iter.getNext() )
		{
			(*C)( charId, clientId, STATUS_LOGOUT );
		}
		rlock.release();
	}
}

//------------------------------------------------------------------------------
DataCallbackJob* BlueNet::GetDataCallbackJob()
{
	DataCallbackJob* ret = 0;

	if ( !m_callbackPool )
	{
		ret = CCP_NEW("BlueNet/DataCallbackJob") DataCallbackJob;
	}
	else
	{
		m_callbackPoolLock.Acquire();
		if ( !m_callbackPool )
		{
			ret = CCP_NEW("BlueNet/DataCallbackJob") DataCallbackJob;
		}
		else
		{
			ret = m_callbackPool;
			m_callbackPool = m_callbackPool->next;
		}
		m_callbackPoolLock.Release();
	}

	return ret;
}

//------------------------------------------------------------------------------
void BlueNet::FreeDataCallbackJob( DataCallbackJob* callback )
{
	m_callbackPoolLock.Acquire();
	callback->next = m_callbackPool;
	m_callbackPool = callback;
	m_callbackPoolLock.Release();
}

//------------------------------------------------------------------------------
bool BlueNet::SendClientPacket( int blueNetKey,
								const char* data,
								const int len,
								TransportRepr* transport,
								const long long clientID,
								const int priority )
{
	BlueNetHeader header;
	memset( &header, 0, sizeof(BlueNetHeader) );

	header.destinationType = ADDRESS_TYPE_CLIENT;
	header.destinationID = clientID;
	header.blueNetKey = blueNetKey;
	header.timestamp = CheckForPing( transport );
    header.latencyAccumulator = transport->latency;
	if ( header.timestamp )
	{
		header.priority = true;
	}
	else
	{
		header.priority = (priority == PRIORITY_HIGH);
	}

	BN_AGG(bnlog("sending packet[%d] to client[%lld] priority[%s]", len, clientID, header.priority ? "HIGH":"NORMAL" ));

	char headerData[ BLUE_HEADER_SCRATCH_SIZE ];
	BitPacker packer( headerData, BLUE_HEADER_SCRATCH_SIZE );
	header.Pack( packer );

	return SendPacketEx( transport, data, len, headerData, packer.Finalize(), PRIORITY_HIGH );
}

//------------------------------------------------------------------------------
void BlueNet::SequenceTransports()
{
	int seq = 0;
	for( TransportRepr** T=m_transportsByTransportID.getFirst(); T; T=m_transportsByTransportID.getNext() )
	{
		(*T)->sequence = seq++;
	}
}

//------------------------------------------------------------------------------
void BlueNet::Route( BlueNetHeader* header,
					 BitPacker* packer,
					 TransportRepr* source,
					 const char* data,
					 const int len )
{
	BN_ROUTE(bnlog("mode:proxy data in from [%lld]", source->ID));

	if ( source->isClientTransport && header->forkedAddresses )
	{
		CCP_LOGERR_CH( s_blueNetChannel, "client[%lld] transport tried to send a forking packet, denied", source->clientID );
		return;
	}

	// if there is a timestamp push it through

	char rebuiltHeader[ BLUE_HEADER_SCRATCH_SIZE ];
	BitPacker repacker( rebuiltHeader, BLUE_HEADER_SCRATCH_SIZE );

	TransportRepr *target = 0;
	if ( header->destinationType == ADDRESS_TYPE_NODE )
	{
		BN_ROUTE(bnlog("destination 'NODE' pulling target[%lld]", header->destinationID));
		target = m_singleton.m_transportsByNodeID.getItem( header->destinationID );
		header->priority = true; // override priority so it is forwarded imediately to the server
	}
	else if ( header->destinationType == ADDRESS_TYPE_CLIENT )
	{
		if ( !header->forkedAddresses )
		{
			BN_ROUTE(bnlog("non-forking client address[%lld]", header->destinationID));
			target = m_singleton.m_transportsByClientID.getItem( header->destinationID );

			if ( !target )
			{
				// transport to a client not found, just dropping the packet
				CCP_LOG_CH( s_blueNetChannel, "Recieved a packet (type: 0x%08X) for client [%lld] but I don't have a transport for that client. Packet will be dropped.", header->blueNetKey, header->destinationID );
				return;
			}
		}
		else
		{
			int forkedCount = header->forkedAddresses; // preserve list but blank the header for retransmit
			header->forkedAddresses = 0;

			unsigned long long forkTargetClientID;
			for( int i=0; i<forkedCount; i++)
			{
				if ( !packer->Unpack(forkTargetClientID) )
				{
					// error in the data (can only mean premature end)
					CCP_LOGERR_CH( s_blueNetChannel, "failed to unpack fork[%d/%d] type[0x%08X] target[%lld]", i, forkedCount, header->blueNetKey, header->destinationID );
					return;
				}

				target = m_transportsByClientID.getItem( forkTargetClientID );
				if ( target )
				{
					BN_ROUTE(bnlog("fork target[%d:%lld] routing on transport[%lld]", i, forkTargetClientID, target->ID));

                    header->latencyAccumulator += target->latency;
                    header->Pack( repacker );
                    int rebuiltHeaderSize = repacker.Finalize();
					if ( !SendPacketEx(target, data, len, rebuiltHeader, rebuiltHeaderSize, header->priority) )
					{
						CCP_LOGERR_CH( s_blueNetChannel, "SendPacket failed for key[0x%08X] client[%lld]", header->blueNetKey, header->destinationID );
						// otherwise.. we don't really care. let other clients get a shot at not failing
					}
					header->latencyAccumulator -= target->latency; // Restore the value for the next fork
				}
				else
				{
					CCP_LOG_CH( s_blueNetChannel, "Could not locate forked transport for target [%lld] for message type[0x%08X]. Client might have disconnected before the source found out about it.", forkTargetClientID, header->blueNetKey );
					// a client must have disconnected before the
					// originating node found out about it, this is
					// unusual but normal, continue.
				}
			}

			return;
		}
	}

	if ( target )
	{
		BN_ROUTE(bnlog("target found, packet routed"));

        header->latencyAccumulator += target->latency;
        header->Pack( repacker );
		int rebuiltHeaderSize = repacker.Finalize();
		if ( !SendPacketEx(target, data, len, rebuiltHeader, rebuiltHeaderSize, header->priority) )
		{
			CCP_LOGERR_CH( s_blueNetChannel, "SendPacket failed for key[0x%08X] client[%lld] despite having a target", header->blueNetKey, header->destinationID );
		}
	}
}

//------------------------------------------------------------------------------
bool BlueNet::Deliver( BlueNetHeader* header,
					   const char* data,
					   const int len,
					   long long TransportIdArrivedOn )
{
	Ccp::RWSpinlockReadScoped rlock( m_singleton.m_callbackListLock );

	DataCallbackRepr *callback = m_singleton.m_callbackList.get( header->blueNetKey );
	if ( !callback )
	{
		CCP_LOGERR_CH( s_blueNetChannel, "BlueNet key[0x%08X] did not match a callback for delivery", header->blueNetKey );
		return false;
	}

	PacketInfo info;
	info.fromNodeID = header->sourceID;
	info.accumulatedLatency = header->latencyAccumulator;
	info.blueNetKey = header->blueNetKey;
	info.TransportIdArrivedOn = TransportIdArrivedOn;

	// does this registrant want the packet the moment it arrives,
	// ASYNChronously? if so it is dispatched right here and forgotten
	if ( callback->style == CALLBACK_ASYNC )
	{
		// a callback might do anything, including register/unregister
		// callbacks, which would result in a deadlock, so copy into
		// temp space and release the lock
		DataCallback tempDataCallback = callback->callback;
		ExtendedDataCallback tempExCallback = callback->exCallback;
		rlock.release();

		if ( tempDataCallback )
		{
			tempDataCallback( header->sourceID, header->blueNetKey, data, len );
		}
		else if ( tempExCallback )
		{
			tempExCallback( &info, data, len );
		}
		else
		{
			CCP_LOGERR_CH( s_blueNetChannel, "async callback with neither callback set" );
		}

		return true;
	}

	BN_DELIVER(bnlog("found owner for type[0x%08X] len[%d]", header->blueNetKey, len));

	DataCallbackJob *job = GetDataCallbackJob();

	// fill in the job
	memcpy( &job->info, &info, sizeof(PacketInfo) );


	job->len = len;
	if ( len < BLUE_NET_SMALL_BUFFER_SIZE )
	{
		job->data = 0;
		memcpy( job->smallData, data, len );
	}
	else
	{
		job->data = CCP_NEW("BlueNet/jobdata") char[ len ];
		memcpy( job->data, data, len );
	}

	job->callback = callback->callback;
	job->exCallback = callback->exCallback;

    m_singleton.m_fastTime = time(0); // update here so we are not hammering it per-packet, accuracy is not essential

	// and queue it
	m_singleton.m_callbackJobLock.Acquire();
	if ( callback->style == CALLBACK_PYTHON )
	{
		if ( m_singleton.m_callbackTail )
		{
			m_singleton.m_callbackTail->next = job;
		}
		else
		{
			m_singleton.m_callbackHead = job;
		}
		job->next = 0;
		m_singleton.m_callbackTail = job;

		if ( !m_pendingCallPending )
		{
			BN_DELIVER(bnlog("pending call false, adding callback"));
			m_pendingCallPending = true;
			m_singleton.m_callbackJobLock.Release();

			while( Py_AddPendingCall(&DeliverEx, 0) == -1 )
			{
				CcpThreadYield();
			}
		}
		else
		{
			BN_DELIVER(bnlog("pending call true"));
			m_singleton.m_callbackJobLock.Release();
		}
	}
	else
	{
		if ( m_singleton.m_callbackTail_C )
		{
			m_singleton.m_callbackTail_C->next = job;
		}
		else
		{
			m_singleton.m_callbackHead_C = job;
		}
		job->next = 0;
		m_singleton.m_callbackTail_C = job;

		m_singleton.m_callbackJobLock.Release();
	}

	return true;
}

//------------------------------------------------------------------------------
int BlueNet::DeliverEx( void* arg )
{
	BN_DELIVER(bnlog("Python delivery"));

	// trivial call?
	if ( !m_singleton.m_callbackHead )
	{
		return 0;
	}

	// get in get the list and get out, so none of the async activity is
	// blocked by long callbacks
	m_singleton.m_callbackJobLock.Acquire();
	DataCallbackJob *jobs = m_singleton.m_callbackHead;
	m_singleton.m_callbackHead = 0;
	m_singleton.m_callbackTail = 0;
	m_singleton.m_pendingCallPending = false;
	m_singleton.m_callbackJobLock.Release();

	DeliverJobs( jobs );

	return 0;
}

//------------------------------------------------------------------------------
void BlueNet::DeliverJobs( DataCallbackJob *jobs )
{
	while( jobs )
	{
		DataCallbackJob *next = jobs->next;
		if ( jobs->callback )
		{
			jobs->callback( jobs->info.fromNodeID,
							jobs->info.blueNetKey,
							jobs->len < BLUE_NET_SMALL_BUFFER_SIZE ? jobs->smallData : jobs->data,
							jobs->len );
		}
		else
		{
			jobs->exCallback( &jobs->info,
							  jobs->len < BLUE_NET_SMALL_BUFFER_SIZE ? jobs->smallData : jobs->data,
							  jobs->len );
		}

		CCP_DELETE[] jobs->data;
		m_singleton.FreeDataCallbackJob( jobs );
		jobs = next;
	}
}

//------------------------------------------------------------------------------
unsigned long long BlueNet::CheckForPing( TransportRepr* transport )
{
	if ( transport->nextLatencyTimeCheck < m_fastTime )
	{
		unsigned long long ret = CcpGetTickCount();
		if ( transport->nextLatencyTimeCheck == 0 )
		{
			// it's our first (or renegotiated) ping, signal reciever to also ping us
			ret |= TIMESTAMP_PINGBACK_BIT;
			BN_PING(bnlog("Latency was zero, adding PINGBACK bit [%016llX]", ret));
		}

		transport->nextLatencyTimeCheck = m_fastTime + m_constMinPingFrequency;

		BN_PING(bnlog("transport [%lld] ping expired, sending timestamp[0x%016llX]", transport->ID, ret));
		return ret;
	}

	return 0;
}

//------------------------------------------------------------------------------
bool BlueNet::SendPacketEx( TransportRepr *transport,
							const char* data,
							unsigned int len,
							const char* headerData,
							unsigned int headerLen,
							const int priority  )
{
	return m_socketAPI->send_packet( transport->descriptor, data, len, headerData, headerLen );
}

//------------------------------------------------------------------------------
unsigned int BlueNet::FlushTransport( TransportRepr* transport )
{
	if ( !transport->bytesAggregated ) // check without locking
	{
		return 0;
	}

	CcpAutoMutex tlock( transport->packetAggregateLock );

	unsigned int bytes = transport->bytesAggregated;
	bool ret = m_socketAPI->send_formatted_packet( transport->descriptor, transport->packetAggregator, transport->bytesAggregated );
	transport->bytesAggregated = 0;

	return ret ? bytes : 0;
}

//------------------------------------------------------------------------------
void BlueNet::AnswerPing( TransportRepr* transport, unsigned long long timestamp )
{
	char pingData[ BLUE_HEADER_SCRATCH_SIZE ];
	BitPacker pingPacker( pingData, BLUE_HEADER_SCRATCH_SIZE );
	BlueNetHeader pingHeader;
	memset( &pingHeader, 0, sizeof(BlueNetHeader) );

	pingHeader.blueNetKey = m_constPongKey;
	pingHeader.timestamp = timestamp & ~TIMESTAMP_PINGBACK_BIT;
	pingHeader.latencyAccumulator = (unsigned int)(CheckForPing( transport ) & ~TIMESTAMP_PINGBACK_BIT); // do they owe us a ping too? piggyback it
	pingHeader.priority = true;
	pingHeader.Pack( pingPacker );

	BN_PING(bnlog("timestamp[0x%016llX] received from [%lld] sending pong", timestamp, transport->ID ));

	m_socketAPI->send_packet( transport->descriptor, pingData, 4, pingData, pingPacker.Finalize() ); // 4 bytes of dummy data

	if ( timestamp & TIMESTAMP_PINGBACK_BIT )
	{
		BN_PING(bnlog("timestamp bit[0x%016llX] means ask for another ping from target", timestamp ));
		// target got a spike and asked us to ping too
		transport->nextLatencyTimeCheck = 0;
	}
}

//----------------------------------------------------------------------------------------------------------
void BlueNet::BlueNetCallback( const BlueNet::PacketInfo* info, const char* data, const int len )
{
	BN_BLUE_CALLBACK(bnlog("BlueNetCallback: [%d] bytes", len));

	BitPacker packer( data, len );

	PyGILState_STATE pyGILState = PyGILState_Ensure();

	PyObject* message;
	char* type;
	unsigned int typelen;
	if ( !m_singleton.UnpackString(&type, &typelen, packer) )
	{
		goto Callback_release;
	}

	message = m_singleton.UnserializeObject( packer );
	if ( !message )
	{
		goto Callback_release;
	}

	if ( !PyTuple_Check(message) )
	{
		CCP_LOGERR_CH( s_blueNetChannel, "BlueNetCallback: Message must be a tuple" );
		goto Callback_release;
	}

	PyOS->PythonEvent( type, message );
	Py_XDECREF( message );

Callback_release:

	PyGILState_Release( pyGILState );
}

//-------------------------------------------------------------------------------
bool BlueNet::SerializeObject( PyObject* object, BitPackerManaged& packer )
{
	if ( !object || (object == Py_None) )
	{
		BN_SERIALIZE(bnlog("packing as 'None'"));
		packer.Pack( (unsigned int)PYOBJECT_NONE ); // indicate a 'None' object. This will cause nulls to become None
		return true;
	}

	BN_SERIALIZE(bnlog("packing [%p]:[%s]", object, object->ob_type->tp_name));

	// see if the type is recognized, and if so pack directly onto the
	// wire, otherwise fall back to blue.marshal

	if ( object == Py_True )
	{
		packer.Pack( (unsigned int)PYOBJECT_TRUE );
	}
	else if ( object == Py_False )
	{
		packer.Pack( (unsigned int)PYOBJECT_FALSE );
	}
	else if ( PyList_CheckExact(object) )
	{
		packer.Pack( (unsigned int)PYOBJECT_LIST );

		const unsigned int len = (unsigned int)PyList_GET_SIZE( object );
		packer.Pack( len );
		BN_SERIALIZE(bnlog("packing a List with [%d] entries", len));
		for( unsigned int i=0; i<len; i++ )
		{
			BN_SERIALIZE(bnlog("packing List entry[%d] of [%d] IN", i, len ));
			if ( !SerializeObject(PyList_GET_ITEM(object, i), packer) )
			{
				return false;
			}
			BN_SERIALIZE(bnlog("packing List entry[%d] of [%d] OUT", i, len ));
		}
	}
	else if ( PyTuple_CheckExact(object) )
	{
		packer.Pack( (unsigned int)PYOBJECT_TUPLE );

		const unsigned int len = (unsigned int)PyTuple_GET_SIZE( object );
		packer.Pack( len );
		BN_SERIALIZE(bnlog("packing a Tuple with [%d] entries", len));
		for( unsigned int i=0; i<len; i++ )
		{
			BN_SERIALIZE(bnlog("packing Tuple entry[%d] of [%d] IN", i, len ));
			if ( !SerializeObject(PyTuple_GET_ITEM(object, i), packer) )
			{
				return false;
			}
			BN_SERIALIZE(bnlog("packing Tuple entry[%d] of [%d] OUT", i, len ));
		}
	}
	else if ( PyDict_CheckExact(object) )
	{
		packer.Pack( (unsigned int)PYOBJECT_DICT );

		const unsigned int items = (unsigned int)PyDict_Size( object );
		packer.Pack( items );

		BN_SERIALIZE_DICT(bnlog("packing Dict [%d] items", items));

		PyObject *key;
		PyObject *value;
		Py_ssize_t pos = 0;
		while ( PyDict_Next(object, &pos, &key, &value) )
		{
			if ( !SerializeObject(key, packer) || !SerializeObject(value, packer) )
			{
				BN_SERIALIZE_DICT(bnlog("dict FAILED"));
				return false;
			}
			BN_SERIALIZE_DICT(bnlog("packing dict succeeded"));
		}
	}
	else if ( PyLong_CheckExact(object) )
	{
		packer.Pack( (uint32_t)PYOBJECT_LONG );
		packer.Pack( (int64_t)PyLong_AsLongLong(object) );
		BN_SERIALIZE(bnlog("packing Long[%lld:0x%016llX]", PyLong_AsLongLong(object), (unsigned long long)PyLong_AsLongLong(object) ));
	}
	else if ( PyFloat_CheckExact(object) )
	{
		packer.Pack( (uint32_t)PYOBJECT_DOUBLE );
		packer.Pack( (double)PyFloat_AS_DOUBLE(object) );
		BN_SERIALIZE(bnlog("packing Float[%f]", PyFloat_AS_DOUBLE(object)));
	}
	else if ( PyUnicode_CheckExact(object) )
	{
		packer.Pack( (unsigned int)PYOBJECT_STRING );
		Py_ssize_t len;
		const char* string = PyUnicode_AsUTF8AndSize( object, &len );
		BN_SERIALIZE(bnlog("packing a String[%s]", string)); // warning- the above code does NOT assume zero termination but this log does
		m_singleton.PackString( string, (unsigned int)len, packer );
	}
	else
	{
		// TODO- write custom packer based on name or something? see blue/Marshal.c

		if ( !strcmp(object->ob_type->tp_name, "blue.MarshalStream") ) // its already packed, just copy it onto the wire
		{
			packer.Pack( (unsigned int)PYOBJECT_MARSHALED_EMBEDDED );

			size_t len;
			const char* buf = ((WriteStream *)object)->GetBuffer( &len );
			packer.Pack( (unsigned int)len );
			packer.QueueAlignedBlock( buf, (int)len );

			BN_SERIALIZE(bnlog("MARSHALED (EMBEDDED)"));
		}
		else
		{
			PyObject* ret = m_singleton.m_marshal->SaveObject( object, 0, 0, 0, 0 );

			if ( ret )
			{
				BN_SERIALIZE(bnlog("MARSHALED"));

				size_t len;
				const char* buf = ((WriteStream *)ret)->GetBuffer( &len );

				packer.Pack( (unsigned int)PYOBJECT_MARSHALED );
				packer.Pack( (unsigned int)len );
				packer.QueueAlignedBlock( buf, (int)len );

				Py_XDECREF( ret );
			}
			else
			{
				CCP_LOGERR_CH( s_blueNetChannel, "BlueNet: marshaler failed" );
			}


			m_singleton.m_stats.numberFellback++;
		}
	}

	m_singleton.m_stats.numberTotal++;

	BN_SERIALIZE(bnlog("serialize pass returning [%s]", PyErr_Occurred() ? "false":"true"));

	return PyErr_Occurred() ? false : true;
}

//------------------------------------------------------------------------------
void BlueNet::PackString( const char* string, const unsigned int len, BitPackerManaged& packer )
{
	unsigned long long key = (unsigned long long)Ccp::HashString( string );

	StringEntry *entry = m_stringTable.get( key );

	if ( entry )
	{
		// we know about this one, just store the index
		entry->frequency++;
		BN_PACKSTRING(bnlog("packing hit[%s]:%d", MARSHAL_STRINGS[entry->index], entry->frequency));

		packer.Pack( true );
		packer.Pack( entry->index );
	}
	else
	{
		// nope, brute-force it in and add it to the new list in case
		// anyone cares about adding it offline
		packer.Pack( false );
		packer.Pack( len );
		packer.QueueAlignedBlock( string, len );

		entry = m_newStrings.get( key );
		if ( entry )
		{
			entry->frequency++;
		}
		else if ( len < m_NewStringMaxLen )
		{
			entry = m_newStrings.add( key );
			entry->newString = CCP_NEW("BlueNet/StringEntry") char[len + 1];
			strncpy( entry->newString, string, len );
			entry->frequency = 1;
		}
		//BN_PACKSTRING(bnlog("packing MISS[%s]:%d", string, entry->frequency));
	}
}

//------------------------------------------------------------------------------
PyObject* BlueNet::UnserializeObject( BitPacker& packer )
{
	PyObject* ret = 0;

	unsigned int type;
	packer.Unpack( type );
	switch( type )
	{
		case PYOBJECT_NONE:
		{
			ret = Py_None;
			Py_INCREF( Py_None );
			break;
		}

		case PYOBJECT_STRING:
		{
			BN_UNSERIALIZE(bnlog("PYOBJECT_STRING"));

			unsigned int len;
			char* buf;
			if ( !m_singleton.UnpackString( &buf, &len, packer ) )
			{
				break;
			}
			ret = PyUnicode_FromStringAndSize( buf, len );
			break;
		}

		case PYOBJECT_INT:
		{
			BN_UNSERIALIZE(bnlog("PYOBJECT_INT"));

			int32_t val;
			packer.Unpack( val );
			return PyLong_FromLong( val );
		}

		case PYOBJECT_LONG:
		{
			BN_UNSERIALIZE(bnlog("PYOBJECT_LONG"));

			int64_t val;
			packer.Unpack( val );
			return PyLong_FromLongLong( val );
		}

		case PYOBJECT_DOUBLE:
		{
			BN_UNSERIALIZE(bnlog("PYOBJECT_DOUBLE"));

			double val;
			packer.Unpack( val );
			return PyFloat_FromDouble( val );
		}

		case PYOBJECT_LIST:
		{
			BN_UNSERIALIZE(bnlog("PYOBJECT_LIST"));

			unsigned int len;
			packer.Unpack( len );
			ret = PyList_New( len ); // create a list of appropriate size
			for( unsigned int i=0; i<len; i++) // populate it
			{
				PyObject *member = UnserializeObject( packer );
				if ( !member ) // problem? nuke list and pass up the error
				{
					Py_DECREF( ret );
					ret = 0;
					break;
				}

				PyList_SET_ITEM( ret, i, member );
			}
			break;
		}

		case PYOBJECT_TUPLE:
		{
			BN_UNSERIALIZE(bnlog("PYOBJECT_TUPLE"));

			unsigned int len;
			packer.Unpack( len );
			ret = PyTuple_New( len ); // create a tuple of appropriate size
			for( unsigned int i=0; i<len; i++) // populate it
			{
				PyObject *member = UnserializeObject( packer );
				if ( !member ) // problem? nuke tuple and pass up the error
				{
					Py_DECREF( ret );
					ret = 0;
					break;
				}

				PyTuple_SET_ITEM( ret, i, member );
			}
			break;
		}

		case PYOBJECT_TRUE:
		{
			Py_INCREF( Py_True );
			ret = Py_True;
			break;
		}

		case PYOBJECT_FALSE:
		{
			Py_INCREF( Py_False );
			ret = Py_False;
			break;
		}

		case PYOBJECT_DICT:
		{
			BN_UNSERIALIZE(bnlog("PYOBJECT_DICT"));

			unsigned int items;
			packer.Unpack( items );
			if ( !(ret = PyDict_New()) )
			{
				break;
			}

			for( unsigned int i=0; i<items; i++)
			{
				PyObject* key = UnserializeObject( packer );
				if ( !key )
				{
					Py_DECREF( ret );
					ret = 0;
					break;
				}

				PyObject* val = UnserializeObject( packer );
				if ( !val )
				{
					Py_DECREF( ret );
					Py_DECREF( key );
					ret = 0;
					break;
				}

				if ( PyDict_SetItem(ret, key, val) )
				{
					Py_DECREF( key );
					Py_DECREF( val );
					Py_DECREF( ret );
					ret = 0;
					break;
				}
			}

			break;
		}

		case PYOBJECT_MARSHALED_EMBEDDED:
		{
			// a marshaled object was sent, and the recieving action is
			// expecting it that way, do NOT unmarshal it here

			BN_UNSERIALIZE(bnlog("PYOBJECT_MARSHALED_EMBEDDED"));

			unsigned int len;
			char* buf;
			packer.Unpack( len );
			packer.DeQueueAlignedBlock( &buf, len );
			ret = PyBytes_FromStringAndSize( buf, len );

			break;
		}

		case PYOBJECT_MARSHALED:
		{
			BN_UNSERIALIZE(bnlog("PYOBJECT_MARSHALED"));

			unsigned int len;
			char* buf;
			packer.Unpack( len );
			packer.DeQueueAlignedBlock( &buf, len );

			ReadStream stream( buf, len, 0 );
			if ( m_singleton.m_marshal->ReadWrapper(stream, &Marshal::ReadHeader) )
			{
				ret = m_singleton.m_marshal->ReadWrapper( stream, &Marshal::ReadObject );
			}

			if ( !ret )
			{
				CCP_LOGERR_CH( s_blueNetChannel, "BlueNet:Destiny unmarshal function borked" );
			}
			else
			{
				BN_UNSERIALIZE(bnlog( "unmarshaled: %s", PyString_AS_STRING(PyObject_Repr(ret))));
			}

			break;
		}

		default:
		{
			BN_UNSERIALIZE(bnlog("unhandled type [%d]", type ));
			CCP_LOGERR_CH( s_blueNetChannel, "BlueNet:Destiny unhandled type[%d]", type );
			break;
		}
	}

	if ( !packer.Valid() )
	{
		BN_UNSERIALIZE(bnlog("Packer invalid on exit"));
		Py_XDECREF( ret );
		return 0;
	}
	else
	{
		return ret;
	}
}

//------------------------------------------------------------------------------
bool BlueNet::UnpackString( char** string, unsigned int* len, BitPacker& packer )
{
	bool indexed;
	packer.Unpack( indexed );

	if ( indexed )
	{
		unsigned int index;
		packer.Unpack( index );

		if ( index >= (unsigned int)MARSHAL_STRING_ARRAY_LEN )
		{
			CCP_LOGERR_CH( s_blueNetChannel, "string index out of range [%d]", index );
			return false;
		}

		*string = const_cast<char *>(MARSHAL_STRINGS[index]);
		*len = (unsigned int)strlen(MARSHAL_STRINGS[index]);

		BN_PACKSTRING(bnlog("unpacked hit [%s]", *string));
	}
	else
	{
		packer.Unpack( *len );
		packer.DeQueueAlignedBlock( string, *len );

//		BN_PACKSTRING(char str[10000]; memcpy(str,*string,*len); str[*len]=0);
//		BN_PACKSTRING(bnlog("unpacked MISSED [%d][%s]", *len, str));
	}

	return packer.Valid();
}

//------------------------------------------------------------------------------
void BlueNet::PackMarshalStream( PyObject* object, BitPackerManaged& packer )
{
	if ( !object )
	{
		return;
	}

	//	SERIALIZE(PyObject* repr = PyObject_Repr(ret));
	//	SERIALIZE(bnlog("MARSHALED into [%s]", PyString_AS_STRING(repr) ));
	//	SERIALIZE(Py_DECREF(repr));

	PyObject *stream = PyObject_CallMethod( object, (char*)"Str", 0 );
	if ( stream )
	{
		Py_ssize_t len;
		char* buffer;
		PyBytes_AsStringAndSize( stream, &buffer, &len );

		packer.Pack( (unsigned int)len );
		packer.QueueAlignedBlock( buffer, (int)len );

		Py_DECREF( stream );
	}
}

//------------------------------------------------------------------------------
void BlueNet::LogStatus( const char* msg )
{
	CCP_LOG_CH( s_blueNetChannel, "CarbonIO: %s", msg );
}

//------------------------------------------------------------------------------
void BlueNet::LogError( const char* msg )
{
	CCP_LOGERR_CH( s_blueNetChannel, "CarbonIO: %s", msg );
}

#endif
#if __clang__
#pragma clang diagnostic pop
#endif
