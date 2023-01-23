#include "StdAfx.h"
#include "ssl_pipe.h"

#include <openssl/err.h>
#include <openssl/rand.h>

bool SslPipe::m_enableDebug = false;
SSL_CTX* SslPipe::m_serverCtx = 0;
SSL_CTX* SslPipe::m_clientCtx = 0;
CRITICAL_SECTION *SslPipe::m_sslLocks = 0;

#include <stdarg.h>
//------------------------------------------------------------------------------
extern "C" void ciolog( const char* format, ... );
static void log( const char* format, ... )
{
	if ( !format || !format[0] )
	{
		return;
	}

	char temp[16000];
	va_list arg;
	va_start( arg, format );
	vsprintf( temp, format, arg );
	va_end ( arg );
	ciolog( "%s", temp );
}

//------------------------------------------------------------------------------
SslPipe::SslPipe()
{
	m_bioWrite = 0;
	m_bioRead = 0;
	m_connection = 0;
	m_handshakeEvent = CreateEvent( NULL, true, false, NULL );
	m_handshakeComplete = false;
}

//------------------------------------------------------------------------------
SslPipe::~SslPipe()
{
	if ( m_connection )
	{
		SSL_free( m_connection );
	}
}

//------------------------------------------------------------------------------
void SslPipe::initSSL( bool enableDebug )
{
	SSL_library_init();

	m_enableDebug = enableDebug;
	
	if ( m_enableDebug )
	{
		SSL_load_error_strings();
		ERR_load_BIO_strings();
		ERR_load_SSL_strings();
	}

	// set up the synchro callbacks
	m_sslLocks = new(std::nothrow) CRITICAL_SECTION[ CRYPTO_num_locks() ];
	for( int i=0; i<CRYPTO_num_locks() ; i++ )
	{
		InitializeCriticalSection( &(m_sslLocks[i]) );
	}

	CRYPTO_set_locking_callback( lockingCallback );
	CRYPTO_set_id_callback( idCallback );
	CRYPTO_set_dynlock_create_callback( dynlockCreateCallback );
	CRYPTO_set_dynlock_destroy_callback( dynlockDestroyCallback );
	CRYPTO_set_dynlock_lock_callback( dynlockLockCallback );

	// add some entropy to the random number generator
	char buf[64];
	sprintf( buf, "%lld%lld%lld",
			 (unsigned long long)GetCurrentProcessId(),
			 (unsigned long long)time(0),
			 (unsigned long long)GetTickCount() );
	
	RAND_seed( buf, (int)strlen(buf) );
}

//------------------------------------------------------------------------------
void SslPipe::shutdownSSL()
{
	if ( m_sslLocks )
	{
		for( int i=0; i<CRYPTO_num_locks() ; i++ )
		{
			DeleteCriticalSection( &(m_sslLocks[i]) );
		}
		delete[] m_sslLocks;
	}

	if ( m_serverCtx )
	{
		SSL_CTX_free( m_serverCtx );
	}

	if ( m_clientCtx )
	{
		SSL_CTX_free( m_clientCtx );
	}
}

//------------------------------------------------------------------------------
bool SslPipe::initAsServer()
{
	if ( !m_serverCtx )
	{
		if ( !(m_serverCtx = SSL_CTX_new(SSLv23_server_method())) )
		{
			logError();
			return false;
		}

		SSL_CTX_set_mode( m_serverCtx, SSL_MODE_AUTO_RETRY );

		if ( m_enableDebug )
		{
			SSL_CTX_set_info_callback( m_serverCtx, serverInfoCallback );
		}

		if ( SSL_CTX_use_RSAPrivateKey_file(m_serverCtx, "server_key.pem", SSL_FILETYPE_PEM) == 0
			 || SSL_CTX_use_certificate_file(m_serverCtx, "server_cert.pem", SSL_FILETYPE_PEM) == 0 )
		{
			RSA* rsa = RSA_generate_key( 1024, 65537, 0, 0 );
			if (!rsa)
			{
				log( "Could not generate RSA key" );
				return false;
			}

			// TODO-- this is currently created randomly on-demand, which
			// is fine for the game, but https:// browsers lose their
			// mind. If only to keep them sane this should probably be
			// generated once and persisted so the exception need only be
			// added once. Or a genuine certificate created.
			X509* certificate = createX509Certificate( rsa, rsa );
			if ( !certificate )
			{
				log( "Could not create X509 certificate" );
				return false;
			}

			if (SSL_CTX_use_RSAPrivateKey(m_serverCtx, rsa) <= 0 )
			{
				logError();
				return false;
			}

			if ( SSL_CTX_use_certificate(m_serverCtx, certificate) <= 0 )
			{
				logError();
				return false;
			}

			RSA_free( rsa );
			X509_free( certificate );
		}

		if ( !SSL_CTX_check_private_key(m_serverCtx) )
		{
			log( "invalid server private key" );
			return false;
		}
	}

	if ( !(m_bioWrite = BIO_new(BIO_s_mem())) || !(m_bioRead = BIO_new(BIO_s_mem())) )
	{
		return false;
	}

	if ( !(m_connection = SSL_new(m_serverCtx)) )
	{
		return false;
	}

	SSL_set_bio( m_connection, m_bioRead, m_bioWrite );
	SSL_set_accept_state( m_connection );

	return true;
}

//------------------------------------------------------------------------------
bool SslPipe::initAsClient()
{
	if ( !m_clientCtx )
	{
		if ( !(m_clientCtx = SSL_CTX_new(SSLv23_client_method())) )
		{
			return false;
		}

		SSL_CTX_set_quiet_shutdown( m_clientCtx, 1 );
		SSL_CTX_set_mode( m_clientCtx, SSL_MODE_AUTO_RETRY );

		if ( m_enableDebug )
		{
			SSL_CTX_set_info_callback( m_clientCtx, clientInfoCallback );
		}

		RSA* rsa = RSA_generate_key( 1024, 65537, 0, 0 );
		if (!rsa)
		{
			log( "Could not generate RSA key" );
			return false;
		}

		X509* certificate = createX509Certificate( rsa, rsa );
		if ( !certificate )
		{
			log( "Could not create X509 certificate" );
			return false;
		}

		if (SSL_CTX_use_RSAPrivateKey(m_clientCtx, rsa) <= 0 )
		{
			logError();
			return false;
		}

		if ( SSL_CTX_use_certificate(m_clientCtx, certificate) <= 0 )
		{
			logError();
			return false;
		}

		RSA_free( rsa );
		X509_free( certificate );

		if ( !SSL_CTX_check_private_key(m_clientCtx) )
		{
			log( "invalid client private key" );
			return false;
		}
	}

	if ( !(m_bioWrite = BIO_new(BIO_s_mem())) || !(m_bioRead = BIO_new(BIO_s_mem())) )
	{
		return false;
	}

	if ( !(m_connection = SSL_new(m_clientCtx)) )
	{
		return false;
	}

	SSL_clear( m_connection );
	SSL_set_bio( m_connection, m_bioRead, m_bioWrite );
	SSL_set_connect_state( m_connection );

	SSL_connect( m_connection );

	return true;
}

//------------------------------------------------------------------------------
int SslPipe::injectReceivedData( char* data, int len )
{
	if ( !data || !m_bioRead )
	{
		return false;
	}
	
	if ( BIO_write( m_bioRead, data, len ) <= 0 )
	{
		logError();
		return 0;
	}
	
	SSL_read( m_connection, 0, 0 );

	if ( !m_handshakeComplete && SSL_is_init_finished(m_connection) )
	{
		m_handshakeComplete = true;
		SetEvent( m_handshakeEvent );

		if ( m_enableDebug )
		{
			char buf[128];
			const SSL_CIPHER* sc = SSL_get_current_cipher( m_connection );
			log( "negotiation complete: %s", SSL_CIPHER_description(sc, buf, sizeof(buf)) );
		}
	}
	
	return len;
}

//------------------------------------------------------------------------------
bool SslPipe::write( const char* data, int len )
{
	if ( !data || !m_connection )
	{
		return false;
	}
	
	if ( len == 0 )
	{
		return true;
	}
	
	int written = 0;
	int ret;
	do
	{
		ret = SSL_write( m_connection, data, len );
		if ( ret > 0 )
		{
			written += ret;
		}
		else
		{
			return false;
		}

	} while( written < len );

	return true;
}

//------------------------------------------------------------------------------
int SslPipe::read( char* data, int *len )
{
	if ( !data || !m_connection )
	{
		return false;
	}

	*len = SSL_read( m_connection, data, *len );
	if ( *len <= 0 )
	{
		*len = 0;
		if ( !SSL_want_read(m_connection) )
		{
			logError();
		}
	}
	return *len;
}

//------------------------------------------------------------------------------
int SslPipe::getPendingTransmitData( char* data, int *len )
{
	if ( !data || !m_bioWrite )
	{
		return false;
	}

	*len = BIO_read( m_bioWrite, data, *len );
	if ( *len <= 0 )
	{
		*len = 0;
	}
	return *len;
}

//------------------------------------------------------------------------------
bool SslPipe::waitOnHandshake( unsigned int millisecondsToWait )
{
	if ( !m_connection )
	{
		return false;
	}
	
	if ( m_handshakeComplete )
	{
		return true;
	}

	if ( WaitForSingleObject(m_handshakeEvent, millisecondsToWait == 0 ? INFINITE : millisecondsToWait) != WAIT_OBJECT_0 )
	{
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------
void SslPipe::clientInfoCallback( const SSL *s, int where, int ret)
{
	infoCallback( "CLIENT", where, s, ret );
}

//------------------------------------------------------------------------------
void SslPipe::serverInfoCallback( const SSL *s, int where, int ret)
{
	infoCallback( "SERVER", where, s, ret );
}

//------------------------------------------------------------------------------
void SslPipe::infoCallback( const char* type, int where, const SSL *s, int ret )
{
	char state[1024];
	sprintf( state, "%s :", type );
	
	if ( where & SSL_ST_CONNECT )
	{
		strcat( state, "SSL_ST_CONNECT " );
	}
	if ( where & SSL_ST_ACCEPT )
	{
		strcat( state, "SSL_ST_ACCEPT " );
	}
	if ( SSL_in_init( s ) )
	{
		strcat( state, "SSL_ST_INIT " );
	}
	if ( SSL_in_before( s ) )
	{
		strcat( state, "SSL_ST_BEFORE " );
	}
	if ( SSL_is_init_finished( s ) )
	{
		strcat( state, "SSL_ST_OK " );
	}
	if ( SSL_renegotiate_pending( s ) )
	{
		strcat( state, "SSL_ST_RENEGOTIATE " );
	}
	if ( where & SSL_CB_LOOP )
	{
		strcat( state, "SSL_CB_LOOP " );
	}
	if ( where & SSL_CB_EXIT )
	{
		strcat( state, "SSL_CB_EXIT " );
	}
	if ( where & SSL_CB_READ )
	{
		strcat( state, "SSL_CB_READ " );
	}
	if ( where & SSL_CB_WRITE )
	{
		strcat( state, "SSL_CB_WRITE " );
	}
	if ( where & SSL_CB_ALERT )
	{
		strcat( state, "SSL_CB_ALERT " );
	}
	if ( where & SSL_CB_HANDSHAKE_START )
	{
		strcat( state, "SSL_CB_HANDSHAKE_START " );
	}
	if ( where & SSL_CB_HANDSHAKE_DONE )
	{
		strcat( state, "SSL_CB_HANDSHAKE_DONE " );
	}

	log( "%s : %s", state, SSL_state_string_long(s) );
}

//------------------------------------------------------------------------------
void SslPipe::logError()
{
	if ( !m_enableDebug )
	{
		return;
	}
	
	char buf[256];
	ERR_error_string( ERR_get_error(), buf );
	log( "%s", buf );
}

//------------------------------------------------------------------------------
EVP_PKEY* SslPipe::getEVPKey( RSA* rsa, int priv )
{
	RSA* key = priv ? RSAPrivateKey_dup(rsa) : RSAPublicKey_dup(rsa);
	if (!key)
	{
		return 0;
	}

	EVP_PKEY* pkey = EVP_PKEY_new();
	if (!pkey)
	{
		RSA_free( key );
		return 0;
	}

	if (!(EVP_PKEY_assign_RSA(pkey, key)))
	{
		EVP_PKEY_free(pkey);
		RSA_free(key);
		return 0;
	}

	return pkey;
}

//------------------------------------------------------------------------------
X509* SslPipe::createX509Certificate( RSA* rsa, RSA* rsaSign )
{
	X509* x509 = 0;
	time_t currentTime = time(0);
	time_t expireTime = currentTime + 400000000;

	EVP_PKEY* signKey = getEVPKey( rsaSign, 1 );
	if ( signKey )
	{
		EVP_PKEY* privateKey = getEVPKey( rsa, 0 );
		if ( privateKey )
		{
			x509 = X509_new();
			if ( x509 )
			{
				X509_name_st* name = X509_NAME_new();
				if ( name )
				{
					X509_name_st* nameIssuer = X509_NAME_new();
					if ( nameIssuer )
					{
						if ( !(X509_set_version(x509, 2))
							 || !(ASN1_INTEGER_set(X509_get_serialNumber(x509), (long)currentTime))
							 || !(X509_NAME_add_entry_by_NID(name, OBJ_txt2nid("organizationName"), MBSTRING_ASC, (unsigned char*)"CCP", -1, -1, 0))
							 || !(X509_NAME_add_entry_by_NID(name, OBJ_txt2nid("commonName"), MBSTRING_ASC, (unsigned char *)"ssl_pipe.cpp", -1, -1, 0))
							 || !(X509_set_subject_name(x509, name))
							 || !(X509_NAME_add_entry_by_NID(nameIssuer, OBJ_txt2nid("organizationName"), MBSTRING_ASC, (unsigned char*)"CCP", -1, -1, 0))
							 || !(X509_NAME_add_entry_by_NID(nameIssuer, OBJ_txt2nid("commonName"), MBSTRING_ASC, (unsigned char*)"ssl_pipe.cpp", -1, -1, 0))
							 || !(X509_set_issuer_name(x509, nameIssuer))
							 || !(X509_time_adj(X509_get_notBefore(x509), 0, &currentTime))
							 || !(X509_time_adj(X509_get_notAfter(x509), 0, &expireTime))
							 || !(X509_set_pubkey(x509, privateKey))
							 || !(X509_sign(x509, signKey, EVP_sha1())) )
						{
							X509_free(x509);
							x509 = 0;
						}
						
						X509_NAME_free( nameIssuer );
					}
					else
					{
						X509_free(x509);
						x509 = 0;
					}
					
					X509_NAME_free( name );
				}
				else
				{
					X509_free(x509);
					x509 = 0;
				}
			}
			
			EVP_PKEY_free( privateKey );
		}
				
		EVP_PKEY_free( signKey );
	}

	return x509;
}
