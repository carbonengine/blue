////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		February 2012
// Copyright:	CCP 2012
//

#pragma once
#ifndef BlackWriter_h
#define BlackWriter_h

#include "IRootWriter.h"

BLUE_DECLARE_INTERFACE( IBlueStream );

BLUE_CLASS( BlackWriter ): 
	public IRootWriter
{
public:
	EXPOSE_TO_BLUE();

	BlackWriter();
	~BlackWriter();

	uint32_t GetCurrentVersion() const;

	virtual Be::Result<std::string> WriteObjectToStream( const IRoot* obj, IBlueStream* stream );

	const std::vector<std::string> GetStrings() { return m_strings; }
	const std::vector<std::wstring> GetWStrings() { return m_wstrings; }

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
	virtual void WriteBinaryBlock( ICustomPersist* cPersist, const char* propertyName );
	virtual void WriteDouble( double value );

	virtual void WriteWChar( const wchar_t* value );
	virtual void WriteChar( const char* value );

	virtual void WriteIRoot( const IRoot& instance, IRoot* defaultInstance );
	virtual void WriteIRoot( const IRoot* instance );

	virtual void WriteVectorBegin( size_t size );
	virtual void WriteVectorEnd( size_t size );

#if BLUE_WITH_PYTHON
	virtual void WriteStructureList( IBlueStructureList* structureList );
#endif

	//
	// Other functions
	//

	uint16_t GetStringIndex( const char* s );
	uint16_t GetWStringIndex( const wchar_t* s );

private:
	void PatchStringsInStructureList( IBlueStructureList* structureList, void* listData );
	IBlueStreamPtr m_outputStream;
	
	// String table, mapping strings to indices
	std::map<std::string, int> m_stringMap;

	// Strings encountered
	std::vector<std::string> m_strings;

	// Wide string table, mapping wstrings to indices
	std::map<std::wstring, int> m_wstringMap;

	// Wide strings encountered
	std::vector<std::wstring> m_wstrings;

	// This is for keeping track of objects written out so
	// multiple references to the same object can be maintained.
	std::map<IRoot*, int> m_referenceMap;
};

TYPEDEF_BLUECLASS( BlackWriter );

#endif // BlackWriter_h