// Copyright © 2014 CCP ehf.

#pragma once
#ifndef IRootWriter_H
#define IRootWriter_H

struct IBlueStream;

// IRootWriter is a base class for writers that persist Blue objects, such as the
// YamlWriter and BlackWriter. It defines the public interface - primarily the
// WriteObjectToStream function that writes an object to an IBlueStream. It also
// provides the convenience function WriteObjectToFile that is implemented with the
// help of the abstract WriteObjectToStream function.
//
// Subclasses must provide implementations for all the pure virtual functions, but
// this base class provides an implementation of the WriteMembers function that does
// all the work involved in determining what to write.
BLUE_CLASS( IRootWriter ) : public IRoot
{
public:
	EXPOSE_TO_BLUE();

	IRootWriter();

	// Force subclasses to have a virtual destructor
	virtual ~IRootWriter() {}

	// Write 'root' to a file named 'fileName'
	virtual Be::Result<std::string> WriteObjectToFile( const IRoot* root, std::wstring fileName );

	// Write 'root' to the given 'stream'. Subclasses must provide the implementation
	// for this function.
	virtual Be::Result<std::string> WriteObjectToStream( const IRoot* obj, IBlueStream* stream ) = 0;

protected:
    //
    // Functions that subclasses must implement 
    //

    // Write the given name of a member
    virtual void WriteMemberName( const char* key ) = 0;

	// Write basic types
	virtual void WriteInt8( int8_t value ) = 0;
	virtual void WriteInt16( int16_t value ) = 0;
    virtual void WriteInt32( int32_t value ) = 0;
	virtual void WriteUInt32( uint32_t value ) = 0;
	virtual void WriteInt64( int64_t value ) = 0;
	virtual void WriteUInt64( uint64_t value ) = 0;
	virtual void WriteFloat( float value ) = 0;
	virtual void WriteFloatArray( float* values, size_t numValues ) = 0;
	virtual void WriteBinaryBlock( ICustomPersist* cPersist, const char* propertyName ) = 0;
    virtual void WriteDouble( double value ) = 0;
	
    // Write zero-terminated string
    virtual void WriteWChar( const wchar_t* value ) = 0;
	virtual void WriteChar( const char* value ) = 0;
	
    // Write an object
    virtual void WriteIRoot( const IRoot& instance, IRoot* defaultInstance ) = 0;

    // Write an object pointed to - may be aliased
	virtual void WriteIRoot( const IRoot* instance ) = 0;
	
    // Mark the beginning and end of a vector
    virtual void WriteVectorBegin( size_t size ) = 0;
	virtual void WriteVectorEnd( size_t size ) = 0;

#if BLUE_WITH_PYTHON
	virtual void WriteStructureList( IBlueStructureList* structureList ) = 0;
#endif

	//
	// Functions with a provided implementation
	//

	// Subclasses call this function from WriteIRoot implementations to
    // iterate over the members of 'instance' and write them out.
    void WriteMembers( const IRoot &instance, IRoot* defaultInstance );

	void WriteList( IListPtr list );
	void WriteDict( IBlueDictPtr list );

	// File name, used for reporting errors
	std::wstring m_fileName;

	// If set (default), then attributes with default values are not
	// written out.
	bool m_skipDefaults;
};




#endif