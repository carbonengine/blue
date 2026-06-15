// Copyright © 2015 CCP ehf.

#pragma once
#ifndef BlueHttpStream_H
#define BlueHttpStream_H

#include "IBluePersist.h"
#include "curl/curl.h"


BLUE_CLASS( BlueNetworkStream ) :
	public IBlueStream
{
public:
	EXPOSE_TO_BLUE();

	BlueNetworkStream();
	~BlueNetworkStream();

	BlueStdResult Open( const char* url );
	void Close();
	BlueStdResult ReadData( Be::Optional<size_t> size, PyObject*& contents );
	BlueStdResult Tell( ptrdiff_t& position );

	/////////////////////////////////////////
	// IBlueStream interface
	ptrdiff_t Read( void* dest, ptrdiff_t count ) override;
	ptrdiff_t Write( const void* source, size_t count	) override;
	ptrdiff_t Seek( ptrdiff_t distance, SeekOrigin method	) override;
	ptrdiff_t GetPosition() override;
	ptrdiff_t GetSize() override;
	bool LockData( void** data,	size_t size	) override;
	bool UnlockData() override;
private:
	void PerformTransfer();
	static void PerformTransferHelper( void* context );
	static size_t WriteMemoryCallback( void* contents, size_t size, size_t nmemb, void* context );
	static size_t WriteHeaderCallback( void* contents, size_t size, size_t nmemb, void* context );
#if LIBCURL_VERSION_MAJOR > 7 || LIBCURL_VERSION_MINOR >= 32
	static int ProgressCallback( void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow );
#else
	static int ProgressCallback( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow );
#endif
	void ReceiveData( const void* data, size_t size );
	void ReceiveHeaders( const void* data, size_t size );

	struct Block
	{
		uint8_t* data;
	};
	static const size_t BLOCK_SIZE = CURL_MAX_WRITE_SIZE;
	enum State
	{
		UNINITIALIZED,
		OPENED,
		FINISHED,
	};

	std::string m_url;
	size_t m_blockReadPosition;
	size_t m_blockWritePosition;
	TrackableStdVector<Block> m_data;
	ptrdiff_t m_readPosition;
	size_t m_size;
	size_t m_receivedSize;
	CcpSemaphore m_sizeAvailable;
	CcpMutex m_dataMutex;
	CcpSemaphore m_dataAvailable;
	CcpAtomic<uint32_t> m_backgroundCallId;

	CcpAtomic<uint32_t> m_state;
	std::string m_headers;
	CURLcode m_transferResult;
};

TYPEDEF_BLUECLASS( BlueNetworkStream );

#endif
