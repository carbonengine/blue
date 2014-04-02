////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		July 2011
// Copyright:	CCP 2011
//

#pragma once

#ifndef BlueResFile2_h
#define BlueResFile2_h

#if USE_RESFILE_2
#define RESFILE_2_CLASSNAME ResFile
#else
#define RESFILE_2_CLASSNAME BlueResFile2
#endif

#include "include/Blue.h"
#include "include/IBluePersist.h"
#include "include/ICacheable.h"

BLUE_DECLARE( RESFILE_2_CLASSNAME );

BLUE_CLASS( RESFILE_2_CLASSNAME ) :
	public IResFile
{
public:
	EXPOSE_TO_BLUE();

	// This is to support existing Python code - use BluePaths::FileExists
	bool FileExists( const wchar_t* filename );

	/////////////////////////////////////////
	// IResFile interface
	bool Open( const char* filename, bool readOnly );
	bool Close();
	bool OpenW( const wchar_t* filename, bool readOnly );
	bool CreateW( const wchar_t* filename );
	bool FileExistsW( const wchar_t* filename );
	bool Preload(bool &);
	bool PreloadInProgress();

	/////////////////////////////////////////
	// IBlueStream interface
	ptrdiff_t Read( void* dest, ptrdiff_t count );
	ptrdiff_t Write( const void* source, size_t count	);
	ptrdiff_t Seek( ptrdiff_t distance, SeekOrigin method	);
	ptrdiff_t GetPosition();
	ptrdiff_t GetSize();
	bool LockData( void** data,	size_t size	);
	bool UnlockData();

private:
	IBlueStreamPtr m_stream;
};

#if USE_RESFILE_2
TYPEDEF_BLUECLASS( ResFile );
#else
TYPEDEF_BLUECLASS( BlueResFile2 );
#endif

#endif
