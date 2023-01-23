#ifndef API_H
#define API_H
/*************************************************************************

api.h

Author:    Curt Hartung
Created:   Jun 2010
OS:        Win32
Project:   Stackless Socket

Description: interface with a C-compile only file

(c) CCP 2010

***************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#include <winsock2.h>
#include <windows.h>
#include <Python.h>

/*

 socketmodule.c must be compiled as 'C' code due to use of coding
 patterns acceptable only in that language, therefore CarbonIO is
 unavailable to it.

 This is a very thin abstration allowing it to make calls up to the
 Completion Ports class

*/


typedef struct _PySocketSockObject PySocketSockObject;
typedef union sock_addr sock_addr_t;


#ifdef __cplusplus
extern "C" {
#endif


void cio_init();

SOCKET cio_socket( int family, int type, int proto );
int cio_closehandle( SOCKET fd );
SOCKET cio_accept( SOCKET fd, struct sockaddr *addr, int *addrlen, int* timeoutFlag, float timeout );
int cio_connect( SOCKET fd, sock_addr_t *addr, int addrlen, int* timeoutFlag, float timeout );

PyObject* cio_recv( SOCKET fd, int len, int flags, float timeout );
int cio_recv_into( SOCKET fd, char *buf, int len, int flags );
PyObject* cio_recvpacket_ex( SOCKET fd );
PyObject* cio_recvpacketoob_ex( SOCKET fd );
PyObject* cio_recvFrom( SOCKET fd, int len, struct sockaddr* from, int* fromlen );
int cio_recvFromInto( SOCKET fd, char *buf, int len, struct sockaddr* from, int* fromlen );

PyObject* cio_byteswaiting( SOCKET fd );

int cio_send( SOCKET fd, char *buf, int len, int flags );
int cio_send_to( SOCKET fd, char *buf, int len, int flags, struct sockaddr* to, int* tolen );
int cio_send_sequence( SOCKET fd, PyObject *obj, int flags );
int cio_send_sequence_to( SOCKET fd, PyObject *obj, int flags, struct sockaddr* to, int* tolen );
int cio_sendpacket_ex( SOCKET fd, PyObject *args );
int cio_shutdown( SOCKET fd, int how );
int cio_setblockingsend( SOCKET fd, int block );

int cio_setmaxpacketsize_ex( SOCKET fd, PyObject *args );
int cio_getaddrinfo( const char *nodename, const char *servname, struct addrinfo *hints, struct addrinfo **res );
void *cio_gethostbyname( const char *name );
void *cio_gethostbyaddr( const char *addr, int len, int type );

int cio_issocket_valid( SOCKET fd );

PyObject* cio_set_global_compression_threshold( SOCKET fd, PyObject *args );
void cio_enableSSL( SOCKET fd );

PyObject* cio_getstats( SOCKET fd );

#ifdef __cplusplus
}
#endif

#endif