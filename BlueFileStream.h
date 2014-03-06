////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//

#pragma once

#ifndef BlueFileStream_h
#define BlueFileStream_h

#include "include/IBluePersist.h"

BLUE_DECLARE( BlueFileStream );

BLUE_CLASS( BlueFileStream ) :
	public IBlueStream
{
public:
	EXPOSE_TO_BLUE();

	BlueFileStream();
	~BlueFileStream();

	enum OpenMode
	{
		OM_READWRITE,
		OM_READONLY
	};

	enum ShareMode
	{
		SM_NOSHARING,
		SM_READSHARING,
		SM_RWSHARING
	};

	bool Open( const wchar_t* filename, OpenMode mode, ShareMode shareMode );
	bool Create( const wchar_t* filename );
	void Close();

	Be::Result<std::string> ReadEntireFile( const wchar_t* filename, std::string& contents );

	/////////////////////////////////////////
	// IBlueStream interface
	ssize_t Read( void* dest, ssize_t count );
	ssize_t Write( const void* source, size_t count	);
	ssize_t Seek( ssize_t distance, BLUESEEK method	);
	bool SetSize( size_t newsize );
	ssize_t CopyFrom( IBlueStream* source, size_t count	);
	ssize_t GetPosition();
	ssize_t GetSize();
	bool LockData( void** data,	size_t size	);
	bool UnlockData();

private:
	// File descriptor
	int m_fileDescriptor;

	// Pointer to data block in memory (after calling LockData)
	void* m_data;

	// Size of the data in m_data
	size_t m_dataSize;
};


TYPEDEF_BLUECLASS( BlueFileStream );

#endif