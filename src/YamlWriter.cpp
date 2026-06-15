// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "YamlWriter.h"
#include "IBluePersist.h"
#include "Base64.h"
#include "IBlueObjectMetadata.h"
#include "BlueMemStream.h"
#include <yaml.h>
#ifndef _WIN32
#include <locale>
#include <codecvt>
#endif

YamlWriter::YamlWriter()
{
	Cleanup();
}

YamlWriter::~YamlWriter()
{
	Cleanup();
}

int YamlWriter::WriteToStreamStatic( void *data, unsigned char *buffer, size_t size )
{
    return static_cast<YamlWriter*>( data )->WriteToStream( buffer, size );
}

int YamlWriter::WriteToStream( unsigned char *buffer, size_t size )
{
    ssize_t written = m_dataStream->Write( buffer, size );
    return (written == size);
}

Be::Result<std::string> YamlWriter::WriteObjectToStream( const IRoot* root, IBlueStream* stream )
{
	Cleanup();

    m_dataStream = stream;

	// Wrapper events to properly enclose the YAML document:
	yaml_stream_start_event_initialize( AddEvent(), YAML_UTF8_ENCODING );
	yaml_document_start_event_initialize( AddEvent(), NULL, NULL, NULL, 1 );

	WriteIRoot( *root, NULL );

	// Terminate YAML document properly
	yaml_document_end_event_initialize( AddEvent(), 1 );
	yaml_stream_end_event_initialize( AddEvent() );

    yaml_emitter_t emitter;
	int res = yaml_emitter_initialize( &emitter );
	CCP_ASSERT( res );
	CCP_UNUSED( res );

	int canonical = 0;
    if (canonical) {
        yaml_emitter_set_canonical(&emitter, 1);
    }

	int unicode = 0;
    if (unicode) {
        yaml_emitter_set_unicode(&emitter, 1);
    }

	yaml_emitter_set_indent( &emitter, 4 );

	yaml_emitter_set_output( &emitter, WriteToStreamStatic, (void*)this );

	for( EventArray_t::iterator it = m_events.begin(); it != m_events.end(); ++it )
	{
		yaml_event_t* event = *it;
		res = yaml_emitter_emit( &emitter, event );
		if( !res )
		{
			CCP_LOGERR( emitter.problem );
			yaml_emitter_flush( &emitter );
		}
	}
	yaml_emitter_flush( &emitter );
	// This cleans up all memory allocated by *_event_initialize calls because the
	// emitter takes ownership of the events passed to it (by emitter_emit calls)
	yaml_emitter_delete( &emitter );

	Cleanup();

	return Be::Result<std::string>();
}


Be::Result<std::string> YamlWriter::WriteObjectToString( const IRoot* root, std::string& output )
{
	MemStreamPtr ms;
	ms.CreateInstance();

	WriteObjectToStream( root, ms );

	ssize_t size = ms->GetSize();
	void* data;
	ms->LockData( &data, size );

	output.assign( (char*)data, size );
	ms->UnlockData();

	return Be::Result<std::string>();
}

// Floating point numbers are written out with the %f format specifier as using
// the e notation causes issues with the yaml module in Python. There is no format
// specifier that allows to specify a maximum number of digits without also going
// to the e notation. So, we use f and trim the trailing zeroes.
static void TrimTrailingZeroes( char * buffer )
{
	char* decimalPoint = nullptr;
	char* p;
	for( p = buffer; *p; ++p )
	{
		if( *p == '.' )
		{
			decimalPoint = p;
		}
	}

	// p is now at the zero terminator
	if( decimalPoint )
	{
		--p;
		while( p > decimalPoint + 1 )
		{
			if( *p == '0' )
			{
				*p = 0;
				--p;
			}
			else
			{
				break;
			}
		}
	}
}

void YamlWriter::WriteFloat( float value )
{
	char buffer[ 64 ];
	sprintf_s( buffer, "%.7f", value );
	
	TrimTrailingZeroes( buffer );

	AddScalarEvent( buffer );
}

void YamlWriter::WriteBinaryBlock( ICustomPersist* cPersist, const char* propertyName )
{
	unsigned char* buffer = NULL;
	size_t bufferSize = 0;
	cPersist->GetWriteBufferAndSize( propertyName, &buffer, &bufferSize );
	if( bufferSize == 0 )
	{
		return;
	}
	WriteMemberName(propertyName);

	char* outBuffer = NULL;
	size_t nLenOut = bufferSize*2;
	// the outputted size is roughly 1.37 the original size
	// bytes = (string_length(encoded_string) - 814) / 1.37
	// We just play it safe and make it 2x the size.
	outBuffer = (char*)CCP_MALLOC("yaml_writer_binary_block", nLenOut);
	nLenOut = ToBase64( (const uint8_t*)buffer, bufferSize, outBuffer, nLenOut );
	if(nLenOut > 0)
	{
		int res = yaml_scalar_event_initialize( AddEvent(), 
			NULL, 
			(yaml_char_t*)"!!binary",
			(yaml_char_t*)outBuffer, static_cast<int>( nLenOut ), 1, 0, 
			YAML_ANY_SCALAR_STYLE );
		CCP_ASSERT( res );
        CCP_UNUSED( res );
	}
	CCP_FREE(outBuffer);	
	cPersist->ReleaseWriteBuffer( buffer );
}

void YamlWriter::WriteDouble( double value )
{
    char buffer[ 64 ];
    sprintf_s( buffer, "%.12f", value );

	TrimTrailingZeroes( buffer );

    AddScalarEvent( buffer );
}

void YamlWriter::WriteInt8( int8_t value )
{
	WriteInt32( value );
}

void YamlWriter::WriteInt16( int16_t value )
{
	WriteInt32( value );
}

void YamlWriter::WriteInt32( int32_t value )
{
	char buffer[ 32 ];
	sprintf_s( buffer, "%d", value );
	AddScalarEvent( buffer );
}

void YamlWriter::WriteUInt32( uint32_t value )
{
	char buffer[32];
	sprintf_s( buffer, "%u", value );
	AddScalarEvent( buffer );
}

void YamlWriter::WriteInt64( int64_t value )
{
	char buffer[ 32 ];
#ifdef _MSC_VER
	sprintf_s( buffer, "%I64d", value );
#elif defined(__ANDROID__)
	sprintf_s( buffer, "%lld", value );
#else
	sprintf_s( buffer, "%" PRId64, value );
#endif
	AddScalarEvent( buffer );
}

void YamlWriter::WriteUInt64( uint64_t value )
{
	char buffer[32];
#ifdef _MSC_VER
	sprintf_s( buffer, "%I64u", value );
#elif defined( __ANDROID__ )
	sprintf_s( buffer, "%llu", value );
#else
	sprintf_s( buffer, "%" PRIu64, value );
#endif
	AddScalarEvent( buffer );
}

void YamlWriter::WriteWChar( const wchar_t* value )
{
	if( value )
	{
#ifdef _WIN32
		int requiredSize = WideCharToMultiByte( CP_UTF8, 0, value, -1, NULL, 0, NULL, NULL );
		char* buffer = CCP_NEW("YamlWriter/MultiByte") char[ requiredSize ];
		WideCharToMultiByte( CP_UTF8, 0, value, -1, buffer, requiredSize, NULL, NULL );
		AddScalarEvent( buffer, YAML_DOUBLE_QUOTED_SCALAR_STYLE );
		CCP_DELETE [] buffer;
#else
        auto str = WideToUTF8( value );
        AddScalarEvent( str.c_str(), YAML_DOUBLE_QUOTED_SCALAR_STYLE );
#endif
	}
	else
	{
		AddScalarEvent( "null" );
	}
}

void YamlWriter::WriteChar( const char* value )
{
	AddScalarEvent( value, YAML_DOUBLE_QUOTED_SCALAR_STYLE );
}

void YamlWriter::WriteVectorBegin( size_t size )
{
	yaml_event_t* evt = AddEvent();
	yaml_sequence_start_event_initialize( evt, NULL, NULL, 1, YAML_ANY_SEQUENCE_STYLE );
}

void YamlWriter::WriteVectorEnd( size_t size )
{
	yaml_event_t* evt = AddEvent();
	yaml_sequence_end_event_initialize( evt );
}

void YamlWriter::Cleanup()
{
    m_dataStream = (IBlueStream*)NULL;
	m_anchorNumber = 0;
	for( auto it = m_events.begin(); it != m_events.end(); ++it )
	{
		delete (*it);
	}
	m_events.clear();
	m_classEventMap.clear();
}

yaml_event_t* YamlWriter::AddEvent()
{
	yaml_event_t* event = new yaml_event_t;
	m_events.push_back( event );
	return event;
}

void YamlWriter::AddScalarEvent( const char* value, yaml_scalar_style_t style )
{
	int quoted_implicit = 0;
	if( style == YAML_DOUBLE_QUOTED_SCALAR_STYLE || style == YAML_SINGLE_QUOTED_SCALAR_STYLE )
	{
		quoted_implicit = 1;
	}
	int res = yaml_scalar_event_initialize( AddEvent(), NULL, NULL, (yaml_char_t*)value, -1, 1, quoted_implicit, style );
	CCP_ASSERT( res );
	CCP_UNUSED( res );
}

yaml_event_t* YamlWriter::AddMappingStartEvent()
{
	yaml_event_t* event = AddEvent();
	yaml_mapping_start_event_initialize( event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE ); 
	return event;
}

void YamlWriter::WriteIRoot( const IRoot& instance, IRoot* defaultInstance )
{
	m_classEventMap[ &instance ] = AddMappingStartEvent();
	
	AddScalarEvent( "type" );
	AddScalarEvent( instance.ClassType()->mClassId->GetName() );

	if( IWeakObjectPtr weak = BlueCastPtr( const_cast<IRoot*>( &instance ) ) )
	{
		if( auto metadata = BeObjectMetadata->GetMetadata( weak ) )
		{
			AddScalarEvent( BLUE_OBJECT_METADATA_KEY );
			AddMappingStartEvent();
			for( auto it = metadata->begin(); it != metadata->end(); ++it )
			{
				AddScalarEvent( it->first.c_str(), YAML_DOUBLE_QUOTED_SCALAR_STYLE );
				AddScalarEvent( it->second.c_str(), YAML_DOUBLE_QUOTED_SCALAR_STYLE );
			}
			AddMappingEndEvent();
		}
	}
    WriteMembers( instance, defaultInstance );

    AddMappingEndEvent();
}

extern "C" yaml_char_t* yaml_strdup(const yaml_char_t *);
void YamlWriter::WriteIRoot( const IRoot* instance )
{	
	// Note that we never add NULL to the class-anchor map because that would be just silly.
	if( instance == NULL )
	{
		AddScalarEvent( "null" );
		return;
	}

	IRootPtr theRawRoot;
	const_cast<IRoot*>( instance )->QueryInterface( GetIRootIID(), (void**)&theRawRoot );

	ClassEventMap_t::iterator it = m_classEventMap.find( theRawRoot );
	if( it != m_classEventMap.end() )
	{
		// We've already written this class, use alias.
		// First determine if we have an anchor already:
		yaml_event_t* event = (*it).second;
		if( event->data.mapping_start.anchor == NULL )
		{
			// Create anchor
			char buffer[ 32 ];
			sprintf_s( buffer, "ID%u", m_anchorNumber++ );
			// The memory allocated by strdup will get cleaned out once this event is emitted
			event->data.mapping_start.anchor = yaml_strdup( (yaml_char_t*)buffer );
		}

		AddAliasEvent( event->data.mapping_start.anchor );			
	}
	else
	{
		WriteIRoot( *theRawRoot, NULL );
	}
}

void YamlWriter::ReportError( const char* message )
{
	CCP_LOGERR( "%S : %s", m_fileName.c_str(), message );
}

void YamlWriter::WriteFloatSequence( const float* values, unsigned count )
{
    char buffer[ 32 ];
    yaml_sequence_start_event_initialize( AddEvent(), NULL, NULL, 0, YAML_FLOW_SEQUENCE_STYLE );
    for (unsigned i = 0; i < count; ++i)
    {
        sprintf_s( buffer, "%g", values[i]);
        AddScalarEvent( buffer );
    }
    yaml_sequence_end_event_initialize( AddEvent() );
}

void YamlWriter::AddMappingEndEvent()
{
    yaml_mapping_end_event_initialize( AddEvent() );
}

void YamlWriter::AddAliasEvent( yaml_char_t* anchor )
{
    yaml_alias_event_initialize( AddEvent(), (yaml_char_t*)anchor );
}

void YamlWriter::WriteMemberName( const char* key )
{
    AddScalarEvent( key );
}

void YamlWriter::WriteFloatArray( float* values, size_t numValues )
{
	yaml_event_t* evt = AddEvent();
	yaml_sequence_start_event_initialize( evt, NULL, NULL, 0, YAML_FLOW_SEQUENCE_STYLE );

	for( size_t i = 0; i < numValues; ++i )
	{
		WriteFloat( values[i] );
	}

	evt = AddEvent();
	yaml_sequence_end_event_initialize( evt );
}

void YamlWriter::WriteFloatMatrix( float* values, size_t numRows, size_t numColumns )
{
	WriteVectorBegin( numRows );
	float* currentRow = values;
	for( size_t i = 0; i < numRows; ++i )
	{
		WriteFloatArray( currentRow, numColumns );
		currentRow += numColumns;
	}
	WriteVectorEnd( numRows );
}

void YamlWriter::WriteStructureList( IBlueStructureList* structureList )
{
	size_t n = structureList->GetSize();
	BlueStructureDefinition* sd = structureList->GetStructureDefinition();

	//
	// Write structure definition to allow validation on load
	//

	AddMappingStartEvent();

	WriteMemberName( "structure" );

	yaml_event_t* evt = AddEvent();
	yaml_sequence_start_event_initialize( evt, NULL, NULL, 0, YAML_FLOW_SEQUENCE_STYLE );

	BlueStructureDefinition* memberDef = sd;
	while( memberDef->m_name )
	{
		evt = AddEvent();
		yaml_sequence_start_event_initialize( evt, NULL, NULL, 0, YAML_FLOW_SEQUENCE_STYLE );

		WriteChar( memberDef->m_name );
		WriteInt32( memberDef->m_dataType );
		WriteInt32( memberDef->m_offset );

		evt = AddEvent();
		yaml_sequence_end_event_initialize( evt );

		++memberDef;
	}

	evt = AddEvent();
	yaml_sequence_end_event_initialize( evt );


	//
	// Write the vector of structures
	//

	WriteMemberName( "items" );

	WriteVectorBegin( n );

	for( size_t i = 0; i < n; ++i )
	{
		evt = AddEvent();
		yaml_sequence_start_event_initialize( evt, NULL, NULL, 0, YAML_FLOW_SEQUENCE_STYLE );

		uint8_t* item = (uint8_t*)structureList->GetAt( i );

		memberDef = sd;
		while( memberDef->m_name )
		{
			int type = memberDef->m_dataType & Be::DT_TYPE_MASK;
			int size = ((memberDef->m_dataType & Be::DT_SIZE_MASK) >> Be::DT_SIZE_OFFSET) + 1;
			uint8_t* member = item + memberDef->m_offset;

			if( size > 1 )
			{
				evt = AddEvent();
				yaml_sequence_start_event_initialize( evt, NULL, NULL, 0, YAML_FLOW_SEQUENCE_STYLE );

			}

			switch( type )
			{
				case Be::DT_INT8:
					if( memberDef->m_dataType & Be::DT_UNSIGNED_BIT )
					{
						for( int compIx = 0; compIx < size; ++compIx )
						{						 
							WriteInt32(*(uint8_t*)member);
							member += sizeof( uint8_t );
						}
					}
					else
					{
						for( int compIx = 0; compIx < size; ++compIx )
						{						 
							WriteInt8(*(int8_t*)member);
							member += sizeof( int8_t );
						}
					}
					break;
				case Be::DT_INT16:
					if( memberDef->m_dataType & Be::DT_UNSIGNED_BIT )
					{
						for( int compIx = 0; compIx < size; ++compIx )
						{						 
							WriteInt32(*(uint16_t*)member);
							member += sizeof( uint16_t );
						}
					}
					else
					{
						for( int compIx = 0; compIx < size; ++compIx )
						{						 
							WriteInt32(*(int16_t*)member);
							member += sizeof( int16_t );
						}
					}
					break;
				case Be::DT_INT32:
					if( memberDef->m_dataType & Be::DT_UNSIGNED_BIT )
					{
						for( int compIx = 0; compIx < size; ++compIx )
						{
							WriteInt64( *(uint32_t*)member );
							member += sizeof( uint32_t );
						}
					}
					else
					{
						for( int compIx = 0; compIx < size; ++compIx )
						{
							WriteInt32( *(int32_t*)member );
							member += sizeof( int32_t );
						}
					}
					break;
				case Be::DT_FLOAT32:
					for( int compIx = 0; compIx < size; ++compIx )
					{
						WriteFloat( *(float*)member );
						member += sizeof( float );
					}
					break;
				case Be::DT_FLOAT16:
					for( int compIx = 0; compIx < size; ++compIx )
					{
						WriteFloat( BlueFloat16To32( *(uint16_t*)member ) );
						member += sizeof( uint16_t );
					}
					break;
				case Be::DT_SHAREDSTRING:
					for( int compIx = 0; compIx < size; ++compIx )
					{
						WriteChar( ( (BlueSharedString*)member )->c_str() );
						member += sizeof( BlueSharedString );
					}
					break;
				case Be::DT_FLOAT32x4:
					for( int compIx = 0; compIx < size; ++compIx )
					{
						WriteFloatSequence( (float*)member, 4 );
						member += sizeof( float ) * 4;
					}
					break;
				case Be::DT_BOOL8:
					for( int compIx = 0; compIx < size; ++compIx )
					{
						WriteInt8( *(bool*)member );
						member += sizeof( bool );
					}
					break;
			}

			if( size > 1 )
			{
				evt = AddEvent();
				yaml_sequence_end_event_initialize( evt );
			}

			++memberDef;
		}

		evt = AddEvent();
		yaml_sequence_end_event_initialize( evt );
	}

	WriteVectorEnd( n );

	AddMappingEndEvent();
}
