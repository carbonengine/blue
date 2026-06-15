// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#include "BlueTestHelpers.h"

#if BLUE_WITH_PYTHON
#include "BluePython.h"
#endif

CcpLogChannel_t s_myChannel = CCP_LOG_DEFINE_CHANNEL( "myChannel" );

static BlueStructureDefinition BlueTestStructureDef[] =
{ 
	{ "intValue", Be::INT32_1, 0 }, 
	{ "floatValue", Be::FLOAT32_1, 4 }, 
	{ "vecValue", Be::FLOAT32_3, 8 }, 
	{0} 
};

BlueTestHelperAttributes::BlueTestHelperAttributes( IRoot* lockobj ) :
	m_myBool( false ),
	m_myInt( 0 ),
	m_myUInt( 0 ),
	m_myFloat( 0 ),
	m_myDouble( 0 ),
	m_myInt64( 0 ),
	m_myUInt64( 0 ),
	PARENTLOCK( m_myVector ),
	PARENTLOCK( m_myDict )
#if BLUE_WITH_PYTHON
	,
	PARENTLOCK( m_myStructureList )
#endif
{
#if BLUE_WITH_PYTHON
	m_myStructureList.SetStructureDefinition( BlueTestStructureDef );
#endif
}

BlueTestHelperAttributes::~BlueTestHelperAttributes()
{
}

BlueTestHelperProperties::BlueTestHelperProperties( IRoot* lockobj ) :
	m_myBool( false ),
	m_myInt( 0 ),
	m_myFloat( 0 ),
	m_myDouble( 0 ),
	m_myInt64( 0 )
{
}

BlueTestHelperProperties::~BlueTestHelperProperties()
{
}

bool BlueTestHelperProperties::GetBool() const
{
	return m_myBool;
}

void BlueTestHelperProperties::SetBool( bool val )
{
	m_myBool = val;
}

int32_t BlueTestHelperProperties::GetInt() const
{
	return m_myInt;
}

void BlueTestHelperProperties::SetInt( int32_t val )
{
	m_myInt = val;
}

uint32_t BlueTestHelperProperties::GetUInt() const
{
	return m_myUInt;
}

void BlueTestHelperProperties::SetUInt( uint32_t val )
{
	m_myUInt = val;
}

float BlueTestHelperProperties::GetFloat() const
{
	return m_myFloat;
}

void BlueTestHelperProperties::SetFloat( float val )
{
	m_myFloat = val;
}

double BlueTestHelperProperties::GetDouble() const
{
	return m_myDouble;
}

void BlueTestHelperProperties::SetDouble( double val )
{
	m_myDouble = val;
}

int64_t BlueTestHelperProperties::GetInt64() const
{
	return m_myInt64;
}

void BlueTestHelperProperties::SetInt64( int64_t val )
{
	m_myInt64 = val;
}

uint64_t BlueTestHelperProperties::GetUInt64() const
{
	return m_myUInt64;
}

void BlueTestHelperProperties::SetUInt64( uint64_t val )
{
	m_myUInt64 = val;
}

std::string BlueTestHelperProperties::GetString() const
{
	return m_myString;
}

void BlueTestHelperProperties::SetString( const std::string& val )
{
	m_myString = val;
}

std::wstring BlueTestHelperProperties::GetUnicode() const
{
	return m_myUnicode;
}

void BlueTestHelperProperties::SetUnicode( const std::wstring& val )
{
	m_myUnicode = val;
}

//////////////////////////////////////////////////////////////////////////
// BlueTestAsyncRes
//

//
// Path name gives instructions to the load process:
//
// First character:
//  f - fails to open
//  n - fails to load
//  s - short load - sleeps for ~10ms
//  l - long load - sleeps for ~100ms
//
// Second character:
//  f - fails to prepare
//  s - short prepare - sleeps for 1ms
//  l - long prepare - sleeps for 10ms
//  

BlueTestAsyncRes::BlueTestAsyncRes( IRoot* lockobj ) : m_ex( false )
{
}

BlueTestAsyncRes::~BlueTestAsyncRes()
{
}

bool BlueTestAsyncRes::IsMemoryUsageKnown()
{
	return !IsLoading();
}

size_t BlueTestAsyncRes::GetMemoryUsage()
{
	return 1024;
}

bool BlueTestAsyncRes::DoOpenStream()
{
	std::wstring path = GetPath();
	if( path.size() < 8 )
	{
		// Path must be at least 2 characters, plus the '.' and extension 'async'
		return false;
	}

	if( path[0] == L'f' )
	{
		return false;
	}

	m_dataStream.CreateInstance( Be::Clsid( "blue", "MemStream" ) );
	return true;
}

BlueAsyncRes::LoadingResult BlueTestAsyncRes::DoLoad()
{
	std::wstring path = GetPath();
	if( path[0] == L'n' )
	{
		return LR_FAILED;
	}

	if( path[0] == L's' )
	{
		CcpThreadSleep( 10 ); // todo - randomize this?
	}
	else if( path[0] == L'l' )
	{
		CcpThreadSleep( 100 ); // todo - randomize this?
	}

	return LR_SUCCESS;
}

bool BlueTestAsyncRes::DoPrepare()
{
	std::wstring path = GetPath();
	if( path[1] == L'f' )
	{
		return false;
	}

	if( path[1] == L's' )
	{
		CcpThreadSleep( 1 ); // todo - randomize this?
	}
	else if( path[1] == L'l' )
	{
		CcpThreadSleep( 10 ); // todo - randomize this?
	}

	return true;
}

void BlueTestAsyncRes::SetEx( bool b )
{
	m_ex = true;
}


#if BLUE_WITH_PYTHON

namespace
{

BlueStructureDefinition Uint8Def[] =
{ 
	{ "uint8", Be::UBYTE_1, 0 }, 
	{0} 
};

BlueStructureDefinition Int8Def[] =
{ 
	{ "int8", Be::BYTE_1, 0 }, 
	{0} 
};

BlueStructureDefinition Uint16Def[] =
{ 
	{ "uint16", Be::USHORT_1, 0 }, 
	{0} 
};

BlueStructureDefinition Int16Def[] =
{ 
	{ "int16", Be::SHORT_1, 0 }, 
	{0} 
};

BlueStructureDefinition Uint32Def[] =
{ 
	{ "uint32", Be::UINT32_1, 0 }, 
	{0} 
};

BlueStructureDefinition Int32Def[] =
{ 
	{ "int32", Be::INT32_1, 0 }, 
	{0} 
};

BlueStructureDefinition Float32Def[] =
{ 
	{ "float32", Be::FLOAT32_1, 0 }, 
	{0} 
};

BlueStructureDefinition Float16Def[] =
{ 
	{ "float16", Be::FLOAT16_1, 0 }, 
	{0} 
};

BlueStructureDefinition SharedStringDef[] =
{ 
	{ "string", Be::SHAREDSTRING_1, 0 }, 
	{0} 
};

BlueStructureDefinition MixedDef[] =
{ 
	{ "float", Be::FLOAT32_1, 0 }, 
	{ "string", Be::SHAREDSTRING_2, offsetof( BlueTestMixedStructure, string ) }, 
	{0} 
};

BlueStructureDefinition MatrixDef[] =
{ 
	{ "matrix", Be::MATRIX32_4x4, 0 }, 
	{0} 
};

BlueStructureDefinition BoolDef[] =
{ 
	{ "bool", Be::BOOL8_1, offsetof( BlueTestBoolStructure, bool1 ) }, 
	{0} 
};

Be::VarChooser BlueTestEnumChooser[] =
{
	{
		"ValueOne",
		BeCast( 1 ),
		""
	},
	{
		"ValueTwo",
		BeCast( 2 ),
		""
	},
	{ 0 }
};

BlueStructureDefinition EnumDef[] =
{ 
	{ "enumValue", Be::UINT32_1, offsetof( BlueTestEnumStructure, enumValue ), BlueTestEnumChooser }, 
	{0} 
};

}

BlueTestStructureLists::BlueTestStructureLists( IRoot* lockobj )
	:PARENTLOCK( m_uint8 ),
	PARENTLOCK( m_int8 ),
	PARENTLOCK( m_uint16 ),
	PARENTLOCK( m_int16 ),
	PARENTLOCK( m_uint32 ),
	PARENTLOCK( m_int32 ),
	PARENTLOCK( m_float32 ),
	PARENTLOCK( m_float16 ),
	PARENTLOCK( m_string ),
	PARENTLOCK( m_mixed ),
	PARENTLOCK( m_matrix ),
	PARENTLOCK( m_bool ),
	PARENTLOCK( m_enum )
{
	m_uint8.SetStructureDefinition( Uint8Def );
	m_int8.SetStructureDefinition( Int8Def );
	m_uint16.SetStructureDefinition( Uint16Def );
	m_int16.SetStructureDefinition( Int16Def );
	m_uint32.SetStructureDefinition( Uint32Def );
	m_int32.SetStructureDefinition( Int32Def );
	m_float32.SetStructureDefinition( Float32Def );
	m_float16.SetStructureDefinition( Float16Def );
	m_string.SetStructureDefinition( SharedStringDef );
	m_mixed.SetStructureDefinition( MixedDef );
	m_matrix.SetStructureDefinition( MatrixDef );
	m_bool.SetStructureDefinition( BoolDef );
	m_enum.SetStructureDefinition( EnumDef );
}

PyObject* BlueTestEvents::PyPostEvent( PyObject* args )
{
	int first;
	double second;

	if( !PyArg_ParseTuple( args, "id", &first, &second ) )
		return nullptr;

	bool success = PyOS->PostEvent(
		GetRawRoot(),
		"BlueTestEvents::OnPostEvent",
		"OnPostEvent",
		"id",
		first,
		second);

	if( !success )
	{
		Py_RETURN_FALSE;
	}
	Py_RETURN_TRUE;
}

PyObject* BlueTestEvents::PySendEvent( PyObject* args )
{
	int first;
	double second;

	if( !PyArg_ParseTuple( args, "id", &first, &second ) )
		return nullptr;

	bool success = PyOS->SendEvent(
		GetRawRoot(),
		"BlueTestEvents::DoSendEvent",
		"DoSendEvent",
		nullptr,
		"id",
		first,
		second );

	if( !success )
	{
		Py_RETURN_FALSE;
	}
	Py_RETURN_TRUE;
}

#endif