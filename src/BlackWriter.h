// Copyright © 2012 CCP ehf.

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

	Be::Result<std::string> WriteObjectToStream( const IRoot* obj, IBlueStream* stream ) override;

	const std::vector<std::string> GetStrings() { return m_strings; }
	const std::vector<std::wstring> GetWStrings() { return m_wstrings; }

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
	void WriteBinaryBlock( ICustomPersist* cPersist, const char* propertyName ) override;
	void WriteDouble( double value ) override;

	void WriteWChar( const wchar_t* value ) override;
	void WriteChar( const char* value ) override;

	void WriteIRoot( const IRoot& instance, IRoot* defaultInstance ) override;
	void WriteIRoot( const IRoot* instance ) override;

	void WriteVectorBegin( size_t size ) override;
	void WriteVectorEnd( size_t size ) override;

#if BLUE_WITH_PYTHON
	void WriteStructureList( IBlueStructureList* structureList ) override;
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