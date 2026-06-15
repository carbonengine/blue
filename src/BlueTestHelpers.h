// Copyright © 2012 CCP ehf.

//
// This file contains some classes that are purely intended to help test
// functionality of blue, such as Python exposure, persistence and resource
// loading.
//

#pragma once
#ifndef BlueTestHelpers_h
#define BlueTestHelpers_h

#include "BlueAsyncRes.h"
#include "ICacheable.h"

BLUE_DECLARE( BlueTestHelperAttributes );
BLUE_DECLARE_VECTOR( BlueTestHelperAttributes );
BLUE_DECLARE_VECTOR_RO( BlueTestHelperAttributes );
BLUE_DECLARE_DICT( BlueTestHelperAttributes );

#if BLUE_WITH_PYTHON

// A simple structure used to test BlueStructureList
struct BlueTestStructure
{
	int intValue;
	float floatValue;
	float vecValue[3];
};

BLUE_DECLARE_STRUCTURE_LIST( BlueTestStructure );

#endif

// This class is used for testing attribute exposure
BLUE_CLASS( BlueTestHelperAttributes ) :
	public IRoot
{
public:
	EXPOSE_TO_BLUE();

	BlueTestHelperAttributes( IRoot* lockobj = NULL );
	~BlueTestHelperAttributes();

private:
	std::string m_myString;
	std::wstring m_myUnicode;
	bool m_myBool;
	int32_t m_myInt;
	uint32_t m_myUInt;
	float m_myFloat;
	double m_myDouble;
	int64_t m_myInt64;
	uint64_t m_myUInt64;
	BlueSharedString m_sharedString;
	BlueSharedStringW m_sharedStringW;

	PBlueTestHelperAttributesVector m_myVector;
	PBlueTestHelperAttributesDict m_myDict;
#if BLUE_WITH_PYTHON
	PBlueTestStructureStructureList m_myStructureList;
#endif
};

TYPEDEF_BLUECLASS( BlueTestHelperAttributes );

// This class is used for testing properties exposed to Python
BLUE_CLASS( BlueTestHelperProperties ) :
	public IRoot
{
public:
	EXPOSE_TO_BLUE();

	BlueTestHelperProperties( IRoot* lockobj = NULL );
	~BlueTestHelperProperties();

	std::string GetString() const;
	void SetString( const std::string& val );

	std::wstring GetUnicode() const;
	void SetUnicode( const std::wstring& val );

	bool GetBool() const;
	void SetBool( bool val );

	int32_t GetInt() const;
	void SetInt( int32_t val );

	uint32_t GetUInt() const;
	void SetUInt( uint32_t val );

	float GetFloat() const;
	void SetFloat( float val );

	double GetDouble() const;
	void SetDouble( double val );

	int64_t GetInt64() const;
	void SetInt64( int64_t val );

	uint64_t GetUInt64() const;
	void SetUInt64( uint64_t val );

private:
	std::string m_myString;
	std::wstring m_myUnicode;
	bool m_myBool;
	int32_t m_myInt;
	uint32_t m_myUInt;
	float m_myFloat;
	double m_myDouble;
	int64_t m_myInt64;
	uint64_t m_myUInt64;
};

TYPEDEF_BLUECLASS( BlueTestHelperProperties );

BLUE_DECLARE( BlueTestAsyncRes );

// This class is used to test the resource manager. It fakes
// load/prepare times by sleeping for short or long periods,
// depending on the first characters in the file name.
BLUE_CLASS( BlueTestAsyncRes ) :
	public BlueAsyncRes,
	public ICacheable
{
public:
	EXPOSE_TO_BLUE();
	BlueTestAsyncRes( IRoot* lockobj = NULL );
	~BlueTestAsyncRes();

	void SetEx( bool b );

	//////////////////////////////////////////////////////////////////////////
	// ICacheable
	bool IsMemoryUsageKnown() override;
	size_t GetMemoryUsage() override;

protected:
	bool DoOpenStream() override;
	LoadingResult DoLoad() override;
	bool DoPrepare() override;

	bool m_ex;
};

TYPEDEF_BLUECLASS_WR_SHUTDOWN( BlueTestAsyncRes );

#if BLUE_WITH_PYTHON

BLUE_DECLARE_STRUCTURE_LIST( uint8_t );
BLUE_DECLARE_STRUCTURE_LIST( int8_t );
BLUE_DECLARE_STRUCTURE_LIST( uint16_t );
BLUE_DECLARE_STRUCTURE_LIST( int16_t );
BLUE_DECLARE_STRUCTURE_LIST( uint32_t );
BLUE_DECLARE_STRUCTURE_LIST( int32_t );
BLUE_DECLARE_STRUCTURE_LIST( float );
BLUE_DECLARE_STRUCTURE_LIST( BlueSharedString );
BLUE_DECLARE_STRUCTURE_LIST( Matrix );

struct BlueTestMixedStructure
{
	float float1;
	BlueSharedString string[2];
};

BLUE_DECLARE_STRUCTURE_LIST( BlueTestMixedStructure );

struct BlueTestBoolStructure
{
	bool bool1;
};

BLUE_DECLARE_STRUCTURE_LIST( BlueTestBoolStructure );

struct BlueTestEnumStructure
{
	uint32_t enumValue;
};

BLUE_DECLARE_STRUCTURE_LIST( BlueTestEnumStructure );

BLUE_CLASS( BlueTestStructureLists ) :
	public IRoot
{
public:
	EXPOSE_TO_BLUE();

	BlueTestStructureLists( IRoot* lockobj = NULL );

private:
	Puint8_tStructureList m_uint8;
	Pint8_tStructureList m_int8;
	Puint16_tStructureList m_uint16;
	Pint16_tStructureList m_int16;
	Puint32_tStructureList m_uint32;
	Pint32_tStructureList m_int32;
	PfloatStructureList m_float32;
	Puint16_tStructureList m_float16;
	PBlueSharedStringStructureList m_string;
	PBlueTestMixedStructureStructureList m_mixed;
	PMatrixStructureList m_matrix;
	PBlueTestBoolStructureStructureList m_bool;
	PBlueTestEnumStructureStructureList m_enum;
};

TYPEDEF_BLUECLASS( BlueTestStructureLists );

BLUE_CLASS( BlueTestEvents ) : public IRoot
{
	public:
		EXPOSE_TO_BLUE();
	private:
		PyObject* PyPostEvent( PyObject* args );
		PyObject* PySendEvent( PyObject * args );
};

TYPEDEF_BLUECLASS( BlueTestEvents );

#endif

#endif // BlueTestHelpers_h
