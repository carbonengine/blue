// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "IBlueObjectProxy.h"
#include "YamlReader.h"

#include "IBlueOS.h"
#include "IRootReader.h"
#include "Base64.h"
#include "BlueMemStream.h"
#include <BlueStatistics.h>
#include "IBluePaths.h"
#include "IBlueObjectMetadata.h"
#include <Scheduler.h>
#ifndef _WIN32
#include <locale>
#include <codecvt>
#endif

namespace
{
const char* s_event2string[] = {
	"NO_EVENT",
	"STREAM_START_EVENT",
	"STREAM_END_EVENT",
	"DOCUMENT_START_EVENT",
	"DOCUMENT_END_EVENT",
	"ALIAS_EVENT",
	"SCALAR_EVENT",
	"SEQUENCE_START_EVENT",
	"SEQUENCE_END_EVENT",
	"MAPPING_START_EVENT",
	"MAPPING_END_EVENT"
};

class TaskletKilledException
{
};
};

DEFINE_CACHED_ALLOCATOR( PoolAllocatedYamlEvent, 64 * 1024, 64 * 1024 );

YamlReader::YamlReader() :
	m_parser(),
	m_hasPushedEvent( false ),
	m_event( NULL ),
	m_pushedEvent( nullptr ),
	m_isStrict( false ),
	m_doInitialize( true ),
	m_timeSlice( 0.005f ),
	m_allowYield( false )
#ifdef _WIN32
	,
	m_locale( _create_locale( LC_ALL, "en_US" ) )
#elif __APPLE__
	,
	m_locale( newlocale(LC_ALL_MASK, "C", 0 ) )
#endif

#if CCP_STACKLESS
	,
	m_currentTasklet( NULL )
#endif
{
}

YamlReader::~YamlReader()
{
	ClearCachedEvents();

	if( m_event )
	{
		yaml_event_delete( m_event );
		delete m_event;
	}

	for( auto it = m_allocatedStrings.begin(); it != m_allocatedStrings.end(); ++it )
	{
		if( *it )
		{
			CCP_FREE( *it );
		}
	}
#ifdef _WIN32
	_free_locale( m_locale );
#elif __APPLE__
	freelocale( m_locale );
#endif
}

int YamlReader::YamlReadFromStreamStatic( void* data, unsigned char* buffer, size_t size, size_t* size_read )
{
	return static_cast<YamlReader*>( data )->YamlReadFromStream( buffer, size, size_read );
}

int YamlReader::YamlReadFromStream( unsigned char* buffer, size_t size, size_t* size_read )
{
	*size_read = m_dataStream->Read( buffer, size );
	return 1;
}

IRoot* YamlReader::ReadFromStream( IBlueStream* stream )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	IRoot* ret = NULL;

	m_dataStream = stream;

	// Ensure state from previous runs is not interfering. It is not enough
	// to clear this after parsing as errors are reported via exception handling

	yaml_parser_initialize( &m_parser );
	yaml_parser_set_input( &m_parser, YamlReadFromStreamStatic, (void*)this );

	try
	{
		bool foundStart = false;

		// pump the parser
		while( !IsParserExhausted() )
		{
			GetNextEvent();

			if( m_event->type == YAML_MAPPING_START_EVENT )
			{
				foundStart = true;
				ret = ReadIRootClassInternal();
			}
		}

		if( !foundStart )
		{
			ReportError( "mapping start event not found" );
		}
	}
	catch( const IRootReaderException& e )
	{
		ret = nullptr;
		m_errorMessage = e.what();
	}

	if( m_event )
	{
		yaml_event_delete( m_event );
		delete m_event;
		m_event = nullptr;
	}
	m_dataStream = (IBlueStream*)nullptr;
	yaml_parser_delete( &m_parser );

	return ret;
}

bool YamlReader::ReadForCachingFromStream( IBlueStream* stream )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	ClearCachedEvents();

	m_dataStream = stream;

	yaml_parser_initialize( &m_parser );
	yaml_parser_set_input( &m_parser, YamlReadFromStreamStatic, (void*)this );

	int ok = 0;
	try
	{
		// pump the parser
		while( !IsParserExhausted() )
		{
			PoolAllocatedYamlEvent* event = new PoolAllocatedYamlEvent;
			ok = yaml_parser_parse( &m_parser, event );
			if( !ok )
			{
				yaml_event_delete( event );
				delete event;

				if( m_parser.context )
				{
					std::string message = m_parser.context;
					message += " - ";
					message += m_parser.problem;
					ReportError( message.c_str() );
				}
				else
				{
					ReportError( m_parser.problem );
				}
				break;
			}
			m_eventList.push_back( event );
			m_event = event;
		}
	}
	catch( const IRootReaderException& e )
	{
		m_errorMessage = e.what();
		ClearCachedEvents();
	}

	// Take care not to leave m_event set - it is needed for IsParserExhausted
	// but it is deleted in the destructor. This would cause a double delete
	// as all the events are on the event list, which gets cleared.
	m_event = nullptr;

	m_dataStream = (IBlueStream*)NULL;

	yaml_parser_delete( &m_parser );

	try
	{
		if( ok && !m_eventList.empty() )
		{
			m_currentEvent = m_eventList.begin();
			while( ( m_currentEvent != m_eventList.end() ) && ( *m_currentEvent )->type != YAML_MAPPING_START_EVENT )
			{
				GetNextEvent();
			}

			m_event = nullptr;
			if( m_currentEvent != m_eventList.end() )
			{
				m_objectMarkers.push_back( m_currentEvent );

				// We've got a valid red file
				return true;
			}
		}
	}
	catch( const IRootReaderException& e )
	{
		m_errorMessage = e.what();
		ClearCachedEvents();
	}


	// If we get here, the file is invalid or empty
	return false;
}

IRoot* YamlReader::CreateObjectHelper( unsigned int objectMarker, IRoot* callingProxy, YR_YIELD_BEHAVIOR yieldBehavior )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	CCP_ASSERT( objectMarker < m_objectMarkers.size() );

#if CCP_STACKLESS
	PyTaskletObject* current{nullptr};
	if( PyGILState_Check() )
	{
		current = reinterpret_cast<PyTaskletObject*>( SchedulerAPI()->PyScheduler_GetCurrent() );
		Py_DECREF( current );
	}

	bool taskletCantYield = !PyOS->CanYield();

	if( m_allowYield )
	{
		// If m_allowYield is true and we get to this point it's because
		// another tasklet is calling CreateObject for the same .red file.
		// We have to yield until the first one is done. If the new caller
		// can't block, we're in trouble.
		if( taskletCantYield )
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
					m_fileName.c_str() );
			}
			return nullptr;
		}

		if( current == m_currentTasklet )
		{
			CCP_LOGERR( "Object from '%S' references itself!", m_fileName.c_str() );
			return NULL;
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

	m_allowYield = !taskletCantYield;
	m_timeSinceYield.Reset();

#endif

	m_currentEvent = m_objectMarkers[objectMarker];

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

	m_event = NULL;
	m_anchorClassMap.clear();
	m_allowYield = false;
#if CCP_STACKLESS
	m_currentTasklet = NULL;
#endif
	return ret;
}

IRoot* YamlReader::CreateObjectWithYield( unsigned int objectMarker, IRoot* callingProxy )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	return CreateObjectHelper( objectMarker, callingProxy, YR_ALLOW_YIELD );
}

IRoot* YamlReader::CreateObject( unsigned int objectMarker, IRoot* callingProxy )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	return CreateObjectHelper( objectMarker, callingProxy, YR_DONT_ALLOW_YIELD );
}


std::string YamlReader::ConstructMessage( const char* message ) const
{
	std::string finalMessage = "";
	if( !m_fileName.empty() )
	{
		finalMessage += CW2A( m_fileName.c_str() );
	}

	if( m_parser.mark.line )
	{
		char buffer[32];
		sprintf_s( buffer, "%" CCP_SIZET_FORMAT, m_parser.mark.line );
		if( !finalMessage.empty() )
		{
			finalMessage += ", line ";
		}
		else
		{
			finalMessage += "Line ";
		}
		finalMessage += buffer;
	}

	if( !finalMessage.empty() )
	{
		finalMessage += ": ";
	}

	finalMessage += message;

	return finalMessage;
}

void YamlReader::ReportWarning( const char* message ) const
{
	if( m_isStrict )
	{
		ReportError( message );
	}
	else
	{
		std::string finalMessage = ConstructMessage( message );
		CCP_LOGWARN( finalMessage.c_str() );
	}
}

void YamlReader::ReportError( const char* message ) const
{
	std::string finalMessage = ConstructMessage( message );
	throw IRootReaderException( finalMessage.c_str() );
}

bool YamlReader::VerifyEvent( yaml_event_type_t expectedType ) const
{
	if( m_event->type == expectedType )
	{
		return true;
	}
	else
	{
		std::string message = "Expected event ";
		message += s_event2string[expectedType];
		message += " but got ";
		message += s_event2string[m_event->type];
		ReportError( message.c_str() );
		return false;
	}
}

void YamlReader::GetNextEvent()
{
	CCP_STATS_ZONE( __FUNCTION__ );

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

	if( m_hasPushedEvent )
	{
		m_hasPushedEvent = false;
		m_event = m_pushedEvent;
	}
	else if( !m_eventList.empty() )
	{
		m_event = *m_currentEvent;
		++m_currentEvent;
	}
	else
	{
		if( m_event )
		{
			yaml_event_delete( m_event );
		}
		else
		{
			m_event = new PoolAllocatedYamlEvent;
			if( !m_event )
			{
				ReportError( "out of memory" );
			}
			m_event->type = YAML_NO_EVENT;
		}
		int ok = yaml_parser_parse( &m_parser, m_event );
		if( !ok )
		{
			if( m_parser.context )
			{
				std::string message = m_parser.context;
				message += " - ";
				message += m_parser.problem;
				ReportError( message.c_str() );
			}
			else
			{
				ReportError( m_parser.problem );
			}
		}
	}
}

void YamlReader::PushEvent()
{
	assert( m_hasPushedEvent == false );

	m_pushedEvent = m_event;
	m_hasPushedEvent = true;
}

template <typename T>
void YamlReader::ReadValueImpl( T& dst )
{
	GetNextEvent();

	dst = 0;
	if( VerifyEvent( YAML_SCALAR_EVENT ) )
	{
#ifdef _WIN32
		dst = _atoi_l( (const char*)m_event->data.scalar.value, m_locale );
#else
		dst = atoi_l( (const char*)m_event->data.scalar.value, m_locale );
#endif
	}
}

void YamlReader::ReadValue( int32_t& dst )
{
	ReadValueImpl( dst );
}

void YamlReader::ReadValue( uint32_t& dst )
{
	GetNextEvent();

	dst = 0;
	if( VerifyEvent( YAML_SCALAR_EVENT ) )
	{
		const char* start = (const char*)m_event->data.scalar.value;
		char* end = nullptr;
#ifdef _WIN32
		dst = _strtoul_l( start, &end, 10, m_locale );
#else
		dst = strtoul_l( start, &end, 10, m_locale );
#endif
	}
}

void YamlReader::ReadValue( uint16_t& dst )
{
	ReadValueImpl( dst );
}

void YamlReader::ReadValue( uint8_t& dst )
{
	ReadValueImpl( dst );
}

void YamlReader::ReadValue( bool& dst )
{
	int32_t tmp;
	ReadValue( tmp );
	dst = ( tmp != 0 );
}

void YamlReader::ReadBinaryBlock( ICustomPersist* customPersist, const char* propertyName )
{
	CCP_STATS_ZONE( __FUNCTION__ );
	// The standard way to store binary in text format
	//http://en.wikipedia.org/wiki/Base64

	GetNextEvent();

	if( VerifyEvent( YAML_SCALAR_EVENT ) )
	{
		// the outputted size is roughly 1.37 the original size
		// bytes = (string_length(encoded_string) - 814) / 1.37
		// So the text string should be bigger than the original binary data
		size_t dataLength = strlen( (const char*)m_event->data.scalar.value );
		uint8_t* buffer = customPersist->AllocateReadBuffer( propertyName, dataLength );
		dataLength = FromBase64( (const uint8_t*)m_event->data.scalar.value, dataLength, (char*)buffer, dataLength );
		customPersist->SetBufferAndSize( propertyName, (unsigned char*)buffer, dataLength );
	}
}

void YamlReader::ReadValue( uint64_t& dst )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	GetNextEvent();

	dst = 0;
	if( VerifyEvent( YAML_SCALAR_EVENT ) )
	{
		const char* start = (const char*)m_event->data.scalar.value;
		char* end = nullptr;
#ifdef _WIN32
		dst = _strtoull_l( start, &end, 10 , m_locale);
#else
		dst = strtoull_l( start, &end, 10, m_locale);
#endif
	}
}

void YamlReader::ReadValue( int64_t& dst )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	GetNextEvent();

	dst = 0;
	if( VerifyEvent( YAML_SCALAR_EVENT ) )
	{
		const char* start = (const char*)m_event->data.scalar.value;
#ifdef _WIN32
		dst = _atoi64_l( start, m_locale );
#else
		char* end = nullptr;
		dst = strtoll_l( start, &end, 10, m_locale );
#endif
	}
}

void YamlReader::SkipValue()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	GetNextEvent();
	yaml_event_type_t type = m_event->type;


	if( type == YAML_MAPPING_START_EVENT )
	{
		ParseUntilMatchingMappingEnd();
	}
	else if( type == YAML_SEQUENCE_START_EVENT )
	{
		ParseUntilMatchingSequenceEnd();
	}

	// Other events should not need pumping.  Like alias, scalar,
	// etc will all be consumed by the GetNextEvent() at the
	// beginning of this function.
}

void YamlReader::ReadValue( double& dst )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	GetNextEvent();

	dst = 0.0;
	if( VerifyEvent( YAML_SCALAR_EVENT ) )
	{
#ifdef _WIN32
		dst = _atof_l( (const char*)m_event->data.scalar.value, m_locale );
#else
		dst = atof_l( (const char*)m_event->data.scalar.value, m_locale);
#endif
	}
}

void YamlReader::ReadValue( float& dst )
{
	double tmp;
	ReadValue( tmp );
	dst = (float)tmp;
}

void YamlReader::ReadValue( int16_t& dst )
{
	ReadValueImpl( dst );
}

void YamlReader::ReadValue( int8_t& dst )
{
	ReadValueImpl( dst );
}

void YamlReader::ReadValue( BlueSharedString& dst )
{
	dst = BlueSharedString( ReadString() );
}

void YamlReader::ReadValue( Vector4& dst )
{
	ReadFloatArray( reinterpret_cast<float*>( &dst ), 4 );
}

void YamlReader::ReadFloat16( uint16_t& dst )
{
	double tmp;
	ReadValue( tmp );
	dst = BlueFloat32To16( float( tmp ) );
}

void YamlReader::ReadFloatArray( float* values, size_t count )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	GetNextEvent();

	if( m_event->type == YAML_SEQUENCE_START_EVENT )
	{
		GetNextEvent();

		for( unsigned i = 0; i < count; ++i )
		{
			if( m_event->type == YAML_SCALAR_EVENT )
			{
#ifdef _WIN32
				values[i] = (float)_atof_l( (const char*)m_event->data.scalar.value, m_locale );
#else
				values[i] = (float)atof_l( (const char*)m_event->data.scalar.value, m_locale );
#endif
				GetNextEvent();
			}
			else
			{
				break;
			}
		}
		VerifyEvent( YAML_SEQUENCE_END_EVENT );
	}
	else
	{
		// Here are some hacks to help with transitioning from TriVector, TriColor and TriQuaternion
		if( count == 3 )
		{
			// See if we can read a TriVector instead
			const char* type = nullptr;
			ReadClassType( type );

			if( !type || strcmp( type, "TriVector" ) != 0 )
			{
				// Not a TriVector, can't make anything of this data
				ReportError( "Expected a float sequence of 3 floats, or a fallback to TriVector" );
				return;
			}

			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;

			std::string memberName;
			while( ReadMemberName( memberName ) )
			{
				float val;
				ReadValue( val );
				if( strcmp( memberName.c_str(), "x" ) == 0 )
				{
					x = val;
				}
				else if( strcmp( memberName.c_str(), "y" ) == 0 )
				{
					y = val;
				}
				else if( strcmp( memberName.c_str(), "z" ) == 0 )
				{
					z = val;
				}
			}
			values[0] = x;
			values[1] = y;
			values[2] = z;
		}
		else if( count == 4 )
		{
			// See if we can read a TriColor instead
			const char* type = nullptr;
			ReadClassType( type );
			if( !type )
			{
				ReportError( "Expected a float sequence of 4 floats, or a fallback to TriColor/TriQuaternion" );
				return;
			}

			const char** componentNames = nullptr;
			if( strcmp( type, "TriColor" ) == 0 )
			{
				static const char* colorComponentNames[4] = { "r", "g", "b", "a" };

				values[0] = 0.0f;
				values[1] = 0.0f;
				values[2] = 0.0f;
				values[3] = 0.0f;

				componentNames = colorComponentNames;
			}
			else if( strcmp( type, "TriQuaternion" ) == 0 )
			{
				static const char* quatComponentNames[4] = { "x", "y", "z", "w" };

				values[0] = 0.0f;
				values[1] = 0.0f;
				values[2] = 0.0f;
				values[3] = 1.0f;

				componentNames = quatComponentNames;
			}
			else
			{
				// Neither a TriColor nor a TriQuaternion, can't make anything of this data
				ReportError( "Expected a float sequence of 4 floats, or a fallback to TriColor/TriQuaternion" );
				return;
			}

			std::string memberName;
			while( ReadMemberName( memberName ) )
			{
				float val;
				ReadValue( val );
				for( int ix = 0; ix < 4; ++ix )
				{
					if( strcmp( memberName.c_str(), componentNames[ix] ) == 0 )
					{
						values[ix] = val;
						break;
					}
				}
			}
		}
	}
}

const wchar_t* YamlReader::ReadWString()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	GetNextEvent();

	if( !VerifyEvent( YAML_SCALAR_EVENT ) )
	{
		return NULL;
	}

	yaml_scalar_style_t style = m_event->data.scalar.style;
	if( style != YAML_DOUBLE_QUOTED_SCALAR_STYLE && style != YAML_SINGLE_QUOTED_SCALAR_STYLE && style != YAML_PLAIN_SCALAR_STYLE )
	{
		// asString should be equal to NULL or something of that nature - it's at least no string
		return NULL;
	}

#ifdef _WIN32
	const char* asString = (const char*)m_event->data.scalar.value;
	int sizeReq = MultiByteToWideChar( CP_UTF8, 0, asString, -1, NULL, 0 );
	wchar_t* ret = (wchar_t*)CCP_MALLOC( (const char*)m_event->data.scalar.tag, sizeReq * sizeof( wchar_t ) );
	MultiByteToWideChar( CP_UTF8, 0, asString, -1, ret, sizeReq );
#else
	auto str = UTF8ToWide( (const char*)m_event->data.scalar.value ) ;

	wchar_t* ret = (wchar_t*)CCP_MALLOC( (const char*)m_event->data.scalar.tag, ( str.length() + 1 ) * sizeof( wchar_t ) );
	std::copy( begin( str ), end( str ), ret );
	ret[str.length()] = 0;
#endif
	m_allocatedStrings.push_back( ret );

	return ret;
}

const char* YamlReader::ReadString()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	GetNextEvent();

	if( !VerifyEvent( YAML_SCALAR_EVENT ) )
	{
		return NULL;
	}

	yaml_scalar_style_t style = m_event->data.scalar.style;
	if( style != YAML_DOUBLE_QUOTED_SCALAR_STYLE && style != YAML_SINGLE_QUOTED_SCALAR_STYLE && style != YAML_PLAIN_SCALAR_STYLE )
	{
		// asString should be equal to NULL or something of that nature - it's at least no string
		return NULL;
	}
	const char* string = (const char*)m_event->data.scalar.value;
	size_t len = strlen( string );
	char* ret = (char*)CCP_MALLOC( (const char*)m_event->data.scalar.tag, len + 1 );
	memcpy( ret, string, len + 1 );
	m_allocatedStrings.push_back( ret );
	return ret;
}

bool YamlReader::ReadClsid( Be::Clsid& clsid )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Read the type
	const char* type;
	ReadClassType( type );

	if( NULL == type )
	{
		// No type specified
		ReportError( "Type of object is not specified" );
		return false;
	}

	// Find the type
	if( !BeClasses->FindClsid( clsid, NULL, type ) )
	{
		// 'type' not found
		std::string message = "Type '" + std::string( type ) + "' not found in Blue class registry";
		ReportWarning( message.c_str() );
		return false;
	}

	return true;
}

void YamlReader::ReadIRoot( IRoot& instance )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// We have essentially four types of values, SCALARS, ALIASES, SEQUENCES and MAPPINGS.
	// By reading the next event we can find out which of these we will get.
	GetNextEvent();

	if( VerifyEvent( YAML_MAPPING_START_EVENT ) )
	{
		// Read the type
		Be::Clsid clsid;
		bool x = ReadClsid( clsid );
		if( !x )
		{
			// Cannot do anything intelligent with this object data - pump it away
			ParseUntilMatchingMappingEnd();
			return;
		}

		// assert that the types match:
		if( !instance.ClassType()->mClassId->IsEqual( clsid ) )
		{
			std::string message = "Expected type '" + std::string( instance.ClassType()->mClassId->GetName() ) + "' but got object of type '" + std::string( clsid.GetName() ) + "' - skipping it";
			ReportWarning( message.c_str() );
			ParseUntilMatchingMappingEnd();
			return;
		}

		ReadMembers( &instance );

		if( m_doInitialize )
		{
			// Call Initialize, if IInitialize is provided
			IInitializePtr init( BlueCastPtr( &instance ) );
			if( init )
			{
				init->Initialize();
			}
		}
	}
}

uint64_t YamlReader::ExtractAnchorFromMappingStart()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	uint64_t anchorID = 0;
	const char* anchor = (const char*)m_event->data.mapping_start.anchor;
	if( anchor )
	{
		// convert anchor to id - support 8 characters in anchor
		size_t count = strlen( anchor );
		size_t anchorIDSize = sizeof( anchorID );
		if( count > anchorIDSize )
		{
			count = anchorIDSize;
		}
		memcpy( &anchorID, anchor, count );
	}

	return anchorID;
}

IRoot* YamlReader::ReadIRootClassInternal()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// We have a class. Can be of any derived type from IRoot.

	// See if the class has an anchor first:
	uint64_t anchorID = ExtractAnchorFromMappingStart();

	// Read the type
	Be::Clsid clsid;
	bool x = ReadClsid( clsid );
	if( !x )
	{
		ParseUntilMatchingMappingEnd();
		return NULL;
	}

	// need to create our own using the factory
	IRoot* instance = NULL;
	BeClasses->CreateInstance( clsid, GetIRootIID(), (void**)&instance );

	try
	{
		IBlueObjectProxyPtr proxy( BlueCastPtr( instance ) );
		if( !m_eventList.empty() && proxy )
		{
			std::string name;

			if( ReadMemberName( name ) )
			{
				if( name == "object" )
				{
					unsigned int objectMarker = (unsigned int)m_objectMarkers.size();
					m_objectMarkers.push_back( m_currentEvent );
					proxy->SetBuilder( this, objectMarker );
					// Check to see if there's an anchor
					GetNextEvent();
					uint64_t anchorID = ExtractAnchorFromMappingStart();
					if( anchorID )
					{
						// This is a proxy object so we insert a NULL which is safe
						// and actually makes sense. <halldor 2009-09-07>
						m_anchorClassMap[anchorID] = NULL;
					}
					PushEvent();
					ParseUntilMatchingMappingEnd();
				}
				else
				{
					char msg[256];
					sprintf_s( msg, "Error handling property '%s' - skipping it", name.c_str() );
					ReportError( msg );
				}
			}
		}
		else
		{
			// Read members
			ReadMembers( instance );
		}
	}
	catch( const IRootReaderException& )
	{
		if( m_isStrict && instance )
		{
			// in case we clean up using exceptions: release `instance` as it is not guaranteed to happen otherwise
			instance->Unlock();
		}
		throw;
	}

	if( m_doInitialize )
	{
		// Call Initialize, if IInitialize is provided
		IInitializePtr init( BlueCastPtr( instance ) );
		if( init )
		{
			CCP_STATS_ZONE( "Blue/yamlReader/init" );
			init->Initialize();
		}
	}

	if( anchorID )
	{
		m_anchorClassMap[anchorID] = instance;
	}

	return instance;
}

IRoot* YamlReader::ReadIRootClass()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	IRoot* instance = NULL;
	// We have essentially three types of values, SCALARS, SEQUENCES and MAPPINGS.
	// By reading the next event we can find out which of these we will get.
	GetNextEvent();

	if( m_event->type == YAML_SCALAR_EVENT )
	{
		// The only supported value here is null or ~ (both mean 'None')
		int i = strcmp( (const char*)m_event->data.scalar.value, "null" );
		if( i != 0 )
		{
			i = strcmp( (const char*)m_event->data.scalar.value, "~" );
		}

		if( i != 0 )
		{
			std::string message = "Expected 'null' or '~' but got: ";
			message += (const char*)m_event->data.scalar.value;
			ReportError( message.c_str() );
		}
	}
	else if( m_event->type == YAML_MAPPING_START_EVENT )
	{
		instance = ReadIRootClassInternal();
	}
	else if( m_event->type == YAML_ALIAS_EVENT )
	{
		// We have an alias to another already read-in class
		yaml_char_t* anchor = m_event->data.alias.anchor;
		if( anchor )
		{
			uint64_t anchorID = 0;
			size_t count = strlen( (const char*)anchor );
			const size_t anchorIDSize = sizeof( anchorID );
			if( count > anchorIDSize )
			{
				count = anchorIDSize;
			}
			memcpy( &anchorID, anchor, count );

			AnchorClassMap_t::iterator it = m_anchorClassMap.find( anchorID );
			if( it != m_anchorClassMap.end() )
			{
				instance = ( *it ).second;
				if( instance )
				{
					instance->Lock();
				}
				else
				{
					// Did not find the aliased anchor in our map!
					std::string message = "Reference to anchor '";
					message += (const char*)anchor;
					message += "' that hasn't been previously set - referencing inside a proxy?";
					ReportWarning( message.c_str() );
				}
			}
			else
			{
				// Did not find the aliased anchor in our map!
				std::string message = "Reference to anchor '";
				message += (const char*)anchor;
				message += "' that hasn't been previously defined!";
				ReportError( message.c_str() );
			}
		}
		else
		{
			// bogus YAML file
			ReportError( "Expected reference to an anchor but no anchor specified!" );
		}
	}
	else
	{
		ReportError( "Expected description of class (SCALAR, MAPPING, or ALIAS) but got other event" );
	}

	return instance;
}

bool YamlReader::ReadVectorBegin()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	GetNextEvent();

	if( m_event->type == YAML_SEQUENCE_START_EVENT )
	{
		return true;
	}
	else if( m_event->type == YAML_MAPPING_START_EVENT )
	{
		// Type mismatch, expected to read vector but got object!
		ReportError( "Expected to read list of objects but got an object instead!" );
		ParseUntilMatchingMappingEnd();
	}
	else
	{
		// Other types of trouble?  How do we handle these - for now push and see if
		// that handles all other cases gracefully.
		PushEvent();
	}

	return false;
}

bool YamlReader::ReadVectorNext()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	GetNextEvent();

	if( m_event->type == YAML_SEQUENCE_END_EVENT )
	{
		return false;
	}
	else
	{
		PushEvent();
		return true;
	}
}

bool YamlReader::ReadVectorEnd()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( m_event->type == YAML_SEQUENCE_END_EVENT )
	{
		return true;
	}
	else
	{
		PushEvent();
		return false;
	}
}

void YamlReader::ReadClassType( const char*& type )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	const char* key;
	ReadScalar( key );
	if( key && ( strcmp( key, "type" ) == 0 ) )
	{
		ReadScalar( type );
	}
	else
	{
		type = NULL;
	}
}

bool YamlReader::ReadMemberName( std::string& name )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Pump YAML for scalar event
	GetNextEvent();

	if( m_event->type == YAML_SCALAR_EVENT )
	{
		name = (const char*)m_event->data.scalar.value;
		return true;
	}
	else
	{
		name = std::string();
		return false;
	}
}


void YamlReader::ReadScalar( const char*& scalar )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	// Pump YAML for scalar event
	GetNextEvent();

	if( VerifyEvent( YAML_SCALAR_EVENT ) )
	{
		scalar = (const char*)m_event->data.scalar.value;
	}
	else
	{
		scalar = NULL;
	}
}

bool YamlReader::IsParserExhausted() const
{
	bool isDone = m_event ? ( m_event->type == YAML_STREAM_END_EVENT ) : false;

	if( m_parser.error != YAML_NO_ERROR )
	{
		ReportError( "Cannot recover from previous errors, end of parsing." );
		isDone = true;
	}

	return isDone;
}

void YamlReader::ParseUntilMatchingEnd( yaml_event_type_t start, yaml_event_type_t end )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	int level = 1;
	while( level && !IsParserExhausted() )
	{
		GetNextEvent();
		if( m_event->type == start )
		{
			++level;
		}
		else if( m_event->type == end )
		{
			--level;
		}
	}
}

void YamlReader::ParseUntilMatchingMappingEnd()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	ParseUntilMatchingEnd( YAML_MAPPING_START_EVENT, YAML_MAPPING_END_EVENT );
}

void YamlReader::ParseUntilMatchingSequenceEnd()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	ParseUntilMatchingEnd( YAML_SEQUENCE_START_EVENT, YAML_SEQUENCE_END_EVENT );
}

void YamlReader::ClearCachedEvents()
{
	CCP_STATS_ZONE( __FUNCTION__ );

	for( EventList_t::iterator it = m_eventList.begin(); it != m_eventList.end(); ++it )
	{
		yaml_event_delete( *it );
		delete *it;
	}

	if( !m_eventList.empty() )
	{
		size_t used, allocated;
		CachedAllocator<PoolAllocatedYamlEvent>::GetInstance()->GetMemoryUsage( used, allocated );
		if( used < allocated / 10 )
		{
			CachedAllocator<PoolAllocatedYamlEvent>::GetInstance()->Compact();
		}
		m_eventList.clear();
	}
}

void YamlReader::ReadMetadata( IRoot* instance )
{
	IWeakObjectPtr weak( BlueCastPtr( instance ) );

	GetNextEvent();
	if( VerifyEvent( YAML_MAPPING_START_EVENT ) )
	{
		GetNextEvent();
		while( m_event->type != YAML_MAPPING_END_EVENT )
		{
			std::string key;
			if( VerifyEvent( YAML_SCALAR_EVENT ) )
			{
				key = (const char*)m_event->data.scalar.value;
			}
			else
			{
				ParseUntilMatchingMappingEnd();
				break;
			}

			const char* value;
			ReadScalar( value );
			if( !value )
			{
				ParseUntilMatchingMappingEnd();
				break;
			}
			if( weak )
			{
				BeObjectMetadata->Set( weak, key.c_str(), value );
			}
			GetNextEvent();
		}
	}
}

void YamlReader::ReadMembers( IRoot* instance )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	std::string name;

	IInitializePtr init( BlueCastPtr( instance ) );
	INotifyPtr notify( BlueCastPtr( instance ) );
	if( init )
	{
		// If IInitialize is provided, don't do individual notifications
		notify = (INotify*)nullptr;
	}

	bool first = true;

	while( ReadMemberName( name ) )
	{
		if( first && name == BLUE_OBJECT_METADATA_KEY )
		{
			ReadMetadata( instance );
			first = false;
			continue;
		}
		first = false;

		try
		{
			HandleAttribute( name.c_str(), instance, notify );
		}
		catch( InvalidAttributeException& )
		{
			char msg[256];
			sprintf_s( msg, "Error handling attribute '%s' - skipping it", name.c_str() );
			ReportWarning( msg );
		}
	}
}

void YamlReader::ReportTypeMismatch( const char* expected, const char* actual )
{
	CCP_LOGWARN( "Expected a type of '%s' (or derived) but got '%s'", expected, actual );
}

bool YamlReader::IsMemoryUsageKnown()
{
	return true;
}

size_t YamlReader::GetMemoryUsage()
{
	// Todo: This is a rough estimate
	return 128 + m_eventList.size() * 64;
}

void YamlReader::ReadList( IList* list )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	list->Remove( -1 );

	if( ReadVectorBegin() )
	{
		ListInfo info;
		list->GetInfo( &info );
		list->SetNotify( NULL );

		while( ReadVectorNext() )
		{
			IRoot* p = ReadIRootClass();
			if( p )
			{
				list->Insert( -1, p );
				p->Unlock(); // List now owns the object
			}
		};
		ReadVectorEnd();

		list->SetNotify( info.mNotify );
		if( info.mNotify )
		{
			info.mNotify->OnListModified( BELIST_LOADFINISHED, 0, 0, NULL, list );
		}
	}
	else
	{
		throw IRootReaderException( "Error reading list" );
	}
}

void YamlReader::ReadDict( IBlueDict* dict )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	if( ReadVectorBegin() )
	{
		while( ReadVectorNext() )
		{
			const char* key = ReadString();
			IRoot* p = ReadIRootClass();
			if( p )
			{
				dict->AssignSubscript( key, p );
				p->Unlock(); // Dict now owns the object
			}
		};
		ReadVectorEnd();
	}
	else
	{
		throw IRootReaderException( "Error reading dict" );
	}
}

namespace
{

bool CompareStructureListDefinitions( const std::vector<BlueStructureDefinition>& def1, const BlueStructureDefinition* def2 )
{
	for( auto it = begin( def1 ); it != end( def1 ); ++it )
	{
		if( !def2->m_name )
		{
			return false;
		}
		if( strcmp( it->m_name, def2->m_name ) )
		{
			return false;
		}
		if( it->m_dataType != def2->m_dataType || it->m_offset != def2->m_offset )
		{
			return false;
		}
		++def2;
	}
	return def2->m_name == nullptr;
}

}

void YamlReader::ReadStructureList( IBlueStructureList* structureList )
{
	GetNextEvent();

	if( m_event->type != YAML_MAPPING_START_EVENT )
	{
		ReportError( "Expected a mapping start event while reading a structure list" );
	}

	std::string memberName;
	if( !ReadMemberName( memberName ) )
	{
		ReportError( "Expected 'structure'" );
	}

	if( strcmp( memberName.c_str(), "structure" ) != 0 )
	{
		char msg[256];
		sprintf_s( msg, "Expected 'structure' - got '%s'", memberName.c_str() );
		ReportError( msg );
	}

	std::vector<BlueStructureDefinition> structureDef;

	if( ReadVectorBegin() )
	{
		while( ReadVectorNext() )
		{
			BlueStructureDefinition sd;

			ReadVectorBegin();

			ReadVectorNext();
			sd.m_name = ReadString();

			ReadVectorNext();
			int32_t dataType;
			ReadValue( dataType );
			sd.m_dataType = (Be::BlueStructureDataType)dataType;

			ReadVectorNext();
			ReadValue( sd.m_offset );

			ReadVectorNext();
			ReadVectorEnd();

			structureDef.push_back( sd );
		}

		ReadVectorEnd();
	}
	else
	{
		ReportError( "Error reading structure list" );
	}

	if( !ReadMemberName( memberName ) )
	{
		ReportError( "Error reading structure list" );
	}

	if( strcmp( memberName.c_str(), "items" ) != 0 )
	{
		char msg[256];
		sprintf_s( msg, "Expected 'items' - got '%s'", memberName.c_str() );
		ReportError( msg );
	}

	BlueStructureDefinition* sdFromList = structureList->GetStructureDefinition();

	if( ReadVectorBegin() )
	{
		if( !CompareStructureListDefinitions( structureDef, sdFromList ) )
		{
			CCP_LOGWARN( "Structure list definition in the .red file does not match run-time definition. Skipping structure list." );

			// Largest structure list element is Vector4 * 4
			float itemStorage[16];
			// Reset offsets so that we can store entrire item in a single element
			for( auto it = begin( structureDef ); it != end( structureDef ); ++it )
			{
				it->m_offset = 0;
			}
			BlueStructureDefinition tail;
			memset( &tail, 0, sizeof( tail ) );
			structureDef.push_back( tail );

			while( ReadVectorNext() )
			{
				ReadVectorBegin();
				ReadVectorNext();

				ReadStructureListItem( &structureDef.front(), itemStorage );

				ReadVectorNext();
				ReadVectorEnd();
			}
		}
		else
		{
			auto structureSize = structureList->GetStructureSize();
			CcpMallocBuffer item( "item", structureSize );

			auto notify = structureList->SetNotify( nullptr );
			ON_BLOCK_EXIT( [&] { structureList->SetNotify( notify ); } );

			while( ReadVectorNext() )
			{
				ReadVectorBegin();
				ReadVectorNext();

				// Make sure any padding in the structure doesn't retain random contents.
				memset( item.get(), 0, structureSize );

				ReadStructureListItem( sdFromList, item.get() );

				structureList->Append( item.get() );

				ReadVectorNext();
				ReadVectorEnd();
			}
		}
		ReadVectorEnd();
	}
	else
	{
		throw IRootReaderException( "Error reading structure list" );
	}

	GetNextEvent();

	if( m_event->type != YAML_MAPPING_END_EVENT )
	{
		ReportError( "Expected a mapping end event while reading a structure list" );
	}
}

void YamlReader::ReadStructureListItem( const BlueStructureDefinition* memberDef, void* item )
{
	while( memberDef->m_name )
	{
		int type = memberDef->m_dataType & Be::DT_TYPE_MASK;

		switch( type )
		{
		// TODO: Support more types
		case Be::DT_INT8:
			if( memberDef->m_dataType & Be::DT_UNSIGNED_BIT )
			{
				ReadStructureListItemMember<uint8_t, sizeof( uint8_t )>( memberDef, item, &YamlReader::ReadValue );
			}
			else
			{
				ReadStructureListItemMember<int8_t, sizeof( int8_t )>( memberDef, item, &YamlReader::ReadValue );
			}
			break;
		case Be::DT_INT16:
			if( memberDef->m_dataType & Be::DT_UNSIGNED_BIT )
			{
				ReadStructureListItemMember<uint16_t, sizeof( uint16_t )>( memberDef, item, &YamlReader::ReadValue );
			}
			else
			{
				ReadStructureListItemMember<int16_t, sizeof( int16_t )>( memberDef, item, &YamlReader::ReadValue );
			}
			break;
		case Be::DT_INT32:
			if( memberDef->m_dataType & Be::DT_UNSIGNED_BIT )
			{
				ReadStructureListItemMember<uint32_t, sizeof( uint32_t )>( memberDef, item, &YamlReader::ReadValue );
			}
			else
			{
				ReadStructureListItemMember<int32_t, sizeof( int32_t )>( memberDef, item, &YamlReader::ReadValue );
			}
			break;
		case Be::DT_FLOAT32:
			ReadStructureListItemMember<float, sizeof( float )>( memberDef, item, &YamlReader::ReadValue );
			break;
		case Be::DT_FLOAT16:
			ReadStructureListItemMember<uint16_t, sizeof( uint16_t )>( memberDef, item, &YamlReader::ReadFloat16 );
			break;
		case Be::DT_FLOAT32x4:
			ReadStructureListItemMember<Vector4, sizeof( float ) * 4>( memberDef, item, &YamlReader::ReadValue );
			break;
		case Be::DT_BOOL8:
			ReadStructureListItemMember<bool, sizeof( bool )>( memberDef, item, &YamlReader::ReadValue );
			break;
		case Be::DT_SHAREDSTRING:
			ReadStructureListItemMember<BlueSharedString, sizeof( BlueSharedString )>( memberDef, item, &YamlReader::ReadValue );
			break;

		default: {
			ReportError( "Unsupported type for structured lists" );
		}
		}

		++memberDef;
	}
}

void YamlReader::SetFileName( const wchar_t* name )
{
	m_fileName = name;
}

void YamlReader::SetDoInitialize( bool b )
{
	m_doInitialize = b;
}

void YamlReader::SetTimeSlice( float t )
{
	m_timeSlice = t;

	if( m_timeSlice < 0.0f )
	{
		m_timeSlice = 0.0f;
	}
}

void YamlReader::GetErrorMessage( std::string& msg )
{
	msg = m_errorMessage;
}

Be::Result<std::string> YamlReader::CreateObjectFromString( const std::string& s, IRoot** obj )
{
	MemStreamPtr ms;
	ms.CreateInstance();
	ms->SetBuffer( (void*)s.c_str(), s.size() );
	return CreateObjectFromStream( ms, obj );
}

Be::Result<std::string> YamlReader::CreateObjectFromStream( IBlueStream* stream, IRoot** obj )
{
	IRoot* res = ReadFromStream( stream );
	if( res )
	{
		*obj = res;
		return Be::Result<std::string>();
	}
	else
	{
		std::string msg;
		GetErrorMessage( msg );
		if( msg.empty() )
		{
			msg = "Failed to create an object from YAML";
		}
		return Be::Result<std::string>( msg );
	}
}

Be::Result<std::string> YamlReader::CreateObjectFromFile( const std::wstring& filename, IRoot** obj )
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
