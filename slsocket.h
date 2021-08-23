/* this file declares the guts of the stackless slsocket implementation,
 * the actual read and write apis that socketmodule.c will use for the
 * special blocking io
 */

#ifndef _SLSOCKET_H_
#define _SLSOCKET_H_

#include "socket_semantics.h"

#include <Python.h>
#include "slsocketmodule.h"

#ifdef __cplusplus
extern "C" {
#endif
    
#ifndef _WIN32
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#endif
#define SOCKET_PYERROR (SOCKET_ERROR -1)

#ifdef SLSOCKET
    extern PyObject *socket_timeout;  //The timeout exception

    //socket creation and destruction.  The destructor uses refcounting.
    SOCKET_T slsock_socket(void **xtradata, int family, int type, int proto);
    SOCKET_T slsock_socket_from_fd(void **xtradata, int fd);
    int slsock_closesocket(SOCKET_T, void *xtradata);
    void slsock_releasesocket(PySocketSockObject *s);

    //The slsocket recv function.  Will steal the buffer to make sure it
    //exists while worker threads operate, even if the waiting tasklet
    //is killed during IO..  Optionally returns address.
    ssize_t slsock_recvfrom(PySocketSockObject *s, Py_buffer *buf, int len, int flags,
                            struct sockaddr *from, int *fromlen);

#endif

    //a function to parse arguments where we can take a string OR a sequence
    int slsock_parse_send(PyObject *args, const char *fmt1, const char *fmt2, PyObject **obj, Py_buffer *buf, void *flags, void *addro);
    int slsock_sendto(PySocketSockObject *s, PyObject *obj, Py_buffer *buf, int flags, sock_addr_t *addr, socklen_t addrlen);
    int slsock_sendall(PySocketSockObject *s, PyObject *obj, Py_buffer *buf, int flags);

    //ccp packet protocol
    int slsock_sendpacket(PySocketSockObject *s, PyObject *obj, Py_buffer *buf);
    PyObject *slsock_recvpacket(PySocketSockObject *s);
    PyObject *slsock_recvpacket_oob(PySocketSockObject *s);


    int slsock_connect(PySocketSockObject *s, const sock_addr_t *addr, socklen_t addrlen, int raise_timeout);

    struct addrinfo;
    int slsock_getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res);

    struct hostent *slsock_gethostbyname(const char *name);
    struct hostent *slsock_gethostbyaddr(const char *addr, int len, int type);

    SOCKET_T slsock_accept(void **xtradata, PySocketSockObject *s, struct sockaddr *addr, socklen_t *addrlen);

    int slsock_select(int nfds, fd_set * readfds, fd_set * writefds, fd_set *exceptfds, const struct timeval * timeout);

    int slsock_setblockingsend(PySocketSockObject *s, int blocking);
    int slsock_setzerobytereads(PySocketSockObject *s, int on);
    int slsock_setmaxpacketsize(PySocketSockObject *s, int size);

    PyObject *slsock_getstats(PySocketSockObject *s);

    PyObject *slsock_get_settings(PyObject *self);
    PyObject *slsock_apply_settings(PyObject *self, PyObject *settings);

#ifdef __cplusplus
}
#endif

#endif /* _SLSOCKET_H_ */
