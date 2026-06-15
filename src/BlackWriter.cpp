// Copyright © 2012 CCP ehf.

#include "StdAfx.h"
#include "BlackWriter.h"
#include "BlueMemStream.h"


BlackWriter::BlackWriter()
{

}

BlackWriter::~BlackWriter()
{

}

Be::Result<std::string> BlackWriter::WriteObjectToStream( const IRoot* root, IBlueStream* stream )
{
	// Strings are not cleared at the end so they can retrieved from the writer in Python after
	// writing an object. Clear them here to make sure we only get strings from this object.
	m_strings.clear();
	m_wstrings.clear();

	MemStreamPtr dataStream;
	dataStream.CreateInstance();

	m_outputStream = dataStream;
	WriteIRoot( root );

	// Write regular string table to a separate stream
	MemStreamPtr stringTableStream;
	stringTableStream.CreateInstance();

	m_outputStream = stringTableStream;

	int16_t stringEntryCount = (int16_t)m_strings.size();
	WriteInt16( stringEntryCount );

	for( auto it = m_strings.begin(); it != m_strings.end(); ++it )
	{
		const std::string& s = *it;
		size_t len = strlen( s.c_str() );
		m_outputStream->Write( s.c_str(), sizeof( char ) * len );
		
		char terminator = 0;
		m_outputStream->Write( &terminator, sizeof( terminator) );
	}

	// Write wide string table to a separate stream
	MemStreamPtr wstringTableStream;
	wstringTableStream.CreateInstance();

	m_outputStream = wstringTableStream;

	int16_t wstringEntryCount = (int16_t)m_wstrings.size();
	WriteInt16( wstringEntryCount );

	for( auto it = m_wstrings.begin(); it != m_wstrings.end(); ++it )
	{
		const std::wstring& s = *it;
		size_t len = wcslen( s.c_str() );
		m_outputStream->Write( s.c_str(), sizeof( wchar_t ) * len );

		wchar_t terminator = 0;
		m_outputStream->Write( &terminator, sizeof( terminator) );
	}

	// Switch back to the main stream
	m_outputStream = stream;
	
	int32_t magicValue = 0xb1acf11e;
	WriteInt32( magicValue );

	int32_t version = GetCurrentVersion();
	WriteInt32( version );

	
	// Dump the regular string table to the main stream
	int32_t stringTableDataSize = (int32_t)stringTableStream->GetSize();
	WriteInt32( stringTableDataSize );

	void* data = nullptr;
	
	stringTableStream->LockData( &data, stringTableDataSize );
	m_outputStream->Write( data, stringTableDataSize );
	stringTableStream->UnlockData();

	
	// Dump the wide string table to the main stream
	int32_t wstringTableDataSize = (int32_t)wstringTableStream->GetSize();
	WriteInt32( wstringTableDataSize );

	wstringTableStream->LockData( &data, wstringTableDataSize );
	m_outputStream->Write( data, wstringTableDataSize );
	wstringTableStream->UnlockData();

	
	// Dump the data stream to the main stream
	int32_t dataStreamSize = (int32_t)dataStream->GetSize();

	dataStream->LockData( &data, dataStreamSize );
	m_outputStream->Write( data, dataStreamSize );
	dataStream->UnlockData();

	// Clear all state so we can reuse the same writer for writing other objects
	m_outputStream.Unlock();
	m_stringMap.clear();
	m_wstringMap.clear();
	m_referenceMap.clear();

	Be::Result<std::string> result;
	return result;
}

void BlackWriter::WriteMemberName( const char* key )
{
	uint16_t ix = GetStringIndex( key );
	m_outputStream->Write( &ix, sizeof( ix ) );
}

void BlackWriter::WriteInt8( int8_t value )
{
	m_outputStream->Write( &value, sizeof( value ) );
}

void BlackWriter::WriteInt16( int16_t value )
{
	m_outputStream->Write( &value, sizeof( value ) );
}

void BlackWriter::WriteInt32( int32_t value )
{
	m_outputStream->Write( &value, sizeof( value ) );
}

void BlackWriter::WriteUInt32( uint32_t value )
{
	m_outputStream->Write( &value, sizeof( value ) );
}

void BlackWriter::WriteInt64( int64_t value )
{
	m_outputStream->Write( &value, sizeof( value ) );
}

void BlackWriter::WriteUInt64( uint64_t value )
{
	m_outputStream->Write( &value, sizeof( value ) );
}

void BlackWriter::WriteFloat( float value )
{
	m_outputStream->Write( &value, sizeof( value ) );
}

void BlackWriter::WriteFloatArray( float* values, size_t numValues )
{
	m_outputStream->Write( values, sizeof( float ) * numValues );
}

void BlackWriter::WriteFloatMatrix( float* values, size_t numRows, size_t numColumns )
{
	m_outputStream->Write( values, sizeof( float ) * numRows * numColumns );
}

void BlackWriter::WriteDouble( double value )
{
	m_outputStream->Write( &value, sizeof( value ) );
}

void BlackWriter::WriteWChar( const wchar_t* value )
{
	uint16_t ix = GetWStringIndex( value );
	m_outputStream->Write( &ix, sizeof( ix ) );
}

void BlackWriter::WriteChar( const char* value )
{
	uint16_t ix = GetStringIndex( value );
	m_outputStream->Write( &ix, sizeof( ix ) );
}

void BlackWriter::WriteIRoot( const IRoot& instance, IRoot* defaultInstance )
{
	// Object is written out as follows:
	// type name, as an index into the string table
	// size of data for object
	// data for object

	MemStreamPtr dataStream;
	dataStream.CreateInstance();

	IBlueStreamPtr prevStream = m_outputStream;
	m_outputStream = dataStream;

	const char* typeName = instance.ClassType()->mClassId->GetName();
	uint16_t typeNameIx = GetStringIndex( typeName );
	WriteInt16( typeNameIx );

	WriteMembers( instance, defaultInstance );

	m_outputStream = prevStream;
	
	void* data;
	int dataStreamSize = (int)dataStream->GetSize();
	WriteInt32( dataStreamSize );

	dataStream->LockData( &data, dataStreamSize );
	m_outputStream->Write( data, dataStreamSize );
	dataStream->UnlockData();
}

void BlackWriter::WriteIRoot( const IRoot* instance )
{
	if( instance == nullptr )
	{
		int32_t zero = 0;
		m_outputStream->Write( &zero, sizeof( zero ) );
		return;
	}

	IRootPtr theRawRoot;
	const_cast<IRoot*>( instance )->QueryInterface( GetIRootIID(), (void**)&theRawRoot );

	auto foundIt = m_referenceMap.find( theRawRoot );
	if( foundIt != m_referenceMap.end() )
	{
		// We've already written this class, use alias.
		int referenceIx = foundIt->second;
		m_outputStream->Write( &referenceIx, sizeof( referenceIx ) );
	}
	else
	{
		// First time we see this object - store it and add it to the reference map
		int referenceIx = (int)m_referenceMap.size() + 1;
		m_referenceMap[theRawRoot] = referenceIx;

		m_outputStream->Write( &referenceIx, sizeof( referenceIx ) );

		WriteIRoot( *theRawRoot, nullptr );
	}

}

void BlackWriter::WriteVectorBegin( size_t size )
{
	WriteInt32( (int)size );
}

void BlackWriter::WriteVectorEnd( size_t size )
{
}

#if BLUE_WITH_PYTHON

void BlackWriter::PatchStringsInStructureList( IBlueStructureList* structureList, void* listData )
{
	// Patch all shared string (Be::DT_SHAREDSTRING) occurences in the list
	// data with indexes into file's string table.
	BlueStructureDefinition* memberDef = structureList->GetStructureDefinition();
	while( memberDef->m_name )
	{
		int type = memberDef->m_dataType & Be::DT_TYPE_MASK;
		if( type == Be::DT_SHAREDSTRING )
		{
			int size = ( ( memberDef->m_dataType & Be::DT_SIZE_MASK ) >> Be::DT_SIZE_OFFSET ) + 1;
			for( size_t i = 0; i < structureList->GetSize(); ++i )
			{
				for( int j = 0; j < size; ++j )
				{
					uint8_t* stringAddress = reinterpret_cast<uint8_t*>( listData ) + 
						structureList->GetStructureSize() * i + memberDef->m_offset + sizeof( BlueSharedString ) * j;
					const BlueSharedString* src = reinterpret_cast<const BlueSharedString*>( stringAddress );
					uint16_t ix = GetStringIndex( src->c_str() );
					// clear memory used by the string so that we don't get garbage in the file
					memset( stringAddress, 0, sizeof( BlueSharedString ) );
					*reinterpret_cast<uint16_t*>( stringAddress ) = ix;
				}
			}
		}
		++memberDef;
	}
}

void BlackWriter::WriteStructureList( IBlueStructureList* structureList )
{
	// We don't bother with writing the structure definition, just write out
	// the structure size so we can ensure we don't write out of bounds when
	// reading it back in.

	WriteInt32( (int32_t)structureList->GetSize() );
	WriteInt16( (int16_t)structureList->GetStructureSize() );

	auto dataSize = structureList->GetSize() * structureList->GetStructureSize();
	if( dataSize )
	{
		CcpMallocBuffer data( "BlackWriter::WriteStructureList", dataSize );
		memcpy( data.get(), structureList->GetAt( 0 ), dataSize );
		PatchStringsInStructureList( structureList, data.get() );

		m_outputStream->Write( data.get(), dataSize );
	}
}
#endif

void BlackWriter::WriteBinaryBlock( ICustomPersist* cPersist, const char* propertyName )
{
	unsigned char* buffer = NULL;
	size_t bufferSize = 0;
	cPersist->GetWriteBufferAndSize( propertyName, &buffer, &bufferSize );
	WriteMemberName( propertyName );
	WriteInt32( (int32_t)bufferSize );
	m_outputStream->Write( buffer, bufferSize );
	cPersist->ReleaseWriteBuffer( buffer );
}

uint16_t BlackWriter::GetStringIndex( const char* s )
{
	auto foundIt = m_stringMap.find( s );
	if( foundIt != m_stringMap.end() )
	{
		return foundIt->second;
	}
	
	// First time we see this string - add it
	uint16_t ix = (uint16_t)m_stringMap.size();
	m_stringMap[s] = ix;

	m_strings.push_back( s );

	return ix;
}

uint16_t BlackWriter::GetWStringIndex( const wchar_t* s )
{
	auto foundIt = m_wstringMap.find( s );
	if( foundIt != m_wstringMap.end() )
	{
		return foundIt->second;
	}

	// First time we see this string - add it
	uint16_t ix = (uint16_t)m_wstringMap.size();
	m_wstringMap[s] = ix;

	m_wstrings.push_back( s );

	return ix;
}

uint32_t BlackWriter::GetCurrentVersion() const
{
	return 1;
}
