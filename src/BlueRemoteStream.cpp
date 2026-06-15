// Copyright © 2011 CCP ehf.

#include "StdAfx.h"

#include "BlueRemoteStream.h"
#include <BlueStatistics.h>
#include "IBlueOS.h"
#include "IBluePaths.h"

#if CCP_STACKLESS
#include "BluePyCpp.h"
#endif
#include "md5.h"
#ifdef _WIN32
#include <winhttp.h>
#endif
#include "IBlueResMan.h"
#include "IBlueThreadMonitor.h"

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "RemoteStream" );

CCP_STATS_DECLARE( remoteStreamBytesDownloaded, "Blue/BlueRemoteStream/BytesDownloaded", false, CST_COUNTER_HIGH, "Number of bytes downloaded from remote server" );
CCP_STATS_DECLARE( remoteStreamPretransferTime, "Blue/BlueRemoteStream/PretransferTime", false, CST_TIME, "Time spent on pre-transfer activities in milliseconds" );
CCP_STATS_DECLARE( remoteStreamDownloadTime, "Blue/BlueRemoteStream/DownloadTime", false, CST_TIME, "Total download time in milliseconds" );

double g_thresholdForWarningLongDownloadTime = 3.0;
double g_thresholdForAbortingLongDownloadTime = 30.0;

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
			curl_easy_setopt( connection, CURLOPT_FOLLOWLOCATION, 1 );
			curl_easy_setopt( connection, CURLOPT_ACCEPT_ENCODING, "gzip" );
			curl_easy_setopt( connection, CURLOPT_NOPROGRESS, 0 );

			std::wstring cert_str = BePaths->ResolvePathW(L"bin://cacert.pem");
			CW2A cert_path( cert_str.c_str() );
			curl_easy_setopt( connection, CURLOPT_CAINFO, static_cast<const char*>( cert_path ) );

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
	CCP_STATS_ZONE( __FUNCTION__ );

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
	CCP_STATS_ZONE( __FUNCTION__ );

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


	{
		CCP_STATS_ZONE( __FUNCTION__ "WinHttpGetProxyForUrl");

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
}


void ConvertProxySettingToServer( const char* url, const char* setting, std::string& proxyServer )
{
	CCP_STATS_ZONE( __FUNCTION__ );

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
#endif


bool FindHeader( const char* headers, const char* name, std::string& value )
{
	std::string decorated = "\n";
	decorated += name;
	decorated += ": ";
	if( auto found = strstr( headers, decorated.c_str() ) )
	{
		found += decorated.length();
		auto end = strchr( found, '\n' );
		if( end )
		{
			value = std::string( found, end );
			value.append( char( 0 ), size_t( 0 ) );
		}
		else
		{
			value = found;
		}
		return true;
	}
	return false;
}

}

void GetProxySettings( const char* url, CURL* connection )
{
#ifdef _WIN32
	CCP_STATS_ZONE( __FUNCTION__ );

	static bool gotIESettings = false;
	static bool autoProxy = false;
	static std::string explicitProxy;
	static std::string explicitProxyBypass;
	static std::wstring autoConfigUrl;

	if( !gotIESettings )
	{
		GetIEProxySettings( autoProxy, autoConfigUrl, explicitProxy, explicitProxyBypass );
		gotIESettings = true;

		// autoProxy is too slow - disabling until we figure out if it is really needed
		// and we can make it faster somehow
		autoProxy = false;
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
#endif
}

BlueRemoteStream::BlueRemoteStream() :
	m_data( nullptr ),
	m_readLocation( nullptr ),
	m_timeOfLastDataReceived( 0 ),
	m_dataSize( 0 ),
	m_bufferSize( 0 ),
	m_fullHeaderLogging( false )
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

bool BlueRemoteStream::Open( const char* resUrl, size_t expectedSize, const wchar_t* niceName )
{
	CCP_STATS_ZONE( __FUNCTION__ );

#if CCP_STACKLESS
	Ccp::PyAllowThreads allowThreads( true );
#endif

	ScopedThreadStatus threadStatus( IBlueThreadMonitor::BTS_DOWNLOADING );

	if( niceName == nullptr )
	{
		niceName = CA2W( resUrl );
	}

	if( BeResMan->IsOnMainThread() )
	{
		CCP_LOGWARN_CH( s_ch, "BlueRemoteStream::Open( %S ) on main thread", niceName );
	}

	CCP_LOG_CH( s_ch, "Opening %S (%s), expected size %d bytes", niceName, resUrl, expectedSize );

	CURL* connection = PrepareConnection( resUrl );

	if( expectedSize )
	{
		m_data = reinterpret_cast<uint8_t*>( CCP_MALLOC( "BlueRemoteStream/m_data", expectedSize ) );
		m_bufferSize = expectedSize;
	}
	m_headers.clear();

	m_timeOfLastDataReceived = 0;

	CURLcode res;
	
	{
		CCP_STATS_ZONE( CCP_STRINGIZE( __FUNCTION__ ) " curl_easy_perform");
		res = curl_easy_perform( connection );
	}

	if( res != CURLE_OK )
	{
		TrimHeaders();
	}

	if( res == CURLE_PARTIAL_FILE )
	{
		if( expectedSize && m_dataSize == expectedSize )
		{
			res = CURLE_OK;
			double contentLength = -1.0;
			curl_easy_getinfo( connection, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength );
			CCP_LOGWARN_CH( s_ch, "curl_easy_perform() reported partial file download while downloaded size is equal to expected size (%zu), "
				"Content-Length header for the file reported size %f; %s", m_dataSize, contentLength, m_headers.c_str() );
		}
	}

	if( res != CURLE_OK )
	{
		long responseCode = 0;
		curl_easy_getinfo( connection, CURLINFO_RESPONSE_CODE, &responseCode );

		CCP_LOGERR_CH( s_ch, "curl_easy_perform() failed: %s\nHTTP response code: %d\n%s", 
			curl_easy_strerror( res ), 
			responseCode,
			m_headers.c_str() );

		CCP_FREE( m_data );
		m_data = nullptr;
		m_dataSize = 0;
		m_bufferSize = 0;
		m_readLocation = nullptr;
	}
	else
	{
		m_readLocation = m_data;

		GatherStats( connection, niceName, resUrl );
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
	return pThis->ReceiveData( contents, realsize );
}

size_t BlueRemoteStream::WriteHeaderCallback( void* contents, size_t size, size_t nmemb, void* context )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	size_t realsize = size * nmemb;
	BlueRemoteStream* pThis = reinterpret_cast<BlueRemoteStream*>( context );
	pThis->m_headers.append( static_cast<char*>( contents ), realsize );
	return realsize;
}

int BlueRemoteStream::ProgressCallback( void* context, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow )
{
	BlueRemoteStream* pThis = reinterpret_cast<BlueRemoteStream*>(context);
	return (int)pThis->ShouldAbort();
}

size_t BlueRemoteStream::ReceiveData( void* data, size_t size )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	m_timeOfLastDataReceived = CcpGetTimestamp();

	size_t newSize = m_dataSize + size;
	if( newSize > m_bufferSize )
	{
		m_bufferSize = newSize;
		m_data = reinterpret_cast<uint8_t*>( CCP_REALLOC( "BlueRemoteStream", m_data, newSize ) );
	}

	if( m_data )
	{
		memcpy( m_data + m_dataSize, data, size );
		m_dataSize = newSize;
		return size;
	}
	else
	{
		return 0;
	}
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

	if( !m_data )
	{
		return false;
	}

	MD5 checkSum;
	checkSum.update( reinterpret_cast<unsigned char*>( m_data ), (unsigned int)m_dataSize );
	checkSum.finalize();
	char buffer[33];
	const char* checkSumAsHex = checkSum.hex_digest( buffer );
	if( strcmp( expectedChecksum, checkSumAsHex ) != 0 )
	{
		return false;
	}

	return true;
}

void BlueRemoteStream::SetFullHeaderLogging( bool fullHeaders )
{
	m_fullHeaderLogging = fullHeaders;
}

void BlueRemoteStream::TrimHeaders()
{
	if( m_fullHeaderLogging )
	{
		m_headers = "full headers: " + m_headers;
		return;
	}

	// we are only interested in X-Cache header
	std::string xCache;
	if( FindHeader( m_headers.c_str(), "X-Cache", xCache ) )
	{
		m_headers = "X-Cache: " + xCache;
	}
	else
	{
		m_headers = "no X-Cache header found";
	}
}

CURL* BlueRemoteStream::PrepareConnection( const char* resUrl )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	CURL* connection = s_connectionManager.GetConnection();

	curl_easy_setopt( connection, CURLOPT_URL, resUrl );
	curl_easy_setopt( connection, CURLOPT_WRITEFUNCTION, WriteMemoryCallback );
	curl_easy_setopt( connection, CURLOPT_WRITEDATA, (void*)this );
	curl_easy_setopt( connection, CURLOPT_HEADERFUNCTION, WriteHeaderCallback );
	curl_easy_setopt( connection, CURLOPT_HEADERDATA, (void*)this );
	curl_easy_setopt( connection, CURLOPT_XFERINFOFUNCTION, ProgressCallback );
	curl_easy_setopt( connection, CURLOPT_XFERINFODATA, (void*)this );

	GetProxySettings( resUrl, connection );

	return connection;
}

void BlueRemoteStream::GatherStats( CURL* connection, const wchar_t* niceName, const char* resUrl )
{
	double size = 0;
	curl_easy_getinfo( connection, CURLINFO_SIZE_DOWNLOAD, &size );
	CCP_STATS_ADD( remoteStreamBytesDownloaded, size );

	double pretransferTime = 0;
	curl_easy_getinfo( connection, CURLINFO_PRETRANSFER_TIME, &pretransferTime );
	CCP_STATS_ADD( remoteStreamPretransferTime, pretransferTime * 1000000.0 );

	double totalTime = 0;
	curl_easy_getinfo( connection, CURLINFO_TOTAL_TIME, &totalTime );
	CCP_STATS_ADD( remoteStreamDownloadTime, totalTime * 1000000.0 );

	double speed = 0;
	curl_easy_getinfo( connection, CURLINFO_SPEED_DOWNLOAD, &speed );

	if( (g_thresholdForWarningLongDownloadTime > 0.0) && (totalTime > g_thresholdForWarningLongDownloadTime) )
	{
		TrimHeaders();
		CCP_LOGWARN_CH( s_ch, "%S (%s): %g bytes, %g bytes/sec, %g sec pretransfer, %g sec total; %s", 
			niceName, resUrl, size, speed, pretransferTime, totalTime, m_headers.c_str() );
	}
	else
	{
		CCP_LOG_CH( s_ch, "%S (%s): %g bytes, %g bytes/sec, %g sec pretransfer, %g sec total", 
			niceName, resUrl, size, speed, pretransferTime, totalTime );
	}
}

bool BlueRemoteStream::ShouldAbort()
{
	if( !m_timeOfLastDataReceived )
	{
		return false;
	}

	uint64_t now = CcpGetTimestamp();
	uint64_t delta = now - m_timeOfLastDataReceived;
	double deltaInSeconds = (double)delta / (double)CcpGetTimestampFrequency();

	if( deltaInSeconds > g_thresholdForAbortingLongDownloadTime )
	{
		return true;
	}

	return false;
}
