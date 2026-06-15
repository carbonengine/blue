// Copyright © 2011 CCP ehf.

#pragma once

#ifndef BlueRemoteStream_h
#define BlueRemoteStream_h

#include "IBluePersist.h"
#include "curl/curl.h"

BLUE_DECLARE( BlueRemoteStream );

BLUE_CLASS( BlueRemoteStream ) :
public IBlueStream
{
public:
	EXPOSE_TO_BLUE();

	BlueRemoteStream();
	~BlueRemoteStream();

	enum OpenMode
	{
		OM_READWRITE,
		OM_READONLY
	};

	bool Open( const char* filename, size_t expectedSize, const wchar_t* niceName = nullptr );

	bool VerifyContents( const char* expectedChecksum );

	/////////////////////////////////////////
	// IBlueStream interface
	ptrdiff_t Read( void* dest, ptrdiff_t count ) override;
	ptrdiff_t Write( const void* source, size_t count	) override;
	ptrdiff_t Seek( ptrdiff_t distance, SeekOrigin method	) override;
	bool SetSize( size_t newsize );
	ssize_t CopyFrom( IBlueStream* source, size_t count	);
	ptrdiff_t GetPosition() override;
	ptrdiff_t GetSize() override;
	bool LockData( void** data,	size_t size	) override;
	bool UnlockData() override;

	void SetFullHeaderLogging( bool fullHeaders );

private:
	static size_t WriteMemoryCallback( void* contents, size_t size, size_t nmemb, void* context );
	static size_t WriteHeaderCallback( void* contents, size_t size, size_t nmemb, void* context );
	static int ProgressCallback( void* context, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow );

	CURL* PrepareConnection( const char* resUrl );
	void GatherStats( CURL* connection, const wchar_t* niceName, const char* resUrl );

	size_t ReceiveData( void* data, size_t size );
	void InitializeCurl();
	void TrimHeaders();
	bool ShouldAbort();

private:
	// Pointer to data block in memory
	uint8_t* m_data;
	uint8_t* m_readLocation;

	uint64_t m_timeOfLastDataReceived;

	// Size of the data in m_data
	size_t m_dataSize;

	// Allocation size of m_data
	size_t m_bufferSize;

	// Headers received
	std::string m_headers;

	// Log full headers on error/warning
	bool m_fullHeaderLogging;

};


TYPEDEF_BLUECLASS( BlueRemoteStream );


void GetProxySettings( const char* url, CURL* connection );


#endif
