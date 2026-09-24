// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "Blue.h"
#include "IBlueOS.h"
#include "BlueStatistics.h"
#include "BlueTelemetryZones.h"

#if BLUE_WITH_PYTHON
extern const char *Immortalize( PyObject *s );
#endif

BLUE_DEFINE( CcpStatisticsEntry );

const Be::ClassInfo* CcpStatisticsEntry::ExposeToBlue()
{
	EXPOSURE_BEGIN( CcpStatisticsEntry, "Statistic entry" )

		MAP_PROPERTY
		(
			"name",
			GetName, SetName,
			"Name of statistic"
		)

		MAP_PROPERTY
		(
			"description",
			GetDescription, SetDescription,
			"Description of statistic"
		)
		
		MAP_PROPERTY
		(
			"resetPerFrame",
			GetResetPerFrame, SetResetPerFrame,
			"If set, the statistic is reset every frame"
		)

		MAP_PROPERTY
		(
			"type",
			GetType, SetType,
			"Type of statistic (time, low counter, high counter, memory)"
		)
		
		MAP_PROPERTY_READONLY
		(
			"value",
			GetValue,
			"Value of statistic"
		)

		MAP_PROPERTY_READONLY
		(
			"peak",
			GetPeak,
			"Peak value of statistic"
		)

		MAP_METHOD_AND_WRAP
		(
			"Inc",
			Inc,
			"Increment counter by 1"
		)

		MAP_METHOD_AND_WRAP
		(
			"Dec",
			Dec,
			"Decrement counter by 1"
		)

		MAP_METHOD_AND_WRAP
		(
			"Add",
			Add,
			"Add the given value to the statistic\n"
			":param value: increment value"
		)
		MAP_METHOD_AND_WRAP
		(
			"Set",
			Set,
			"Set the statistic to the given value\n"
			":param value: new value"
		)
		MAP_METHOD_AND_WRAP
		(
			"ResetPeak",
			ResetPeak,
			"Resets the statistic peak value"
		)
	EXPOSURE_END()
}


BLUE_DEFINE( BlueStatisticsTelemetryConfig );

const Be::ClassInfo* BlueStatisticsTelemetryConfig::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueStatisticsTelemetryConfig, "Configuration for telemetry capture" )

	MAP_ATTRIBUTE
	(
		"applicationName",
		m_applicationName,
		"Name provided by profiled application, presented in connection list of a Profiler tool",
		Be::READWRITE
	)

	MAP_ATTRIBUTE
	(
		"captureDuration",
		m_captureDurationSec,
		"Capture duration for a telemetry session in seconds (0 = until manually stopped)",
		Be::READWRITE
	)

	MAP_ATTRIBUTE
	(
		"trackMemory",
		m_trackMemoryAllocations,
		"If set, memory allocations/deallocations are visible in Profiler tool",
		Be::READWRITE
	)

	MAP_ATTRIBUTE
	(
		"trackLocks",
		m_trackLocks,
		"If set, locks (mutexes and semaphores) are visible in Profiler tool",
		Be::READWRITE
	)

	EXPOSURE_END()
}


// Mirrors the CcpColor enum of CcpColorConstants.h, providing the constants of the exposed
// BlueTelemetryColor type. New colors added to the enum need to be added here as well to be
// available from Python.
struct TelemetryColorEntry
{
	const char* name;
	CcpColor color;
};

#define BLUE_TELEMETRY_COLOR( colorName ) { #colorName, CcpColor::colorName }

const TelemetryColorEntry s_telemetryColors[] =
{
	BLUE_TELEMETRY_COLOR( AliceBlue ),
	BLUE_TELEMETRY_COLOR( AntiqueWhite ),
	BLUE_TELEMETRY_COLOR( Aqua ),
	BLUE_TELEMETRY_COLOR( Aquamarine ),
	BLUE_TELEMETRY_COLOR( Azure ),
	BLUE_TELEMETRY_COLOR( Beige ),
	BLUE_TELEMETRY_COLOR( Bisque ),
	BLUE_TELEMETRY_COLOR( Black ),
	BLUE_TELEMETRY_COLOR( BlanchedAlmond ),
	BLUE_TELEMETRY_COLOR( Blue ),
	BLUE_TELEMETRY_COLOR( BlueViolet ),
	BLUE_TELEMETRY_COLOR( Brown ),
	BLUE_TELEMETRY_COLOR( BurlyWood ),
	BLUE_TELEMETRY_COLOR( CadetBlue ),
	BLUE_TELEMETRY_COLOR( Chartreuse ),
	BLUE_TELEMETRY_COLOR( Chocolate ),
	BLUE_TELEMETRY_COLOR( Coral ),
	BLUE_TELEMETRY_COLOR( CornflowerBlue ),
	BLUE_TELEMETRY_COLOR( Cornsilk ),
	BLUE_TELEMETRY_COLOR( Crimson ),
	BLUE_TELEMETRY_COLOR( Cyan ),
	BLUE_TELEMETRY_COLOR( DarkBlue ),
	BLUE_TELEMETRY_COLOR( DarkCyan ),
	BLUE_TELEMETRY_COLOR( DarkGoldenrod ),
	BLUE_TELEMETRY_COLOR( DarkGray ),
	BLUE_TELEMETRY_COLOR( DarkGreen ),
	BLUE_TELEMETRY_COLOR( DarkGrey ),
	BLUE_TELEMETRY_COLOR( DarkKhaki ),
	BLUE_TELEMETRY_COLOR( DarkMagenta ),
	BLUE_TELEMETRY_COLOR( DarkOliveGreen ),
	BLUE_TELEMETRY_COLOR( DarkOrange ),
	BLUE_TELEMETRY_COLOR( DarkOrchid ),
	BLUE_TELEMETRY_COLOR( DarkRed ),
	BLUE_TELEMETRY_COLOR( DarkSalmon ),
	BLUE_TELEMETRY_COLOR( DarkSeaGreen ),
	BLUE_TELEMETRY_COLOR( DarkSlateBlue ),
	BLUE_TELEMETRY_COLOR( DarkSlateGray ),
	BLUE_TELEMETRY_COLOR( DarkSlateGrey ),
	BLUE_TELEMETRY_COLOR( DarkTurquoise ),
	BLUE_TELEMETRY_COLOR( DarkViolet ),
	BLUE_TELEMETRY_COLOR( DeepPink ),
	BLUE_TELEMETRY_COLOR( DeepSkyBlue ),
	BLUE_TELEMETRY_COLOR( DimGray ),
	BLUE_TELEMETRY_COLOR( DimGrey ),
	BLUE_TELEMETRY_COLOR( DodgerBlue ),
	BLUE_TELEMETRY_COLOR( FireBrick ),
	BLUE_TELEMETRY_COLOR( FloralWhite ),
	BLUE_TELEMETRY_COLOR( ForestGreen ),
	BLUE_TELEMETRY_COLOR( Fuchsia ),
	BLUE_TELEMETRY_COLOR( Gainsboro ),
	BLUE_TELEMETRY_COLOR( GhostWhite ),
	BLUE_TELEMETRY_COLOR( Gold ),
	BLUE_TELEMETRY_COLOR( Goldenrod ),
	BLUE_TELEMETRY_COLOR( Gray ),
	BLUE_TELEMETRY_COLOR( Green ),
	BLUE_TELEMETRY_COLOR( GreenYellow ),
	BLUE_TELEMETRY_COLOR( Grey ),
	BLUE_TELEMETRY_COLOR( Honeydew ),
	BLUE_TELEMETRY_COLOR( HotPink ),
	BLUE_TELEMETRY_COLOR( IndianRed ),
	BLUE_TELEMETRY_COLOR( Indigo ),
	BLUE_TELEMETRY_COLOR( Ivory ),
	BLUE_TELEMETRY_COLOR( Khaki ),
	BLUE_TELEMETRY_COLOR( Lavender ),
	BLUE_TELEMETRY_COLOR( LavenderBlush ),
	BLUE_TELEMETRY_COLOR( LawnGreen ),
	BLUE_TELEMETRY_COLOR( LemonChiffon ),
	BLUE_TELEMETRY_COLOR( LightBlue ),
	BLUE_TELEMETRY_COLOR( LightCoral ),
	BLUE_TELEMETRY_COLOR( LightCyan ),
	BLUE_TELEMETRY_COLOR( LightGoldenrodYellow ),
	BLUE_TELEMETRY_COLOR( LightGray ),
	BLUE_TELEMETRY_COLOR( LightGreen ),
	BLUE_TELEMETRY_COLOR( LightGrey ),
	BLUE_TELEMETRY_COLOR( LightPink ),
	BLUE_TELEMETRY_COLOR( LightSalmon ),
	BLUE_TELEMETRY_COLOR( LightSeaGreen ),
	BLUE_TELEMETRY_COLOR( LightSkyBlue ),
	BLUE_TELEMETRY_COLOR( LightSlateGray ),
	BLUE_TELEMETRY_COLOR( LightSlateGrey ),
	BLUE_TELEMETRY_COLOR( LightSteelBlue ),
	BLUE_TELEMETRY_COLOR( LightYellow ),
	BLUE_TELEMETRY_COLOR( Lime ),
	BLUE_TELEMETRY_COLOR( LimeGreen ),
	BLUE_TELEMETRY_COLOR( Linen ),
	BLUE_TELEMETRY_COLOR( Magenta ),
	BLUE_TELEMETRY_COLOR( Maroon ),
	BLUE_TELEMETRY_COLOR( MediumAquamarine ),
	BLUE_TELEMETRY_COLOR( MediumBlue ),
	BLUE_TELEMETRY_COLOR( MediumOrchid ),
	BLUE_TELEMETRY_COLOR( MediumPurple ),
	BLUE_TELEMETRY_COLOR( MediumSeaGreen ),
	BLUE_TELEMETRY_COLOR( MediumSlateBlue ),
	BLUE_TELEMETRY_COLOR( MediumSpringGreen ),
	BLUE_TELEMETRY_COLOR( MediumTurquoise ),
	BLUE_TELEMETRY_COLOR( MediumVioletRed ),
	BLUE_TELEMETRY_COLOR( MidnightBlue ),
	BLUE_TELEMETRY_COLOR( MintCream ),
	BLUE_TELEMETRY_COLOR( MistyRose ),
	BLUE_TELEMETRY_COLOR( Moccasin ),
	BLUE_TELEMETRY_COLOR( NavajoWhite ),
	BLUE_TELEMETRY_COLOR( Navy ),
	BLUE_TELEMETRY_COLOR( OldLace ),
	BLUE_TELEMETRY_COLOR( Olive ),
	BLUE_TELEMETRY_COLOR( OliveDrab ),
	BLUE_TELEMETRY_COLOR( Orange ),
	BLUE_TELEMETRY_COLOR( OrangeRed ),
	BLUE_TELEMETRY_COLOR( Orchid ),
	BLUE_TELEMETRY_COLOR( PaleGoldenrod ),
	BLUE_TELEMETRY_COLOR( PaleGreen ),
	BLUE_TELEMETRY_COLOR( PaleTurquoise ),
	BLUE_TELEMETRY_COLOR( PaleVioletRed ),
	BLUE_TELEMETRY_COLOR( PapayaWhip ),
	BLUE_TELEMETRY_COLOR( PeachPuff ),
	BLUE_TELEMETRY_COLOR( Peru ),
	BLUE_TELEMETRY_COLOR( Pink ),
	BLUE_TELEMETRY_COLOR( Plum ),
	BLUE_TELEMETRY_COLOR( PowderBlue ),
	BLUE_TELEMETRY_COLOR( Purple ),
	BLUE_TELEMETRY_COLOR( RebeccaPurple ),
	BLUE_TELEMETRY_COLOR( Red ),
	BLUE_TELEMETRY_COLOR( RosyBrown ),
	BLUE_TELEMETRY_COLOR( RoyalBlue ),
	BLUE_TELEMETRY_COLOR( SaddleBrown ),
	BLUE_TELEMETRY_COLOR( Salmon ),
	BLUE_TELEMETRY_COLOR( SandyBrown ),
	BLUE_TELEMETRY_COLOR( SeaGreen ),
	BLUE_TELEMETRY_COLOR( SeaShell ),
	BLUE_TELEMETRY_COLOR( Sienna ),
	BLUE_TELEMETRY_COLOR( Silver ),
	BLUE_TELEMETRY_COLOR( SkyBlue ),
	BLUE_TELEMETRY_COLOR( SlateBlue ),
	BLUE_TELEMETRY_COLOR( SlateGray ),
	BLUE_TELEMETRY_COLOR( SlateGrey ),
	BLUE_TELEMETRY_COLOR( Snow ),
	BLUE_TELEMETRY_COLOR( SpringGreen ),
	BLUE_TELEMETRY_COLOR( SteelBlue ),
	BLUE_TELEMETRY_COLOR( Tan ),
	BLUE_TELEMETRY_COLOR( Teal ),
	BLUE_TELEMETRY_COLOR( Thistle ),
	BLUE_TELEMETRY_COLOR( Tomato ),
	BLUE_TELEMETRY_COLOR( Turquoise ),
	BLUE_TELEMETRY_COLOR( Violet ),
	BLUE_TELEMETRY_COLOR( Wheat ),
	BLUE_TELEMETRY_COLOR( White ),
	BLUE_TELEMETRY_COLOR( WhiteSmoke ),
	BLUE_TELEMETRY_COLOR( Yellow ),
	BLUE_TELEMETRY_COLOR( YellowGreen ),
};

#undef BLUE_TELEMETRY_COLOR

#if BLUE_WITH_PYTHON
bool BlueTelemetryColor::RegisterConstants()
{
	// The constants live in the type's dictionary rather than being reachable through the getattr
	// of an instance, as they are looked up on the type itself.
	PyTypeObject* type = ClassType_()->mTypeObject;

	for( const TelemetryColorEntry& entry : s_telemetryColors )
	{
		PyObject* color = BlueWrapObjectForPython( Get( entry.color ) );
		if( !color )
		{
			return false;
		}

		const int result = PyDict_SetItemString( type->tp_dict, entry.name, color );
		Py_DECREF( color );

		if( result != 0 )
		{
			return false;
		}
	}

	// The type was already finalized, so its attribute cache needs to be invalidated
	PyType_Modified( type );

	return true;
}
#endif


BLUE_DEFINE( BlueTelemetryColor );

const Be::ClassInfo* BlueTelemetryColor::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueTelemetryColor, "One of the colors of the CcpColor palette" )

		MAP_PROPERTY_READONLY
		(
			"value",
			GetValue,
			"The color as a 0xRRGGBB value"
		)

		MAP_PROPERTY_READONLY
		(
			"name",
			GetName,
			"The name the color is known by, such as 'SteelBlue'"
		)

		MAP_METHOD_AND_WRAP
		(
			"__str__",
			GetName,
			"The name the color is known by, such as 'SteelBlue'\n"
			":rtype: str"
		)

	EXPOSURE_END()
}


BLUE_DEFINE( BlueTelemetryCategory );

const Be::ClassInfo* BlueTelemetryCategory::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueTelemetryCategory, "A registered Telemetry category, used to group Telemetry zones" )

		MAP_PROPERTY_READONLY
		(
			"name",
			GetName,
			"Name the category was registered with"
		)

		MAP_PROPERTY_READONLY
		(
			"color",
			GetColor,
			"Color the zones of this category are rendered with in a Profiler tool\n"
			":rtype: BlueTelemetryColor"
		)

	EXPOSURE_END()
}


#if BLUE_WITH_PYTHON
namespace
{

// The exposure hands out a fresh wrapper for every category it returns, so comparing wrappers by
// identity - which is what BlueWrapper does - would tell two lookups of the same registered category
// apart. Compare the categories behind them instead, which CcpCore does by name.
PyObject* PyTelemetryCategoryRichCompare( PyObject* self, PyObject* other, int op )
{
	if( op != Py_EQ && op != Py_NE )
	{
		Py_RETURN_NOTIMPLEMENTED;
	}

	const BlueTelemetryCategory* left = BluePythonCast<BlueTelemetryCategory*>( self );
	const BlueTelemetryCategory* right = BluePythonCast<BlueTelemetryCategory*>( other );
	if( !left || !right )
	{
		// Comparing against anything but a category is left to Python, which falls back to identity
		Py_RETURN_NOTIMPLEMENTED;
	}

	const CcpTelemetryCategory* leftCategory = left->GetCategory();
	const CcpTelemetryCategory* rightCategory = right->GetCategory();

	// A wrapper that was never attached to a category only ever equals another unattached one
	const bool equal = ( leftCategory && rightCategory )
		? *leftCategory == *rightCategory
		: leftCategory == rightCategory;

	return PyBool_FromLong( equal == ( op == Py_EQ ) );
}

// Hashes the name, as that is what equality is based on. Categories that compare equal have to hash
// the same for them to work as dictionary keys and in sets.
Py_hash_t PyTelemetryCategoryHash( PyObject* self )
{
	const BlueTelemetryCategory* category = BluePythonCast<BlueTelemetryCategory*>( self );
	if( !category )
	{
		PyErr_SetString( PyExc_TypeError, "expected a BlueTelemetryCategory" );
		return -1;
	}

	const Py_hash_t hash = static_cast<Py_hash_t>( std::hash<std::string>{}( category->GetName() ) );

	// -1 is reserved for signalling an error
	return hash != -1 ? hash : -2;
}

} // anonymous namespace

bool BlueTelemetryCategory::RegisterComparison()
{
	PyTypeObject* type = ClassType_()->mTypeObject;

	// The slots have to be in place before the type is finalized, as that is when Python derives the
	// __eq__, __ne__ and __hash__ found on the type from them. Were they patched in afterwards, the
	// operators would go through the ones set here while those methods would keep comparing and
	// hashing the wrapper identity inherited from BlueWrapper.
	if( type->tp_flags & Py_TPFLAGS_READY )
	{
		PyErr_SetString(
			PyExc_RuntimeError,
			"The BlueTelemetryCategory type was finalized before its comparison was registered" );
		return false;
	}

	type->tp_richcompare = PyTelemetryCategoryRichCompare;
	type->tp_hash = PyTelemetryCategoryHash;

	return true;
}
#endif


BLUE_DEFINE( BlueStatistics );

#if BLUE_WITH_PYTHON
PyObject* BlueStatistics::PyGetDescriptions( PyObject* self, PyObject* args )
{
	PyObject* statsDict = PyDict_New();

	const char* typeNames[CST_TYPE_COUNT] = {"counterHigh", "counterLow", "memory", "time"};

	CcpStatistics::EntryArray& a = CcpStatistics::GetEntryArray();
	for( CcpStatistics::EntryArray::iterator it = a.begin(); it != a.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(2);

		CcpStaticStatisticsEntry* entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyUnicode_FromString( entry->GetDescription().c_str() ) );
		PyTuple_SetItem( pyEntry, 1, PyUnicode_FromString( typeNames[entry->GetType()] ) );

		PyDict_SetItemString( statsDict, entry->GetName().c_str(), pyEntry );
	}

	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(2);

		auto entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyUnicode_FromString( entry->GetDescription().c_str() ) );
		PyTuple_SetItem( pyEntry, 1, PyUnicode_FromString( typeNames[entry->GetType()] ) );

		PyDict_SetItemString( statsDict, entry->GetName().c_str(), pyEntry );
	}

	return statsDict;
}


PyObject* BlueStatistics::PyGetStats( PyObject* self, PyObject* args )
{
	PyObject* statsList = PyList_New(0);

	CcpStatistics::EntryArray& a = CcpStatistics::GetEntryArray();
	for( CcpStatistics::EntryArray::iterator it = a.begin(); it != a.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(3);

		CcpStaticStatisticsEntry* entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyUnicode_FromString( entry->GetName().c_str() ) );
		PyTuple_SetItem( pyEntry, 1, PyFloat_FromDouble( entry->GetValue() ) );
		PyTuple_SetItem( pyEntry, 2, PyFloat_FromDouble( entry->GetPeak() ) );

		// OutputDebugString( (*it)->Describe() );

		PyList_Append( statsList, pyEntry ); 
	}

	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(3);

		auto entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyUnicode_FromString( entry->GetName().c_str() ) );
		PyTuple_SetItem( pyEntry, 1, PyFloat_FromDouble( entry->GetValue() ) );
		PyTuple_SetItem( pyEntry, 2, PyFloat_FromDouble( entry->GetPeak() ) );

		// OutputDebugString( (*it)->Describe() );

		PyList_Append( statsList, pyEntry ); 
	}

	return statsList;
}

PyObject* BlueStatistics::PyGetValues( PyObject* self, PyObject* args )
{
	PyObject* statsDict = PyDict_New();

	CcpStatistics::EntryArray& a = CcpStatistics::GetEntryArray();
	for( auto it = a.begin(); it != a.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(2);

		CcpStaticStatisticsEntry* entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyFloat_FromDouble( entry->GetValue() ) );
		PyTuple_SetItem( pyEntry, 1, PyFloat_FromDouble( entry->GetPeak() ) );

		PyDict_SetItemString( statsDict, entry->GetName().c_str(), pyEntry );
	}

	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		PyObject* pyEntry = PyTuple_New(2);

		auto entry = *it;

		PyTuple_SetItem( pyEntry, 0, PyFloat_FromDouble( entry->GetValue() ) );
		PyTuple_SetItem( pyEntry, 1, PyFloat_FromDouble( entry->GetPeak() ) );

		PyDict_SetItemString( statsDict, entry->GetName().c_str(), pyEntry );
	}

	return statsDict;
}

PyObject* PyResetPeaks( PyObject* self, PyObject* args )
{
	CcpStatistics::EntryArray& a = CcpStatistics::GetEntryArray();

	for( CcpStatistics::EntryArray::iterator it = a.begin(); it != a.end(); ++it )
	{
		auto entry = *it;

		entry->ResetPeak();
	}

	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		auto entry = *it;

		entry->ResetPeak();
	}

	Py_RETURN_NONE;
}

PyObject* PyResetDerived( PyObject* self, PyObject* args )
{
	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		CcpDerivedStatisticsEntry* entry = *it;

		entry->Reset();
		entry->ResetPeak();
	}

	Py_RETURN_NONE;
}

PyObject* BlueStatistics::PyGetSingleStat( PyObject* self, PyObject* args )
{
	const char *text;

	if( !PyArg_ParseTuple(args, "s", &text) )
	{
		return NULL;
	}

	if( !text || !text[0] )
	{
		return NULL;
	}

	const CcpStatistics::EntryArray& a = CcpStatistics::GetEntryArray();
	for( CcpStatistics::EntryArray::const_iterator it = a.begin(); it != a.end(); ++it )
	{
		if( (*it)->GetName() == text )
		{
			return PyFloat_FromDouble( (*it)->GetValue() );
		}		
	}

	auto b = CcpStatistics::GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		if( (*it)->GetName() == text )
		{
			return PyFloat_FromDouble( (*it)->GetValue() );
		}		
	}

	Py_RETURN_NONE;
}

BLUE_DEFINE_EXCEPTION( TelemetryCategoryError, BlueStdRuntimeError );

namespace
{

PyObject* PyEnterZone( PyObject* self, PyObject* args )
{
#if CCP_TELEMETRY_ENABLED
	PyObject* zoneO;

	if( !PyArg_ParseTuple( args, "O", &zoneO ) )
	{
		return nullptr;
	}
	const char* zone = Immortalize( zoneO );
	if( !zone )
	{
		return nullptr;
	}

	auto frame = PyEval_GetFrame();
	if ( !frame )
	{
		return nullptr;
	}
	auto codeObj = PyFrame_GetCode( frame );  // Returns a strong reference
	auto fileName = Immortalize( codeObj->co_filename );
	if (!fileName)
	{
		return nullptr;
	}
	BlueTelemetryEnterZone( frame, zone, fileName, static_cast<uint32_t>( PyFrame_GetLineNumber( frame ) ) );
	Py_XDECREF( codeObj );  // Release the reference to the frame code
#endif
	Py_RETURN_NONE;
}

PyObject* PyLeaveZone( PyObject* self, PyObject* args )
{
#if CCP_TELEMETRY_ENABLED
	BlueTelemetryLeaveZone( PyEval_GetFrame() );
#endif
	Py_RETURN_NONE;
}

PyObject* PyAppendToZone( PyObject* self, PyObject* args )
{
#if CCP_TELEMETRY_ENABLED
	PyObject* appendTextO;

	if( !PyArg_ParseTuple( args, "O", &appendTextO ) )
	{
		return nullptr;
	}

	const char* appendText = Immortalize( appendTextO );
	if( !appendText )
	{
		return nullptr;
	}

	BlueTelemetryZoneAddText( PyEval_GetFrame(), appendText );
#endif
	Py_RETURN_NONE;
}

#if CCP_TELEMETRY_ENABLED
static uint64_t s_timespanId = 0xf00000000;
#endif

PyObject* PyBeginTimeSpan( PyObject* self, PyObject* args )
{
#if CCP_TELEMETRY_ENABLED
	PyObject* labelO;

	if( !PyArg_ParseTuple( args, "O", &labelO ) )
	{
		return nullptr;
	}

	const char* label = Immortalize( labelO );
	if( !label )
	{
		return nullptr;
	}

	++s_timespanId;

	return PyLong_FromLongLong( s_timespanId );
#else
    return PyLong_FromLongLong( 0 );
#endif
}

PyObject* PyEndTimeSpan( PyObject* self, PyObject* args )
{
#if CCP_TELEMETRY_ENABLED
	uint64_t id = 0;
	PyObject* labelO;

	if( !PyArg_ParseTuple( args, "LO", &id, &labelO ) )
	{
		return nullptr;
	}

	const char* label = Immortalize( labelO );
	if( !label )
	{
		return nullptr;
	}
#endif
	Py_RETURN_NONE;
}


PyObject* PyRegister( PyObject* self, PyObject* args )
{
	PyObject* obj = NULL;
	if( !PyArg_ParseTuple( args, "O", &obj ) )
	{
		return NULL;
	}

	CcpStatisticsEntry* stat = BluePythonCast<CcpStatisticsEntry*>( obj );
	if( !stat )
	{
		PyErr_SetString( PyExc_TypeError, "Register expects a CcpStatisticsEntry" );
		return NULL;
	}

		stat->GetAttachedStat();

	Py_RETURN_NONE;
}

PyObject* PyUnregister( PyObject* self, PyObject* args )
{
	PyObject* obj = NULL;
	if( !PyArg_ParseTuple( args, "O", &obj ) )
	{
		return NULL;
	}

	CcpStaticStatisticsEntry* stat = BluePythonCast<CcpStaticStatisticsEntry*>( obj );
	if( !stat )
	{
		PyErr_SetString( PyExc_TypeError, "Unregister expects a CcpStatisticsEntry" );
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject* PyRegisterDerived( PyObject* self, PyObject* args )
{
	PyObject* obj = NULL;
	if( !PyArg_ParseTuple( args, "O", &obj ) )
	{
		return NULL;
	}

	CcpDerivedStatisticsEntry* stat = BluePythonCast<CcpDerivedStatisticsEntry*>( obj );
	if( !stat )
	{
		PyErr_SetString( PyExc_TypeError, "Register expects a CcpDerivedStatisticsEntry" );
		return NULL;
	}

	CcpStatistics::RegisterDerived( stat );

	Py_RETURN_NONE;
}

PyObject* PyUnregisterDerived( PyObject* self, PyObject* args )
{
	PyObject* obj = NULL;
	if( !PyArg_ParseTuple( args, "O", &obj ) )
	{
		return NULL;
	}

	CcpDerivedStatisticsEntry* stat = BluePythonCast<CcpDerivedStatisticsEntry*>( obj );
	if( !stat )
	{
		PyErr_SetString( PyExc_TypeError, "Unregister expects a CcpDerivedStatisticsEntry" );
		return NULL;
	}

	CcpStatistics::UnregisterDerived( stat );

	Py_RETURN_NONE;
}

PyObject* PyFind( PyObject* self, PyObject* args )
{
	CcpStatistics* pThis = BluePythonCast<CcpStatistics*>( self );

	char* name = NULL;
	if( !PyArg_ParseTuple( args, "s", &name ) )
	{
		return NULL;
	}

	const CcpStatistics::EntryArray& a = pThis->GetEntryArray();
	for( CcpStatistics::EntryArray::const_iterator it = a.begin(); it != a.end(); ++it )
	{
		if( strcmp( (*it)->GetName().c_str(), name ) == 0 )
		{
			CcpStaticStatisticsEntry* entry = *it;
			CcpStatisticsEntryPtr pyEntry;
			pyEntry.CreateInstance();
			pyEntry->AttachStat( entry );
			return BlueWrapObjectForPython( pyEntry );
		}		
	}

	auto b = pThis->GetDerivedEntryArray();
	for( auto it = b.begin(); it != b.end(); ++it )
	{
		if( strcmp( (*it)->GetName().c_str(), name ) == 0 )
		{
			CcpStaticStatisticsEntry* entry = *it;
			CcpStatisticsEntryPtr pyEntry;
			pyEntry.CreateInstance();
			pyEntry->AttachStat( entry );
			return BlueWrapObjectForPython( pyEntry );
		}		
	}

	Py_RETURN_NONE;
}

PyObject* WrapTelemetryCategory( const CcpTelemetryCategory& category )
{
	BlueTelemetryCategoryPtr pyCategory;
	pyCategory.CreateInstance();
	pyCategory->AttachCategory( &category );
	return BlueWrapObjectForPython( pyCategory );
}

PyObject* WrapTelemetryCategories( const CcpTelemetryCategories& categories )
{
	PyObject* categoryList = PyList_New( 0 );
	if( !categoryList )
	{
		return nullptr;
	}

	for( const CcpTelemetryCategory& category : categories )
	{
		PyObject* pyCategory = WrapTelemetryCategory( category );
		if( !pyCategory )
		{
			Py_DECREF( categoryList );
			return nullptr;
		}

		// PyList_Append does not steal the reference we got from the wrapper
		PyList_Append( categoryList, pyCategory );
		Py_DECREF( pyCategory );
	}

	return categoryList;
}

// Turns a Python sequence of BlueTelemetryCategory into the list CcpCore expects. Sets a Python
// exception and returns false if the sequence holds anything else.
bool UnwrapTelemetryCategories( PyObject* sequenceObject, CcpTelemetryCategories& categories )
{
	PyObject* fastSequence = PySequence_Fast( sequenceObject, "expected a sequence of BlueTelemetryCategory" );
	if( !fastSequence )
	{
		return false;
	}

	const Py_ssize_t count = PySequence_Fast_GET_SIZE( fastSequence );
	categories.reserve( static_cast<size_t>( count ) );

	for( Py_ssize_t i = 0; i < count; ++i )
	{
		// PySequence_Fast_GET_ITEM hands out a borrowed reference
		PyObject* categoryObject = PySequence_Fast_GET_ITEM( fastSequence, i );
		BlueTelemetryCategory* pyCategory = BluePythonCast<BlueTelemetryCategory*>( categoryObject );
		if( !pyCategory || !pyCategory->GetCategory() )
		{
			Py_DECREF( fastSequence );
			categories.clear();
			PyErr_Format(
				PyExc_TypeError,
				"expected a sequence of BlueTelemetryCategory");
			return false;
		}

		categories.emplace_back( *pyCategory->GetCategory() );
	}

	Py_DECREF( fastSequence );
	return true;
}

PyObject* PyRegisterTelemetryCategory( PyObject* self, PyObject* args )
{
	const char* name = nullptr;
	PyObject* colorObject = nullptr;

	if( !PyArg_ParseTuple( args, "s|O", &name, &colorObject ) )
	{
		return nullptr;
	}

	CcpColor color = CcpColor::SteelBlue;
	if( colorObject )
	{
		BlueTelemetryColor* telemetryColor = BluePythonCast<BlueTelemetryColor*>( colorObject );
		if( !telemetryColor )
		{
			PyErr_SetString(
				PyExc_TypeError,
				"RegisterTelemetryCategory expects a BlueTelemetryColor as the color" );
			return nullptr;
		}

		color = telemetryColor->GetColor();
	}

	const auto& [category, ok] = CcpTelemetryCategoryRegister( name, color );
	if( !ok )
	{
		PyErr_Format(
			BLUE_GET_EXCEPTION( TelemetryCategoryError ),
			"Could not register the Telemetry category '%s'",
			name );
		return nullptr;
	}

	return WrapTelemetryCategory( category );
}

PyObject* PyGetRegisteredTelemetryCategories( PyObject* self, PyObject* args )
{
	return WrapTelemetryCategories( CcpTelemetryGetRegisteredCategories() );
}

PyObject* PySetActiveTelemetryCategories( PyObject* self, PyObject* args )
{
	PyObject* categoriesObject = nullptr;
	if( !PyArg_ParseTuple( args, "O", &categoriesObject ) )
	{
		return nullptr;
	}

	CcpTelemetryCategories categories;
	if( !UnwrapTelemetryCategories( categoriesObject, categories ) )
	{
		return nullptr;
	}

	if( !CcpTelemetrySetActiveCategories( categories ) )
	{
		PyErr_SetString(
			BLUE_GET_EXCEPTION( TelemetryCategoryError ),
			"Could not set the active Telemetry categories" );
		return nullptr;
	}

	Py_RETURN_NONE;
}

PyObject* PyGetActiveTelemetryCategories( PyObject* self, PyObject* args )
{
	return WrapTelemetryCategories( CcpTelemetryGetActiveCategories() );
}

} // anonymous namespace

#endif

const Be::ClassInfo* BlueStatistics::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueStatistics, "Trinity statistics gathering" )
		MAP_METHOD
		(
			"Register",
			PyRegister,
			"Register the given statistic\n"
			":param entry: statistic entry\n"
			":type entry: CcpStatisticsEntry\n"
			":rtype: None"
		)

		MAP_METHOD
		(
			"Unregister",
			PyUnregister,
			"Unregister the given statistic\n"
			":param entry: statistic entry\n"
			":type entry: CcpStatisticsEntry\n"
			":rtype: None"
		)
		
		MAP_METHOD
		(
			"RegisterDerived",
			PyRegisterDerived,
			"Register the given derived statistic\n"
			":param entry: derived statistic entry\n"
			":type entry: CcpDerivedStatisticsEntry\n"
			":rtype: None"
		)

		MAP_METHOD
		(
			"UnregisterDerived",
			PyUnregisterDerived,
			"Unregister the given derived statistic\n"
			":param entry: derived statistic entry\n"
			":type entry: CcpDerivedStatisticsEntry\n"
			":rtype: None"
		)
		
		MAP_METHOD
		(
			"Find",
			PyFind,
			"Find the CcpStatisticsEntry for the given name\n"
			":param name: statistic name\n"
			":type name: str\n"
			":rtype: CcpStatisticsEntry"
		)

		MAP_METHOD
		(
			"GetDescriptions", 
			PyGetDescriptions, 
			"Get description and type of stats\n"
			":rtype: dict[str, CcpStatisticsEntry]"
		)

		MAP_METHOD
		( 
			"GetStats", 
			PyGetStats, 
			"Get current stats\n"
			":rtype: list[(str, float, float)]"
		)

		MAP_METHOD
		( 
			"GetValues", 
			PyGetValues, 
			"Get current stats values. Returns a dict with tuples (value,peak)\n"
			":rtype: dict[str, (float, float)]"
		)

		MAP_METHOD
		( 
			"ResetPeaks", 
			PyResetPeaks, 
			"Resets all stats peak values.\n" 
			":rtype: None"
		)

		MAP_METHOD
		( 
			"ResetDerived", 
			PyResetDerived, 
			"Resets all derived stats (including peak values).\n" 
			":rtype: None"
		)

		MAP_METHOD
		( 
			"GetSingleStat", 
			PyGetSingleStat, 
					
			"Get the current value of a single stat.\n" 
			":param stat: the name of the statistic.\n"
			":type stat: str\n"
			":rtype: CcpStatisticsEntry | None"
		)

		MAP_METHOD_AND_WRAP
		( 
			"SetAccumulator", 
			SetAccumulator, 
			"Sets an accumulator (such as a line graph) for the given statistic\n"
			":param name: stat name\n"
			":param accumulator: new accumulator"
		)

		MAP_METHOD_AND_WRAP
		( 
			"GetAccumulator", 
			GetAccumulator, 
			"Gets an accumulator (such as a line graph) for the given statistic\n"
			":param name: stat name"
		)

		MAP_METHOD
		(
			"RegisterTelemetryCategory",
			PyRegisterTelemetryCategory,
			"Registers a Telemetry category, or returns the already registered category if one exists\n"
			"with the given name. Raises a TelemetryCategoryError if the category could not be registered.\n"
			":param name: category name\n"
			":type name: str\n"
			":param color: color of the category, such as one of the constants of\n"
			"              BlueTelemetryColor, steel blue by default\n"
			":type color: BlueTelemetryColor\n"
			":rtype: BlueTelemetryCategory\n"
			":raises TelemetryCategoryError: if the category could not be registered"
		)

		MAP_METHOD
		(
			"GetRegisteredTelemetryCategories",
			PyGetRegisteredTelemetryCategories,
			"Get all currently registered Telemetry categories. Returns an empty list if Telemetry is not\n"
			"available in this build.\n"
			":rtype: list[BlueTelemetryCategory]"
		)

		MAP_METHOD
		(
			"SetActiveTelemetryCategories",
			PySetActiveTelemetryCategories,
			"Replaces the set of Telemetry categories that are captured. Only zones tagged with an active\n"
			"category are recorded; zones of any other category are cheap no-ops. Pass an empty sequence to\n"
			"stop capturing any category. Raises a TelemetryCategoryError if the active categories could not\n"
			"be set.\n"
			":param categories: the categories to activate, as returned by RegisterTelemetryCategory or\n"
			"                   GetRegisteredTelemetryCategories\n"
			":type categories: list[BlueTelemetryCategory]\n"
			":rtype: None\n"
			":raises TelemetryCategoryError: if the active categories could not be set"
		)

		MAP_METHOD
		(
			"GetActiveTelemetryCategories",
			PyGetActiveTelemetryCategories,
			"Get the Telemetry categories that are currently being captured. Returns an empty list if no\n"
			"category is active.\n"
			":rtype: list[BlueTelemetryCategory]"
		)

#if CCP_TELEMETRY_ENABLED

		MAP_METHOD_AND_WRAP
		(
			"StartTelemetry", 
			StartTelemetry, 
			"Starts a profiling/telemetry session, visible in a Profiler tool.\n"
			":param server: the name of the application being profiled."
		)

		MAP_METHOD_AND_WRAP
		(
			"StartTelemetryFromConfig",
			StartTelemetryFromConfig,
			"Starts a profiling/telemetry session based on config, visible in a Profiler tool.\n"
			":param config: the telemetry configuration to use.\n"
			":type config: BlueStatisticsTelemetryConfig"
		)

		MAP_METHOD_AND_WRAP
		(
			"StartTimedTelemetry", 
			StartTimedTelemetry,
			"Starts a profiling/telemetry session for a set time, visible in a Profiler tool.\n"
			":param server: the name of the application being profiled.\n"
			":param samplePeriod: Time to sample for (in seconds) default of 0 means infinite sampling."
		)

		MAP_METHOD_AND_WRAP
		(
			"StartTelemetryDump", 
			StartTelemetryDump, 
			"Works just like StartTelemetry, except that instead of talking to the server it dumps data to disk"
			"\n in the current users Documents directory."
			"\nWill overwrite preexisting files."
			"\nCannot run alongside a regular TCP based Telemetry session."
			"\nStopped using StopTelemetry."
			"\n"
			":param dumpFolder: a string, the path to dump intermediate data to.\n"
			":param samplePeriod: Time to sample for (in seconds). 0 means infinite sampling."
		)

		MAP_METHOD_AND_WRAP
		(
			"PauseTelemetry",
			PauseTelemetry,
			"Pauses Telemetry capture. (deprecated)"
		)

		MAP_METHOD_AND_WRAP
		(
			"ResumeTelemetry",
			ResumeTelemetry,
			"Resumes Telemetry capture. (deprecated)"
		)

		MAP_METHOD_AND_WRAP
		(
			"StopTelemetry",
			StopTelemetry,
			"Disconnect from a Telemetry server."
		)

		MAP_PROPERTY_READONLY
		(
			"isTelemetryConnectionRequested",
			IsTelemetryConnectionRequested,
			"Is Telemetry profiler connection pending?"
		)

		MAP_PROPERTY_READONLY
		(
			"telemetrySamplingTimeLeft",
			TelemetrySamplingTimeLeft,
			"Seconds left to sample with Telemetry."
		)

		MAP_PROPERTY_READONLY
		(
			"isTelemetryConnected",
			IsTelemetryConnected,
			"Is Telemetry connected to a profiler?"
		)

		MAP_PROPERTY_READONLY
		(
			"isTelemetryPaused",
			IsTelemetryPaused,
			"Is Telemetry paused? (deprecated)"
		)

		MAP_PROPERTY_READONLY
		(
			"isTelemetryStarted",
			IsTelemetryStarted,
			"Is Telemetry instrumentation active?"
		)

		MAP_PROPERTY
		(
			"isCppCaptureEnabled",
			IsCppCaptureEnabled, SetCppCaptureEnabled,
			"If set (default), then both Python and C++ zones are captured. If not set, then\n"
			"only Python zones are captured.\n\n"
			"Setting this to False can reduce the size of Telemetry captures drastically,\n"
			"making it easier to grab longer sessions if you are focusing on Python code."
		)

		MAP_PROPERTY(
			"isTaskletCaptureEnabled",
			IsTaskletCaptureEnabled,
			SetTaskletCaptureEnabled,
			"If set (default), then we record a \"Tasklet\" tack in telemetry with all active tasklets" )

		MAP_PROPERTY(
			"isPythonCaptureEnabled",
			IsPythonCaptureEnabled,
			SetPythonCaptureEnabled,
			"If set, then all Python calls are captured" )

		MAP_ATTRIBUTE(
			"telemetryMaxThreadCount",
			m_telemetryMaxThreadCount,
			"Maximum number of threads/tasklets expected during telemetry capture. This attribute needs to be set before the first\n"
			"call to PrimeTelemetry/StartTelemetry. Changing it after starting telemetry capture will not have an effect.",
			Be::READWRITE )
#endif

		MAP_METHOD_AND_WRAP
		(
			"SetTimelineSectionName",
			SetTimelineSectionName,
			"Changes the name of the global state. This helps identifying regions in the Telemetry timeline view.\n"
			":param name: section name"
		)

		MAP_METHOD
		(
			"EnterZone", 
			PyEnterZone, 
			"Enter a Telemetry zone. There must be a corresponding call to LeaveZone\n"
			":param name: must be static string, such as the name of a function.\n"
			":type name: str\n"
			":rtype: None"
		)
		
		MAP_METHOD
		(
			"LeaveZone", 
			PyLeaveZone, 
			"Leave a Telemetry zone. This must match an EnterZone call.\n"
			":param name: must be static string, such as the name of a function.\n"
			":type name: str\n"
			":rtype: None"
		)
		
		MAP_METHOD
		(
			"AppendToZone", 
			PyAppendToZone, 
			"Appends a string to the latest EnterZone's name."
			"\n"
			"\nArguments:"
			"\ntext - The text to append to the zone name."
		)

		MAP_METHOD
		(
			"BeginTimeSpan",
			PyBeginTimeSpan,
			"Adds a time span to Telemetry.\n"
			":param label: The label to give the time span\n"
			":type label: long\n"
			":returns: an id to use with EndTimeSpan\n"
			":rtype: long"
		)

		MAP_METHOD
		(
			"EndTimeSpan",
			PyEndTimeSpan,
			"Ends a time span started with BeginTimeSpan.\n"
			":param id: the id returned from BeginTimeSpan\n"
			":type id: long\n"
			":param label: The closing label for the time span - can be used to\n"
			"          indicate success or failure, for example\n"
			":type label: str\n"
			":rtype: None"
		)

		MAP_METHOD_AND_WRAP
		(
			"BeginCapture",
			BeginCapture,
			"Begins capturing per-frame data for all statistics entries"
		)

		MAP_METHOD_AND_WRAP
		(
			"EndCapture",
			EndCapture,
			"Ends capturing per-frame data started with BeginCapture. Returns a dict with\n"
			"per-frame statistic entries values"
		)

	EXPOSURE_END()
}
