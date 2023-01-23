/*************************************************************************

api.cpp

Author:    Curt Hartung
Created:   Jun 2010
OS:        Win32
Project:   Stackless Socket

Description: interface with a C-compile only file

(c) CCP 2010

***************************************************************************/
#include "StdAfx.h"
#include "api.h"

// This file is compiled with c++ semantics which allow the
// linkage to the CarbonIO class. This is not possible directly
// from socketmodule since that file must be compiled with C semantics
// due to language constraints.

// in particular, this file is strictly a go-between and no pre or post
// processing ever occurs here.

extern "C" void ciolog( const char* format, ... );

#include "CarbonIO.h"
#include "slsocketmodule.h"

//------------------------------------------------------------------------------
// for debug purposes, should never be part of a checkin
#define CALL_TRACE(a) //a
class CALLTRACE
{
public:
	CALLTRACE( const char* function, int param=0 ) : m_param(param) { ciolog("---------- %s IN [%d]", function, param ); strcpy( m_function, function ); }
	~CALLTRACE() { ciolog("---------- %s OUT [%d]", m_function, m_param ); }
	char m_function[256];
	int m_param;
};

//------------------------------------------------------------------------------
SOCKET cio_socket( int family, int type, int proto )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->socket( family, type, proto );
}

//------------------------------------------------------------------------------
int cio_closehandle( SOCKET fd )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->close( (HANDLE)fd );
}

//------------------------------------------------------------------------------
SOCKET cio_accept( SOCKET fd, struct sockaddr *addr, int *addrlen, int* timeoutFlag, float timeout )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->accept( fd, addr, addrlen, timeoutFlag, timeout );
}

//------------------------------------------------------------------------------
int cio_connect( SOCKET fd, sock_addr_t *addr, int addrlen, int* timeoutFlag, float timeout )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	bool b_timeoutFlag;
	int result =  CarbonIO::singleton()->connect( fd, addr, addrlen, b_timeoutFlag, timeout );
	*timeoutFlag = (int)b_timeoutFlag;
	return result;
}

//------------------------------------------------------------------------------
int cio_shutdown( SOCKET fd, int how )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->shutdown( fd, how );
}

//------------------------------------------------------------------------------
int cio_setblockingsend( SOCKET fd, int block )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->setBlockingSend( fd, block );
}

//------------------------------------------------------------------------------
PyObject* cio_recv( SOCKET fd, int len, int flags, float timeout )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__, (int)fd));
	return CarbonIO::singleton()->recv( fd, len, flags, timeout );
}

//------------------------------------------------------------------------------
int cio_recv_into( SOCKET fd, char *buf, int len, int flags )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->recvInto( fd, buf, len, flags );
}

//------------------------------------------------------------------------------
PyObject* cio_recvFrom( SOCKET fd, int len, struct sockaddr* from, int* fromlen )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->recvFrom( fd, len, from, fromlen );
}

//------------------------------------------------------------------------------
int cio_recvFromInto( SOCKET fd, char *buf, int len, struct sockaddr* from, int* fromlen )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->recvFromInto( fd, buf, len, from, fromlen );
}

//------------------------------------------------------------------------------
PyObject* cio_recvpacket_ex( SOCKET fd )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__,(int)fd));
	return CarbonIO::singleton()->recvPacket( fd, false );
}

//------------------------------------------------------------------------------
PyObject* cio_recvpacketoob_ex( SOCKET fd )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__,(int)fd));
	return CarbonIO::singleton()->recvPacket( fd, true );
}

//------------------------------------------------------------------------------
PyObject* cio_byteswaiting( SOCKET fd )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__,(int)fd));
	return CarbonIO::singleton()->getBytesWaiting( fd );
}

//------------------------------------------------------------------------------
int cio_send( SOCKET fd, char *buf, int len, int flags )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__,(int)fd));
	return CarbonIO::singleton()->send( fd, buf, len, flags ) ? len : -1;
}

//------------------------------------------------------------------------------
int cio_send_to( SOCKET fd, char *buf, int len, int flags, struct sockaddr* to, int* tolen )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->sendTo( fd, buf, len, flags, to, tolen );
}

//------------------------------------------------------------------------------
int cio_send_sequence( SOCKET fd, PyObject *obj, int flags )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->sendSequence( fd, obj, flags );
}

//------------------------------------------------------------------------------
int cio_send_sequence_to( SOCKET fd, PyObject *obj, int flags, struct sockaddr* to, int* tolen )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->sendSequenceTo( fd, obj, flags, to, tolen );
}

//------------------------------------------------------------------------------
int cio_sendpacket_ex( SOCKET fd, PyObject *args )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__,(int)fd));
	return CarbonIO::singleton()->sendPacket( fd, args ) ? 1 : 0;
}

//------------------------------------------------------------------------------
int cio_setmaxpacketsize_ex( SOCKET fd, PyObject *args )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->setMaxPacketSize( fd, args );
}

//------------------------------------------------------------------------------
int cio_getaddrinfo( const char *nodename, const char *servname, struct addrinfo *hints, struct addrinfo **res )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->getAddrInfo( nodename, servname, hints, res );
}

//------------------------------------------------------------------------------
void *cio_gethostbyname( const char *name )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return (void*)CarbonIO::singleton()->getHostByName( name );
}

//------------------------------------------------------------------------------
void *cio_gethostbyaddr( const char *addr, int len, int type )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return (void*) CarbonIO::singleton()->getHostByAddr( addr, len, type );
}

//------------------------------------------------------------------------------
int cio_issocket_valid( SOCKET fd )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->isSocketValid(fd) ? 1 : 0;
}

//------------------------------------------------------------------------------
void cio_enableSSL( SOCKET fd )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	CarbonIO::singleton()->enableSSL( fd );
}

//------------------------------------------------------------------------------
PyObject* cio_getstats( SOCKET fd )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->formatStats( fd );
}

//------------------------------------------------------------------------------
PyObject* cio_set_global_compression_threshold( SOCKET fd, PyObject *args )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->setCompressionThreshold( fd, args );
}

//------------------------------------------------------------------------------
static PyObject *getTimersActive( PyObject *self )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	if ( CarbonIO::singleton()->areTimersEnabled() )
	{
		Py_RETURN_TRUE;
	}
	else
	{
		Py_RETURN_FALSE;
	}
}

//------------------------------------------------------------------------------
static PyObject *getStats( PyObject *self )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->getTimerStats();
}

//------------------------------------------------------------------------------
static PyObject *clearStats( PyObject *self )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	CarbonIO::singleton()->resetTimers();
	CarbonIO::singleton()->resetStats();
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
static PyObject *setTimersActive( PySocketSockObject *s, PyObject *obj )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	CarbonIO::singleton()->setTimersEnabled( PyInt_AsLong(obj) != 0 );
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
static PyObject *dispatch( PyObject *self )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	// depricated
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
static PyObject *usePendingCalls( PyObject *self, PyObject *arg )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	// depricated
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
static PyObject* setSSLServerCertificate( PySocketSockObject *s, PyObject *obj )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	CarbonIO::singleton()->setSSLServerCertificate( obj );
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
static PyObject* setSSLClientCertificate( PySocketSockObject *s, PyObject *obj )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	CarbonIO::singleton()->setSSLClientCertificate( obj );
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
static PyObject* setSSLServerPrivateKey( PySocketSockObject *s, PyObject *obj )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	CarbonIO::singleton()->setSSLPrivateServerKey( obj );
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
static PyObject* setSSLClientPrivateKey( PySocketSockObject *s, PyObject *obj )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	CarbonIO::singleton()->setSSLPrivateClientKey( obj );
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
static PyObject* setSSLHandshakeMaxSeconds( PySocketSockObject *s, PyObject *obj )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	CarbonIO::singleton()->setSSLHandshakeMaxSeconds( obj );
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyDoc_STRVAR( setGlobalCompressionThreshold_doc,
			  "setCompressionThreshold(threshold)\n"
			  "\n"
			  "Set the minimum size below which outgoing packets will not be compressed\n"
			  "set to -1 (default) to disable outgoing packet compression\n"
			  "set to 0 to compress everything" );

static PyObject* setGlobalCompressionThreshold( PySocketSockObject *s, PyObject *args )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->setGlobalCompressionThreshold( args );
}

//------------------------------------------------------------------------------
PyDoc_STRVAR( setCompressionLevel_doc,
			  "setCompressionLevel(level)\n"
			  "\n"
			  "Corresponds to the zlib 0-9 compression level where 0 is no compression\n"
			  "and 9 is maximum (a tradeoff of speed for compression strength)\n"
			  "default value is 6" );

static PyObject* setCompressionLevel( PySocketSockObject *s, PyObject *args )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->setCompressionLevel( args );
}

//------------------------------------------------------------------------------
PyDoc_STRVAR( setCompressionMinRatio_doc,
			  "setCompressionMinRatio(ratio)\n"
			  "\n"
			  "The minimum compression ratio (compressed:uncompressed) that must be achieved for a packet\n"
			  "to be send in compressed form, expressed as a decimal percent, default is 90\n"
			  "(ie- compressed data must be at least 10% smaller than the original size)" );

static PyObject* setCompressionMinRatio( PySocketSockObject *s, PyObject *args )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->setCompressionMinRatio( args );
}

//------------------------------------------------------------------------------
PyDoc_STRVAR( setCompressionType_doc,
			  "setCompressionType(\"zlib\")\n"
			  "\n"
			  "Decide what type of compression will be used, this can be changed at any time\n" );

static PyObject* setCompressionType( PySocketSockObject *s, PyObject *args )
{
	CALL_TRACE(CALLTRACE _trace(__FUNCTION__));
	return CarbonIO::singleton()->setCompressionType( args );
}

//------------------------------------------------------------------------------
static PyObject *setOnTaskletScheduled( PyObject *self, PyObject *handleO )
{
	HANDLE h = (HANDLE)PyLong_AsVoidPtr( handleO );
	if ( (h == (HANDLE)-1) && PyErr_Occurred() )
	{
		return NULL;
	}
	
	CarbonIO::singleton()->setOnTaskletScheduledEvent( h );
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
static PyObject* setWakeupMethod( PySocketSockObject *s, PyObject *arg )
{
	int method = PyLong_AsLong( arg );

	if ( method >= 1 && method <= 3 )
	{
		CarbonIO::singleton()->setWakeupMethod( method );
	}
	
	Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
static PyObject* getWakeupMethod( PyObject *self )
{
	return CarbonIO::singleton()->getWakeupMethod();
}

//------------------------------------------------------------------------------
static PyMethodDef cio_extentions[] =
{
	{ "getTimersActive", (PyCFunction)getTimersActive, METH_NOARGS, 0 },
	{ "GetStats", (PyCFunction)getStats, METH_NOARGS, 0 },
	{ "ClearStats", (PyCFunction)clearStats, METH_NOARGS, 0 },

	{ "setTimersActive", (PyCFunction)setTimersActive, METH_O, 0 },

	{ "dispatch", (PyCFunction)dispatch, METH_NOARGS, 0 },
	{ "UsePendingCalls", (PyCFunction)usePendingCalls, METH_O, 0 },
	{ "usePendingCalls", (PyCFunction)usePendingCalls, METH_O, 0 },

	{ "setSSLServerCertificate", (PyCFunction)setSSLServerCertificate, METH_O, 0 },
	{ "setSSLServerPrivateKey", (PyCFunction)setSSLServerPrivateKey, METH_O, 0 },
	{ "setSSLClientCertificate", (PyCFunction)setSSLClientCertificate, METH_O, 0 },
	{ "setSSLClientPrivateKey", (PyCFunction)setSSLClientPrivateKey, METH_O, 0 },
	{ "setSSLHandshakeMaxSeconds", (PyCFunction)setSSLHandshakeMaxSeconds, METH_O, 0 },

	{ "setCompressionThreshold", (PyCFunction)setGlobalCompressionThreshold, METH_VARARGS, setGlobalCompressionThreshold_doc },
	{ "setCompressionLevel", (PyCFunction)setCompressionLevel, METH_VARARGS, setCompressionLevel_doc },
	{ "setCompressionMinRatio", (PyCFunction)setCompressionMinRatio, METH_VARARGS, setCompressionMinRatio_doc },
	{ "setCompressionType", (PyCFunction)setCompressionType, METH_VARARGS, setCompressionType_doc },

	{ "setOnTaskletScheduled", (PyCFunction)setOnTaskletScheduled, METH_O, 0 },

	{ "setWakeupMethod", (PyCFunction)setWakeupMethod, METH_O, 0 },
	{ "getWakeupMethod", (PyCFunction)getWakeupMethod, METH_NOARGS, 0 },

	{ NULL, NULL }
};

//------------------------------------------------------------------------------
extern "C" void initcarbonio(void)
{
	if ( !CarbonIO::singleton()->init() )
	{
		return;
	}
	
	PyObject *module = Py_InitModule3( "carbonio", cio_extentions, 0 );
	if ( module == NULL )
	{
		return;
	}
}
