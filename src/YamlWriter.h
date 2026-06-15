// Copyright © 2014 CCP ehf.

#pragma once
#ifndef YAMLWRITER_H
#define YAMLWRITER_H

// See http://core/wiki/YamlWriter

#include "IRootWriter.h"
#include <yaml.h>
#include <map>
#include <vector>

BLUE_DECLARE_INTERFACE( IBlueStream );

BLUE_CLASS( YamlWriter ) : 
	public IRootWriter
{
public:
	EXPOSE_TO_BLUE();

	YamlWriter();
	~YamlWriter();

	Be::Result<std::string> WriteObjectToStream( const IRoot* root, IBlueStream* stream ) override;
	Be::Result<std::string> WriteObjectToString( const IRoot* root, std::string& output );

protected:
    //
    // Implementations of functions required by IRootWriter
    //

    void WriteMemberName( const char* key ) override;

	void WriteInt8( int8_t value ) override;
	void WriteInt16( int16_t value ) override;
	void WriteInt32( int32_t value ) override;
	void WriteUInt32( uint32_t value ) override;
    void WriteInt64( int64_t value ) override;
	void WriteUInt64( uint64_t value ) override;
	void WriteFloat( float value ) override;
	void WriteFloatArray( float* values, size_t numValues ) override;
	void WriteFloatMatrix( float* values, size_t numRows, size_t numColumns );
    void WriteDouble( double value ) override;
	void WriteBinaryBlock( ICustomPersist* cPersist, const char* propertyName ) override;
    void WriteWChar( const wchar_t* value ) override;
	void WriteChar( const char* value ) override;

    void WriteIRoot( const IRoot& instance, IRoot* defaultInstance ) override;
	void WriteIRoot( const IRoot* instance ) override;

    void WriteVectorBegin( size_t size ) override;
	void WriteVectorEnd( size_t size ) override;
	void WriteStructureList( IBlueStructureList* structureList ) override;


protected:
    // Callback for Yaml library to write data. The static function routes
    // the call to the member function.
    static int WriteToStreamStatic( void *data, unsigned char *buffer, size_t size );
    int WriteToStream( unsigned char *buffer, size_t size );

    void Cleanup();

	yaml_event_t* AddEvent();
	void AddScalarEvent( const char* value, yaml_scalar_style_t style = YAML_ANY_SCALAR_STYLE );
	yaml_event_t* AddMappingStartEvent();
	void AddMappingEndEvent();
	void AddAliasEvent( yaml_char_t* anchor );
    void WriteFloatSequence( const float* values, unsigned count );

	void ReportError( const char* message );

private:
    typedef std::map< const IRoot*, yaml_event_t* > ClassEventMap_t;
    typedef std::vector<yaml_event_t*> EventArray_t;

    IBlueStreamPtr m_dataStream;
    unsigned int m_anchorNumber;
	EventArray_t m_events;
	ClassEventMap_t m_classEventMap;
	

};

TYPEDEF_BLUECLASS( YamlWriter );

#endif