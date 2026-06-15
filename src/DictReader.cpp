// Copyright © 2012 CCP ehf.

#include "StdAfx.h"

#include <BlueStatistics.h>

#if BLUE_WITH_PYTHON

#include "DictReader.h"
#include "IBlueObjectMetadata.h"


DictReader::DictReader( IRoot* lockobj /*= nullptr */ ) :
	m_doInitialize( true ),
	m_persistedAttributesOnly( true ),
	m_contextStack( "DictReader/m_contextStack" ),
	m_currentSource( nullptr ),
	m_anchorClassMap( "DictReader/m_anchorClassMap" )
{
}

IRoot* DictReader::CreateObject( PyObject* src )
{
	try
	{
		m_contextStack.push_front( "CreateObject");
		m_currentSource = src;

		IRoot* instance = CreateObjectInternal();

		CleanupAfterCreate();

		return instance;
	}
	catch( std::exception& )
	{
		CleanupAfterCreate();

		throw;
	}
}


IRoot* DictReader::CreateObjectInternal()
{
	if( !PyDict_Check( m_currentSource ) )
	{
		ThrowError( "Expected a dictionary" );
	}

	PyObject* typeO = PyDict_GetItemString( m_currentSource, "type" );
	if( !typeO )
	{
		ThrowError( "Dictionary must have a 'type' item" );
	}

	const char* type = PyUnicode_AsUTF8( typeO );
	if( !type )
	{
		ThrowError( "'type' must be a string" );
	}

	Be::Clsid clsid;
	if( !BeClasses->FindClsid( clsid, NULL, type ) )
	{
		// 'type' not found
		ThrowError( "Type '%s' not found in the Blue class registry", type );	
	}

	// Add more detailed info for context, to help with error reporting
	std::string ctx = m_contextStack.front();
	ctx += "(";
	ctx += type;
	ctx += ")";

	m_contextStack.pop_front();
	m_contextStack.push_front( ctx );

	IRoot* instance = nullptr;
	BeClasses->CreateInstance( clsid, GetIRootIID(), (void**)&instance );

	ReadMembers( instance );

	if( m_doInitialize )
	{
		// Call Initialize, if IInitialize is provided
		IInitializePtr init( BlueCastPtr( instance ) );
		if( init )
		{
			CCP_STATS_ZONE( CCP_STRINGIZE( __FUNCTION__ ) ": Initialize" );
			init->Initialize();
		}
	}

	return instance;
}

void DictReader::ReadIRoot( IRoot& instance )
{
	if( !PyDict_Check( m_currentSource ) )
	{
		ThrowError( "Expected a dictionary" );
	}

	PyObject* typeO = PyDict_GetItemString( m_currentSource, "type" );
	if( !typeO )
	{
		ThrowError( "Dictionary must have a 'type' item" );
	}

	const char* type = PyUnicode_AsUTF8( typeO );
	if( !type )
	{
		ThrowError( "'type' must be a string" );
	}

	Be::Clsid clsid;
	if( !BeClasses->FindClsid( clsid, NULL, type ) )
	{
		// 'type' not found
		ThrowError( "Type '%s' not found in the Blue class registry", type );	
	}

	// Add more detailed info for context, to help with error reporting
	std::string ctx = m_contextStack.front();
	ctx += "(";
	ctx += type;
	ctx += ")";

	m_contextStack.pop_front();
	m_contextStack.push_front( ctx );

	// assert that the types match:
	if( !instance.ClassType()->mClassId->IsEqual( clsid  ) )
	{
		ThrowError( 
			"Expected type '%s' but got object of type '%s'", 
			instance.ClassType()->mClassId->GetName(),
			type );	
	}

	ReadMembers( &instance );

	if( m_doInitialize )
	{
		// Call Initialize, if IInitialize is provided
		IInitializePtr init( BlueCastPtr( &instance ) );
		if( init )
		{
			CCP_STATS_ZONE( CCP_STRINGIZE( __FUNCTION__ ) ": Initialize" );
			init->Initialize();
		}
	}
}

void DictReader::ReadMembers( IRoot* instance )
{
	PyObject* key;
	PyObject* value;
	Py_ssize_t pos = 0;

	IInitializePtr init( BlueCastPtr(  instance ) );
	INotifyPtr notify( BlueCastPtr( instance ) );
	if( init )
	{
		// If IInitialize is provided, don't do individual notifications
		notify = (INotify*)nullptr;
	}

	PyObject* sourceDict = m_currentSource;
	while( PyDict_Next( sourceDict, &pos, &key, &value ) )
	{
		const char* name = PyUnicode_AsUTF8( key );
		if( !name )
		{
			ThrowError( "Keys in member dict must be strings" );
		}

		if( strcmp( name, "type" ) == 0 )
		{
			// Skip over the 'type' - that was used to construct the instance
			continue;
		}

		if( strcmp( name, BLUE_OBJECT_METADATA_KEY ) == 0 )
		{
			ReadMetadata( instance, value );
		}
		else
		{
			m_currentSource = value;
			m_contextStack.push_front( name );

			HandleAttribute( name, instance, notify );
			m_contextStack.pop_front();
		}
	}

	m_currentSource = sourceDict;
}

void DictReader::ReadMetadata( IRoot* owner, PyObject* metadata )
{
	IWeakObjectPtr weak( BlueCastPtr( owner ) );
	if( !weak )
	{
		ThrowError( "Metadata for a non weakref enabled object" );
	}
	if( !PyDict_Check( metadata ) )
	{
		ThrowError( "Expected a dictionary" );
	}
	PyObject* key;
	PyObject* value;
	Py_ssize_t pos = 0;

	while( PyDict_Next( metadata, &pos, &key, &value ) )
	{
		if( !PyUnicode_Check( key ) || !PyUnicode_Check( value ) )
		{
			ThrowError( "Expected strings in metadata" );
		}
		BeObjectMetadata->Set( weak, PyUnicode_AsUTF8( key ), PyUnicode_AsUTF8( value ) );
	}
}

void DictReader::ReadValue( uint64_t& dst )
{
	if( !BlueExtractArgument( m_currentSource, dst, 0 ) )
	{
		ThrowError( "Expected an int" );
	}
}

void DictReader::ReadValue( int64_t& dst )
{
	if( !BlueExtractArgument( m_currentSource, dst, 0 ) )
	{
		ThrowError( "Expected an int" );
	}
}

void DictReader::ReadValue( int32_t& dst )
{
	if( !BlueExtractArgument( m_currentSource, dst, 0 ) )
	{
		ThrowError( "Expected an int" );
	}
}

void DictReader::ReadValue( uint32_t& dst )
{
	if( !BlueExtractArgument( m_currentSource, dst, 0 ) )
	{
		ThrowError( "Expected an int" );
	}
}

void DictReader::ReadValue( uint16_t& dst )
{
	if( !BlueExtractArgument( m_currentSource, dst, 0 ) )
	{
		ThrowError( "Expected an int" );
	}
}

void DictReader::ReadValue( uint8_t& dst )
{
	if( !BlueExtractArgument( m_currentSource, dst, 0 ) )
	{
		ThrowError( "Expected an int" );
	}
}

void DictReader::ReadValue( float& dst )
{
	if( !BlueExtractArgument( m_currentSource, dst, 0 ) )
	{
		ThrowError( "Expected a float" );
	}
}

void DictReader::ReadValue( double& dst )
{
	if( !BlueExtractArgument( m_currentSource, dst, 0 ) )
	{
		ThrowError( "Expected a double" );
	}
}

void DictReader::ReadValue( bool& dst )
{
	if( !BlueExtractArgument( m_currentSource, dst, 0 ) )
	{
		ThrowError( "Expected a bool" );
	}
}

void DictReader::ReadValue( int16_t& dst )
{
	int32_t value;
	if( !BlueExtractArgument( m_currentSource, value, 0 ) )
	{
		ThrowError( "Expected an int" );
	}
	dst = int16_t( value );
}

void DictReader::ReadValue( int8_t& dst )
{
	int32_t value;
	if( !BlueExtractArgument( m_currentSource, value, 0 ) )
	{
		ThrowError( "Expected an int" );
	}
	dst = int8_t( value );
}

void DictReader::ReadValue( BlueSharedString& dst )
{
	if( !BlueExtractArgument( m_currentSource, dst, 0 ) )
	{
		ThrowError( "Expected a bool" );
	}
}

void DictReader::ReadValue( Vector4& dst )
{
	if( !BlueExtractArgument( m_currentSource, dst, 0 ) )
	{
		ThrowError( "Expected a vector" );
	}
}

void DictReader::ReadFloat16( uint16_t& dst )
{
	float value;
	if( !BlueExtractArgument( m_currentSource, value, 0 ) )
	{
		ThrowError( "Expected a float" );
	}
	dst = BlueFloat32To16( value );
}

const char* DictReader::ReadString()
{
	PyErr_Clear();
	const char* val = PyUnicode_AsUTF8( m_currentSource );
	if( val )
	{
		return CCP_STRDUP( "DictReader/ReadChar", val );
	}
        return nullptr;
}

const wchar_t* DictReader::ReadWString()
{
	PyErr_Clear();
	wchar_t* val = PyUnicode_AsWideCharString( m_currentSource, NULL );
	if( val )
	{
		wchar_t* duped_val = CCP_WSTRDUP( "DictReader/ReadWChar", val );
		PyMem_Free( val );
		return duped_val;
	}
        return nullptr;
}

void DictReader::ReadFloatArray( float* values, size_t numValues )
{
	if( !PySequence_Check( m_currentSource ) )
	{
		ThrowError( "Expected a list or a tuple" );
	}

	auto numEntries = PySequence_Size( m_currentSource );

	if( numEntries != numValues )
	{
		ThrowError( "List/tuple does not match float array - expected %d floats, got %d", numValues, numEntries );
	}

	PyObject* prevSrc = m_currentSource;
	for( auto i = 0; i < numEntries; ++i )
	{
		PyObject* entry = PySequence_GetItem( prevSrc, i );
		m_currentSource = entry;
		ReadValue( values[i] );
	};

	m_currentSource = prevSrc;
}

IRoot* DictReader::ReadIRootClass()
{
	IRoot* obj = nullptr;

	auto found = m_anchorClassMap.find( m_currentSource );
	if( found != m_anchorClassMap.end() )
	{
		obj = found->second;

		if( obj )
		{
			obj->Lock();
		}
		else
		{
			ThrowError( "Circular reference found" );
		}
	}
	else
	{
		// Add a null pointer to the map to indicate we're reading from
		// source object.
		m_anchorClassMap[m_currentSource] = nullptr;

		obj = CreateObjectInternal();

		// Set the valur for 'src' to the finalized object
		m_anchorClassMap[m_currentSource] = obj;
	}

	return obj;
}

void DictReader::ReadList( IList* list )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( !PyList_Check( m_currentSource ) )
	{
		ThrowError( "Expected a list" );
	}

	PyObject* sourceList = m_currentSource;

	list->Remove( -1 );

	ListInfo info;
	list->GetInfo( &info );
	list->SetNotify( nullptr );

	std::string ctx = m_contextStack.front();
	m_contextStack.pop_front();
	
	auto numEntries = PyList_Size( sourceList );

	for( auto i = 0; i < numEntries; ++i )
	{
		char buffer[256];
		sprintf_s( buffer, "%s [%d]", ctx.c_str(), i );
		
		m_contextStack.push_front( buffer );

		PyObject* entry = PyList_GetItem( sourceList, i );
		m_currentSource = entry;
		IRoot* p = ReadIRootClass();
		if( p )
		{
			list->Append( p );
			p->Unlock(); // List now owns the object
		}

		m_contextStack.pop_front();
	};

	m_currentSource = sourceList;

	m_contextStack.push_front( ctx );

	list->SetNotify( info.mNotify );
	if( info.mNotify )
	{
		info.mNotify->OnListModified( BELIST_LOADFINISHED, 0, 0, NULL, list );
	}
}

void DictReader::ReadDict( IBlueDict* dict )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// The reason we're expecting a list here rather than a dict has to do with the
	// way the YamlWriter outputs BlueDicts.
	if( !PyList_Check( m_currentSource ) )
	{
		ThrowError( "Expected a list" );
	}

	auto numEntries = PyList_Size( m_currentSource );
	if( numEntries % 2 )
	{
		ThrowError( "List for dict should have an even number of items" );
	}

	std::string ctx = m_contextStack.front();
	m_contextStack.pop_front();

	PyObject* sourceList = m_currentSource;

	for( auto i = 0; i < numEntries; i += 2 )
	{
		PyObject* keyObj = PyList_GetItem( sourceList, i );
		const char* key = PyUnicode_AsUTF8( keyObj );
		if( !key )
		{
			ThrowError( "Key must be a string" );
		}
		PyObject* entry = PyList_GetItem( sourceList, i + 1 );

		char buffer[256];
		sprintf_s( buffer, "%s ['%s' - %d]", ctx.c_str(), key, i );

		m_contextStack.push_front( buffer );

		m_currentSource = entry;
		IRoot* p = ReadIRootClass();
		if( p )
		{
			dict->AssignSubscript( key, p );
			p->Unlock(); // Dict now owns the object
		}

		m_contextStack.pop_front();
	};

	m_currentSource = sourceList;

	m_contextStack.push_front( ctx );
}

void DictReader::ReadStructureList( IBlueStructureList* structureList )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( !PyDict_Check( m_currentSource ) )
	{
		ThrowError( "Expected a dict" );
	}

	if( PyDict_Size( m_currentSource ) == 0 )
	{
		// Dict is empty, nothing to do
		return;
	}

	PyObject* items = PyDict_GetItemString( m_currentSource, "items" );
	if( !items )
	{
		ThrowError( "Dictionary must have a 'items' item" );
	}

	if( !PyList_Check( items ) )
	{
		ThrowError( "'items' must be a list" );
	}

	BlueStructureDefinition* sdFromList = structureList->GetStructureDefinition();
	CcpMallocBuffer item( "item", structureList->GetStructureSize() );

	auto notify = structureList->SetNotify( nullptr );
	ON_BLOCK_EXIT( [&] { structureList->SetNotify( notify ); } );

	auto n = PyList_Size( items );
	for( auto i = 0; i < n; ++i )
	{
		PyObject* itemObj = PyList_GetItem( items, i );
		PyObject* memberObj;

		if( structureList->GetMemberCount() > 1 )
		{
			if( !PySequence_Check( itemObj ) )
			{
				ThrowError( "Expected a list" );
			}

			if( structureList->GetMemberCount() != PySequence_Size( itemObj) )
			{
				ThrowError( "List does not match structure - expected %d components, got %d", structureList->GetMemberCount(), PyList_Size( itemObj) );
			}
		}

		
		int memberIx = 0;
		BlueStructureDefinition* memberDef = sdFromList;
		while( memberDef->m_name )
		{
			if( structureList->GetMemberCount() > 1 )
			{
				memberObj = PySequence_GetItem( itemObj, memberIx );
			}
			else
			{
				memberObj = itemObj;
			}

			int type = memberDef->m_dataType & Be::DT_TYPE_MASK;

			switch( type )
			{
				case Be::DT_INT8:
					if( memberDef->m_dataType & Be::DT_UNSIGNED_BIT )
					{
						ReadStructureListItemMember<uint8_t>( memberDef, memberObj, item.get(), &DictReader::ReadValue );
					}
					else
					{
						ReadStructureListItemMember<int8_t>( memberDef, memberObj, item.get(), &DictReader::ReadValue );
					}
					break;
				case Be::DT_INT16:
					if( memberDef->m_dataType & Be::DT_UNSIGNED_BIT )
					{
						ReadStructureListItemMember<uint16_t>( memberDef, memberObj, item.get(), &DictReader::ReadValue );
					}
					else
					{
						ReadStructureListItemMember<int16_t>( memberDef, memberObj, item.get(), &DictReader::ReadValue );
					}
					break;
				case Be::DT_INT32:
					if( memberDef->m_dataType & Be::DT_UNSIGNED_BIT )
					{
						ReadStructureListItemMember<uint32_t>( memberDef, memberObj, item.get(), &DictReader::ReadValue );
					}
					else
					{
						ReadStructureListItemMember<int32_t>( memberDef, memberObj, item.get(), &DictReader::ReadValue );
					}
					break;
				case Be::DT_FLOAT32:
					ReadStructureListItemMember<float>( memberDef, memberObj, item.get(), &DictReader::ReadValue );
					break;
				case Be::DT_FLOAT16:
					ReadStructureListItemMember<uint16_t>( memberDef, memberObj, item.get(), &DictReader::ReadFloat16 );
					break;
				case Be::DT_SHAREDSTRING:
					ReadStructureListItemMember<BlueSharedString>( memberDef, memberObj, item.get(), &DictReader::ReadValue );
					break;
				case Be::DT_FLOAT32x4:
					ReadStructureListItemMember<Vector4>( memberDef, memberObj, item.get(), &DictReader::ReadValue );
					break;
				case Be::DT_BOOL8:
					ReadStructureListItemMember<bool>( memberDef, memberObj, item.get(), &DictReader::ReadValue );
					break;
			}

			++memberIx;
			++memberDef;
		}
		structureList->Append( item.get() );
	}
}

void DictReader::ThrowError( const char* msg, ... )
{
	char buffer[512];

	va_list args;
	va_start( args, msg );

	vsprintf_s( buffer, msg, args );
	va_end( args );

	std::string finalMsg;

	for( auto it = m_contextStack.rbegin(); it != m_contextStack.rend(); ++it )
	{
		finalMsg += "\t";
		finalMsg += *it;
		finalMsg += "\n";
	}

	finalMsg += buffer;

	throw( IRootReaderException( finalMsg.c_str() ) );
}

void DictReader::CleanupAfterCreate()
{
	m_anchorClassMap.clear();
	m_contextStack.clear();
	m_currentSource = nullptr;
}

const Be::VarEntry* DictReader::FindEntry( const char* name, const Be::ClassInfo* type, ssize_t& offs )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Loop over all entries - this double loop covers chaining
	for (; type; offs += type->mOffsetToParent, type = type->mParentClassInfo)
	{
		for (const Be::VarEntry* entry = type->mMemberTable; entry->mName; entry++)
		{
			if( strcmp(entry->mName, name) == 0 )
			{
				if( m_persistedAttributesOnly )
				{
					if( entry->mEditFlags & Be::PERSIST )
					{
						return entry;
					}
				}
				else
				{
					if( (entry->mEditFlags & Be::PERSIST) || ((entry->mEditFlags & Be::WRITE) && (!entry->mGetProperty)) )
					{
						return entry;
					}
				}
			}
		}
	}

	return NULL;
}

void DictReader::ReadBinaryBlock( ICustomPersist* instance, const char* propertyName )
{
	ThrowError( "DictReader does not support binary blocks" );
}

#endif
