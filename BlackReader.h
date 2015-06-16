////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Snorri Sturluson
// Created:		February 2012
// Copyright:	CCP 2012
//

#pragma once
#ifndef BlackReader_h
#define BlackReader_h

#include "IRootReader.h"
#include "include/Blue.h"
#include "include/IBlueObjectBuilder.h"
#include "include/IBluePersist.h"
#include "include/IMotherLode.h"
#include "include/ICacheable.h"

BLUE_CLASS( BlackReader ) :
	public IBlueObjectBuilder,
	public ICacheable,
	public IRootReader,
	public IRootReaderBase
{
public:
	EXPOSE_TO_BLUE();

	BlackReader();
	~BlackReader();

	uint32_t GetCurrentVersion() const;

	Be::Result<std::string> CreateObjectFromStream( IBlueStream* stream, IRoot** obj );
	Be::Result<std::string> CreateObjectFromFile( const std::wstring& filename, IRoot** obj );

	bool IsHeaderValid( IBlueStream* stream );

	//////////////////////////////////////////////////////////////////////////
	// IRootReader
	IRoot* ReadFromStream( IBlueStream* stream );
	bool ReadForCachingFromStream( IBlueStream* stream );

	void SetFileName( const wchar_t* name );
	void SetDoInitialize( bool b );
	void SetTimeSlice( float t );

	void GetErrorMessage( std::string& msg );

	//////////////////////////////////////////////////////////////////////////
	// IBlueObjectBuilder
	IRoot* CreateObjectWithYield( unsigned int objectMarker, IRoot * callingProxy );
	IRoot* CreateObject( unsigned int objectMarker, IRoot * callingProxy );

	//////////////////////////////////////////////////////////////////////////
	// ICacheable
	bool IsMemoryUsageKnown();
	size_t GetMemoryUsage();

protected:
	enum YR_YIELD_BEHAVIOR { YR_DONT_ALLOW_YIELD, YR_ALLOW_YIELD };

	IRoot* CreateObjectHelper( unsigned int objectMarker, IRoot* callingProxy, YR_YIELD_BEHAVIOR );

	bool FillWStringTable( IBlueStream* stream );
	bool FillStringTable( IBlueStream* stream );

	void Cleanup();

	template<typename T>
	bool ReadValueFromStream( IBlueStream* stream, T& value );
	template<typename T>
	void ReadValueImpl( T& value );

	void ReadMembers( IRoot* instance, uint8_t* streamEnd );

	std::vector<int> GetVersionsSupported();

protected:
	// Virtual functions required by base:
	virtual void ReadValue( int64_t& dst );
	virtual void ReadValue( uint32_t& dst );
	virtual void ReadValue( int32_t& dst );
	virtual void ReadValue( uint16_t& dst );
	virtual void ReadValue( uint8_t& dst );
	virtual void ReadValue( bool& dst );
	virtual void ReadValue( float& dst );
	virtual void ReadValue( double& dst );

	virtual const char* ReadString();
	virtual const wchar_t* ReadWString();
	virtual void ReadBinaryBlock( ICustomPersist* instance, const char* propertyName );

	virtual void ReadFloatArray( float* mFloat, size_t count );
	virtual void ReadList( IList* list );
	virtual void ReadDict( IBlueDict* dict );
	virtual void ReadStructureList( IBlueStructureList* structureList );
	virtual void ReadIRoot( IRoot& obj );
	virtual IRoot* ReadIRootClass();
	
private:
	void PatchStringsInStructureList( IBlueStructureList* structureList );

	// Filename for this black file - used for error reporting
	std::string m_filename;

	// Last error message
	std::string m_errorMessage;

	// Should Initialize be called on objects created?
	bool m_doInitialize;

	// Maximum time slice for CreateObjectWithYield
	float m_timeSlice;

	// String table
	std::vector<const char*> m_strings;

	// Raw data for string table
	char* m_stringData;

	// Size of raw data for string table
	unsigned int m_stringDataSize;

	// String table for wide strings
	std::vector<const wchar_t*> m_wstrings;

	// Raw data for wide string table
	wchar_t* m_wstringData;

	// Size of raw data for wide strings
	unsigned int m_wstringDataSize;
	
	// Raw data
	uint8_t* m_data;

	// Size of raw data
	unsigned int m_dataSize;

	// Location of current read pointer
	uint8_t* m_curData;

	// An object marker is the position in the stream at which each object starts
	std::vector<uint8_t*> m_objectMarkers;

	// References to objects encountered so far - used for instancing. Note that
	// these must be strong refs to ensure any weak refs within the structure
	// being read stay alive until the whole structure is set up (the weak ref
	// might come before the strong ref in the file).
	std::map<int, IRootPtr> m_referenceMap;

#if CCP_STACKLESS
	// If set, the reader is allowed to yield the current tasklet
	bool m_allowYield;

	// Timer used to determine when to yield, if allowed
	BeTimer m_timeSinceYield;

	// Track current tasklet so we can catch self-referencing files.
	PyTaskletObject* m_currentTasklet;
#endif
};

TYPEDEF_BLUECLASS( BlackReader );

#endif // BlackReader_h
