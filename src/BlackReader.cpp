// Copyright © 2012 CCP ehf.

#include "StdAfx.h"
#include "BlackReader.h"
#include "BlueMemStream.h"
#include <BlueStatistics.h>
#include "IRootReader.h"
#include "IBlueObjectProxy.h"
#include "IBluePaths.h"
#include "IBlueOS.h"
#include <Scheduler.h>

namespace
{
	class TaskletKilledException
	{
	};
}

BlackReader::BlackReader() :
	m_doInitialize( true ),
	m_timeSlice( 0.005f ),
	m_stringData( nullptr ),
	m_stringDataSize( 0 ),
	m_wstringData( nullptr ),
	m_wstringDataSize( 0 ),
	m_data( nullptr ),
	m_dataSize( 0 ),
	m_curData( nullptr )
#if CCP_STACKLESS
	,
	m_allowYield( false ),
	m_currentTasklet( nullptr )
#endif
{
}

BlackReader::~BlackReader()
{
	Cleanup();
}

bool BlackReader::IsHeaderValid( IBlueStream* stream )
{
	int32_t magicValue;

	if( !ReadValueFromStream( stream, magicValue ) )
	{
		Cleanup();
		return false;
	}

	if( magicValue != 0xb1acf11e )
	{
		Cleanup();
		return false;
	}

	int32_t version;
	if( !ReadValueFromStream( stream, version ) )
	{
		Cleanup();
		return false;
	}

	if( version != 1 )
	{
		// Note - keep the PyGetVersionsSupported up to date if this version
		// changes, or newer versions are added.
		Cleanup();
		return false;
	}

	return true;
}

IRoot* BlackReader::ReadFromStream( IBlueStream* stream )
{
	Cleanup();

	if( !ReadForCachingFromStream( stream ) )
	{
		return nullptr;
	}

	return CreateObject( 0, nullptr );
}

bool BlackReader::ReadForCachingFromStream( IBlueStream* stream )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	m_errorMessage = "";

	try
	{
		if( !IsHeaderValid( stream ) )
		{
			Cleanup();
			throw IRootReaderException( "Invalid file" );
		}

		if( !FillStringTable( stream ) )
		{
			Cleanup();
			throw IRootReaderException( "String table is invalid" );
		}

		if( !FillWStringTable( stream ) )
		{
			Cleanup();
			throw IRootReaderException( "Wide string table is invalid" );
		}

		m_dataSize = (unsigned int)(stream->GetSize() - stream->GetPosition());
		m_data = (uint8_t*)CCP_MALLOC( "BlackReader/m_data", m_dataSize );
		stream->Read( m_data, m_dataSize );

		m_objectMarkers.push_back( m_data );

		return true;
	}
	catch( const IRootReaderException& e )
	{
		m_errorMessage = e.what();
		return false;
	}
}

template<typename T>
bool BlackReader::ReadValueFromStream( IBlueStream* stream, T& value )
{
	if( stream->Read( &value, sizeof( value ) ) == sizeof( value ) )
	{
		return true;
	}

	return false;
}

template<typename T> void BlackReader::ReadValueImpl( T& value )
{
	int toCopy = sizeof( value );

	if( m_curData + toCopy > m_data + m_dataSize )
	{
		throw IRootReaderException( "Reading out of bounds" );
	}
	memcpy( &value, m_curData, toCopy );
	m_curData += toCopy;
}

void BlackReader::ReadValue( int64_t& dst )
{
	ReadValueImpl( dst );
}

void BlackReader::ReadValue( uint64_t& dst )
{
	ReadValueImpl( dst );
}

void BlackReader::ReadValue( int32_t& dst )
{
	ReadValueImpl( dst );
}

void BlackReader::ReadValue( uint32_t& dst )
{
	ReadValueImpl( dst );
}

void BlackReader::ReadValue( uint16_t& dst )
{
	ReadValueImpl( dst );
}

void BlackReader::ReadValue( uint8_t& dst )
{
	ReadValueImpl( dst );
}

void BlackReader::ReadValue( bool& dst )
{
	ReadValueImpl( dst );
}

void BlackReader::ReadValue( float& dst )
{
	ReadValueImpl( dst );
}

void BlackReader::ReadValue( double& dst )
{
	ReadValueImpl( dst );
}

IRoot* BlackReader::CreateObjectWithYield( unsigned int objectMarker, IRoot * callingProxy )
{
	return CreateObjectHelper( objectMarker, callingProxy, YR_ALLOW_YIELD );
}

IRoot* BlackReader::CreateObject( unsigned int objectMarker, IRoot * callingProxy )
{
	return CreateObjectHelper( objectMarker, callingProxy, YR_DONT_ALLOW_YIELD );
}

IRoot* BlackReader::CreateObjectHelper( unsigned int objectMarker, IRoot * callingProxy, YR_YIELD_BEHAVIOR yieldBehavior )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	CCP_ASSERT( objectMarker < m_objectMarkers.size() );

	m_errorMessage = "";

#if CCP_STACKLESS
	PyTaskletObject* current{nullptr};
	if( PyGILState_Check() )
	{
		current = (PyTaskletObject*)SchedulerAPI()->PyScheduler_GetCurrent();
		Py_DECREF(current);
	}

	bool taskletCanYield;
	
	if( yieldBehavior == YR_DONT_ALLOW_YIELD )
	{
		taskletCanYield = false;
	}
	else
	{
		taskletCanYield = PyOS->CanYield();
	}

	if( m_allowYield )
	{
		// If m_allowYield is true and we get to this point it's because
		// another tasklet is calling CreateObject for the same .black file.
		// We have to yield until the first one is done. If the new caller
		// can't block, we're in trouble.
		if( !taskletCanYield )
		{
			if( !callingProxy )
			{
				// If this CreateObject request comes from a proxy object we silently
				// return NULL and rely on it to try again. This can happen with
				// proxies used for LODs - they're supposed to handle NULL returns
				// anyway.
				CCP_LOGERR( 
					"Trying to create an object from '%S' on a non-yielding"
					"tasklet while another tasklet is still creating an object from it.",
					m_filename.c_str() );
			}
			return nullptr;
		}

		if( current == m_currentTasklet )
		{
			CCP_LOGERR( "Object from '%S' references itself!", m_filename.c_str() );
			return nullptr;
		}

		while( m_allowYield )
		{
			if( !PyOS->Yield() )
			{
				// If we couldn't yield it's likely due the tasklet having been killed.
				return nullptr;
			}
		}
	}

	m_currentTasklet = current;

	m_allowYield = taskletCanYield;
	m_timeSinceYield.Reset();
#endif

	m_curData = m_objectMarkers[objectMarker];

	IRoot* ret = nullptr;
	
	try
	{
		ret = ReadIRootClass();
	}
	catch( const IRootReaderException& exc )
	{
		m_errorMessage = exc.what();
		ret = nullptr;
	}
	catch( const TaskletKilledException& )
	{
		m_errorMessage = "Tasklet killed";
		ret = nullptr;
	}

	m_referenceMap.clear();

#if CCP_STACKLESS
	m_allowYield = false;
	m_currentTasklet = nullptr;
#endif
	return ret;
}

IRoot* BlackReader::ReadIRootClass()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	IRootPtr instance;

	int referenceIx;
	ReadValue( referenceIx );

	if( referenceIx == 0 )
	{
		// Object is a nullptr - this can happen if the default value is non-null
		// but the saved object had it explicitly nulled out.
		return nullptr;
	}

	auto foundIt = m_referenceMap.find( referenceIx );

	if( foundIt != m_referenceMap.end() )
	{
		// We've seen this object before
		IRoot* existingInstance = foundIt->second;
		if( existingInstance )
		{
			existingInstance->Lock();
		}
		return existingInstance;
	}

	uint32_t dataSize;
	ReadValue( dataSize );

	uint8_t* startPos = m_curData;

	const char* typeName = ReadString();

	{
		CCP_STATS_ZONE( CCP_STRINGIZE( __FUNCTION__ ) ": CreateInstance" );

		// need to create our own using the factory
		if( !BeClasses->CreateInstanceFromName( typeName, GetIRootIID(), (void**)&instance ) )
		{
			std::string msg = "Failed to create an instance of ";
			msg += typeName;
			throw IRootReaderException( msg.c_str() );
		}
	}

	IBlueObjectProxyPtr proxy( BlueCastPtr( instance ) );

	if( proxy )
	{
		// Note that the proxy might be empty
		if( m_curData < startPos + dataSize )
		{
			const char* propertyName = ReadString();

			if( strcmp( propertyName, "object" ) == 0 )
			{
				unsigned int objectMarker = (unsigned int)m_objectMarkers.size();
				m_objectMarkers.push_back( m_curData );

				unsigned int proxiedObjReferenceIx;
				ReadValue( proxiedObjReferenceIx );

				m_referenceMap[proxiedObjReferenceIx] = nullptr;

				unsigned int dataSize;
				ReadValue( dataSize );

				m_curData += dataSize;

				proxy->SetBuilder( this, objectMarker );
			}
			else
			{
				std::string msg = "Invalid property: ";
				msg += propertyName;
				throw IRootReaderException( msg.c_str() );
			}
		}
	}
	else
	{
		ReadMembers( instance, startPos + dataSize );
	}

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

	m_referenceMap[referenceIx] = instance;

	return instance.Detach();
}

void BlackReader::ReadMembers( IRoot* instance, uint8_t* streamEnd )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Unless IInitialize is present, we look for INotify to pass down
	// to HandleProperty, to issue notifications to the object.
	INotifyPtr notify;
	IInitializePtr init( BlueCastPtr( instance ) );
	if( !init )
	{
		notify = BlueCastPtr( instance );
	}

	while( m_curData < streamEnd )
	{
#if CCP_STACKLESS
		if( m_allowYield && ( m_timeSinceYield.GetSeconds() > m_timeSlice ) )
		{
			if( !PyOS->Yield() )
			{
				throw TaskletKilledException();
			}
			m_timeSinceYield.Reset();
		}
#endif

		const char* attributeName = ReadString();
		HandleAttribute( attributeName, instance, notify );
	}
}

void BlackReader::ReadFloatArray( float* dest, size_t count )
{
	size_t toCopy = sizeof( float ) * count;
	memcpy( dest, m_curData, toCopy );
	m_curData += toCopy;
}

// Read a string table index, look it up and return the string. Throws an exception
// if the index is out of range.
const char* BlackReader::ReadString()
{
	uint16_t stringIndex;
	ReadValue( stringIndex );

	if( stringIndex < m_strings.size() )
	{
		return m_strings[stringIndex];
	}
	else
	{
		throw IRootReaderException( "Invalid string index" );
	}
}

// Read a string table index, look it up and return the string. Throws an exception
// if the index is out of range.
const wchar_t* BlackReader::ReadWString()
{
	uint16_t stringIndex;
	ReadValue( stringIndex );

	if( stringIndex < m_wstrings.size() )
	{
		return m_wstrings[stringIndex];
	}
	else
	{
		throw IRootReaderException( "Invalid string index" );
	}
}

void BlackReader::ReadList( IList* list )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	list->Remove( -1 );

	int numEntries;
	ReadValue( numEntries );

	ListInfo info;
	list->GetInfo( &info );
	list->SetNotify( NULL );

	for( int i = 0; i < numEntries; ++i )
	{
		IRoot* p = ReadIRootClass();
		if( p )
		{
			list->Append( p );
			p->Unlock(); // List now owns the object
		}
	};

	list->SetNotify( info.mNotify );
	if( info.mNotify )
	{
		info.mNotify->OnListModified( BELIST_LOADFINISHED, 0, 0, NULL, list );
	}
}

void BlackReader::ReadDict( IBlueDict* dict )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	int numEntries;
	ReadValue( numEntries );

	for( int i = 0; i < numEntries; ++i )
	{
		const char* key = ReadString();
		IRoot* p = ReadIRootClass();

		if( p )
		{
			dict->AssignSubscript( key, p );

			// The dictionary now owns the object
			p->Unlock();
		}
	}
}

void BlackReader::PatchStringsInStructureList( IBlueStructureList* structureList )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Changes string table indexes stored in structure list data to actual
	// BlueSharedString instances
	if( structureList->GetSize() == 0 )
	{
		return;
	}
	void* listData = structureList->GetAt( 0 );
	size_t structureSize = structureList->GetStructureSize();
	BlueStructureDefinition* memberDef = structureList->GetStructureDefinition();
	while( memberDef->m_name )
	{
		int type = memberDef->m_dataType & Be::DT_TYPE_MASK;
		if( type == Be::DT_SHAREDSTRING )
		{
			int size = ( ( memberDef->m_dataType & Be::DT_SIZE_MASK ) >> Be::DT_SIZE_OFFSET ) + 1;
			for( size_t i = 0; i < structureList->GetSize(); ++i )
			{
				for( int j = 0; j < size; ++j )
				{
					uint8_t* stringAddress = reinterpret_cast<uint8_t*>( listData ) + 
						structureSize * i + memberDef->m_offset + sizeof( BlueSharedString ) * j;
					uint16_t stringIndex = *reinterpret_cast<uint16_t*>( stringAddress );
					if( stringIndex >= m_strings.size() )
					{
						throw IRootReaderException( "Invalid string index" );
					}
					BlueSharedString* str = reinterpret_cast<BlueSharedString*>( stringAddress );
					*str = BlueSharedString( m_strings[stringIndex] );
				}
			}
		}
		++memberDef;
	}
}

void BlackReader::ReadStructureList( IBlueStructureList* structureList )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	int32_t numItems;
	uint16_t structureSize;

	ReadValue( numItems );
	structureList->Resize( numItems );

	ReadValue( structureSize );

	if( structureList->GetStructureSize() != structureSize )
	{
		throw IRootReaderException( "Structure is not compatible with data in file" );
	}

	auto dataSize = numItems * structureSize;
	void* dst = structureList->GetAt( 0 );

	memcpy( dst, m_curData, dataSize );
	m_curData += dataSize;

	PatchStringsInStructureList( structureList );
}

void BlackReader::ReadBinaryBlock( ICustomPersist* cPersist, const char* propertyName )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	int32_t size;
	ReadValue( size );

	unsigned char* data = cPersist->AllocateReadBuffer( propertyName, size );
	memcpy( data, m_curData, size );

	// The persist class needs to handles the data from here.
	cPersist->SetBufferAndSize(propertyName, data, size);

	m_curData += size;
}


void BlackReader::ReadIRoot( IRoot& obj )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	unsigned int dataSize;
	ReadValue( dataSize );

	uint8_t* startPos = m_curData;

	ReadString();

	ReadMembers( &obj, startPos + dataSize );

	if( m_doInitialize )
	{
		// Call Initialize, if IInitialize is provided
		IInitializePtr init( BlueCastPtr( &obj ) );
		if( init )
		{
			init->Initialize();
		}
	}
}

void BlackReader::SetDoInitialize( bool b )
{
	m_doInitialize = b;
}

void BlackReader::SetTimeSlice( float t )
{
	m_timeSlice = t;

	if( m_timeSlice < 0.0f )
	{
		m_timeSlice = 0.0f;
	}
}

bool BlackReader::IsMemoryUsageKnown()
{
	return true;
}

size_t BlackReader::GetMemoryUsage()
{
	return m_dataSize + m_stringDataSize + m_wstringDataSize + sizeof( this );
}

void BlackReader::SetFileName( const wchar_t* name )
{
	m_filename = CW2A( name );
}

bool BlackReader::FillStringTable( IBlueStream* stream )
{
	// Read and process the regular string table
	uint32_t stringTableDataSize;
	ReadValueFromStream( stream, stringTableDataSize );

	// Guard against bogus values - file could be corrupt
	if( stringTableDataSize > (uint32_t)stream->GetSize() )
	{
		return false;
	}

	if( stringTableDataSize < sizeof( uint16_t ) )
	{
		return false;
	}

	uint16_t numStrings;
	ReadValueFromStream( stream, numStrings );
	stringTableDataSize -= sizeof( uint16_t );

	m_stringData = (char*)CCP_MALLOC( "BlackReader/m_stringData", stringTableDataSize );

	if( !m_stringData )
	{
		return false;
	}

	stream->Read( m_stringData, stringTableDataSize );

	m_strings.resize( numStrings );

	const char* p = m_stringData;
	const char* stringEnd = m_stringData + stringTableDataSize;
	int i;
	for( i = 0; (i < numStrings) && (p < stringEnd); ++i )
	{
		m_strings[i] = p;
		while( *p && p < stringEnd )
		{
			++p;
		}
		if( p < stringEnd )
		{
			// Skip the zero terminator
			++p;
		}
	}

	if( i < numStrings || p != stringEnd )
	{
		m_strings.clear();
		CCP_FREE( m_stringData );
		m_stringData = nullptr;
		return false;
	}

	m_stringDataSize = stringTableDataSize;
	return true;
}

bool BlackReader::FillWStringTable( IBlueStream* stream )
{
	// Read and process the wide string table
	uint32_t wstringTableDataSize;
	ReadValueFromStream( stream, wstringTableDataSize );

	// Guard against bogus values - file could be corrupt
	if( wstringTableDataSize > (uint32_t)stream->GetSize() )
	{
		return false;
	}

	if( wstringTableDataSize < sizeof( uint16_t ) )
	{
		return false;
	}

	uint16_t numWStrings;
	ReadValueFromStream( stream, numWStrings );
	wstringTableDataSize -= sizeof( uint16_t );

    if( sizeof(wchar_t) == 2 )
    {
        m_wstringData = (wchar_t*)CCP_MALLOC( "BlackReader/m_wstringData", wstringTableDataSize );
        if( !m_wstringData )
        {
            return false;
        }

        stream->Read( m_wstringData, wstringTableDataSize );
    }
    else
    {
        m_wstringData = (wchar_t*)CCP_MALLOC( "BlackReader/m_wstringData", wstringTableDataSize * 2 );
        if( !m_wstringData )
        {
            return false;
        }

        std::unique_ptr<uint8_t[]> ucs2( new uint8_t[wstringTableDataSize] );
        if( !ucs2.get() )
        {
            return false;
        }
        stream->Read( ucs2.get(), wstringTableDataSize );
        
        auto src = reinterpret_cast<const char16_t*>( ucs2.get() );
        auto dest = m_wstringData;
        for( size_t i = 0; i < wstringTableDataSize; i += 2 )
        {
            *dest++ = *src++;
        }
        wstringTableDataSize *= 2;
    }

    m_wstrings.resize( numWStrings );

    const wchar_t* wp = m_wstringData;
    const wchar_t* wstringEnd = (const wchar_t*)((uintptr_t)m_wstringData + wstringTableDataSize);
    int i;
    for( i = 0; (i < numWStrings) && (wp < wstringEnd); ++i )
    {
        m_wstrings[i] = wp;
        while( *wp && wp < wstringEnd )
        {
            ++wp;
        }
        if( wp < wstringEnd )
        {
            // Skip the zero terminator
            ++wp;
        }
    }

    if( i < numWStrings || wp != wstringEnd )
    {
        m_wstrings.clear();
        CCP_FREE( m_wstringData );
        m_wstringData = nullptr;
        return false;
    }

    m_wstringDataSize = wstringTableDataSize;
    return true;
}

void BlackReader::Cleanup()
{
	CCP_FREE( m_stringData );
	m_stringData = nullptr;
	m_stringDataSize = 0;

	CCP_FREE( m_wstringData );
	m_wstringData = nullptr;
	m_wstringDataSize = 0;

	CCP_FREE( m_data );
	m_data = nullptr;
	m_dataSize = 0;
	
	m_curData = nullptr;
	m_objectMarkers.clear();
	m_referenceMap.clear();
}

void BlackReader::GetErrorMessage( std::string& msg )
{
	msg = m_errorMessage;
}

std::vector<int> BlackReader::GetVersionsSupported()
{
	std::vector<int> versions;
	versions.push_back( 1 );

	return versions;
}

Be::Result<std::string> BlackReader::CreateObjectFromFile( const std::wstring& filename, IRoot** obj )
{
	IBlueStreamPtr stream;
	bool isOK = BePaths->GetStreamFromPathW( filename.c_str(), &stream );

	if( !isOK )
	{
		std::string msg = "Couldn't open file ";
		msg += CW2A( filename.c_str() );
		return Be::Result<std::string>( msg );
	}

	SetFileName( filename.c_str() );

	return CreateObjectFromStream( stream, obj );
}

Be::Result<std::string> BlackReader::CreateObjectFromStream( IBlueStream* stream, IRoot** obj )
{
	IRoot* res = ReadFromStream( stream );
	if( res )
	{
		*obj = res;
		return Be::Result<std::string>();
	}
	else
	{
		return Be::Result<std::string>( m_errorMessage );
	}
}
