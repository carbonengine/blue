// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#include "BlueTestHelpers.h"
#include "IBlueResMan.h"

BLUE_DEFINE( BlueTestHelperAttributes );
BLUE_DEFINE( BlueTestHelperProperties );

const Be::ClassInfo* BlueTestHelperAttributes::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueTestHelperAttributes, "BlueTestHelperAttributes is used to help test Blue functionality" )
		MAP_INTERFACE( BlueTestHelperAttributes )

		MAP_ATTRIBUTE
		( 
			"myString", 
			m_myString, 
			"std::string test", 
			Be::READWRITE | Be::PERSIST
		)

		MAP_ATTRIBUTE
		( 
			"myUnicode", 
			m_myUnicode, 
			"std::wstring test", 
			Be::READWRITE | Be::PERSIST
		)

		MAP_ATTRIBUTE
		( 
			"myBool", 
			m_myBool, 
			"bool test", 
			Be::READWRITE | Be::PERSIST
		)

		MAP_ATTRIBUTE
		( 
			"myInt", 
			m_myInt, 
			"int32_t test", 
			Be::READWRITE | Be::PERSIST
		)

		MAP_ATTRIBUTE
		(
			"myUInt",
			m_myUInt,
			"uint32_t test",
			Be::READWRITE | Be::PERSIST
		)

		MAP_ATTRIBUTE
		( 
			"myFloat", 
			m_myFloat, 
			"float test", 
			Be::READWRITE | Be::PERSIST
		)

		MAP_ATTRIBUTE
		( 
			"myDouble", 
			m_myDouble, 
			"double test", 
			Be::READWRITE | Be::PERSIST
		)

		MAP_ATTRIBUTE
		( 
			"myInt64", 
			m_myInt64, 
			"int64_t test", 
			Be::READWRITE | Be::PERSIST
		)

		MAP_ATTRIBUTE
		(
			"myUInt64",
			m_myUInt64,
			"uint64_t test",
			Be::READWRITE | Be::PERSIST
		)

		MAP_ATTRIBUTE
		(
			"myVector",
			m_myVector,
			"vector test",
			Be::READ | Be::PERSIST
		)

		MAP_ATTRIBUTE
		(
			"myDict",
			m_myDict,
			"dict test",
			Be::READ | Be::PERSIST
		)

		MAP_ATTRIBUTE
		(
			"sharedString",
			m_sharedString,
			"shared string test",
			Be::READWRITE | Be::PERSIST
		)

		MAP_ATTRIBUTE
		(
			"sharedStringW",
			m_sharedStringW,
			"shared string test",
			Be::READWRITE | Be::PERSIST 
		)

#if BLUE_WITH_PYTHON
		MAP_ATTRIBUTE
		(
			"myStructureList",
			m_myStructureList,
			"structure list test",
			Be::READ | Be::PERSIST
		)
#endif
	EXPOSURE_END()
}

const Be::ClassInfo* BlueTestHelperProperties::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueTestHelperProperties, "BlueTestHelperProperties is used to help test Blue functionality" )
		MAP_INTERFACE( BlueTestHelperProperties )

		MAP_PROPERTY
		( 
			"myString", 
			GetString, SetString, 
			"std::string test"
		)

		MAP_PROPERTY
		( 
			"myUnicode", 
			GetUnicode, SetUnicode, 
			"std::wstring test"
		)

		MAP_PROPERTY
		( 
			"myBool", 
			GetBool, SetBool, 
			"bool test"
		)

		MAP_PROPERTY
		( 
			"myInt", 
			GetInt, SetInt, 
			"int test"
		)

		MAP_PROPERTY
		(
			"myUInt",
			GetUInt,
			SetUInt,
			"uint test"
		)

		MAP_PROPERTY
		( 
			"myFloat", 
			GetFloat, SetFloat, 
			"float test"
		)

		MAP_PROPERTY
		( 
			"myDouble", 
			GetDouble, SetDouble, 
			"double test"
		)

		MAP_PROPERTY
		( 
			"myInt64", 
			GetInt64, SetInt64, 
			"int64_t test"
		)

		MAP_PROPERTY
		(
			"myUInt64",
			GetUInt64,
			SetUInt64,
			"uint64_t test"
		)
	EXPOSURE_END()
}

BLUE_DEFINE( BlueTestAsyncRes );

IBlueResource* CreateBlueTestAsyncRes( const wchar_t* name )
{
	BlueTestAsyncResPtr p;
	p.CreateInstance();
	return p.Detach();
}

IBlueResource* CreateBlueTestAsyncResEx( const wchar_t* name )
{
	BlueTestAsyncResPtr p;
	p.CreateInstance();
	p->SetEx( true );
	return p.Detach();
}

BLUE_REGISTER_RESOURCE_EXTENSION( L"blueasync", CreateBlueTestAsyncRes );
BLUE_REGISTER_RESOURCE_EXTENSION( L"blueasyncex", CreateBlueTestAsyncResEx );

const Be::ClassInfo* BlueTestAsyncRes::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueTestAsyncRes, "" )

		MAP_INTERFACE( BlueTestAsyncRes )
		MAP_INTERFACE( IBlueResource )
		MAP_INTERFACE( ICacheable )
		MAP_ICACHEABLE_METHODS()

		MAP_ATTRIBUTE( "ex", m_ex, "Set to True when loaded with 'ex' extra parameter", Be::READ )

	EXPOSURE_CHAINTO( BlueAsyncRes )
}

#if BLUE_WITH_PYTHON

BLUE_DEFINE( BlueTestStructureLists );

const Be::ClassInfo* BlueTestStructureLists::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueTestStructureLists, "" )

		MAP_INTERFACE( BlueTestStructureLists )

		MAP_ATTRIBUTE( "uint8", m_uint8, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "int8", m_int8, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "uint16", m_uint16, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "int16", m_int16, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "uint32", m_uint32, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "int32", m_int32, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "float32", m_float32, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "float16", m_float16, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "string", m_string, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "mixed", m_mixed, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "matrix", m_matrix, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "bool", m_bool, "", Be::READ | Be::PERSIST )
		MAP_ATTRIBUTE( "enum", m_enum, "", Be::READ | Be::PERSIST )

	EXPOSURE_END()
}

BLUE_DEFINE( BlueTestEvents );

const Be::ClassInfo* BlueTestEvents::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueTestEvents, "" )

		MAP_INTERFACE( BlueTestEvents )
		MAP_METHOD_AS_METHOD( "SendEvent", PySendEvent, "Sends DoSendEvent via PyOS->SendEvent" );
		MAP_METHOD_AS_METHOD( "PostEvent", PyPostEvent, "Sends OnPostEvent via PyOS->PostEvent" );

	EXPOSURE_END()
}
#endif
