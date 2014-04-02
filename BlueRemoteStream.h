////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//

#pragma once

#ifndef BlueRemoteStream_h
#define BlueRemoteStream_h

#if USE_RESFILE_2

#include "include/IBluePersist.h"
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

	bool Open( const char* filename );
	bool VerifyContents( const char* expectedChecksum );

	/////////////////////////////////////////
	// IBlueStream interface
	ptrdiff_t Read( void* dest, ptrdiff_t count );
	ptrdiff_t Write( const void* source, size_t count	);
	ptrdiff_t Seek( ptrdiff_t distance, SeekOrigin method	);
	bool SetSize( size_t newsize );
	ssize_t CopyFrom( IBlueStream* source, size_t count	);
	ptrdiff_t GetPosition();
	ptrdiff_t GetSize();
	bool LockData( void** data,	size_t size	);
	bool UnlockData();

private:
	static size_t WriteMemoryCallback( void* contents, size_t size, size_t nmemb, void* context );
	void ReceiveData( void* data, size_t size );
	void InitializeCurl();

private:
	CURL* m_curl;

	// Pointer to data block in memory
	uint8_t* m_data;
	uint8_t* m_readLocation;

	// Size of the data in m_data
	size_t m_dataSize;
};


TYPEDEF_BLUECLASS( BlueRemoteStream );

#endif
#endif
