// Copyright © 2014 CCP ehf.

#pragma once
#ifndef YamlReader_H
#define YamlReader_H

#include "IRootReader.h"
#include "Blue.h"
#include "IBlueObjectBuilder.h"
#include "IBluePersist.h"
#include "IMotherLode.h"
#include <map>
#include <string>
#include <yaml.h>
#include <cstdint>

#ifdef __APPLE__
#include <clocale>
#endif


// Forward declare type from the <Scheduler.h> header, include will be in .cpp file
struct PyTaskletObject;


struct PoolAllocatedYamlEvent : public yaml_event_t
{
	USE_CACHED_ALLOCATOR( PoolAllocatedYamlEvent );
};

BLUE_CLASS(YamlReader) : 
	public IBlueObjectBuilder,
	public ICacheable,
	public IRootReader,
	public IRootReaderBase
{
public:
	EXPOSE_TO_BLUE();

	YamlReader();
	~YamlReader();

	Be::Result<std::string> CreateObjectFromString( const std::string& s, IRoot** obj );
	Be::Result<std::string> CreateObjectFromStream( IBlueStream*, IRoot** obj );
	Be::Result<std::string> CreateObjectFromFile( const std::wstring& filename, IRoot** obj );


	//////////////////////////////////////////////////////////////////////////
	// IRootReader
    IRoot* ReadFromStream( IBlueStream* stream ) override;
	bool ReadForCachingFromStream( IBlueStream* stream ) override;

	// If there is a filename associated with the stream call this function before Read* for
	// better error reporting!
	void SetFileName( const wchar_t* name ) override;

	void SetDoInitialize( bool b ) override;
	void SetTimeSlice( float t ) override;
	
	void GetErrorMessage( std::string& msg ) override;

	//////////////////////////////////////////////////////////////////////////
	// IBlueObjectBuilder
	IRoot* CreateObjectWithYield( unsigned int objectMarker, IRoot * callingProxy ) override;
	IRoot* CreateObject( unsigned int objectMarker, IRoot * callingProxy ) override;

	//////////////////////////////////////////////////////////////////////////
	// ICacheable
	bool IsMemoryUsageKnown() override;
	size_t GetMemoryUsage() override;

protected:
	enum YR_YIELD_BEHAVIOR { YR_DONT_ALLOW_YIELD, YR_ALLOW_YIELD };

	IRoot* CreateObjectHelper( unsigned int objectMarker, IRoot* callingProxy, YR_YIELD_BEHAVIOR );

	bool ReadMemberName( std::string& name );

    IRoot* ReadIRootClassInternal();

    bool ReadVectorBegin();
	bool ReadVectorEnd();
	bool ReadVectorNext();

	std::string ConstructMessage( const char* message ) const;

    void ReportError( const char* message ) const;
	void ReportWarning( const char* message ) const;

	void SkipValue();

	void ReadMembers( IRoot* instance );

    static int YamlReadFromStreamStatic( void *data, unsigned char *buffer, size_t size, size_t *size_read );
    int YamlReadFromStream( unsigned char *buffer, size_t size, size_t *size_read );

	void ReadFloatSequence( float* values, size_t count );
	bool VerifyEvent( yaml_event_type_t expectedType ) const;
	bool IsParserExhausted() const;
	void ParseUntilMatchingEnd( yaml_event_type_t start, yaml_event_type_t end );
	void ParseUntilMatchingMappingEnd();
	void ParseUntilMatchingSequenceEnd();
	uint64_t ExtractAnchorFromMappingStart();

	void ReadClassType( const char*& type );
	bool ReadClsid( Be::Clsid& id );
	void ReadScalar( const char*& scalar );

	void GetNextEvent();
	void PushEvent();

	void ClearCachedEvents();

	void ReportTypeMismatch( const char* expected, const char* actual );

	template< typename T >
	void ReadValueImpl( T& dst );

protected:
	// Virtual functions required by base:
	void ReadValue( uint64_t & dst ) override;
	void ReadValue( int64_t& dst ) override;
	void ReadValue( uint32_t& dst ) override;
	void ReadValue( int32_t& dst ) override;
	void ReadValue( uint16_t& dst ) override;
	void ReadValue( uint8_t& dst ) override;
	void ReadValue( bool& dst ) override;
	void ReadValue( float& dst ) override;
	void ReadValue( double& dst ) override;

	const char* ReadString() override;
	const wchar_t* ReadWString() override;
	void ReadBinaryBlock( ICustomPersist* instance, const char* propertyName ) override;

	void ReadFloatArray( float* mFloat, size_t count ) override;
	void ReadList( IList* list ) override;
	void ReadDict( IBlueDict* dict ) override;
	void ReadStructureList( IBlueStructureList* structureList ) override;
	void ReadIRoot( IRoot& obj ) override;
	IRoot* ReadIRootClass() override;


private:
	void ReadValue( int16_t& dst );
	void ReadValue( int8_t& dst );
	void ReadValue( BlueSharedString& dst );
	void ReadValue( Vector4& dst );
	void ReadFloat16( uint16_t& dst );
	void ReadStructureListItem( const BlueStructureDefinition* memberDef, void* item );
	void ReadMetadata( IRoot* instance );

	template <typename T, size_t TypeSize>
	void ReadStructureListItemMember( const BlueStructureDefinition* memberDef, void* item, void ( YamlReader::*conversionFunc )( T& ) )
	{
		int size = ( ( memberDef->m_dataType & Be::DT_SIZE_MASK ) >> Be::DT_SIZE_OFFSET ) + 1;
		uint8_t* member = static_cast<uint8_t*>( item ) + memberDef->m_offset;
		if( size > 1 )
		{
			ReadVectorBegin();
			ReadVectorNext();
		}
		for( int compIx = 0; compIx < size; ++compIx )
		{
			( this->*conversionFunc )( *reinterpret_cast<T*>( member ) );
			member += TypeSize;
		}
		if( size > 1 )
		{
			ReadVectorNext();
			ReadVectorEnd();
		}
	}

	typedef std::map< uint64_t, IRootPtr > AnchorClassMap_t;
	typedef std::list<PoolAllocatedYamlEvent*> EventList_t;

    IBlueStreamPtr m_dataStream;
	yaml_parser_t m_parser;
	PoolAllocatedYamlEvent* m_event;
	PoolAllocatedYamlEvent* m_pushedEvent;
	bool m_hasPushedEvent;
	AnchorClassMap_t m_anchorClassMap;

	EventList_t m_eventList;
	EventList_t::iterator m_currentEvent;
	std::vector<EventList_t::iterator> m_objectMarkers;

	// Filename for this black file - used for error reporting
	std::wstring m_fileName;

	// Last error message
	std::string m_errorMessage;

	// If set, then unrecognized classes and attributes are treated as errors
	// and an exception is thrown. Otherwise a warning is written to the log.
	bool m_isStrict;

	// Should Initialize be called on objects created?
	bool m_doInitialize;

	// Maximum time slice for CreateObjectWithYield
	float m_timeSlice;

	// If set, the reader is allowed to yield the current tasklet
	bool m_allowYield;

	// Timer used to determine when to yield, if allowed
	BeTimer m_timeSinceYield;

#ifdef _WIN32
	// So user locale doesn't affect parsing results
	_locale_t m_locale;
#elif __APPLE__
	locale_t m_locale;
#endif

#if CCP_STACKLESS
	// Track current tasklet so we can catch self-referencing files.
	PyTaskletObject* m_currentTasklet;
#endif

	// Keep track of strings we allocated to delete them with the reader
	std::vector<void*> m_allocatedStrings;
};

TYPEDEF_BLUECLASS_WR( YamlReader );

#endif
