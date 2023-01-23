#pragma once
#ifndef SSL_PIPE_HPP
#define SSL_PIPE_HPP
/*************************************************************************

ssl_pipe.h

Author:    Curt Hartung
Created:   Aug 2010
OS:        Win32/64
Project:   CarbonIO

Description: Abstracts the SSL standard through OpenSSL and BIO
functions to be compatible with the completion-ports model. This
is necessary since the low-level SSL calls which normally handle
sockets are completion-port-unaware

(c) CCP 2010

***************************************************************************/

#include <socket_semantics.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>

#include <assert.h>
#include <new>

#include <windows.h>

//------------------------------------------------------------------------------
struct CRYPTO_dynlock_value
{
	CRITICAL_SECTION lock;
};

//------------------------------------------------------------------------------
class SslPipe
{
public:

	static void initSSL( bool enableDebug =false ); // must be called before anything else
	static void shutdownSSL(); // call when SSL services are no longer needed
	
	bool initAsServer(); // expects incoming connections (implements server method)
	bool initAsClient(); // expects to be initiating a connection (implements client method)

	// the 4 methods below represent the normal I/O operations
	int injectReceivedData( char* data, int len ); // call when raw data is received
	bool write( const char* data, int len ); // input to the SSL socket
	int read( char* data, int *len ); // output of the SSL socket
	int getPendingTransmitData( char* data, int* len ); // raw data is ready for transmission

	bool handshakeComplete() const { return m_handshakeComplete; }
	bool waitOnHandshake( unsigned int millisecondsToWait );

	SslPipe();
	~SslPipe();
	
private:

	// pointers describing this particular session instance
	BIO* m_bioRead;
	BIO* m_bioWrite;
	SSL* m_connection;
	bool m_handshakeComplete;
	HANDLE m_handshakeEvent;
	
	// single global contexts handle all server and client sessions
	static SSL_CTX* m_serverCtx;
	static SSL_CTX* m_clientCtx;
	static bool m_enableDebug;

	// for randomly generating the keys and certificates
	EVP_PKEY* getEVPKey( RSA* rsa, int priv );
	X509* createX509Certificate( RSA* rsa, RSA* rsaSign );

	// logging callbacks
	static void serverInfoCallback( const SSL *s, int where, int ret );
	static void clientInfoCallback( const SSL *s, int where, int ret );
	static void infoCallback( const char* type, int where, const SSL *s, int ret );
	void logError();

	// callback implementations for SSL to operate safely in multithreaded mode
	inline static void lockingCallback( int mode, int type, const char* file, int line );
	inline static unsigned long idCallback() { return GetCurrentThreadId(); }
	inline static CRYPTO_dynlock_value* dynlockCreateCallback( const char* file, int line );
	inline static void dynlockDestroyCallback( CRYPTO_dynlock_value* dynlock, const char* file, int line );
	inline static void dynlockLockCallback( int mode, CRYPTO_dynlock_value* dynlock, const char* file, int line );
	static CRITICAL_SECTION *m_sslLocks;
};

//------------------------------------------------------------------------------
void SslPipe::lockingCallback( int mode, int type, const char* file, int line )
{
	assert( m_sslLocks );
	if ( mode & CRYPTO_LOCK )
	{
		EnterCriticalSection( &(m_sslLocks[type]) );
	}
	else
	{
		LeaveCriticalSection( &(m_sslLocks[type]) );
	}
}

//------------------------------------------------------------------------------
CRYPTO_dynlock_value* SslPipe::dynlockCreateCallback( const char* file, int line )
{
	CRYPTO_dynlock_value *dynlock = new(std::nothrow) CRYPTO_dynlock_value;
	InitializeCriticalSection( &(dynlock->lock) );
	return dynlock;
}

//------------------------------------------------------------------------------
void SslPipe::dynlockDestroyCallback( CRYPTO_dynlock_value* dynlock, const char* file, int line )
{
	DeleteCriticalSection( &(dynlock->lock) );
	delete dynlock;
}

//------------------------------------------------------------------------------
void SslPipe::dynlockLockCallback( int mode, CRYPTO_dynlock_value* dynlock, const char* file, int line )
{
	if (mode & CRYPTO_LOCK)
	{
		EnterCriticalSection( &(dynlock->lock) );
	}
	else
	{
		LeaveCriticalSection( &(dynlock->lock) );
	}
}


#endif
