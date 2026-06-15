// Copyright © 2011 CCP ehf.

#pragma once

#ifndef BlueFileStream_h
#define BlueFileStream_h

#include "IBluePersist.h"

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
	ptrdiff_t Read( void* dest, ptrdiff_t count ) override;
	ptrdiff_t Write( const void* source, size_t count	) override;
	ptrdiff_t Seek( ptrdiff_t distance, SeekOrigin method	) override;
	bool SetSize( size_t newsize );
	ssize_t CopyFrom( IBlueStream* source, size_t count	);
	ptrdiff_t GetPosition() override;
	ptrdiff_t GetSize() override;
	bool LockData( void** data,	size_t size	) override;
	bool UnlockData() override;

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
