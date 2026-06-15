// Copyright © 2011 CCP ehf.

#pragma once

#ifndef BlueResFile2_h
#define BlueResFile2_h

#include "Blue.h"
#include "IBluePersist.h"
#include "ICacheable.h"

BLUE_DECLARE( ResFile );

BLUE_CLASS( ResFile ) :
	public IResFile
{
public:
	EXPOSE_TO_BLUE();

	// This is to support existing Python code - use BluePaths::FileExists
	bool FileExists( const wchar_t* filename );

	/////////////////////////////////////////
	// IResFile interface
	bool Open( const char* filename, bool readOnly ) override;
	bool Close() override;
	bool OpenW( const wchar_t* filename, bool readOnly ) override;
	bool CreateW( const wchar_t* filename ) override;
	bool FileExistsW( const wchar_t* filename ) override;
	bool Preload(bool &) override;
	bool PreloadInProgress() override;

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
	IBlueStreamPtr m_stream;
};

TYPEDEF_BLUECLASS( ResFile );

#endif
