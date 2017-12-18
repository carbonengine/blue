////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//

#pragma once

#ifndef BlueFileStream_h
#define BlueFileStream_h

#include "Include/IBluePersist.h"

BLUE_DECLARE( BlueFileStream );

BLUE_CLASS( BlueFileStream ) :
	public IBlueStream
{
public:
	EXPOSE_TO_BLUE();

	BlueFileStream();
	~BlueFileStream();

	bool Open( const wchar_t* filename, CcpOpenMode mode, CcpShareMode shareMode );
	bool Create( const wchar_t* filename );
	void Close();

	Be::Result<std::string> ReadEntireFile( const wchar_t* filename, std::string& contents );
	Be::Result<std::string> ReadEntireFileWithYield( const wchar_t* filename, std::string& contents );

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
	// File descriptor
	int m_fileDescriptor;

	// Pointer to data block in memory (after calling LockData)
	void* m_data;

	// Size of the data in m_data
	size_t m_dataSize;
};


TYPEDEF_BLUECLASS( BlueFileStream );

#endif