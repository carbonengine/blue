// Copyright © 2003 CCP ehf.

#include "StdAfx.h"
#if BLUE_WITH_PYTHON

// TODO: PyTemplates needs a lot of work to compile under clang

#include "Marshal.h"
#include "BlueMemStream.h"
#include "BlueResFile.h"
#include <BlueStatistics.h>
#include "PyTemplates.h"
#include "ITaskletTimer.h"
#include "IBluePython.h"

#include "PyRowset.h"

#include <algorithm>
#include "zlib.h"

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "Marshal" );


//--------------------------------------------------------------------
// Static members
//--------------------------------------------------------------------
// The Python type
const PyTypeObject* MarshalType = Marshal::GetType();

// EOF exception
static uint32_t const CODE_READPASTEOF = 0xE0000001;
static uint32_t const CODE_INVALIDMAPREF = 0xE0000002;


///////////////////////////////////////////////////////////////////
// Initialize the blue module
bool MarshalInit(PyObject *module)
{
	memset( Marshal::s_typeHandlers, 0, sizeof( Marshal::s_typeHandlers ) );

	Marshal::s_typeHandlers[ TY_NONE ] = &Marshal::ReadObjectNone;
	Marshal::s_typeHandlers[ TY_GLOBAL ] = &Marshal::ReadObjectGlobal;
	Marshal::s_typeHandlers[ TY_INT64 ] = &Marshal::ReadObjectInt64;
	Marshal::s_typeHandlers[ TY_INT32 ] = &Marshal::ReadObjectInt32;
	Marshal::s_typeHandlers[ TY_INT16 ] = &Marshal::ReadObjectInt16;
	Marshal::s_typeHandlers[ TY_INT8 ] = &Marshal::ReadObjectInt8;
	Marshal::s_typeHandlers[ TY_INT_N1 ] = &Marshal::ReadObjectIntNegativeOne;
	Marshal::s_typeHandlers[ TY_INT_0 ] = &Marshal::ReadObjectIntZero;
	Marshal::s_typeHandlers[ TY_INT_1 ] = &Marshal::ReadObjectIntOne;
	Marshal::s_typeHandlers[ TY_FLOAT ] = &Marshal::ReadObjectFloat;
	Marshal::s_typeHandlers[ TY_FLOAT_0 ] = &Marshal::ReadObjectFloatZero;
	Marshal::s_typeHandlers[ TY_TRUE ] = &Marshal::ReadObjectTrue;
	Marshal::s_typeHandlers[ TY_FALSE ] = &Marshal::ReadObjectFalse;
	Marshal::s_typeHandlers[ TY_STR	] = &Marshal::ReadObjectUnicode;
	Marshal::s_typeHandlers[ TY_STR_EMPTY ] = &Marshal::ReadObjectStrEmpty;
	Marshal::s_typeHandlers[ TY_STR_CHAR ] = &Marshal::ReadObjectStrChar;
	Marshal::s_typeHandlers[ TY_STR_SHORT ] = &Marshal::ReadObjectStrShort;
	Marshal::s_typeHandlers[ TY_STR_TABLE ] = &Marshal::ReadObjectStrTable;
	Marshal::s_typeHandlers[ TY_UNICODE	] = &Marshal::ReadObjectUnicode;
	Marshal::s_typeHandlers[ TY_UNICODE_0 ] = &Marshal::ReadObjectUnicode0;
	Marshal::s_typeHandlers[ TY_UNICODE_1 ] = &Marshal::ReadObjectUnicode1;
	Marshal::s_typeHandlers[ TY_BUFFER ] = &Marshal::ReadObjectBuffer;
	Marshal::s_typeHandlers[ TY_TUPLE0 ] = &Marshal::ReadObjectTuple0;
	Marshal::s_typeHandlers[ TY_TUPLE1 ] = &Marshal::ReadObjectTuple1;
	Marshal::s_typeHandlers[ TY_TUPLE2 ] = &Marshal::ReadObjectTuple2;
	Marshal::s_typeHandlers[ TY_TUPLE ] = &Marshal::ReadObjectTuple;
	Marshal::s_typeHandlers[ TY_LIST0 ] = &Marshal::ReadObjectList0;
	Marshal::s_typeHandlers[ TY_LIST1 ] = &Marshal::ReadObjectList1;
	Marshal::s_typeHandlers[ TY_LIST ] = &Marshal::ReadObjectList;
	Marshal::s_typeHandlers[ TY_DICT ] = &Marshal::ReadObjectDict;
	Marshal::s_typeHandlers[ TY_CALLBACK ] = &Marshal::ReadObjectCallback;
	Marshal::s_typeHandlers[ TY_PICKLE ] = &Marshal::ReadObjectPickle;
	Marshal::s_typeHandlers[ TY_PICKLER	] = &Marshal::ReadObjectPickler;
	Marshal::s_typeHandlers[ TY_REFERENCE ] = &Marshal::ReadObjectReference;
	Marshal::s_typeHandlers[ TY_CRC_CHECK ] = &Marshal::ReadObjectCrcCheck;
	Marshal::s_typeHandlers[ TY_REDUCE ] = &Marshal::ReadObjectReduce;
	Marshal::s_typeHandlers[ TY_NEWOBJ ] = &Marshal::ReadObjectNewobj;
	Marshal::s_typeHandlers[ TY_DBROW ] = &Marshal::ReadObjectDBRow;
	Marshal::s_typeHandlers[ TY_WSTREAM	] = &Marshal::ReadObjectWStream;
	Marshal::s_typeHandlers[ TY_LONG ] = &Marshal::ReadObjectLong;
#ifdef PY27_COMPATIBILITY_MODE
	// Handle types that are never sent by Python 3.
	Marshal::s_typeHandlers[ TY_INSTANCE ] = &Marshal::ReadObjectInstance;
	Marshal::s_typeHandlers[ TY_UTF8_OBSOLETE ] = &Marshal::ReadObjectUnicode;
#endif

	Py_INCREF( WriteStream::GetType() );
	if (PyModule_AddObject(module, "MarshalStream", (PyObject*)WriteStream::GetType()))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////
//
// Public member functions
//
//////////////////////////////////////////////////////////////////////

bool Marshal::Incrementor::Test(int limit)
{
	if (mI > limit) {
		PyErr_Format(PyExc_RuntimeError, "Maximum recursion depth of %d reached", limit);
		return false;
	}
	return true;
}

//--------------------------------------------------------------------
// Marshal::New()
Marshal *Marshal::sMarshal = 0;
Marshal *Marshal::New()
{
	if (sMarshal) {
		Py_INCREF(sMarshal);
		return sMarshal;
	}
	Marshal *o = CCP_NEW("Marshal") Marshal;
	if (!o->Init()) {
		Py_DECREF(o);
		return 0;
	}
	sMarshal = o;
	Py_INCREF(sMarshal);
	return o;
}

Marshal::Marshal() :
	mStrTable(PyDict_New()),
	mStrTableRev(PyList_New(0)),
	mGuidTable(PyDict_New()),

	// Initialize python stock objects
	mStock_IntN1(PyLong_FromLong(-1)),
	mStock_Int0(PyLong_FromLong(0)),
	mStock_Int1(PyLong_FromLong(1)),
	mStock_Float0(PyFloat_FromDouble(0.0)),
	mStock_EmptyStr(""),
	mStock_ModuleString("__module__"),
	mStock_GuidString("__guid__"),
	mStock_GetState("__getstate__"),
	mStock_SetState("__setstate__"),
	mStock_Dict("__dict__"),
	mStock_New("__new__"),
	mStock_Name("__name__"),
	mStock_Append("append"),
	mStock_SetItem("__setitem__"),

	// Tasklet timing context strings
	mTimer_Load(PyUnicode_FromString("Marshal::Load")),
	mTimer_Save(PyUnicode_FromString("Marshal::Save")),
	mTimer_GetState(PyUnicode_FromString("Marshal::GetState")),
	mTimer_SetState(PyUnicode_FromString("Marshal::SetState")),
	mTimer_SaveCallback(PyUnicode_FromString("Marshal::SaveCallback")),
	mTimer_LoadCallback(PyUnicode_FromString("Marshal::LoadCallback")),

	m_typesLoaded(64, 0),
	m_typesSaved(64, 0)
{
	mMapSent = mTotalSent = 0;
	mGlobalsBlacklistInit = false;
	mRecursionLevel = 0;
	mCollectWhitelist = 0;
}



// String table for marshaller - was previously stored in script/sys/strings.txt
const char* MARSHAL_STRINGS[] = {
	"*corpid",
	"*locationid",
	"age",
	"Asteroid",
	"authentication",
	"ballID",
	"beyonce",
	"bloodlineID",
	"capacity",
	"categoryID",
	"character",
	"characterID",
	"characterName",
	"characterType",
	"charID",
	"chatx",
	"clientID",
	"config",
	"contraband",
	"corporationDateTime",
	"corporationID",
	"createDateTime",
	"customInfo",
	"description",
	"divisionID",
	"DoDestinyUpdate",
	"dogmaIM",
	"EVE System",
	"flag",
	"foo.SlimItem",
	"gangID",
	"Gemini",
	"gender",
	"graphicID",
	"groupID",
	"header",
	"idName",
	"invbroker",
	"itemID",
	"items",
	"jumps",
	"line",
	"lines",
	"locationID",
	"locationName",
	"macho.CallReq",
	"macho.CallRsp",
	"macho.MachoAddress",
	"macho.Notification",
	"macho.SessionChangeNotification",
	"modules",
	"name",
	"objectCaching",
	"objectCaching.CachedObject",
	"OnChatJoin",
	"OnChatLeave",
	"OnChatSpeak",
	"OnGodmaShipEffect",
	"OnItemChange",
	"OnModuleAttributeChange",
	"OnMultiEvent",
	"orbitID",
	"ownerID",
	"ownerName",
	"quantity",
	"raceID",
	"RowClass",
	"securityStatus",
	"Sentry Gun",
	"sessionchange",
	"singleton",
	"skillEffect",
	"squadronID",
	"typeID",
	"used",
	"userID",
	"util.CachedObject",
	"util.IndexRowset",
	"util.Moniker",
	"util.Row",
	"util.Rowset",
	"*multicastID",
	"AddBalls",
	"AttackHit3",
	"AttackHit3R",
	"AttackHit4R",
	"DoDestinyUpdates",
	"GetLocationsEx",
	"InvalidateCachedObjects",
	"JoinChannel",
	"LSC",
	"LaunchMissile",
	"LeaveChannel",
	"OID+",
	"OID-",
	"OnAggressionChange",
	"OnCharGangChange",
	"OnCharNoLongerInStation",
	"OnCharNowInStation",
	"OnDamageMessage",
	"OnDamageStateChange",
	"OnEffectHit",
	"OnGangDamageStateChange",
	"OnLSC",
	"OnSpecialFX",
	"OnTarget",
	"RemoveBalls",
	"SendMessage",
	"SetMaxSpeed",
	"SetSpeedFraction",
	"TerminalExplosion",
	"address",
	"alert",
	"allianceID",
	"allianceid",
	"bid",
	"bookmark",
	"bounty",
	"channel",
	"charid",
	"constellationid",
	"corpID",
	"corpid",
	"corprole",
	"damage",
	"duration",
	"effects.Laser",
	"gangid",
	"gangrole",
	"hqID",
	"issued",
	"jit",
	"languageID",
	"locationid",
	"machoVersion",
	"marketProxy",
	"minVolume",
	"orderID",
	"price",
	"range",
	"regionID",
	"regionid",
	"role",
	"rolesAtAll",
	"rolesAtBase",
	"rolesAtHQ",
	"rolesAtOther",
	"shipid",
	"sn",
	"solarSystemID",
	"solarsystemid",
	"solarsystemid2",
	"source",
	"splash",
	"stationID",
	"stationid",
	"target",
	"userType",
	"userid",
	"volEntered",
	"volRemaining",
	"weapon",

	"agent.missionTemplatizedContent_BasicKillMission",
	"agent.missionTemplatizedContent_ResearchKillMission",
	"agent.missionTemplatizedContent_StorylineKillMission",
	"agent.missionTemplatizedContent_GenericStorylineKillMission",
	"agent.missionTemplatizedContent_BasicCourierMission",
	"agent.missionTemplatizedContent_ResearchCourierMission",
	"agent.missionTemplatizedContent_StorylineCourierMission",
	"agent.missionTemplatizedContent_GenericStorylineCourierMission",
	"agent.missionTemplatizedContent_BasicTradeMission",
	"agent.missionTemplatizedContent_ResearchTradeMission",
	"agent.missionTemplatizedContent_StorylineTradeMission",
	"agent.missionTemplatizedContent_GenericStorylineTradeMission",
	"agent.offerTemplatizedContent_BasicExchangeOffer",
	"agent.offerTemplatizedContent_BasicExchangeOffer_ContrabandDemand",
	"agent.offerTemplatizedContent_BasicExchangeOffer_Crafting",

	"agent.LoyaltyPoints",
	"agent.ResearchPoints",
	"agent.Credits",
	"agent.Item",
	"agent.Entity",
	"agent.Objective",
	"agent.FetchObjective",
	"agent.EncounterObjective",
	"agent.DungeonObjective",
	"agent.TransportObjective",
	"agent.Reward",
	"agent.TimeBonusReward",
	"agent.MissionReferral",
	"agent.Location",

	"agent.StandardMissionDetails",
	"agent.OfferDetails",
	"agent.ResearchMissionDetails",
	"agent.StorylineMissionDetails",

};

int MARSHAL_STRING_ARRAY_LEN = sizeof(MARSHAL_STRINGS) / sizeof (MARSHAL_STRINGS[0]);

bool Marshal::Init()
	
{
	BluePy pickler(PyImport_ImportModule("pickle"));
	if (!pickler)
		return 0;
	mPickleDumps = BluePy(PyObject_GetAttrString(pickler, "dumps"));
	mPickleLoads = BluePy(PyObject_GetAttrString(pickler, "loads"));
	
	for (int i = 0; i < MARSHAL_STRING_ARRAY_LEN; i++)
	{
		PyObject* str = PyUnicode_InternFromString(MARSHAL_STRINGS[i]);
		PyObject* index = PyLong_FromLong(i + 1);
		PyDict_SetItem(mStrTable, str, index);
		PyList_Append(mStrTableRev, str);
		Py_DECREF(str);
		Py_DECREF(index);
	}

	return true;
}


//--------------------------------------------------------------------
Marshal::~Marshal()
{}



//--------------------------------------------------------------------
// WriteStream utility
//--------------------------------------------------------------------

#define RETFAIL(x) if (!(x)) return false;

//Object constructor - destructor
PyObject *WriteStream::_New(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
	void *raw = _Alloc(subtype);
	if (!raw)
		return 0;
	WriteStream *stream = new(raw) WriteStream; //c++ constructor
	if (args) {
		char *data;
		int datalen;
		if (!PyArg_ParseTuple(args, "s#", &data, &datalen))
			return 0;
		if (!stream->Init(data, datalen))
			return 0;
	}
	return stream;
}

int WriteStream::sNStreams = 0;
size_t WriteStream::sMemUse = 0;

PyObject *Marshal::GetNWriteStreams()
{
	return PyLong_FromLong(WriteStream::sNStreams);
}
PyObject *Marshal::GetWriteStreamMem()
{
	return PyLong_FromUnsignedLongLong(WriteStream::sMemUse);
}

WriteStream::WriteStream()
{
	mBuff = 0;
	mPos = mSize = 0;
	mFinalized = false;
	mUseChecksum = false;
	sNStreams++;
	sMemUse += sizeof(*this);
	mTypeFlags = TY_INVALID;
}


WriteStream::~WriteStream()
{
	sNStreams--;
	sMemUse -= sizeof(*this);
	sMemUse -= mSize;
	CCP_FREE(mBuff);
}


bool WriteStream::Init(PyObject* callback, bool useChecksum, int version)
{
	// allocate output buffer
	size_t size =128;
	mBuff = (char*)CCP_MALLOC( "WriteStream/mBuff", size*sizeof(char));
	if (!mBuff)
		return PyErr_NoMemory(), false;
	sMemUse += size;
	mSize = size;
	mPos = 0;
	mCallback = BluePy(callback, true);
	
	mVersion = version;
	mUseChecksum = useChecksum;
	mChecksumPos = -1;
	mFinalized = false;

	if (mVersion == 0) {
		RETFAIL(WriteType(TY_SIGNATURE));
		RETFAIL(Write<int>(0)); // make room for num. shared objects.
	} else {
		RETFAIL(WriteType(TY_SIGNATURE2));
		RETFAIL(Write<unsigned char>((unsigned char)version));
	}
	if (useChecksum) {
		RETFAIL(WriteType(TY_CRC_CHECK));
		mChecksumPos = GetPos();
		RETFAIL(Write<int>(0)); // make room for checksum
	}
	return true;
}

//to init from a string.  When unpickling.
bool WriteStream::Init(const void *data, size_t datalen)
{
	mBuff = (char*)CCP_MALLOC( "WriteStream/mBuff", datalen);
	if (!mBuff)
		return PyErr_NoMemory(), false;
	memcpy(mBuff, data, datalen);
	mSize = mPos = datalen;
	sMemUse += datalen;
	mFinalized = true;
	return true;
}


bool WriteStream::InitType(PyTypeObject *type)
{
	//set up the buffer interface
	static PyBufferProcs bufferProcs = {
		getbuffer,
		nullptr
	};
	type->tp_as_buffer = &bufferProcs;

	static PySequenceMethods sequenceMethods = {
		SequenceLength,
		0,
		0,
		SequenceGet,
		0,
		0,
		0
	};
	type->tp_as_sequence = &sequenceMethods;

	static PyMappingMethods mappingMethods = {
		SequenceLength,
		SequenceSubscript,
		nullptr
	};
	type->tp_as_mapping = &mappingMethods;

	type->tp_str = &tp_str_method;
	type->tp_repr = &tp_repr_method;
	type->tp_richcompare = &tp_richcompare_method;

	return true;
}

bool WriteStream::WriteType(PYTYPES type)
{
	//update with the latest typeflags for sharing
	int itype = type;
	itype |= (int)mTypeFlags;
	mTypeFlags = TY_INVALID;
	return Write<unsigned char>(itype);
}

bool WriteStream::WriteBuff(const void* buff, size_t size)
{
	return WriteInteger((int)size) && WriteBuffWoSize(buff, size);
}

bool WriteStream::WriteBuffWoSize(const void* buff, size_t size)
{
	if (!size)
		return true;
	if (Prepare(size)) {
		memcpy(mBuff + mPos, buff, size);
		mPos += size;
		return true;
	} else
		return false;
}

	//later add writing of large integers.
bool WriteStream::WriteInteger(int i)
{
	if (i >= 0 && i < UCHAR_MAX)
		return Write<uint8_t>(i);
	else
		return Write<uint8_t>(UCHAR_MAX) && Write<int32_t>(i);
}

PyObject *WriteStream::write(PyObject *s)
{
	if (mFinalized)
		//after finalization, object is immutable
		return PyErr_SetString(PyExc_IOError, "finalized buffer is read only"), nullptr;
	Py_ssize_t len;
	char *data;
	if (PyBytes_AsStringAndSize(s, &data, &len))
		return 0;
	if (!WriteBuffWoSize(data, len))
		return PyErr_NoMemory();
		Py_INCREF(Py_None);
		return Py_None;
}

PyObject *WriteStream::Str()
{
	if (!mFinalized)
		return PyErr_SetString(PyExc_RuntimeError, "stream isn't finalized yet"), nullptr;
	PyObject* bytes = PyBytes_FromStringAndSize( mBuff, mPos );
	PyObject* str = PyUnicode_FromFormat( "%R", bytes );
	Py_DECREF( bytes );
	return str;
}


PyObject *WriteStream::Len()
{
	if (!mFinalized)
		return PyErr_SetString(PyExc_RuntimeError, "stream isn't finalized yet"), nullptr;
	return PyLong_FromLongLong(mPos);
}


PyObject *WriteStream::__reduce_ex__(PyObject *proto)
{
	if (!mFinalized)
		return PyErr_SetString(PyExc_RuntimeError, "stream isn't finalized yet"), nullptr;
	return Py_BuildValue("O(y#)", GetType(), mBuff, mPos);
}


PyObject *WriteStream::tp_str_method(PyObject *self)
{
	return static_cast<WriteStream*>( self )->Str();
}


PyObject *WriteStream::tp_repr_method(PyObject *_self)
{
	WriteStream *self = static_cast<WriteStream*>(_self);
	PyObject *str = (PyObject*)self->Str();
	if (!str)
		return 0;
	PyObject *res = PyUnicode_FromFormat("<MarshalStream %U>", str);
	Py_DECREF(str);
	return res;
}

PyObject *WriteStream::tp_richcompare_method(PyObject *self, PyObject* other, int op)
{
	if (op != Py_EQ)
	{
		Py_RETURN_RICHCOMPARE(self, other, op);
	}
	if (self == other)
	{
		Py_RETURN_TRUE;
	}

	WriteStream *_self = static_cast<WriteStream*>(self);
	WriteStream *_other = static_cast<WriteStream*>(other);
	if (_self->mPos != _other->mPos)
	{
		Py_RETURN_FALSE;
	}
	auto smaller = std::min(_self->mPos, _other->mPos);
	if (memcmp(_self->mBuff, _other->mBuff, smaller) != 0)
	{
		Py_RETURN_FALSE;
	}
	Py_RETURN_TRUE;
}


PyObject *WriteStream::GetPickler()
{
	//a pickler associated with this stream, to hold on to references and stuff
	//a set of multiple pickle.dumps() calls are inefficient.
	if (!mPickler) {
		BluePy pickle(PyImport_ImportModule("pickle"));
		if (!pickle ) return 0;
		mPickler = BluePy(PyObject_CallMethod( pickle, (char*)"Pickler", (char*)"Oi", this, 2)); //2 == HIGHEST_PROTOCOL
	}
	return mPickler;
}

bool WriteStream::Prepare(size_t s)
{
	CCP_ASSERT(mBuff);
	CCP_ASSERT(mPos <= mSize);
	if (mPos + s > mSize) {
		size_t oldsize = mSize, size = mSize;
		do {
			size *= 2;
		} while(mPos + s > size);
		void *tmp = CCP_REALLOC( "WriteStream/mBuff", mBuff, size*sizeof(char));
		if (!tmp) {
			//try again, with tighter size
			size = mPos+s;
			tmp = CCP_REALLOC( "WriteStream/mBuff", mBuff, size*sizeof(char));
		}
		if (!tmp) {
			CCP_LOGERR_CH( s_ch, "WriteStream failed growing buff from %d bytes, mPos=%d, req=%d, realloc=%d", mSize, mPos, s, size);
			return PyErr_NoMemory(), false;
		}
		sMemUse += size-oldsize;
		mBuff = (char*)tmp;
		mSize = size;
	}
	CCP_ASSERT(mPos + s <= mSize);
	return true;
}


//--------------------------------------------------------------------
bool WriteStream::ObjectReferenced(bool &referred, PyObject* o)
{
	//insert into map.  Note that we incref the reference.  This is to make sure
	//that any temporary values (from custom marshaling) that we may encounter
	//doesn't go away during marshaling, thus making us hold pointers to
	//old objects, possibly creating false references when new temporaries
	//are encountered
	referred = false;
	mTypeFlags = TY_INVALID;
	if (o->ob_refcnt < 2)
		return true;
	std::pair<refMap_t::iterator, bool> ins = mRefMap.insert(
			refMap_t::value_type(BluePy(o,true), Refs(mPos)));
	if (mVersion == 0) {
		if (!ins.second) {
			//it already contained a dude!
			referred = true;
			Refs &r = ins.first->second;
			if (r.id==0) {
				//first time the dude is found
				int serial = (int)mRefHits.size();
				r.id = serial+1;
				mBuff[r.streampos] |= TY_SHAREDFLAG;
				mRefHits.push_back(std::pair<size_t, size_t>(r.streampos, r.id));
			}
			RETFAIL(WriteType(TY_REFERENCE));
			RETFAIL(WriteInteger((int)r.id));
		}
	} else {
		// new style. Don't use indirect maps, just use indices.
		// There will be a number of TY_REFERENCE marked objects that
		// aren't actually referenced, and the acutal reference indices
		// will be higher, but the pickle will be smaller.
		if (ins.second) {
			//first time this object is seen.  Mark the index
			ins.first->second.id = mRefMap.size()-1;
			mTypeFlags = TY_SHAREDFLAG; //next type written gets this
		} else {
			//Seen again!
			RETFAIL(WriteType(TY_REFERENCE));
			RETFAIL(WriteInteger((int)ins.first->second.id));
			referred = true;
		}
	}
	return true; //function succeeded
}


bool WriteStream::Finalize(size_t &totalLen, size_t &mapLen)
{
	mapLen = 0;
	if (mVersion == 0) {
		//finish the reference indirection map
		// Write out shared object mapping
		int32_t n = (int32_t)mRefHits.size();
		//first, sort according to streampos
		std::sort(mRefHits.begin(), mRefHits.end());
		//write out the map
		for (int32_t i = 0; i < n; i++) {
			int32_t id = (int)mRefHits[i].second;
			if (!Write<int32_t>(id)) return false;
		}
		mapLen = n*sizeof(int);
		
		//write lenght
		size_t tmp = GetPos();
		SetPos(1);
		if (!Write<int32_t>(n)) return false;
		SetPos(tmp);
	}
	size_t size = GetPos();
	totalLen = size;

	if (mChecksumPos != -1)
	{
		int32_t a32 = int32_t( adler32(1, (const unsigned char*)mBuff + mChecksumPos+4, (unsigned int)(size - (mChecksumPos+4))) ); //add support for long buffers
		SetPos(mChecksumPos);
		if (!Write<int>(a32)) return false;
		SetPos(size);
	}
	
	mFinalized = true;
	mRefMap.clear();
	mRefHits.clear();
	mScratch.clear();
	mPickler.Release();
	mCallback.Release();
	return true;
}


// buffer protocol.  We pass these objects out, and so they must be buffers
int WriteStream::getbuffer( PyObject* exporter, Py_buffer* view, int flags )
{
	WriteStream *self = static_cast<WriteStream*>(exporter);
	if (!self->mFinalized)
	{
		PyErr_SetString( PyExc_BufferError, "buffer not ready" );
		return -1;
	}

    return PyBuffer_FillInfo( view, exporter, self->mBuff, self->mPos, 1, flags );
}

//SequenceProtocol
Py_ssize_t WriteStream::SequenceLength(PyObject *selfO)
{
	WriteStream *self = static_cast<WriteStream*>(selfO);
	if (!self->mFinalized)
		return PyErr_SetString(PyExc_RuntimeError, "buffer not ready"), -1;
	return self->mPos;
}	
PyObject *WriteStream::SequenceGet(PyObject *selfO, Py_ssize_t i)
{
	WriteStream *self = static_cast<WriteStream*>(selfO);
	if (!self->mFinalized)
		return PyErr_SetString(PyExc_RuntimeError, "buffer not ready"), nullptr;
	if (i<0 || (size_t)i >= self->mPos)
		return PyErr_Format(PyExc_IndexError, "index out of range: %d", int( i )), nullptr;
	return PyBytes_FromStringAndSize(self->mBuff+i, 1);
}
PyObject *WriteStream::SequenceGetSlice(PyObject *selfO, Py_ssize_t ilow, Py_ssize_t ihigh)
{
	WriteStream *self = static_cast<WriteStream*>(selfO);
	if (!self->mFinalized)
		return PyErr_SetString(PyExc_RuntimeError, "buffer not ready"), nullptr;
	if (ilow<0 || (size_t)ilow >= self->mPos)
		return PyErr_Format(PyExc_IndexError, "index out of range: %d", int( ilow )), nullptr;

	if (ihigh < ilow)
		ihigh = ilow;
	else if (ihigh > (Py_ssize_t)self->mPos)
		ihigh = self->mPos;
	Py_ssize_t n = ihigh-ilow;
	return PyBytes_FromStringAndSize(self->mBuff+ilow, n);
}

PyObject *WriteStream::SequenceSubscript( PyObject* self, PyObject* key )
{
	auto _this = static_cast<WriteStream*>( self );
	if ( PyIndex_Check( key ) ) {
		Py_ssize_t index;
		index = PyNumber_AsSsize_t( key, PyExc_IndexError );
		if ( index == -1 && PyErr_Occurred() ) {
			return nullptr;
		}
		return _this->SequenceGet( self, index );
	} else if ( PySlice_Check( key ) ) {
		Py_ssize_t start, stop, step;

		if ( PySlice_Unpack( key, &start, &stop, &step ) ) {
			return nullptr;
		}

		if (step != 1) {
			PyErr_SetString( PyExc_NotImplementedError, "Supported for slices with a step size other than 1 is not implemented." );
			return nullptr;
		}

		return _this->SequenceGetSlice( self, start, stop );
	}

	PyErr_Format( PyExc_TypeError, "list indices must be integers or slices, not %.200s", Py_TYPE(key)->tp_name );
	return nullptr;
}

//custom marshaling of a write stream
bool WriteStream::Write(Marshal &m, WriteStream &stream)
{
	if (!mFinalized)
		return PyErr_SetString(PyExc_RuntimeError, "buffer not ready"), false;
	return stream.WriteBuff(mBuff, mPos);
}
PyObject *WriteStream::Read(Marshal &m, ReadStream &stream)
{
	BluePy res(WriteStream::_New(WriteStream::GetType(), 0, 0));
	if (!res) return 0;
	WriteStream *out = static_cast<WriteStream*>(res.o);
	const char *data;
	int datalen;
	if (!stream.GetBuff(data, datalen)) return 0;
	if (!out->Init(data, datalen)) return 0;
	return res.Detach();
}



//--------------------------------------------------------------------
// ReadStream utility
//--------------------------------------------------------------------
bool ReadStream::InitType(PyTypeObject *type)
{
		//disable pyton alloc and dealloc.
		type->tp_new = 0;
		return true;
}

ReadStream::ReadStream() : mSize(0) {CCP_ASSERT(false);}

ReadStream::ReadStream(const void* buff, size_t bufflen, PyObject* callback) :
		mBuff(reinterpret_cast<const char*>(buff)), mSize(bufflen), mCallback(callback, true)
{
		ob_type = GetType();
		ob_refcnt = 1;

		mPos = 0;
		mSSize = mSize; //shortened size, adjusted for map.
		mMapCount = 0;
		mMapping = NULL;
		mNumShared = 0;
		mCrcPos = -1;
		mCrc = 0;
		mGotCRC = false;
		mVersion = 0;
}

ReadStream::~ReadStream()
{}

bool ReadStream::CheckSpace(int n, size_t esize)
{
		//don't multiply n and esize, to avoid rollover
		if (n<0)
            return PyErr_Format( PyExc_RuntimeError, "Can't read %d elements", n ), false;
		size_t left = mSSize - mPos;
		if (esize > 1) {
			if ((size_t)n > left/esize)
                return PyErr_Format(PyExc_RuntimeError, "Can't read %d elements of %d bytes, only have %" CCP_SIZET_FORMAT " bytes left", n, int( esize ), left), false;
		} else {
			if ((size_t)n > left)
                return PyErr_Format(PyExc_RuntimeError, "Can't read %d elements of %d bytes, only have %" CCP_SIZET_FORMAT " bytes left", n, int( esize ), left), false;
		}
		return true;
}


bool ReadStream::ReadType(PYTYPES &t)
{
		char c;
		if (!Read(c)) return false;
		t = (PYTYPES)c;
		return true;
}


bool ReadStream::PeekType(PYTYPES &t)
{
	if (!ReadType(t))
		return false;
	Rewind(sizeof(char));
	return true;
}


bool ReadStream::ReadInteger(int &r)
{
		unsigned char c;
		if (!Read(c))
			return 0;
		if (c == UCHAR_MAX)
			return Read<int32_t>(r);
		r = (int)c;
		return true;
}


//allocate an index when the shared object is seen
size_t ReadStream::MarkShared_Int(PyObject *o)
{
	// `o` may be a `nullptr`, to reserve space for an object yet to be read from the stream.
	if (mVersion == 0) {
		if (mNumShared >= mMapCount)
			return PyErr_SetString(PyExc_RuntimeError, "Shared object table overflow"), -1;
		size_t ix = mMapping[mNumShared++]-1;
		if (o)
			mShared[ix] = BluePy(o, true);
		return ix;
	}
	size_t ix = mShared.size();
	try {
		mShared.push_back(BluePy(o, true));
	} catch (std::bad_alloc) {
		return PyErr_NoMemory(), -1;
	}
	return ix;
}


size_t ReadStream::MarkShared()
{
	return MarkShared_Int(0);
}


//fill in the shared object when construction is complete
bool ReadStream::UpdateShared(size_t ix, PyObject *o)
{
	// A constructed object should never be a `nullptr`.
	if ( !o )
	{
		PyErr_SetString( PyExc_ValueError, "Invalid shared object read from stream");
		return false;
	}
	mShared[ix] = BluePy(o, true); //new ref
	return true;
}


//and combination of both.  Use when this makes sense, when there has been trivial
//binary data read since the shared object was discovered in the stream.
bool ReadStream::MarkShared(PyObject *o)
{
	size_t ix = MarkShared_Int(o);
	return ix != -1;
}


// compute the data CRC and compare it with the reference value
bool ReadStream::Crc() const {
	CCP_ASSERT(mGotCRC);
	size_t end;
	if (mVersion == 0)
		end = mSize;
	else
		end = GetPos();
	int a32	= int( adler32(1, (const unsigned char*)mBuff + mCrcPos, (unsigned int)(end - mCrcPos)) );
	if (a32 != mCrc)
		return PyErr_SetString(PyExc_ValueError, "Bad CRC"), false;
	return true;
}

//Python file methods
PyObject *ReadStream::read(PyObject *len)
{
		CCP_ASSERT(PyLong_Check(len));
		long l = PyLong_AsLong(len);
		if (l==-1 && PyErr_Occurred())
			return 0;
		const char *buf;
		if (GetBuffer(buf, int( l )))
			return PyBytes_FromStringAndSize(buf, l);
		return 0;
}
PyObject *ReadStream::readline()
{
		size_t start = mPos;
		char c;
		do {
			if (!Read(c)) return 0;
		} while (c!='\n');
		size_t end = mPos;
		PyObject *r;
		if (end >= start+2 && *(char*)(mBuff+end-2) == '\r') {
			//remove CR before LF, must hand copy into string
			r = PyBytes_FromStringAndSize(0, end-start-1);
			memcpy(PyBytes_AS_STRING(r), mBuff+start, end-start-2);
			PyBytes_AS_STRING(r)[end-start-2] = '\n';
		} else
			r = PyBytes_FromStringAndSize(mBuff+start, end-start); //there was no CR
		return r;
}

	
//Get unpicklers associated with this stream
PyObject *ReadStream::GetUnpicklerInt(){
		BluePy pickle(PyImport_ImportModule("pickle"));
		if (!pickle ) return 0;
		return PyObject_CallMethod( pickle, (char*)"Unpickler", (char*)"(O)", (PyObject*)this);
}


PyObject *ReadStream::GetUnpickler(Marshal *m)
{
		//a pickler associated with this stream, to hold on to references and stuff
		//we use the same pickler, generated on demand,so that we reuse objects known.
		//a set of multiple pickle.dumps() calls are inefficient.
		if (!mUnpickler) {
			mUnpickler = BluePy(GetUnpicklerInt());
			if (!mUnpickler) return 0;
			if (!m->HookUnpickler(mUnpickler)) {
				mUnpickler.Release();
				return 0;
			}
		}
		return mUnpickler;
}

PyObject *ReadStream::GetOSUnpickler(Marshal *m) //similar, but returns a one-shot unpickler, with no memory
{
		if (!mOSUnpickler) {
			mOSUnpickler = BluePy(GetUnpicklerInt());
			if (!mOSUnpickler) return 0;
			if (!m->HookUnpickler(mOSUnpickler)) return 0;
			return mOSUnpickler;
		}
		//clear the memory
		BluePy memo(PyObject_GetAttrString(mOSUnpickler, "memo"));
		if (!memo) return 0;
		BluePy r(PyObject_CallMethod(memo, (char*)"clear", 0));
		if (!r) return 0;
		return mOSUnpickler;
}
	

//--------------------------------------------------------------------

PyObject * Marshal::ReadObjectTuple0( ReadStream * stream, bool isShared )
{
	return PyTuple_New( 0 );
}

PyObject * Marshal::ReadObjectBuffer( ReadStream * stream, bool isShared )
{
	BluePy r( ReadObjectBuffer( *stream ) );
	if( !r ) return 0;
	if (isShared && !stream->MarkShared( r )) return 0;
	return r.Detach();
}

PyObject * Marshal::ReadObjectUnicode1( ReadStream * stream, bool isShared )
{
	char code;
	if( !stream->Read( code ) ) return 0;
    return PyUnicode_FromStringAndSize((const char*) &code, 1);
}

PyObject * Marshal::ReadObjectUnicode0( ReadStream * stream, bool isShared )
{
    return PyUnicode_FromString("");
}

PyObject * Marshal::ReadObjectUnicode( ReadStream * stream, bool isShared )
{
	int len;
	const char *buff;
	if( !stream->ReadInteger( len ) || !stream->GetBuffer( buff, len ) ) return nullptr;
	return PyUnicode_FromStringAndSize( buff, len );
}

PyObject * Marshal::ReadObjectStrTable( ReadStream * stream, bool isShared )
{
	unsigned char uc;
	if( !stream->Read( uc ) ) return 0;
	if( uc<1 || uc>PyList_GET_SIZE( mStrTableRev.o ) )
		return PyErr_Format( PyExc_RuntimeError, "Invalid string table index %d", uc ), nullptr;
	--uc;
	PyObject *r = PyList_GET_ITEM( mStrTableRev.o, uc );
	Py_INCREF( r );
	return r;
}

PyObject * Marshal::ReadObjectStrShort( ReadStream * stream, bool isShared )
{
	unsigned char uc;
	const char *buf;
	if( !stream->Read( uc ) || !stream->GetBuffer( buf, uc ) ) return 0;
	return PyUnicode_FromStringAndSize( buf, uc );
}

PyObject * Marshal::ReadObjectStrChar( ReadStream * stream, bool isShared )
{
	char chr;
	if( !stream->Read( chr ) ) return 0;
	return PyUnicode_FromStringAndSize( &chr, 1 );
}

PyObject * Marshal::ReadObjectStrEmpty( ReadStream * stream, bool isShared )
{
	return mStock_EmptyStr.NewRef();
}

PyObject * Marshal::ReadObjectFalse( ReadStream * stream, bool isShared )
{
	Py_INCREF( Py_False );
	return Py_False;
}

PyObject * Marshal::ReadObjectTrue( ReadStream * stream, bool isShared )
{
	Py_INCREF( Py_True );
	return Py_True;
}

PyObject * Marshal::ReadObjectFloatZero( ReadStream * stream, bool isShared )
{
	return mStock_Float0.NewRef();
}

PyObject * Marshal::ReadObjectFloat( ReadStream * stream, bool isShared )
{
	double r;
	if( !stream->Read( r ) ) return 0;
	return PyFloat_FromDouble( r );
}

PyObject * Marshal::ReadObjectIntOne( ReadStream * stream, bool isShared )
{
	return mStock_Int1.NewRef();
}

PyObject * Marshal::ReadObjectIntZero( ReadStream * stream, bool isShared )
{
	return mStock_Int0.NewRef();
}

PyObject * Marshal::ReadObjectIntNegativeOne( ReadStream * stream, bool isShared )
{
	return mStock_IntN1.NewRef();
}

PyObject * Marshal::ReadObjectInt8( ReadStream * stream, bool isShared )
{
	int8_t r;
	if( !stream->Read( r ) ) return 0;
	return PyLong_FromLong( r );
}

PyObject * Marshal::ReadObjectInt16( ReadStream * stream, bool isShared )
{
	int16_t r;
	if( !stream->Read( r ) ) return 0;
	return PyLong_FromLong( r );
}

PyObject * Marshal::ReadObjectInt32( ReadStream * stream, bool isShared )
{
	int32_t r;
	if( !stream->Read( r ) ) return 0;
	return PyLong_FromLong( r );
}

PyObject * Marshal::ReadObjectInt64( ReadStream * stream, bool isShared )
{
	int64_t r;
	if( !stream->Read( r ) ) return 0;
	return PyLong_FromLongLong( r );
}

PyObject * Marshal::ReadObjectNone( ReadStream* stream, bool isShared )
{
	Py_INCREF( Py_None );
	return Py_None;
}

PyObject * Marshal::ReadObjectWStream( ReadStream * stream, bool isShared )
{
	return WriteStream::Read( *this, *stream );
}

PyObject * Marshal::ReadObjectDBRow( ReadStream * stream, bool isShared )
{
	return DBRow::Read( *this, *stream );
}

PyObject * Marshal::ReadObjectCrcCheck( ReadStream * stream, bool isShared )
{
	stream->mGotCRC = true;
	if( !stream->Read<int>( stream->mCrc ) ) return 0;
	stream->mCrcPos = stream->GetPos();
	if( stream->GetVersion() == 0 && !mSkipCrcCheck ) {
		if( !stream->Crc() )
			return 0;
	}
	return ReadObject( stream );
}

PyObject * Marshal::ReadObjectReference( ReadStream * stream, bool isShared )
{
	int len;
	PyObject* ret{nullptr};

	if( !stream->ReadInteger( len ) )
	{
		return nullptr;
	}
	if( stream->GetVersion() == 0 ) {
		if( len < 1 || len > stream->mMapCount || !(ret = stream->mShared[len - 1]) ) {
			PyErr_SetString( PyExc_ValueError, "Invalid TY_REFERENCE in stream" );
			return nullptr;
		}
	}
	else {
		if( len < 0 || len >= (int)stream->mShared.size() || !(ret = stream->mShared[len]) )
		{
			PyErr_SetString( PyExc_ValueError, "Invalid TY_REFERENCE in stream" );
			return nullptr;
		}
	}
	Py_INCREF( ret );
	return ret;
}

PyObject * Marshal::ReadObjectPickler( ReadStream * stream, bool isShared )
{
	PyObject *up = stream->GetUnpickler( this );
	if( !up ) return 0;
	BluePy r( PyObject_CallMethod( up, (char*)"load", 0 ) );
	if( !r ) return 0;
	if (isShared && !stream->MarkShared( r )) return 0;
	return r.Detach();
}

PyObject * Marshal::ReadObjectPickle( ReadStream * stream, bool isShared )
{
	int len;
	if( !stream->ReadInteger( len ) ) return 0; //don't need this, length is inherent in pickle,
	PyObject *up = stream->GetOSUnpickler( this ); //borrowed ref 
	if( !up ) return 0;
	return PyObject_CallMethod( up, (char*)"load", 0 );
}

PyObject * Marshal::ReadObjectCallback( ReadStream * stream, bool isShared )
{
	if( stream->mCallback == NULL ) {
		PyErr_SetString( PyExc_RuntimeError,
			"Unmarshal stream contains custom data but I have no callback method" );
		return NULL;
	}
	BluePy data( ReadObject( stream ) );
	if( !data ) return 0;

	AutoTasklet _at( PyOS->GetTaskletTimer(), mTimer_LoadCallback );
	return PyObject_CallFunctionObjArgs( stream->mCallback, data.o, 0 );
}

PyObject * Marshal::ReadObjectDict( ReadStream * stream, bool isShared )
{
	int len;
	if( !stream->ReadInteger( len ) ) return 0;
	BluePyDict r( 1 );
	if( !r ) return 0;
	if (isShared && !stream->MarkShared( r )) return 0;

	for( int i = 0; i < len; i++ ) {
		BluePy val( ReadObject( stream ) );
		if( !val ) return 0;
		BluePy key( ReadObject( stream ) );
		if( !key || !r.Set( key, val ) ) return 0;
	}
	return r.Detach();
}

PyObject * Marshal::ReadObjectList( ReadStream * stream, bool isShared )
{
	int len;
	if( !stream->ReadInteger( len ) ) return 0;
	if( !stream->CheckSpace( len ) ) return 0; //rudimentary security check
	BluePy r( PyList_New( len ) );
	if( !r ) return 0;
	if (isShared && !stream->MarkShared( r )) return 0;

	for( int i = 0; i < len; i++ ) {
		PyObject* tmp = ReadObject( stream );
		if( !tmp ) return 0;
		PyList_SET_ITEM( r.o, i, tmp );
	}
	return r.Detach();
}

PyObject * Marshal::ReadObjectList1( ReadStream * stream, bool isShared )
{
	BluePy r( PyList_New( 1 ) );
	if( !r ) return 0;
	if (isShared && !stream->MarkShared( r )) return 0;
	PyObject* tmp = ReadObject( stream );
	if( !tmp ) return 0;
	PyList_SET_ITEM( r.o, 0, tmp );
	return r.Detach();
}

PyObject * Marshal::ReadObjectList0( ReadStream * stream, bool isShared )
{
	return PyList_New( 0 );
}

PyObject * Marshal::ReadObjectTuple( ReadStream * stream, bool isShared )
{
	int len;
	if( !stream->ReadInteger( len ) ) return 0;
	if( !stream->CheckSpace( len ) ) return 0; //must be at least this many bytes left
	BluePy r( PyTuple_New( len ) );
	if( !r ) return 0;
	if (isShared && !stream->MarkShared( r )) return 0;

	for( int i = 0; i < len; i++ ) {
		PyObject* tmp = ReadObject( stream );
		if( !tmp ) return 0;
		if( tmp == r )
			return PyErr_Format( PyExc_RuntimeError, "Self referencing tuple" ), nullptr;
		PyTuple_SET_ITEM( r.o, i, tmp );
	}
	return r.Detach();
}

PyObject * Marshal::ReadObjectTuple2( ReadStream * stream, bool isShared )
{
	BluePy r( PyTuple_New( 2 ) );
	if( !r ) return 0;
	if (isShared && !stream->MarkShared( r )) return 0;
	for( int i = 0; i < 2; i++ ) {
		PyObject* tmp = ReadObject( stream );
		if( !tmp ) return 0;
		if( tmp == r )
			return PyErr_Format( PyExc_RuntimeError, "Self referencing tuple" ), nullptr;
		PyTuple_SET_ITEM( r.o, i, tmp );
	}
	return r.Detach();
}

PyObject * Marshal::ReadObjectTuple1( ReadStream * stream, bool isShared )
{
	BluePy r( PyTuple_New( 1 ) );
	if( !r ) return 0;
	if (isShared && !stream->MarkShared( r )) return 0;
	PyObject* tmp = ReadObject( stream );
	if( !tmp ) return 0;
	//Directly self referencing tuples must be prevented.  A lot of code
	//in python assumes that tuples are not directly self referencing.
	//The protocol allows for specifying them, unlike the pickle protocol.
	if( tmp == r )
		return PyErr_Format( PyExc_RuntimeError, "Self referencing tuple" ), nullptr;
	PyTuple_SET_ITEM( r.o, 0, tmp );
	return r.Detach();
}

PyObject* Marshal::ReadObject( ReadStream *stream )
{
	CCP_STATS_ZONE( __FUNCTION__ );

	Incrementor _inc(mRecursionLevel);
	if( !_inc.Test( sRecursionLimit ) )
		return nullptr;

	PYTYPES type;
	if (!stream->ReadType(type)) return 0;

	m_typesLoaded[type & TY_TYPEMASK] += 1;

	bool isShared = (type & TY_SHAREDFLAG) == TY_SHAREDFLAG;

	ReadObjectTypeHandler handler = s_typeHandlers[type & TY_TYPEMASK];
	if( handler )
	{
		return (this->*handler)( stream, isShared );
	}
	PyErr_Format(PyExc_RuntimeError, "Invalid type tag %d in stream.", type);
	return NULL;
}


//Special version that also checks for a "mark" token in the stream.
PyObject* Marshal::ReadObjectOrMarker(ReadStream *stream, bool &mark)
{
	mark = false;
	PYTYPES type;
	if (!stream->PeekType(type)) return 0;
	if (type == TY_MARK) {
		m_typesLoaded[type] += 1;
		if (!stream->ReadType(type)) return 0; //perform the actual read
		mark = true;
		return 0;
	}
	return ReadObject(stream);
}


//This is a special function since it is done often.
PyObject *Marshal::ReadObjectBuffer(ReadStream &s)
{
	int len;
	const char *c;
	if (!s.ReadInteger(len) || !s.GetBuffer(c, len))
		return 0;
	PyObject *o = PyBytes_FromStringAndSize(c, len);
	if (!o) return 0;
	return o;
}


//reads and instantiates a global object from its string name.
PyObject *Marshal::ReadObjectGlobal(ReadStream *stream, bool shared)
{
	int len;
	const char *c;
	if (!stream->ReadInteger(len) || !stream->GetBuffer(c, len))
		return 0;
	BluePy name(PyUnicode_FromStringAndSize(c, len));
	if (!name) return 0;
	//automatically intern short strings
	if (len <= 30)
		PyUnicode_InternInPlace(&name.o);
	BluePy obj(GetGlobalObject(name));
	if (!obj) return 0;
	if (shared && !stream->MarkShared(obj)) return 0;
	if (!UpdateGlobalNames(0, name)) return 0;
	return obj.Detach();
}

#ifdef PY27_COMPATIBILITY_MODE
//Read an instance of an old-style class
PyObject *Marshal::ReadObjectInstance(ReadStream *stream, bool shared)
{
//	mark shared according to stream position
	size_t index;
	if (shared && (index = stream->MarkShared()) == -1)
    {
		return nullptr;
    }

	BluePy guid(ReadObject(stream));
	if (!guid)
    {
        return nullptr;
    }
	BluePy klass(GetGlobalObject(guid));
	if (!klass)
    {
        return nullptr;
    }
	if (!PyObject_IsInstance(klass.o, (PyObject *)&PyType_Type))
    {
		BluePy r(PyObject_Repr(guid));
		BluePy t(PyObject_Type(klass));
		BluePy tr(PyObject_Repr(t));
		PyErr_Format(PyExc_TypeError, "global object %s should be of class type but is of %s",
			r ? PyUnicode_AsUTF8(r.o): "<>", tr ? PyUnicode_AsUTF8(tr.o): "<>");
		return nullptr;
	}

	if (!UpdateGlobalNames(guid, 0))
    {
        return nullptr;
    }

    BluePy __new__(PyObject_GetAttr(klass, mStock_New));
    if (!__new__)
    {
        return nullptr;
    }

    // We require that the new version of the class
    // does not require any additional arguments when calling __new__
    BluePy args( PyTuple_New( 1 ) );
    Py_IncRef( klass ); // PyTuple_SetItem steals a reference.
    if( PyTuple_SetItem( args, 0, klass ) != 0 )
    {
        return nullptr;
    }
    BluePy inst(PyObject_CallObject(__new__, args));
	if (!inst)
    {
		return nullptr;
    }
	if (shared)
    {
        stream->UpdateShared(index, inst);
    }

	// Get the instance data
	BluePy state(ReadObject(stream));
	if (!state)
    {
        return nullptr;
    }

	// set the state
	bool setstate = PyObject_HasAttr(klass, mStock_SetState) != 0;
	if (setstate)
	{
		AutoTasklet _at(PyOS->GetTaskletTimer(), mTimer_SetState);
		BluePy r(PyObject_CallMethodObjArgs(inst, mStock_SetState, state.o, NULL));
		if (!r)
        {
			return nullptr;
        }
	}
	else
	{
		BluePy dict(PyObject_GetAttr(inst, mStock_Dict));
		if (!dict)
        {
			return nullptr;
        }
		// Ensure keys are encoded in UTF-8 unicode
		BluePy convertedState;
		PyObject *key, *value;
		Py_ssize_t pos = 0;
		while ( PyDict_Next( state, &pos, &key, &value ) )
		{
			if ( !PyBytes_Check(key) )
			{
				break;
			}
			if( !convertedState )
			{
				convertedState = BluePy( PyDict_New() );
				if( !convertedState )
				{
					return nullptr;
				}
			}
			BluePy encodedKey(PyUnicode_FromEncodedObject( key, "UTF-8", nullptr ) );
			PyDict_SetItem( convertedState, encodedKey, value );
		}
		if (PyDict_Update( dict, convertedState ? convertedState : state ) == -1)
        {
			return nullptr;
        }
	}
	return inst.Detach();
}
#endif
	

PyObject *Marshal::ReadObjectReduce(ReadStream *stream, bool shared)
{
	size_t ix;
	if (shared && (ix = stream->MarkShared()) == -1) return 0;

	BluePy rv(ReadObject(stream));
	if (!rv)
		return 0;
	PyObject *callable = PyTuple_GetItem(rv, 0);
	if (!callable) return 0;
	PyObject *args = PyTuple_GetItem(rv, 1);
	if (!args) return 0;
	
	BluePy r;
	r = BluePy(PyObject_CallObject(callable, args));
	if (!r) return 0;
	//object is constructed, now update r
	if (shared && !stream->UpdateShared(ix, r)) return 0;
	
	//further initialization
	if (PyTuple_GET_SIZE(rv.o)>2) {
		PyObject *state = PyTuple_GET_ITEM(rv.o, 2);
		BluePy setstate(PyObject_GetAttr(r, mStock_SetState));
#ifdef PY27_COMPATIBILITY_MODE
		if ( setstate ) {
			BluePy exceptionSetState( PyObject_GetAttr( PyExc_Exception, mStock_SetState ) );
			BluePy classSetState( PyObject_GetAttr( reinterpret_cast<PyObject*>( r.o->ob_type ), mStock_SetState ) );
			if( classSetState == exceptionSetState )
			{
				BluePy items( PyDict_Items( state ) );
				BluePy convertedState;

				// Ensure keys are encoded in UTF-8 unicode
				PyObject *key, *value;
				Py_ssize_t pos = 0;
				while ( PyDict_Next( state, &pos, &key, &value ) )
				{
					if ( !PyBytes_Check(key) )
					{
						break;
					}
					if( !convertedState )
					{
						convertedState = BluePy( PyDict_New() );
						if( !convertedState )
						{
							return nullptr;
						}
					}
					BluePy encodedKey(PyUnicode_FromEncodedObject( key, "UTF-8", nullptr ) );
					PyDict_SetItem( convertedState, encodedKey, value );
				}
				BluePy tmp( PyObject_CallFunctionObjArgs( setstate, convertedState ? convertedState : state, 0 ) );
				if ( !tmp )
				{
					return nullptr;
				}
			}
			else
			{
				BluePy tmp( PyObject_CallFunctionObjArgs( setstate, state, 0 ) );
				if ( !tmp )
				{
					return nullptr;
				}
			}
		} else {
			PyErr_Clear();
			BluePy dict( PyObject_GetAttr( r, mStock_Dict ) );
			if ( !dict )
			{
				return nullptr;
			}
			BluePy items( PyDict_Items( state ) );
			BluePy convertedState;

			// Ensure keys are encoded in UTF-8 unicode
			PyObject *key, *value;
			Py_ssize_t pos = 0;
			while ( PyDict_Next( state, &pos, &key, &value ) )
			{
				if ( !PyBytes_Check( key ) )
				{
					break;
				}
				if( !convertedState )
				{
					convertedState = BluePy( PyDict_New() );
				}
				BluePy encodedKey( PyUnicode_FromEncodedObject( key, "UTF-8", nullptr ) );
				PyDict_SetItem( convertedState, encodedKey, value );
			}
			if ( PyDict_Update( dict, convertedState ? convertedState : state ) )
			{
				return nullptr;
			}
		}
#else
		if (setstate) {
			BluePy tmp(PyObject_CallFunctionObjArgs(setstate, state, 0));
			if (!tmp) return 0;
		} else {
			PyErr_Clear();
			BluePy dict(PyObject_GetAttr(r, mStock_Dict));
			if (!dict) return 0;
			if (PyDict_Update(dict, state)) return 0;
		}
#endif
	}

	//read the iterators
	if (!ReadObjectListIter(*stream, r) || !ReadObjectDictIter(*stream, r))
		return 0;

	return r.Detach();
}


PyObject *Marshal::ReadObjectNewobj(ReadStream* stream, bool shared)
{
	size_t ix;
	if ( shared && (ix = stream->MarkShared()) == -1 )
	{
		return nullptr;
	}

	BluePy rv(ReadObject(stream));
	if ( !rv )
	{
		return nullptr;
	}

	PyObject *args = PyTuple_GetItem(rv, 0);
	if ( !args )
	{
		return nullptr;
	}

	PyObject *cls = PyTuple_GetItem(args, 0);
	if ( !cls )
	{
		return nullptr;
	}
	
	BluePy __new__(PyObject_GetAttr( cls, mStock_New ));
	if ( !__new__ )
	{
		return nullptr;
	}

	BluePy r(PyObject_CallObject(__new__, args));
	if ( !r )
	{
		return nullptr;
	}
	//object is constructed, now update r
	if ( shared && !stream->UpdateShared(ix, r) )
	{
		return nullptr;
	}
	
	//further initialization
	if ( PyTuple_GET_SIZE(rv.o) > 1 )
	{
		auto state = PyTuple_GET_ITEM(rv.o, 1);

		BluePy setstate(PyObject_GetAttr(r, mStock_SetState));
		if ( setstate )
		{
			BluePy tmp(PyObject_CallFunctionObjArgs(setstate, state, 0));
			if ( !tmp )
			{
				return nullptr;
			}
		}
		else
		{
			PyErr_Clear();
			BluePy dict(PyObject_GetAttr(r, mStock_Dict));
			if (!dict)
			{
				return nullptr;
			}
#ifdef PY27_COMPATIBILITY_MODE
			BluePy convertedState;

			// Ensure keys are encoded in UTF-8 unicode
			PyObject *key, *value;
			Py_ssize_t pos = 0;
			while (PyDict_Next(state, &pos, &key, &value))
			{
				if ( !PyBytes_Check(key) )
				{
					break;
				}
				if( !convertedState )
				{
					convertedState = BluePy( PyDict_New() );
				}
				BluePy encodedKey(PyUnicode_FromEncodedObject(key, "UTF-8", nullptr));
				PyDict_SetItem(convertedState, encodedKey, value);
			}
			if( convertedState )
			{
				state = convertedState;
			}
#endif
			if ( PyDict_Update(dict, state) )
			{
				return nullptr;
			}
		}
	}

	//read the iterators
	if ( !ReadObjectListIter(*stream, r) || !ReadObjectDictIter(*stream, r) )
	{
		return nullptr;
	}

	return r.Detach();
}


//read and apply any list iterator to a reduced object
bool Marshal::ReadObjectListIter(ReadStream &stream, PyObject *target)
	{
	for(;;) {
		bool mark;
		BluePy o(ReadObjectOrMarker(&stream, mark));
		if (mark)
			return true;
		if (!o)
			return false;
		BluePy ret(PyObject_CallMethodObjArgs(target, mStock_Append, o.o, 0));
		if (!ret)
			return false;
	}
}


//read and apply any dict iterator to a reduced object
bool Marshal::ReadObjectDictIter(ReadStream &stream, PyObject *target)
{
	for(;;) {
		bool mark;
		BluePy key(ReadObjectOrMarker(&stream, mark));
		if (mark)
			return true;
		if (!key)
			return false;
		
		BluePy val(ReadObject(&stream));
		if (!val)
			return false;
		BluePy ret(PyObject_CallMethodObjArgs(target, mStock_SetItem, key.o, val.o, 0));
		if (!ret)
			return false;
	}
}


//Read a Long object
PyObject *Marshal::ReadObjectLong(ReadStream *stream, bool shared)
{
	const char *buf;
	int s;
	if (!stream->GetBuff(buf, s))
		return 0;
	PyObject *result;
	if (!s)
		result = PyLong_FromLong(0);
	else
		result = _PyLong_FromByteArray((unsigned char*)buf, s, 1, 1);
	if (!result)
		return 0;

	if (shared && !stream->MarkShared(result)) {
		Py_DECREF(result);
		return 0;
	}
	return result;
}


//Get an object by module.name convention.  if not dot, then we expect the __builtin__.
//The stuff is cached for quick retrieval
PyObject *Marshal::GetGlobalObject(PyObject *nameO)
{
	PyObject *cached = PyDict_GetItem(mGuidTable, nameO);
	if (cached) {
		Py_INCREF(cached);
		return cached;
	}

#ifdef PY27_COMPATIBILITY_MODE
    // Backwards compatibility with old-style classes marshalled from python 2.7
    BluePy obj(nameO, true);
    if (PyBytes_Check(obj.o))
    {
        obj = BluePy(PyUnicode_FromEncodedObject(nameO, "UTF-8", 0));
    }

    const char *name;
	if (PyUnicode_Check(obj.o))
    {
        name = PyUnicode_AsUTF8(obj.o);
    }
    else
    {
        return PyErr_SetString(PyExc_RuntimeError, "expected string"), nullptr;
    }
#else
	if( !PyUnicode_Check(nameO) )
	{
		PyErr_SetString(PyExc_RuntimeError, "expected string");
		return nullptr;
	}
	const char* name = PyUnicode_AsUTF8( nameO );
#endif

	const char *dot = strrchr(name, '.');
	BluePyStr modulename;
	if (dot){
#ifdef PY27_COMPATIBILITY_MODE
		if( strncmp( name, "__builtin__.", 12 ) == 0 || strncmp( name, "exceptions.", 11 ) == 0 )
		{
			modulename = BluePyStr("builtins");
		}
		else
		{
			modulename = BluePyStr(dot-name, name);
		}
#else
		modulename = BluePyStr( dot-name, name );
#endif
		name = dot+1;
	} else {
		modulename = BluePyStr("builtins");
	}

	BluePy module( PyImport_ImportModule( PyUnicode_AsUTF8( modulename ) ) );
	if (!module) return 0;
	if (!CheckGlobalsBlacklist(module, modulename.Str(), 0)) return 0; //check if module is blacklisted
	BluePy r(PyObject_GetAttrString(module, (char*)name));
	if (!r) return 0;

	if (!CheckGlobalsBlacklist(r, modulename.Str(), name)) return 0; //check if object is blacklisted

	if (PyDict_SetItem(mGuidTable, nameO, r)) return 0;
	return r.Detach();
}

	

//--------------------------------------------------------------------
bool Marshal::WriteObject(WriteStream* stream, PyObject* o)
{
	CCP_ASSERT(o != static_cast<PyObject*>(stream));

	Incrementor _inc(mRecursionLevel);
	if( !_inc.Test( sRecursionLimit ) )
		return false;


#define CHECKREF() do {\
	if (o->ob_refcnt  > 1) {\
		bool referred; \
		if (!stream->ObjectReferenced(referred, o))\
			/* error in output */\
			return false; \
		if (referred) {\
			/* reference link successful, exit true */\
			m_typesSaved[TY_REFERENCE] += 1; \
			return true;\
		} /* otherwise, continue */\
	}\
} while(false)

	switch ( Py_TYPE( o )->tp_name[0] )
	{
	case 'd':
		if (o->ob_type == &PyDict_Type) //PyDict_CheckExact(o)
		{
			CHECKREF();
			Py_ssize_t size = PyDict_Size(o);
			RETFAIL(WriteType(stream, TY_DICT));
			RETFAIL(stream->WriteInteger((int)size));
			Py_ssize_t pos = 0;
			PyObject* key;
			PyObject* value;

			while (PyDict_Next(o, &pos, &key, &value))
			{
				RETFAIL(WriteObject(stream, value));
				RETFAIL(WriteObject(stream, key));
			}
			return true;
		}
		break;

	case 'N':
		if (o == Py_None)
		{
			RETFAIL(WriteType(stream, TY_NONE));
			return true;
		}
		break;

	case 't':
		if (PyTuple_CheckExact(o))
		{
			Py_ssize_t size = PyTuple_GET_SIZE(o);
			if (!size) {
				RETFAIL(WriteType(stream, TY_TUPLE0));
				return true;
			}			
			CHECKREF();
			if (size == 1) {
				RETFAIL(WriteType(stream, TY_TUPLE1));
				RETFAIL(WriteObject(stream, PyTuple_GET_ITEM(o, 0)));
				return true;
			} else if (size == 2) {
				RETFAIL(WriteType(stream, TY_TUPLE2));
				RETFAIL(WriteObject(stream, PyTuple_GET_ITEM(o, 0)));
				RETFAIL(WriteObject(stream, PyTuple_GET_ITEM(o, 1)));
				return true;
			}
			RETFAIL(WriteType(stream, TY_TUPLE));
			RETFAIL(stream->WriteInteger((int)size));
			for (int i = 0; i < PyTuple_GET_SIZE(o); i++)
				RETFAIL(WriteObject(stream, PyTuple_GET_ITEM(o, i)));
			return true;
		} else if (PyType_CheckExact(o)) {
			return WriteObjectGlobal(stream, o);
		}
		break;

	case  'i':
		if (PyLong_CheckExact(o))
		{
			long i = PyLong_AsLong(o);

			// Deal with overflow errors; Python long objects are arbitrarily ... long
			if ( PyErr_Occurred() ) {
				if ( ! PyErr_ExceptionMatches( PyExc_OverflowError ) )
				{
					return false;
				}
				PyErr_Clear();
				auto success = WriteLong(stream, o);
				return success;
			}

			switch(i)
			{
			case -1:
				return WriteType(stream, TY_INT_N1);

			case 0:
				return WriteType(stream, TY_INT_0);
				
			case 1:
				return WriteType(stream, TY_INT_1);
				
			default:
				if (i >= CHAR_MIN && i <= CHAR_MAX)
				{
					RETFAIL(WriteType(stream, TY_INT8));
					RETFAIL(stream->Write<int8_t>(int8_t(i)));
				}
				else if (i >= SHRT_MIN && i <= SHRT_MAX)
				{
					RETFAIL(WriteType(stream, TY_INT16));
					RETFAIL(stream->Write<int16_t>(int16_t(i)));
				}
				else if (i >= INT_MIN && i <= INT_MAX)
				{
					RETFAIL(WriteType(stream, TY_INT32));
					RETFAIL(stream->Write<int32_t>(i));
				}
				else
                {
				    auto success = WriteLong(stream, o);
				    return success;
                }
				return true;
			}
		}
		break;


	case  'l':
		if (PyLong_CheckExact(o))
		{
			int sign = _PyLong_Sign(o);
			if (sign != 0)
				CHECKREF();  //do this for non-zero longs
			return WriteLong(stream, o);
		}
		else if (PyList_CheckExact(o))
		{
			Py_ssize_t size = PyList_GET_SIZE(o);
			if (!size)
				return WriteType(stream, TY_LIST0);
				
			CHECKREF();
			if (size == 1)
				return WriteType(stream, TY_LIST1) &&
					   WriteObject(stream, PyList_GET_ITEM(o, 0));
				
			RETFAIL(WriteType(stream, TY_LIST));
			RETFAIL(stream->WriteInteger((int)size));
			for (int i = 0; i < PyList_GET_SIZE(o); i++)
				RETFAIL(WriteObject(stream, PyList_GET_ITEM(o, i)));
			return true;
		}
		break;

	case  'f':
		if (PyFloat_CheckExact(o))
		{
			if (PyFloat_AS_DOUBLE(o) == 0.0)
				return WriteType(stream, TY_FLOAT_0);
			else
				return WriteType(stream, TY_FLOAT) &&
					   stream->Write(PyFloat_AS_DOUBLE(o));
		}
		else if (PyFunction_Check(o))
		{
			if (WriteObjectGlobal(stream, o))
				return true;
		}
		break;

	case  's':
		if (PyUnicode_CheckExact(o))
		{
#ifdef PY27_COMPATIBILITY_MODE
			BluePy encoded( PyUnicode_AsUTF8String( o ) );
			RETFAIL( encoded.o );
			Py_ssize_t size = PyObject_Length( encoded.o );
			const char* data = PyBytes_AsString( encoded.o );
			if (size == 0) {
				return WriteType(stream, TY_UNICODE_0);
			} else {
				PyObject* index = PyDict_GetItem( mStrTable, o );
				if( index )
				{
					RETFAIL( WriteType( stream, TY_STR_TABLE ) );
					return stream->Write( (char)PyLong_AS_LONG( index ) );
				}
				else
				{
					return WriteType( stream, TY_UTF8_OBSOLETE ) && stream->WriteInteger( (int)size ) &&
						stream->WriteBuffWoSize( data, size );
				}
			}
#else
			Py_ssize_t size = 0;
			const char *data = PyUnicode_AsUTF8AndSize(o, &size);
			if (size == 0) {
				return WriteType(stream, TY_UNICODE_0);
			} else if (size == 1) {
				return WriteType(stream, TY_UNICODE_1) && stream->Write(*data);
			} else {
				PyObject* index = PyDict_GetItem( mStrTable, o );
				if( index )
				{
					RETFAIL( WriteType( stream, TY_STR_TABLE ) );
					return stream->Write( (char)PyLong_AS_LONG( index ) );
				}
				else
				{
					return WriteType( stream, TY_UNICODE ) && stream->WriteInteger( (int)size ) &&
						stream->WriteBuffWoSize( data, size );
				}
			}
#endif
		}
		break;

	case  'b':
		if (o == Py_True)
			return WriteType(stream, TY_TRUE);
		else if (o == Py_False)
			return WriteType(stream, TY_FALSE);

		if (o->ob_type == DBRow::GetType())
			//name is blue.DBRow
			return WriteType(stream, TY_DBROW) && static_cast<DBRow*>(o)->Write(*this, *stream);

		if (o->ob_type == WriteStream::GetType())
			return WriteType(stream, TY_WSTREAM) && static_cast<WriteStream*>(o)->Write(*this, *stream);

		if ( PyObject_CheckBuffer( o ) )
		{
			CHECKREF();
			Py_buffer srcbuff;
			if ( PyObject_GetBuffer( o, &srcbuff, PyBUF_SIMPLE | PyBUF_ANY_CONTIGUOUS ) ) {
				return false;
			}
			auto success = WriteType(stream, TY_BUFFER) && stream->WriteBuff(srcbuff.buf, srcbuff.len);
			PyBuffer_Release(&srcbuff);
			return success;
		}

		break;

	case  'c':
		if (PyObject_IsInstance(o, (PyObject *)&PyType_Type))
		{
			if (WriteObjectGlobal(stream, o))
				return true;
		}
		break;
	}

	//Other classes, instances of subclasses of "type"  (we've alredy handled instances of 'type' above)
	if (PyType_Check(o))
	{
		if (WriteObjectGlobal(stream, o))
			return true;
	}

	//it's something else.  Check dict.
	CHECKREF();
	
	// Let's check if we have a callback that knows how to deal with this.
	if( stream->mCallback )
	{
		if( WriteCallbackResult( stream, o ) )
		{
			return true;
		}
	}

	//okay, none of the above worked.  Let's try the simple reduce protocol (works for new style stuff)
	bool handled;
	if (!WriteObjectReduce(handled, stream, o))
		return false;
	if (handled)
		return true;

	//The above simple rules did not apply.  Generic approach, then:
	CCP_LOGWARN_CH( s_ch, "defaulting to pickle for object %p of type %s", o, Py_TYPE( o )->tp_name );
	
	//new style pickle generation fallback.
	PyObject *p = stream->GetPickler();
	if (!p) return false;
	RETFAIL(WriteType(stream, TY_PICKLER));
	PyObject *r = PyObject_CallMethod(p, (char*)"dump", (char*)"(O)", o);
	Py_XDECREF(r);
	return r!=0;
}


//Write marker object
bool Marshal::WriteMarker(WriteStream *stream)
{
	return WriteType(stream, TY_MARK);
}


//Write an object by global name
bool Marshal::WriteObjectGlobal(WriteStream* stream, PyObject *o)
{
	CHECKREF();
	
	BluePyStr fullname;
	BluePyStr name = BluePy(PyObject_GetAttr(o, mStock_Name));
	if (!name) return false;
	if (!name.Check())
		return PyErr_SetString(PyExc_RuntimeError, "__name__ must be string"), false;
	BluePyStr module = BluePy(PyObject_GetAttr(o, mStock_ModuleString));
	if (module) {
		if (!module.Check())
			return PyErr_SetString(PyExc_RuntimeError, "__module__ must be string"), false;
		fullname = BluePyStr::Format("%U.%U", module.o, name.o);
		if (!fullname) return 0;
	} else
		fullname = name;

	BluePy verify(GetGlobalObject(fullname));
	if (!verify)
		return false;
	if (verify != o)
		return PyErr_Format(PyExc_RuntimeError, "object %s isn't same as %p", fullname.Str(), o), false;

	Py_ssize_t len;
	const char *buff = fullname.Str(len);
	if (!buff)
		return false;
	RETFAIL(WriteType(stream, TY_GLOBAL));
	return stream->WriteBuff(buff, len);
}


bool Marshal::WriteCallbackResult( WriteStream* stream, PyObject* o )
{
	if( stream->mCallback != nullptr )
	{
		// Call the callback object. If the return value is not None, return the return value.
		PyObject* args = PyTuple_New( 1 );
		PyTuple_SET_ITEM( args, 0, o );
		Py_INCREF( o );
		PyObject* ret;
		{
			AutoTasklet _at( PyOS->GetTaskletTimer(), mTimer_SaveCallback );
			ret = PyObject_CallObject( stream->mCallback, args );
		}
		Py_DECREF( args );
		if( ret == nullptr )
		{
			PyOS->PyFlushError( "Marshalling callback failed" );
			return false;
		}
		if( ret != Py_None )
		{
			bool ok = WriteType( stream, TY_CALLBACK ) && WriteObject( stream, ret );
			Py_DECREF( ret );
			return ok;
		}
		Py_DECREF( ret );
	}
	return false;
}


bool Marshal::WriteObjectInstance(WriteStream* stream, PyObject* o)
{
	CCP_LOGERR("Does not support writing object instances");
	return false;
//	CHECKREF();
//	PyInstanceObject* inst = (PyInstanceObject*)o;
//	PyObject* guid;
//
//	// Custom marshal?
//	if (stream->mCallback != NULL)
//	{
//		PyObject* args = PyTuple_New(1);
//		PyTuple_SET_ITEM(args, 0, o); Py_INCREF(o);
//		PyObject *ret;
//		{
//			AutoTasklet _at(PyOS->GetTaskletTimer(), mTimer_SaveCallback);
//			ret = PyObject_CallObject(stream->mCallback, args);
//		}
//		Py_DECREF(args);
//		if (ret == NULL)
//			return false;
//
//		if (ret != Py_None)
//		{
//			bool ok = WriteType(stream, TY_CALLBACK) && WriteObject(stream, ret);
//			Py_DECREF(ret);
//			return ok;
//		}
//		Py_DECREF(ret); //ok, the callback decided not to interfere
//	}
//
//	// Get module.classname string
//	PyObject* module = PyDict_GetItem(inst->in_class->cl_dict, mStock_ModuleString);
//	if (module == NULL)
//	{
//		PyObject* repr = PyObject_Repr(o);
//		PyErr_Format(
//			PyExc_RuntimeError,
//			"Instance %s has no __module__ attribute.",
//			PyString_AS_STRING(repr)
//			);
//		Py_DECREF(repr);
//		return false;
//	}
//
//	guid = PyString_FromFormat(
//		"%s.%s",
//		PyString_AS_STRING(module), PyString_AS_STRING(inst->in_class->cl_name)
//		);
//	if (!guid) return false;
//
//	//intern the string, that will make it subject to object sharing
//	PyString_InternInPlace(&guid);
//	if (!guid) return false;
//
//	// Write guid out as python string so it will get tokenized in the
//	// table cache.
//	bool ok = WriteType(stream, TY_INSTANCE) && WriteObject(stream, guid);
//	Py_DECREF(guid);
//	if (!ok)
//		return false;
//
//	// If instance has __getstate__ it wants custom marshalling
//	if (PyObject_HasAttr(o, mStock_GetState))
//	{
//		PyObject *data;
//		{
//			AutoTasklet _at(PyOS->GetTaskletTimer(), mTimer_GetState);
//			data = PyObject_CallMethodObjArgs(o, mStock_GetState, NULL);
//		}
//		if (data == NULL)
//			return false;
//		bool ok = WriteObject(stream, data);
//		Py_DECREF(data);
//		return ok;
//	}
//
//	// Write out the instance's dictionary
//	return WriteObject(stream, inst->in_dict);
}


//Write an object using the simple __reduce__ protocol.
bool Marshal::WriteObjectReduce(bool &handled, WriteStream* stream, PyObject* o)
{
	handled = false;
	BluePy rv(PyObject_CallMethod(o, (char*)"__reduce_ex__", (char*)"(i)", 2));
	if (!rv) {
		//fallback
		PyErr_Clear();
		rv = BluePy(PyObject_CallMethod(o, (char*)"__reduce__", 0));
	}
	if (!rv || !PyTuple_Check(rv.o))
		return PyErr_Clear(), true; //no, can't reduce.
	
	Py_ssize_t tsize = PyTuple_GET_SIZE(rv.o);
	//strip trailing Nones
	while(tsize) {
		if (PyTuple_GET_ITEM(rv.o, tsize-1) != Py_None)
			break;
		tsize--;
	}

	if (tsize < 2 || tsize > 5)
		return PyErr_Clear(), true; //some weird protocol

	//first and second must be callable and tuple
	PyObject *callable = PyTuple_GET_ITEM(rv.o, 0);
	if (!PyCallable_Check(callable))
		return PyErr_Clear(), true; //no, can't reduce.
	PyObject *args = PyTuple_GET_ITEM(rv.o, 1);
	if (!PyTuple_Check(args)) return true; //unexpected: args is not a tuple.
	
	//check for special case, the __newobj__ thing.  See pickle.py.  basicle a tuple with class and args
	bool newobj = false;
	BluePy name(PyObject_GetAttr(callable, mStock_Name));
	if (!name)
		PyErr_Clear();
	if (name && PyUnicode_Check(name.o) && !PyUnicode_CompareWithASCIIString( name.o, "__newobj__" ) )
		newobj = true;

	// we have to be careful not to incref the stuff in the __reduce__ pickle before writing,
	// since it degrades performance to write objects with ref>1 due to shared-checking.

	//do we have state?
	PyObject *state = 0;
	if (tsize >2) {
		state = PyTuple_GET_ITEM(rv.o, 2); //borrow ref
		if (state == Py_None)
			state = 0;  //ignore an empty dict;
	}

	//what about list iterator?
	BluePy listitems;
	if (tsize>3)
		listitems = BluePy(PyTuple_GET_ITEM(rv.o, 3), true);
	//and dictitems?
	BluePy dictitems;
	if (tsize>4)
		dictitems = BluePy(PyTuple_GET_ITEM(rv.o, 4), true);

	//Set up the stuff to write.
	PYTYPES type;
	BluePy data;
	if (newobj) {
		type = TY_NEWOBJ;
		BluePyTuple stuff(1+(state?1:0));
		stuff.Set(0, args);
		if (state)
			stuff.Set(1, state);
		data = stuff;
	} else {
		type = TY_REDUCE;
		BluePyTuple stuff(2+(state?1:0));
		stuff.Set(0, callable);
		stuff.Set(1, args);
		if (state)
			stuff.Set(2, state);
		data = stuff;
	}
	rv.Release();  //do this, to keep only a single reference to temporary objects

	//do it!
	handled = true;
	RETFAIL(WriteType(stream, type));
	return WriteObject(stream, data) && WriteListIter(stream, listitems) && WriteDictIter(stream, dictitems);
}


// Write the iterators returned as 4th and 5th arguments in the pickle protocol
bool Marshal::WriteListIter(WriteStream* stream, PyObject* o)
{
	if (o && o != Py_None) {
		for(;;) {
			BluePy next(PyIter_Next(o));
			if (!next)
				break;
			if (!WriteObject(stream, next))
				return false;
		}
		if (PyErr_Occurred())
			return false;
	}
	if (!WriteMarker(stream))
		return false;
	return true;
}


bool Marshal::WriteDictIter(WriteStream* stream, PyObject* o)
{
	if (o && o != Py_None) {
		for(;;) {
			BluePy next(PyIter_Next(o));
			if (!next)
				break;
			if (!PyTuple_Check(next.o) || PyTuple_GET_SIZE(next.o) != 2) {
				PyErr_SetString(PyExc_TypeError, "iterator should return 2-tuple");
				break;
			}
			if (!WriteObject(stream, PyTuple_GET_ITEM(next.o, 0)) || !WriteObject(stream, PyTuple_GET_ITEM(next.o, 1)))
				return false;
		}
		if (PyErr_Occurred())
			return false;
	}
	if (!WriteMarker(stream))
		return false;
	return true;
}


bool Marshal::WriteLong(WriteStream *stream, PyObject *lng)
{
	if (!WriteType(stream, TY_LONG))
		return false;
	
	int sign = _PyLong_Sign(lng);
	if (sign == 0)
		// empty bytestring
		return stream->WriteBuff(0, 0);

	size_t nbits = _PyLong_NumBits(lng);
	if (nbits == (size_t)-1 && PyErr_Occurred())
		return false;

	size_t nbytes = (nbits>>3)+1;
	std::vector<unsigned char> buf(nbytes);
	int i = _PyLong_AsByteArray((PyLongObject*)lng, &buf[0], nbytes, 1, 1);
	if (i)
		return false;
	//the last byte may be redundant
	if (sign < 0 && nbytes > 1 && buf[nbytes - 1] == 0xff && (buf[nbytes - 2] & 0x80) != 0)
		nbytes--;
	return stream->WriteBuff(&buf[0], nbytes);
}
		

//--------------------------------------------------------------------
PyObject* Marshal::SaveObject(PyObject* o, PyObject* cb, int useChecksum, int version, PyObject *stringMap)
{
	AutoTasklet _at(PyOS->GetTaskletTimer(), mTimer_Save);

	BluePy streamO(WriteStream::_New(WriteStream::GetType(), 0, 0));
	if (!streamO) return 0;
	WriteStream *stream = static_cast<WriteStream*>(streamO.o);
	if (!stream->Init(cb, !!useChecksum, version)) return 0;

	if( useChecksum )
	{
		m_typesSaved[TY_CRC_CHECK] += 1;
	}

	//swap out the stringmap dict if necessary for the duration
	BluePy tmpMap = mStrTable;
	if (stringMap)
		mStrTable = BluePy(stringMap, true);
	bool ok = WriteObject(stream, o);
	mStrTable = tmpMap;

	if (!ok) return 0;
	
	size_t totalLen, mapLen;
	ok = stream->Finalize(totalLen, mapLen);
	if (!ok) return 0;
	mTotalSent += totalLen;
	mMapSent += mapLen;
	return streamO.Detach();
}


//--------------------------------------------------------------------
PyObject* Marshal::Save(PyObject* args, PyObject* kw)
{
	PyObject* o;
	PyObject* callback = NULL;
	int useChecksum = 0;
	int version = 0; //default is 0 until later
	PyObject *stringMap = Py_None;

	static const char *kwlist[] = {"object", "callback", "useChecksum", "version", "stringMap", NULL};

	if (PyTuple_Check(args)) {
		if (!PyArg_ParseTupleAndKeywords(args, kw, "O|OiiO:Save", (char**)kwlist,	&o, &callback, &useChecksum, &version, &stringMap))
			return NULL;
		if (callback == Py_None)
			callback = NULL;
		if (callback && !PyCallable_Check(callback))
			return PyErr_SetString(PyExc_TypeError, "callback argument must be callable"), nullptr;
		if (version != 0 && version != 1)
			return PyErr_SetString(PyExc_ValueError, "invalid version"), nullptr;
	} else
		o = args;
	
	if (stringMap == Py_None)
		stringMap = 0;
	else if (!PyDict_Check(stringMap))
		return PyErr_SetString(PyExc_TypeError, "stringMap must be a dict"), nullptr;

	return SaveObject(o, callback, useChecksum, version, stringMap);
}


//--------------------------------------------------------------------
PyObject* Marshal::Load(PyObject* args, PyObject *kw)
{
	PyObject* o;
	PyObject* callback = NULL;
	mPacketHadCrc = 0;
	mSkipCrcCheck = 0;
	Py_ssize_t offset = -1;
	PyObject *stringTable = Py_None;

	AutoTasklet _at(PyOS->GetTaskletTimer(), mTimer_Load);

	if (PyTuple_Check(args)) {
		static const char *kwlist[] = {"buffer", "callback", "skipCrcCheck", "offset", "stringTable", 0};
		if (!PyArg_ParseTupleAndKeywords(args, kw, "O|OinO:Load", (char**)kwlist, &o, &callback, &mSkipCrcCheck, &offset, &stringTable ))
			return 0;
		if (callback == Py_None)
			callback = NULL;
		if (callback && !PyCallable_Check(callback))
			return PyErr_SetString(PyExc_TypeError, "callback argument must be callable"), nullptr;
	} else
		o = args;  //internal calls do this.
		
	// 'o' must support buffer protocol
	if ( ! PyObject_CheckBuffer( o ) ) //o->ob_type->tp_as_buffer == NULL ||	o->ob_type->tp_as_buffer->bf_getreadbuffer == NULL)
		return PyErr_SetString(PyExc_TypeError, "argument must be a string or buffer"), nullptr;

	Py_buffer buffer;
	if ( PyObject_GetBuffer( o, &buffer, PyBUF_ANY_CONTIGUOUS | PyBUF_SIMPLE ) )
	{
		return nullptr;
	}

	void* srcbuff = buffer.buf;
	Py_ssize_t bufflen = buffer.len; // o->ob_type->tp_as_buffer->bf_getreadbuffer( buffer, 0, &srcbuff);
	if (offset >= 0) {
		if (offset > bufflen) {
			PyErr_Format(PyExc_ValueError, "invalid offset %d", (int)offset);
			PyBuffer_Release(&buffer);
			return nullptr;
		}
		srcbuff = (void*)((char*)srcbuff + offset);
		bufflen -= offset;
	}

	ReadStream stream(srcbuff, bufflen, callback);

	//replace the string table for the duration, if required
	BluePy oldStringTable = mStrTableRev;
	if (stringTable != Py_None) {
		if (!PyList_Check(stringTable)) {
			PyErr_SetString(PyExc_TypeError, "stringTable must be a list");
			PyBuffer_Release(&buffer);
			return nullptr;
		}
		mStrTableRev = BluePy(stringTable, true);
	}

	//Read stream header (instance maps), returns borrowed ref
	PyObject *result = ReadWrapper(stream, &Marshal::ReadHeader);
	if (result)
		//Read the single object requested
		result = ReadWrapper(stream, &Marshal::ReadObject);
	
	//restore the string table
	mStrTableRev = oldStringTable;

	if (!result) {
		CResFile f;
		if (f.CreateW(L"cache:/bogus_stream.marshal"))
			f.Write(srcbuff, bufflen);
	}

	PyBuffer_Release(&buffer);

	if (result && stream.mGotCRC) {
		if (stream.GetVersion() > 0 && !mSkipCrcCheck) {
			if (!stream.Crc())
			{
				return nullptr;
			}
		}
		mPacketHadCrc = 1;
	}

	if (offset >= 0)
		return Py_BuildValue("Nn", result, stream.GetPos() + offset);
	return result;
}


//Wraps any exception from the inside into a neat litle unmarshal exception
PyObject *Marshal::ReadWrapper(ReadStream &stream, reader_t func)
{
	PyObject *result = (this->*func)(&stream);
	if (result)
		return result;

	//now, we have an exception.
	//Create exception instance from exception type and value
	BluePy err[3];
	PyErr_Fetch(&err[0].o, &err[1].o, &err[2].o);

	//get unmarshal exception
	BluePy ume(PyDict_GetItemString(PyEval_GetBuiltins(), "UnmarshalError"), true);
	if (!ume)
		ume = BluePy(PyExc_RuntimeError);

	//and value
	BluePy umv(Py_BuildValue("OOiii", 
		err[0].o?err[0].o:Py_None,
		err[1].o?err[1].o:Py_None,
		stream.mSize, stream.mPos, stream.mGotCRC));

	PyErr_SetObject(ume, umv);
	return 0;
}


PyObject* Marshal::ReadHeader(ReadStream *streamptr)
{
	ReadStream &stream = *streamptr;

	PYTYPES token;
	if (!stream.ReadType(token)) return 0;
	if (token != TY_SIGNATURE && token != TY_SIGNATURE2) {
		//Must start with the header
		return PyErr_Format(PyExc_ValueError, "invalid marshal header"), nullptr;
	}
	if (token == TY_SIGNATURE2) {
		unsigned char version;
		if (!stream.Read<unsigned char>(version)) return 0;
		stream.SetVersion(version);
	} else
		stream.SetVersion(0);
		
	if (stream.GetVersion() == 0) {
		if (!stream.Read(stream.mMapCount)) return 0;
		if (stream.mMapCount < 0) {
			PyErr_Format(PyExc_ValueError, "invalid mapcount in marshal stream, %d.", stream.mMapCount);
			return 0;
		}
		if (stream.mMapCount > 0) {
			//read the map
			//don't multiply maplen, it may cause overflow and destroy our test.
			if (stream.Remaining()/(int)sizeof(int) < stream.mMapCount)	{
				PyErr_Format(PyExc_ValueError,
						"Too little data in marshal stream, %" CCP_SIZET_FORMAT " bytes. "
						"I really wanted at least %" CCP_SIZET_FORMAT " bytes total, mapcount in header is %d",
						stream.mSize, stream.mPos + (size_t)stream.mMapCount * sizeof (int), stream.mMapCount
					);
				return NULL;
			}
			//should be safe now to multiply
			size_t maplen = stream.mMapCount*sizeof(int);

			//set the pointer to map to the mapping
			stream.mMapping = (int*)(stream.mBuff+stream.mSize-maplen);
			try {
				stream.mShared.resize(stream.mMapCount);
			} catch (std::bad_alloc) {
				return PyErr_NoMemory();
			}
			stream.mSSize -= (unsigned int) maplen; //for all our safety checks
		
			// validate content
			for (int i = 0; i < stream.mMapCount; i++) {
				if (stream.mMapping[i] < 1 || stream.mMapping[i] > stream.mMapCount)
					return PyErr_SetString(PyExc_ValueError,	"Bogus map data in marshal stream."	), nullptr;
			}
		}
	} else
		stream.mMapCount = 0;
	return Py_None;  //borrowed ref, indicates success.
}


//////////////
// Functions for unpickling and logging what gets unpickled.

//This function hooks the unpickler up to our load global thingy
bool Marshal::HookUnpickler(PyObject *up)
{
	BluePy lg(PyObject_GetAttrString(this, "find_global"));
	if (!lg) return false;
	return PyObject_SetAttrString(up, "find_global", lg) == 0;
}


//This function is used by the unpickler to register the classes we unpickle.
//it is also used as an filter to blacklist objects we don't want to unpickle, for security reasons.
PyObject *Marshal::find_global(PyObject *args)
{
	PyObject *module, *name;
	if (!PyArg_UnpackTuple(args, "load_global", 2, 2, &module, &name))
		return 0;
	if (!UpdateGlobalNames(module, name))
		return 0;

	BluePy m(PyImport_Import(module));
	if (!m) return 0;
	if (!CheckGlobalsBlacklist(m, PyUnicode_AsUTF8(module), 0)) return 0; //module blacklist
	BluePy obj(PyObject_GetAttr(m, name));
	if (!obj) return 0;
	if (!CheckGlobalsBlacklist(obj, PyUnicode_AsUTF8(module), PyUnicode_AsUTF8(name))) return 0; //Object blacklist.
	return obj.Detach();
}


PyObject *Marshal::Flush(PyObject *args)
{
	if (!PyArg_ParseTuple(args, "")) return 0;
	if (mGuidTable)
		PyDict_Clear(mGuidTable);
	Py_INCREF(Py_None);
	return Py_None;
}


void Marshal::InitGlobalsBlacklist()
{
	struct pair {
		const char *module;
		const char *name;
	};
	//full objects are more powerful, they prevent aliasing.  Disabling a module means that the
	//objects in there can still be accessed if they are aliased to another module.
	static const pair list[] = {
		{"builtins", "eval"}, //The evil of all evils
		{"builtins", "execfile"}, //also evil
		{"builtins", "getattr"}, //can be used with __import__ to acess anything
		{"builtins", "setattr"},
		{"builtins", "__import__"},
		{"builtins", "globals"},
		{"builtins", "locals"},
		{"builtins", "vars"},
		{"builtins", "reload"},
		{"builtins", 0}, //Also, disallow the __builtin__ module
		{"__main__", 0}, //all sorts of junk here
		{"os", 0}, //all of module os, which contains evil process management and stuff.
		{"pickle", 0}, //don't want the hacker to instantiate another pickler
		{"marshal", 0}, //or a marshaller
		{"codeop", 0},  //codeop and code used for command line execution
		{"code", 0},
		{"new", 0},		//don't want to create function objects
		//{"sys", 0},		//for good measure
		{"sys", "setrecursionlimit"}, //Can't disable sys yet, there is a "sys" module used by nasty.py
		{"sys", "settrace"},
		{"sys", "setprofile"},
		{"sys", "setcheckinterval"},
		{"sys", "_getframe"},
		{"sys", "exit"},
		{"uthread", 0},	//ditto
		{"sched" ,0},	//ditto
		{"blue.win32", 0}, //evil debugging functions like DebugBreak and DebugCrash
		{"blue.pyos", 0}, //various stuff for good measure.
		{"blue.os", 0},
		{"base", 0},
		{"eve", 0},
		{"nasty", 0},
		{"scheduler", 0},
		{"bluepy", 0}, //product specific thread management stuff
		{"bluepycore", 0}, //our own thread management stuff

		// Recent additions from Veritech, 04.04.2006
		{"types", 0},
		{"builtins", "type"},
		{"types", "FunctionType"},
		{"types", "CodeType"},
		{"types", "InstanceType"},
		{"types", "ClassType"},
		{"types", "ModuleType"},
		{"types", "MethodType"},

		{"gc", 0}, //gc module can slow system down

		{0}
	};

	for (const pair *p = list; p->module; p++) {
		BluePyStr ms(p->module);
		BluePy m(PyImport_Import(ms));
		if (!m) {
			PyErr_Clear();
			continue;
		} 
		if (p->name) {
			PyObject *obj = PyObject_GetAttrString(m, (char*)p->name);
			if (!obj) {
				PyErr_Clear();
				continue;
			}
			Py_DECREF(obj); //we just want to borrow the ref
			mGlobalsBlacklist.insert(obj);
		} else {
			//blacklisting an entire module
			mGlobalsBlacklist.insert(m);
		}
	}
	mGlobalsBlacklistInit = true;
}


bool Marshal::CheckGlobalsBlacklist(PyObject *obj, const char *mod, const char *name)
{
	CCP_ASSERT(mod);
	//If there is a whitelist, just use that
	if ( mGlobalsWhitelist && !mCollectWhitelist && PyDict_Check(mGlobalsWhitelist.o)) {
		if (!name)
			//allow all modules (items in modules will be checked later
			return true;
		if (!PyDict_GetItem(mGlobalsWhitelist, obj))
			return PyErr_Format(PyExc_RuntimeError, "HACKER WARNING! object %s.%s is not in whitelist", mod, name), false;
		else
			return true;
	}

	//Check blacklist.  We blacklist the actual objects rather than names, since objects can be
	//aliased.
	if (!mGlobalsBlacklistInit)
		InitGlobalsBlacklist();
	if (mGlobalsBlacklist.find(obj) != mGlobalsBlacklist.end())
		//found in the blacklist
		return PyErr_Format(PyExc_RuntimeError, "HACKER WARNING! object %s.%s is blacklisted", mod, name?name:"*"), false;

	//collect whitelist for the object if so specified.
	if (name && mCollectWhitelist) {
		if (!mGlobalsWhitelist || !PyDict_Check(mGlobalsWhitelist.o))
			mGlobalsWhitelist = BluePy(PyDict_New());
		BluePy val(Py_BuildValue("(s,s)", mod, name));
		if (PyDict_SetItem(mGlobalsWhitelist, obj, val))
			return false;
	}
	return true;
}


bool Marshal::UpdateGlobalNames(PyObject *module, PyObject *name)
{
	//use different dicts for guids and globals;
	PyObject *dict = mStatGlobals.o;
	if (!dict || !PyDict_Check(dict))
		return true;
	BluePy s;
	if (module && name)
		s = BluePy(PyUnicode_FromFormat("pickle: %s.%s", PyUnicode_AsUTF8(module), PyUnicode_AsUTF8(name)));
	else if (name)
		s = BluePy(PyUnicode_FromFormat("TY_GLOBAL: %s", PyUnicode_AsUTF8(name)));
	else
		s = BluePy(PyUnicode_FromFormat("ClassType: %s", PyUnicode_AsUTF8(module)));
		
	PyObject *v = PyDict_GetItem(dict, s);
	long val;
	if (v) {
		val = PyLong_AsLong(v);
		if (val == -1 && PyErr_Occurred())
			return false;
	} else
		val = 0;
	BluePyInt n(int(1 + val));
	return (0==PyDict_SetItem(dict, s, n));
}

PyObject *Marshal::Get_overhead()
{
	double f = (double)mMapSent / (double)mTotalSent;
	return PyFloat_FromDouble(f);
}

PyObject* Marshal::ResetTypeStats()
{
	m_typesLoaded = std::vector<unsigned int>( 64, 0 );
	m_typesSaved = std::vector<unsigned int>( 64, 0 );
	Py_RETURN_NONE;
}

PyObject* Marshal::GetTypeStats()
{
	auto n = m_typesLoaded.size();
	
	PyObject* loaded = PyList_New( n );
	for( size_t i = 0; i < n; ++i )
	{
		PyList_SET_ITEM( loaded, i, PyLong_FromLong( m_typesLoaded[i] ) );
	}

	PyObject* saved = PyList_New( n );
	for( size_t i = 0; i < n; ++i )
	{
		PyList_SET_ITEM( saved, i, PyLong_FromLong( m_typesSaved[i] ) );
	}

	PyObject* result = PyTuple_New( 2 );
	PyTuple_SET_ITEM( result, 0, loaded );
	PyTuple_SET_ITEM( result, 1, saved );

	return result;
}

bool Marshal::WriteType( class WriteStream* stream, enum PYTYPES type )
{
	m_typesSaved[type & TY_TYPEMASK] += 1;
	return stream->WriteType(type );
}

Marshal::ReadObjectTypeHandler Marshal::s_typeHandlers[64];


#endif
