////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//

#include "StdAfx.h"

#include "BlueRemoteStream.h"
#include "include/IBlueOS.h"

#if CCP_STACKLESS
#include "CcpUtils/PyCpp.h"
#endif
#include "md5.h"
#ifdef _WIN32
#include <winhttp.h>
#endif

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "RemoteStream" );

CCP_STATS_DECLARE( remoteStreamBytesDownloaded, "Blue/BlueRemoteStream/BytesDownloaded", false, CST_COUNTER_HIGH, "Number of bytes downloaded from remote server" );
CCP_STATS_DECLARE( remoteStreamPretransferTime, "Blue/BlueRemoteStream/PretransferTime", false, CST_COUNTER_HIGH, "Time spent on pre-transfer activities in milliseconds" );
CCP_STATS_DECLARE( remoteStreamDownloadTime, "Blue/BlueRemoteStream/DownloadTime", false, CST_COUNTER_HIGH, "Total download time in milliseconds" );

class ConnectionManager
{
public:
	ConnectionManager() : m_mutex( "Connectionmanager", "m_mutex" )
	{}

	CURL* GetConnection()
	{
		CcpAutoMutex guard( m_mutex );

		CURL* connection;

		if( m_connections.empty() )
		{
			connection = curl_easy_init();
			curl_easy_setopt( connection, CURLOPT_FAILONERROR, 1 );
			curl_easy_setopt( connection, CURLOPT_ACCEPT_ENCODING, "gzip" );
		}
		else
		{
			connection = m_connections.back();
			m_connections.pop_back();
		}

		return connection;
	}

	void ReleaseConnection( CURL* connection )
	{
		CcpAutoMutex guard( m_mutex );

		m_connections.push_back( connection );
	}

private:
	CcpMutex m_mutex;
	std::vector<CURL*> m_connections;
};

static ConnectionManager s_connectionManager;

namespace
{

#ifdef _WIN32
void GetIEProxySettings( bool& autoProxy, std::wstring& autoConfigUrl, std::string& explicitProxy, std::string& explicitProxyBypass )
{
	autoProxy = false;
	autoConfigUrl = L"";
	explicitProxy = "";
	explicitProxyBypass = "";

	WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ieProxyConfig;
	memset( &ieProxyConfig, 0, sizeof( ieProxyConfig ) );

	ON_BLOCK_EXIT( [&] { GlobalFree( ieProxyConfig.lpszAutoConfigUrl ); } );
	ON_BLOCK_EXIT( [&] { GlobalFree( ieProxyConfig.lpszProxy ); } );
	ON_BLOCK_EXIT( [&] { GlobalFree( ieProxyConfig.lpszProxyBypass ); } );

	WINHTTP_AUTOPROXY_OPTIONS proxyOptions;
	memset( &proxyOptions, 0, sizeof( proxyOptions ) );

	if( WinHttpGetIEProxyConfigForCurrentUser( &ieProxyConfig ) )
	{
		if( ieProxyConfig.fAutoDetect )
		{
			autoProxy = true;
		}

		if( ieProxyConfig.lpszAutoConfigUrl )
		{
			autoProxy = true;
			autoConfigUrl = ieProxyConfig.lpszAutoConfigUrl;
		}

		if( ieProxyConfig.lpszProxy )
		{
			explicitProxy = CW2A( ieProxyConfig.lpszProxy );
		}
		if( ieProxyConfig.lpszProxyBypass )
		{
			explicitProxyBypass = CW2A( ieProxyConfig.lpszProxyBypass );
			for( auto it = explicitProxyBypass.begin(); it != explicitProxyBypass.end(); ++it )
			{
				if( *it == ';' )
				{
					*it = ',';
				}
			}
		}
	}
	else
	{
		autoProxy = true;
	}
}

void GetAutoProxyUrl( const char* url, const wchar_t* autoConfigUrl, std::string& proxy, std::string& proxyBypass )
{
	static HINTERNET http = nullptr;
	static bool createdHttp = false;
	proxy = "";
	proxyBypass = "";

	if( !createdHttp )
	{
		http = WinHttpOpen( nullptr, 0, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 );
		if( !http )
		{
			CCP_LOGWARN( "BlueRemoteStream: could not create windows HTTP object for detecting a proxy" );
			return;
		}
		createdHttp = true;
	}

	if( !http )
	{
		return;
	}

	WINHTTP_AUTOPROXY_OPTIONS proxyOptions;
	memset( &proxyOptions, 0, sizeof( proxyOptions ) );

	if ( autoConfigUrl && *autoConfigUrl )
	{
		proxyOptions.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
		proxyOptions.lpszAutoConfigUrl = autoConfigUrl;
	}
	else
	{
		proxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
		proxyOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
	}
	proxyOptions.fAutoLogonIfChallenged = TRUE;

	WINHTTP_PROXY_INFO proxyInfo;
	if( WinHttpGetProxyForUrl( http, CA2W( url ), &proxyOptions, &proxyInfo ) )
	{
		if( proxyInfo.lpszProxy )
		{
			proxy = CW2A( proxyInfo.lpszProxy );
		}
		if( proxyInfo.lpszProxyBypass )
		{
			proxyBypass = CW2A( proxyInfo.lpszProxyBypass );
			for( auto it = proxyBypass.begin(); it != proxyBypass.end(); ++it )
			{
				if( *it == ';' || *it == ' ' )
				{
					*it = ',';
				}
			}
		}
		GlobalFree( proxyInfo.lpszProxy );
		GlobalFree( proxyInfo.lpszProxyBypass );
	}
}


void ConvertProxySettingToServer( const char* url, const char* setting, std::string& proxyServer )
{
	if( !strchr( setting, '=' ) )
	{
		proxyServer = setting;
		return;
	}
	std::string protocol;
	const char* column = strchr( url, ':' );
	if( column )
	{
		protocol = std::string( url, column ) + "=";
	}
	else
	{
		protocol = "http=";
	}
	if( auto found = strstr( setting, protocol.c_str() ) )
	{
		auto end = strchr( found, ';' );
		if( end )
		{
			proxyServer = std::string( found + protocol.length(), end );
		}
		else
		{
			proxyServer = found + protocol.length();
		}
	}
	else
	{
		proxyServer = "";
	}
}

void GetProxySettings( const char* url, CURL* connection )
{
	static bool gotIESettings = false;
	static bool autoProxy = false;
	static std::string explicitProxy;
	static std::string explicitProxyBypass;
	static std::wstring autoConfigUrl;

	if( !gotIESettings )
	{
		GetIEProxySettings( autoProxy, autoConfigUrl, explicitProxy, explicitProxyBypass );
		gotIESettings = true;
	}

	if( autoProxy )
	{
		std::string proxy;
		std::string proxyBypass;
		GetAutoProxyUrl( url, autoConfigUrl.c_str(), proxy, proxyBypass );

		if( !proxy.empty() )
		{
			curl_easy_setopt( connection, CURLOPT_PROXY, proxy.c_str() );
			curl_easy_setopt( connection, CURLOPT_NOPROXY, proxyBypass.c_str() );
			return;
		}
	}
	else if( !explicitProxy.empty() )
	{
		std::string proxy;
		ConvertProxySettingToServer( url, explicitProxy.c_str(), proxy );
		if( !proxy.empty() )
		{
			curl_easy_setopt( connection, CURLOPT_PROXY, proxy.c_str() );
			curl_easy_setopt( connection, CURLOPT_NOPROXY, explicitProxyBypass.c_str() );
		}
		return;
	}
	curl_easy_setopt( connection, CURLOPT_PROXY, nullptr );
	curl_easy_setopt( connection, CURLOPT_NOPROXY, nullptr );
}
#endif

}

BlueRemoteStream::BlueRemoteStream() :
	m_data( nullptr ),
	m_readLocation( nullptr ),
	m_dataSize( 0 ),
	m_bufferSize( 0 )
{
	InitializeCurl();
}

BlueRemoteStream::~BlueRemoteStream()
{
	if( m_data )
	{
		CCP_FREE( m_data );
	}
}

bool BlueRemoteStream::Open( const char* resUrl, size_t expectedSize )
{
	CCP_STATS_ZONE( __FUNCTION__ );

#if CCP_STACKLESS
	Ccp::PyAllowThreads allowThreads( true );
#endif
	CCP_LOG_CH( s_ch, "Opening %s, expected size %d bytes", resUrl, expectedSize );

	CURL* connection = s_connectionManager.GetConnection();

	curl_easy_setopt( connection, CURLOPT_URL, resUrl );
	curl_easy_setopt( connection, CURLOPT_WRITEFUNCTION, WriteMemoryCallback );
	curl_easy_setopt( connection, CURLOPT_WRITEDATA, (void*)this );

#ifdef _WIN32
	GetProxySettings( resUrl, connection );
#endif

	if( expectedSize )
	{
		m_data = reinterpret_cast<uint8_t*>( CCP_MALLOC( "BlueRemoteStream/m_data", expectedSize ) );
		m_bufferSize = expectedSize;
	}

	CURLcode res;
	
	{
		CCP_STATS_ZONE( __FUNCTION__ " curl_easy_perform");
		res = curl_easy_perform( connection );
	}

	if( res != CURLE_OK )
	{
		CCP_LOGERR_CH( s_ch, "curl_easy_perform() failed: %s\n", curl_easy_strerror( res ) );
	}
	else
	{
		m_readLocation = m_data;

		double size = 0;
		curl_easy_getinfo( connection, CURLINFO_SIZE_DOWNLOAD, &size );
		CCP_STATS_ADD( remoteStreamBytesDownloaded, size );

		double pretransferTime = 0;
		curl_easy_getinfo( connection, CURLINFO_PRETRANSFER_TIME, &pretransferTime );
		CCP_STATS_ADD( remoteStreamPretransferTime, pretransferTime * 1000.0 );

		double totalTime = 0;
		curl_easy_getinfo( connection, CURLINFO_TOTAL_TIME, &totalTime );
		CCP_STATS_ADD( remoteStreamDownloadTime, totalTime * 1000.0 );

		double speed = 0;
		curl_easy_getinfo( connection, CURLINFO_SPEED_DOWNLOAD, &speed );
		CCP_LOG_CH( s_ch, "%s: %g bytes, %g bytes/sec, %g sec pretransfer, %g sec total", resUrl, size, speed, pretransferTime, totalTime );
	}

	s_connectionManager.ReleaseConnection( connection );
	connection = nullptr;

	if( m_data )
	{
		if( strcmp( (const char*)m_data, "error" ) == 0 )
		{
			return false;
		}
	}

	return res == CURLE_OK;
}

ptrdiff_t BlueRemoteStream::Read( void* dest, ptrdiff_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );

#ifdef CCP_STACKLESS
	Ccp::PyAllowThreads allowThreads( true );
#endif

	if( count == -1 )
	{
		// Count of -1 is taken to mean the remainder of the file
		count = INT_MAX;
	}

	if( count < 0 )
	{
		return -1;
	}

	uint8_t* end = m_data + m_dataSize;

	if( m_readLocation >= end )
	{
		return 0;
	}

	ssize_t numLeft = end - m_readLocation;
	if( count > numLeft )
	{
		count = numLeft;
	}

	memcpy( dest, m_readLocation, count );
	m_readLocation += count;

	return count;
}

ptrdiff_t BlueRemoteStream::Write( const void* source, size_t count )
{
	return -1;
}

ptrdiff_t BlueRemoteStream::Seek( ptrdiff_t distance, SeekOrigin method )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( method == SO_BEGIN )
	{
		m_readLocation = m_data + distance;
	}
	else if( method == SO_CURRENT )
	{
		m_readLocation += distance;
	}
	else
	{
		m_readLocation = m_data + m_dataSize - distance;
	}

	if( m_readLocation > m_data + m_dataSize )
	{
		CCP_LOGERR( "Seeking past EOF" );
		m_readLocation = m_data + m_dataSize;
		return -1;
	}

	return true;
}

bool BlueRemoteStream::SetSize( size_t newsize )
{
	return false;
}

ssize_t BlueRemoteStream::CopyFrom( IBlueStream* source, size_t count )
{
	return -1;
}

ptrdiff_t BlueRemoteStream::GetPosition()
{
	return m_readLocation - m_data;
}

ptrdiff_t BlueRemoteStream::GetSize()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	return m_dataSize;
}

bool BlueRemoteStream::LockData( void** data, size_t size )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_data && m_dataSize && ((size == m_dataSize) || (size == 0)) )
	{
		*data = m_data;
		return true;
	}

	return false;
}

bool BlueRemoteStream::UnlockData()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	return true;
}

size_t BlueRemoteStream::WriteMemoryCallback( void* contents, size_t size, size_t nmemb, void* context )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	size_t realsize = size * nmemb;
	BlueRemoteStream* pThis = reinterpret_cast<BlueRemoteStream*>( context );
	pThis->ReceiveData( contents, realsize );
	return realsize;
}

void BlueRemoteStream::ReceiveData( void* data, size_t size )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	size_t newSize = m_dataSize + size;
	if( newSize > m_bufferSize )
	{
		m_bufferSize = newSize;
		m_data = reinterpret_cast<uint8_t*>( CCP_REALLOC( "BlueRemoteStream", m_data, newSize ) );
	}
	memcpy( m_data + m_dataSize, data, size );
	m_dataSize = newSize;
}

void BlueRemoteStream::InitializeCurl()
{
	static bool isInitialized = false;
	if( !isInitialized )
	{
		curl_global_init( CURL_GLOBAL_DEFAULT );
		isInitialized = true;
	}
}

bool BlueRemoteStream::VerifyContents( const char* expectedChecksum )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	MD5 checkSum;
	checkSum.update( reinterpret_cast<unsigned char*>( m_data ), (unsigned int)m_dataSize );
	checkSum.finalize();
	const char* checkSumAsHex = checkSum.hex_digest();
	if( strcmp( expectedChecksum, checkSumAsHex ) != 0 )
	{
		return false;
	}

	return true;
}
