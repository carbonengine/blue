////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//

#pragma once

#ifndef BlueStuffFileStream_h
#define BlueStuffFileStream_h

#include "include\IBluePersist.h"

BLUE_DECLARE(  BlueStuffFileStream );

class BlueStuffFileStream :
	public IBlueStream
{
public:
	EXPOSE_TO_BLUE();

	BlueStuffFileStream();
	~BlueStuffFileStream();

	void SetHandle( HANDLE handle, size_t offset, size_t size );

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

	void ClearLockedData();

	bool UnlockData();

private:
	// File handle to the stuff file
	HANDLE m_fileHandle;

	// Offset within the stuff file
	size_t m_offset;

	// Pointer to data block in memory (after calling LockData)
	void* m_data;

	// Size of the data in the stuff file
	size_t m_dataSize;
};


TYPEDEF_BLUECLASS( BlueStuffFileStream );

#endif