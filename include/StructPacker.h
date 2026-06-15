// Copyright © 2011 CCP ehf.

#pragma once
#ifndef StructPacker_H
#define StructPacker_H

#include "BitPacker.h"
#include <wchar.h>

/*
 
Usage:

SP_START( StructName )

	SP_BLOB( type, name ); // opaque object, will be included but not packed

	SP_CHAR( name, length );
	SP_WCHAR( name, length );

	SP_TYPE( type, name ); // any type supported by BitPackerCore

	SP_FIXEDFLOAT( name, ^2 places );
	SP_FIXEDDOUBLE( name, ^2 places );

SP_END;

*/

#define SP_START( N ) \
struct N \
{ \
public: \
	bool Unpack( BitPackerCore &packer ) { return Pack(packer, false); } \
	bool Unpack( const char *data, unsigned int len ) { BitPacker packer(data,len); return Pack(packer, false); } \
	bool Pack( BitPackerCore &packer, const bool pack =true ) \
	{ \

#define SP_BLOB( T, N ) \
		return N##_func( packer, pack ); \
	} \
	T N; \
	bool N##_func( BitPackerCore &packer, const bool pack ) \
	{ \
		if ( pack ) packer.QueueBits((char*)&N, sizeof(T)*8); else packer.DeQueueBits((char*)&N, sizeof(T)*8); \


#define SP_TYPE( T, Y ) \
		return Y##_func( packer, pack ); \
	} \
	T Y; \
	bool Y##_func( BitPackerCore &packer, const bool pack ) \
	{ \
		if ( pack ) packer.Pack(Y); else packer.Unpack(Y); \


#define SP_CHAR( C, L ) \
		return C##_func( packer, pack ); \
	} \
	char C[L+1]; \
	bool C##_func( BitPackerCore &packer, const bool pack ) \
	{ \
		if ( pack ) packer.Pack(C); else packer.Unpack(C, 0, L); \


#define SP_WCHAR( C, L ) \
		return C##_func( packer, pack ); \
	} \
	wchar_t C[L+1]; \
	bool C##_func( BitPackerCore &packer, const bool pack ) \
	{ \
		if ( pack ) packer.Pack(C); else packer.Unpack(C, 0, L); \


#define SP_FIXEDFLOAT( F, PLACES ) \
		return F##_func( packer, pack ); \
	} \
	float F; \
	bool F##_func( BitPackerCore &packer, const bool pack ) \
	{ \
		if ( pack ) packer.Pack((F),(PLACES)); else packer.Unpack((F),(PLACES)); \


#define SP_FIXEDDOUBLE( D, PLACES ) \
		return D##_func( packer, pack ); \
	} \
	double D; \
	bool D##_func( BitPackerCore &packer, const bool pack ) \
	{ \
		if ( pack ) packer.Pack((D),(PLACES)); else packer.Unpack((D),(PLACES)); \


#define SP_END \
		return packer.Valid(); \
	} \
}; \


#endif
