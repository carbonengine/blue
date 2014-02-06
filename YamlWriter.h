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

	virtual Be::Result<std::string> WriteObjectToStream( const IRoot* root, IBlueStream* stream );
	virtual Be::Result<std::string> WriteObjectToString( const IRoot* root, std::string& output );

protected:
    //
    // Implementations of functions required by IRootWriter
    //

    virtual void WriteMemberName( const char* key );

	virtual void WriteInt8( int8_t value );
	virtual void WriteInt16( int16_t value );
	virtual void WriteInt32( int32_t value );
    virtual void WriteInt64( int64_t value );
	virtual void WriteFloat( float value );
	virtual void WriteFloatArray( float* values, size_t numValues );
	virtual void WriteFloatMatrix( float* values, size_t numRows, size_t numColumns );
    virtual void WriteDouble( double value );
	virtual void WriteBinaryBlock( ICustomPersist* cPersist, const char* propertyName );
    virtual void WriteWChar( const wchar_t* value );
	virtual void WriteChar( const char* value );

    virtual void WriteIRoot( const IRoot& instance, IRoot* defaultInstance );
	virtual void WriteIRoot( const IRoot* instance );

    virtual void WriteVectorBegin( size_t size );
	virtual void WriteVectorEnd( size_t size );
	virtual void WriteStructureList( IBlueStructureList* structureList );


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