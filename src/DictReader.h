// Copyright © 2012 CCP ehf.

// DictReader is similar to the YamlReader except it uses a Python dict as its
// input. This is useful for the art pipeline, where the red file is read in
// with the Python yaml module and processed and therefore already exists as a
// dict. Creating the objects from the dict is more efficient than converting
// it to text and parsing the text.

#pragma once
#ifndef DictReader_h
#define DictReader_h

#include "IRootReader.h"
#include "Blue.h"
#include "IBlueObjectBuilder.h"
#include "IBluePersist.h"

BLUE_DECLARE( DictReader );

class DictReader : 
	public IRoot, 
	public IRootReaderBase
{
public:
	EXPOSE_TO_BLUE();

	DictReader( IRoot* lockobj = nullptr );

	IRoot* CreateObject( PyObject* src );

protected:
	IRoot* CreateObjectInternal();
	void ReadMembers( IRoot* instance );

	const Be::VarEntry* FindEntry( const char* name, const Be::ClassInfo* type, ssize_t& offs ) override;

	// Helper function to clean up after CreateObject, as C++ doesn't have a finally
	// clause for exception handlers
	void CleanupAfterCreate();

	// Throw an error with the formatted error message given.
	void ThrowError( const char* msg, ... )
#ifdef __clang__
    __attribute__( ( __analyzer_noreturn__ ) )
#endif
;

protected:
	// Virtual functions required by base:
	void ReadValue( uint64_t& dst ) override;
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
	void ReadMetadata( IRoot* owner, PyObject* metadata );

	template <typename T>
	void ReadStructureListItemMember( BlueStructureDefinition* memberDef, PyObject* memberObj, void* item, void ( DictReader::*conversionFunc )( T& ) )
	{
		int size = ( ( memberDef->m_dataType & Be::DT_SIZE_MASK ) >> Be::DT_SIZE_OFFSET ) + 1;
		uint8_t* member = static_cast<uint8_t*>( item ) + memberDef->m_offset;
		if( size > 1 )
		{
			if( !PySequence_Check( memberObj ) )
			{
				ThrowError( "Expected a list or a tuple" );
			}
			for( int compIx = 0; compIx < size; ++compIx )
			{
				m_currentSource = PySequence_GetItem( memberObj, compIx );
				( this->*conversionFunc )( *reinterpret_cast<T*>( member ) );
				member += sizeof( T );
			}
		}
		else
		{
			m_currentSource = memberObj;
			( this->*conversionFunc )( *reinterpret_cast<T*>( member ) );
		}
	}

	TrackableStdList<std::string> m_contextStack;

	PyObject* m_currentSource;

	typedef TrackableStdMap< PyObject*, IRootPtr > AnchorClassMap_t;
	AnchorClassMap_t m_anchorClassMap;

	// Should Initialize be called on objects created?
	bool m_doInitialize;

	// Should the reader allow setting of attributes that aren't flagged for persist?
	bool m_persistedAttributesOnly;
};

TYPEDEF_BLUECLASS_WR( DictReader );

#endif
