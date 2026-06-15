// Copyright © 2003 CCP ehf.

/*
	*************************************************************************

	Marshal.h

	Project:   Custom marshallers

	Description:

		Speed up marshalling of selected python objects


	Dependencies:

		Python

	*************************************************************************
*/

#ifndef _MARSHAL_H_
#define _MARSHAL_H_

#include "PyTemplates.h"
#include <set>
#include <map>
#include <vector>


bool MarshalInit(PyObject *module);

struct ReadStream;

//--------------------------------------------------------------------
// Python builtin types
//
// Fundamental types:
//		<type 'NoneType'>
//		<type 'type'>
//
// Numerical types:
//		<type 'int'>
//		<type 'long'>
//		<type 'float'>
//		<type 'complex'>
//
// Sequence objects:
//		<type 'str'>
//		<type 'unicode'>
//		<type 'buffer'>
//		<type 'tuple'>
//		<type 'list'>
//
// Mapping objects:
//		<type 'dict'>
//
// Other objects:
//		<type 'instance'>
//		<type 'class'> ? need to support this, perhaps not
enum PYTYPES
{
    TY_INVALID = 0,
    TY_SIGNATURE = 126,	// marks beginning of marshal format
    TY_SIGNATURE2 = 125,  // new marker, followed by a version char
    
    TY_NONE = 1,
    TY_GLOBAL = 2,	//A global object by name
    
    TY_INT64 = 3,
    TY_INT32 = 4,
    TY_INT16 = 5,
    TY_INT8 = 6,
    TY_INT_N1 = 7,
    TY_INT_0 = 8,
    TY_INT_1 = 9,
    
    TY_FLOAT = 10,
    TY_FLOAT_0 = 11,
    
    TY_COMPLEX = 12,
    
    TY_STR = 13,
    TY_STR_EMPTY = 14,
    TY_STR_CHAR = 15,
    TY_STR_SHORT = 16,
    TY_STR_TABLE = 17,
    TY_UNICODE = 18,
    
    TY_BUFFER = 19,
    TY_TUPLE = 20,
    TY_LIST = 21,
    
    TY_DICT = 22,
    
    TY_INSTANCE = 23,
    // TY_BLUEWRAPPER		= 24, // Deprecated
    TY_CALLBACK = 25,	// callback method specific
    
    TY_PICKLE = 26,	// fallback.  Deprecated, backwards compatibility only.
    
    // reference pointers
    TY_REFERENCE = 27,
    
    // packet format and flags
    TY_CRC_CHECK = 28,
    //TY_COMPRESSED  		= 29,   //unused
    
    TY_TRUE = 31,	//these two are so common, they deserve their own thing
    TY_FALSE = 32,
    
    TY_PICKLER = 33,	//new style pickle, use a single pickler for the whole stream
    TY_REDUCE = 34,	//__reduce__ protocol
    TY_NEWOBJ = 35,	//the newobj special case of __reduce__ protocol 2
    
    TY_TUPLE0 = 36,	//an empty tuple
    TY_TUPLE1 = 37,	//a tuple of one item
    TY_LIST0 = 38,	//an empty list
    TY_LIST1 = 39,	//a list of one item
    
    TY_UNICODE_0 = 40,	//empty unicode string
    TY_UNICODE_1 = 41,	//single unicode char
    
    TY_DBROW = 42,	//a custom marshaled DBROW
    TY_WSTREAM = 43,	//the marshallers own WriteStream
    
    TY_TUPLE2 = 44,	//a two-tuple, surprisingly common
    TY_MARK = 45,	//a marker for dynamic lists and other dynamic forms

    TY_UTF8_OBSOLETE = 46,	//unicode as utf-8 -- obsolete and unused as of Python 3 upgrade
    
    TY_LONG = 47,	//a proper long
    
    // flags and masks
    TY_SHAREDFLAG = 0x40, //64.  don't go above that
    TY_TYPEMASK = 0x3F	// type id's are from 1 to 63
};


class Marshal :
	public PyXObject<Marshal>
{
public:
	/////////////////////////////////////////
	// data members
	BluePy mPickleDumps, mPickleLoads, mStrTable, mStrTableRev, mGuidTable;
	
	// Stock objects
	BluePy mStock_IntN1, mStock_Int0, mStock_Int1, mStock_Float0;
	BluePyStr mStock_EmptyStr;
	BluePyStr mStock_ModuleString;	// "__module__"
	BluePyStr mStock_GuidString;	// "__guid__"
	BluePyStr mStock_GetState;		// "__getstate__"
	BluePyStr mStock_SetState;		// "__setstate__"
	BluePyStr mStock_Dict;			// "__dict__"
	BluePyStr mStock_New;			// "__new__"
	BluePyStr mStock_Name;			// "__name__"
	BluePyStr mStock_Append;		// "append"
	BluePyStr mStock_SetItem;		// "__setitem__"

	// Tasklet timing context strings
	BluePy mTimer_Load;			// "Marshal::Load"
	BluePy mTimer_Save;			// "Marshal::Save"
	BluePy mTimer_GetState;		// "Marshal::GetState"
	BluePy mTimer_SetState;		// "Marshal::SetState"
	BluePy mTimer_SaveCallback;	// "Marshal::SaveCallback"
	BluePy mTimer_LoadCallback;	// "Marshal::LoadCallback"

	// Exception classes
	BluePy mUnmarshalExc;

	/////////////////////////////////////////
	// Data statistic

	// String statistics, optional dict, key=string, value=num. occurrences
	BluePy mStatStrings;

	// Global class instancing statistics.  optional dict, key = global, value = num occurrance.
	BluePy mStatGlobals;

	std::vector<unsigned int> m_typesSaved;
	std::vector<unsigned int> m_typesLoaded;

	// Last packet checksumed or not
	int mPacketHadCrc;

	// Skip crc check
	int mSkipCrcCheck;

	//stats for packet overhead
	size_t mMapSent;
	size_t mTotalSent;

	//Stuff for blacklisting UNPICKLE objects
	std::set<PyObject *> mGlobalsBlacklist;
	bool mGlobalsBlacklistInit;

	BluePy mGlobalsWhitelist;  //a whitelist, consulted before the blacklist
	int mCollectWhitelist;
	
public:
	static Marshal *New();
	~Marshal();
private:
	Marshal();
	bool Init();
	static Marshal *sMarshal; // for the singleton constructor.

	/////////////////////////////////////////
	// Public member functions
public:

	// Python blurb
	PYTHON_CLASS("Marshal");
	PYTHON_METHODS_BEGIN()
		METHOD_KEYWORDS(Save, "Save object")
		METHOD_KEYWORDS(Load, "Load object")
		METHOD_VARARGS(find_global, "Unpickler hook")
		METHOD_VARARGS(Flush, "")
		METHOD_NOARGS(GetNWriteStreams, "")
		METHOD_NOARGS(GetWriteStreamMem, "")
		METHOD_NOARGS(ResetTypeStats, "Resets the statistics on types saved/loaded")
		METHOD_NOARGS(GetTypeStats, "")
	PYTHON_METHODS_END()

	// published members
	PYTHON_MEMBERS_BEGIN()
		PYTHON_MEMBER("stringTable", T_OBJECT, mStrTable.o, READONLY)
		PYTHON_MEMBER("stringTableRev", T_OBJECT, mStrTableRev.o, READONLY)
		PYTHON_MEMBER("packetHadCrc", T_INT, mPacketHadCrc, READONLY)
		PYTHON_MEMBER("statStrings", T_OBJECT, mStatStrings.o, 0)
		PYTHON_MEMBER("statGlobals", T_OBJECT, mStatGlobals.o, 0) //classes instantiated from globals		
		PYTHON_MEMBER("globalsWhitelist", T_OBJECT, mGlobalsWhitelist.o, 0) //whitelist for globals
		PYTHON_MEMBER("collectWhitelist", T_INT, mCollectWhitelist, 0) //collect data for whitelist
	PYTHON_MEMBERS_END()

	PYTHON_GETSET_BEGIN()
		PYTHON_GET(overhead, "")
	PYTHON_GETSET_END()

	PyObject* Load(PyObject* args, PyObject *kw);
	
	PyObject* SaveObject(PyObject* o, PyObject* cb, int useChecksum, int version=1, PyObject *stringMap=0); //blue internal use	
	bool HookUnpickler(PyObject *up);

	//a recursion limit incrementor
	class Incrementor
	{
	public:
		Incrementor(int &_i) : mI(_i) {mI++;}
		~Incrementor() {mI--;}
		bool Test(int limit);
	private:
		int &mI;
	};
	int mRecursionLevel;
	const static int sRecursionLimit = 1000;

	typedef PyObject *(Marshal::*reader_t)(struct ReadStream *s);
	PyObject *ReadWrapper(struct ReadStream &stream, reader_t func);
	PyObject *ReadHeader(struct ReadStream *s);
	PyObject *ReadObject(struct ReadStream *s);  //reads any object

private:
	PyObject* Save(PyObject* args, PyObject* kw);
	PyObject *find_global(PyObject *args);
	PyObject *Flush(PyObject *args); //flush global name cache
	bool CheckGlobalsBlacklist(PyObject *o, const char *mod, const char *name);
	void InitGlobalsBlacklist();

	PyObject *Get_overhead();

	bool WriteObject(class WriteStream* stream, PyObject* o);
	bool WriteType( class WriteStream* stream, enum PYTYPES type );
	
	bool UpdateGlobalNames(PyObject *module, PyObject*name); //update globalname stats

	//unpickling support routines
	PyObject *ReadObjectOrMarker(struct ReadStream *s, bool &mark); //reads an object or a marker
	PyObject *ReadObjectBuffer(struct ReadStream &s);
	PyObject *ReadObjectGlobal( struct ReadStream *s, bool shared );
#ifdef PY27_COMPATIBILITY_MODE
	PyObject *ReadObjectInstance(struct ReadStream *s, bool shared);
#endif
	PyObject *ReadObjectReduce(struct ReadStream *s, bool shared);
	PyObject *ReadObjectNewobj(struct ReadStream *s, bool shared);
	PyObject *ReadObjectLong(struct ReadStream *s, bool shared);

	PyObject *GetGlobalObject(PyObject *name);
	bool ReadObjectListIter(struct ReadStream &s, PyObject *target);
	bool ReadObjectDictIter(struct ReadStream &s, PyObject *target);
	
	//Pickling support routines
	bool WriteObjectGlobal(WriteStream *s, PyObject *obj);
	bool WriteCallbackResult( WriteStream* stream, PyObject* o );
	bool WriteObjectInstance(WriteStream *s, PyObject *obj);
	bool WriteObjectReduce(bool &handled, WriteStream *s, PyObject *obj);
	bool WriteMarker(WriteStream *s);
	bool WriteListIter(WriteStream *s, PyObject *i);
	bool WriteDictIter(WriteStream *s, PyObject *i);
	bool WriteLong(WriteStream *s, PyObject *i);

	//other classes that we need
	friend class DBRow;

	PyObject *GetNWriteStreams();
	PyObject *GetWriteStreamMem();
	PyObject* ResetTypeStats();
	PyObject* GetTypeStats();

	typedef PyObject * (Marshal::*ReadObjectTypeHandler)( ReadStream * stream, bool isShared );
	static ReadObjectTypeHandler s_typeHandlers[64];
	friend bool MarshalInit( PyObject *module );

	PyObject * ReadObjectIntOne( ReadStream * stream, bool isShared );
	PyObject * ReadObjectIntZero( ReadStream * stream, bool isShared );
	PyObject * ReadObjectIntNegativeOne( ReadStream * stream, bool isShared );
	PyObject * ReadObjectInt8( ReadStream * stream, bool isShared );
	PyObject * ReadObjectInt16( ReadStream * stream, bool isShared );
	PyObject * ReadObjectInt32( ReadStream * stream, bool isShared );
	PyObject * ReadObjectInt64( ReadStream * stream, bool isShared );
	PyObject * ReadObjectNone( ReadStream* stream, bool isShared );
	PyObject * ReadObjectTuple0( ReadStream * stream, bool isShared );
	PyObject * ReadObjectUnicode1( ReadStream * stream, bool isShared );
	PyObject * ReadObjectUnicode0( ReadStream * stream, bool isShared );
	PyObject * ReadObjectUnicode( ReadStream * stream, bool isShared );
	PyObject * ReadObjectStrTable( ReadStream * stream, bool isShared );
	PyObject * ReadObjectStrShort( ReadStream * stream, bool isShared );
	PyObject * ReadObjectStrChar( ReadStream * stream, bool isShared );
	PyObject * ReadObjectStrEmpty( ReadStream * stream, bool isShared );
	PyObject * ReadObjectFalse( ReadStream * stream, bool isShared );
	PyObject * ReadObjectTrue( ReadStream * stream, bool isShared );
	PyObject * ReadObjectFloatZero( ReadStream * stream, bool isShared );
	PyObject * ReadObjectFloat( ReadStream * stream, bool isShared );
	PyObject * ReadObjectWStream( ReadStream * stream, bool isShared );
	PyObject * ReadObjectDBRow( ReadStream * stream, bool isShared );
	PyObject * ReadObjectCrcCheck( ReadStream * stream, bool isShared );
	PyObject * ReadObjectReference( ReadStream * stream, bool isShared );
	PyObject * ReadObjectPickler( ReadStream * stream, bool isShared );
	PyObject * ReadObjectPickle( ReadStream * stream, bool isShared );
	PyObject * ReadObjectCallback( ReadStream * stream, bool isShared );
	PyObject * ReadObjectDict( ReadStream * stream, bool isShared );
	PyObject * ReadObjectList( ReadStream * stream, bool isShared );
	PyObject * ReadObjectList1( ReadStream * stream, bool isShared );
	PyObject * ReadObjectList0( ReadStream * stream, bool isShared );
	PyObject * ReadObjectTuple( ReadStream * stream, bool isShared );
	PyObject * ReadObjectTuple2( ReadStream * stream, bool isShared );
	PyObject * ReadObjectTuple1( ReadStream * stream, bool isShared );
	PyObject * ReadObjectBuffer( ReadStream * stream, bool isShared );
};


//--------------------------------------------------------------------
// WriteStream utility
//--------------------------------------------------------------------
enum PYTYPES;

class WriteStream :
	public PyXObject2<WriteStream>
{
public:
	WriteStream();
	~WriteStream();
	
	//Object constructor - destructor
	static PyObject *_New(PyTypeObject *type, PyObject *args, PyObject *kwds);
	bool Init(PyObject *callback, bool useChecksum, int version=0); //init when creating
	bool Init(const void *data, size_t datalen); // init when unpickling
	bool Finalize(size_t &totalLen, size_t &mapLen); //when one has done stuffing into it
	
	PYTHON_CLASS("blue.MarshalStream")  //a slightly more indicative name
	PYTHON_METHODS_BEGIN()
		METHOD_O(write, "python write")
		METHOD_NOARGS(Str, "convert to string")
		METHOD_NOARGS(Len, "get length")
		METHOD_O(__reduce_ex__, "Pickling support")
	PYTHON_METHODS_END()
	static bool InitType(PyTypeObject *type);
	
	//Marshaling support methods
	bool ObjectReferenced(bool &referred, PyObject* o);
	template <class T>
	bool Write(const T value)
	{
		if (Prepare(sizeof (T)))
		{
			*reinterpret_cast<T*>( mBuff + mPos ) = value;
			mPos += sizeof (T);
			return true;
		} else
			return false;
	}

	inline bool WriteType(PYTYPES type);
    bool WriteBuff(const void* buff, size_t size);
	inline bool WriteBuffWoSize(const void* buff, size_t size);
	inline bool WriteInteger(int i);
	
	//these are used to memorize locations and rewind
	size_t GetPos() const {return mPos;}
	void SetPos(size_t p) {mPos = p;}

	PyObject *GetPickler();

	//marshaling support (to marshal a stream object)
	bool Write(Marshal &m, WriteStream &stream);
	static PyObject *Read(Marshal &m, ReadStream &stream);

	static int	sNStreams;
	static size_t sMemUse;

	inline const char* GetBuffer( size_t* len ) const;
		
private:
	//python methyod. stuff
	PyObject *write(PyObject *s);
	PyObject *Str();
	PyObject *Len();

	//str() method
	static PyObject *tp_str_method(PyObject *self);
	static PyObject *tp_repr_method(PyObject *self);
	static PyObject *tp_richcompare_method(PyObject *self, PyObject* other, int op);
	
	//buffer protocol.
	static int getbuffer(PyObject* exporter, Py_buffer* view, int flags);
//	static Py_ssize_t getreadbuffer(PyObject *self, Py_ssize_t segment, void **ptrptr);
//	static Py_ssize_t getsegcount(PyObject *self, Py_ssize_t *lenp);
	
	//sequence protocol
	static Py_ssize_t SequenceLength(PyObject *self);
	static PyObject *SequenceGet(PyObject *self, Py_ssize_t i);
	static PyObject *SequenceSubscript(PyObject *self, PyObject* key);
	static PyObject *SequenceGetSlice(PyObject *self, Py_ssize_t ilow, Py_ssize_t ihigh);

	//python picling support
	PyObject *__reduce_ex__(PyObject *proto);

	
private:
	//data handling
	inline bool Prepare(size_t size);
	
	char* mBuff;	//The main data buffer
	size_t mPos;
	size_t mSize;
	int mVersion;
	bool mUseChecksum;
	bool mFinalized; //when true, the stream is ready as a read buffer.
	//remember stream positions
	size_t mChecksumPos;

	// references
	struct Refs
	{
		Refs(size_t s) : id(0), streampos(s) {}
		size_t id;		   //index of shared object, stating at 1
		size_t streampos; //position of first (only) instance of object
	};
	typedef std::map<BluePy, Refs> refMap_t;
	refMap_t mRefMap;
	typedef std::vector<std::pair<size_t, size_t> > refHits_t;
	refHits_t mRefHits;
	PYTYPES mTypeFlags;
	
	BluePy mPickler; //for pickling

public:
	BluePy mCallback;
	std::vector<char> mScratch; //for scratch space
};


//--------------------------------------------------------------------
// ReadStream utility
//--------------------------------------------------------------------
struct ReadStream :
	public PyXObject2<ReadStream>
{
	PYTHON_CLASS("WriteStream")
	PYTHON_METHODS_BEGIN()
		METHOD_O(read, "python read")
		METHOD_NOARGS(readline, "python readline")
	PYTHON_METHODS_END()
	static bool InitType(PyTypeObject *type);
	static void __Dealloc(PyObject *self) {}
	ReadStream();

	ReadStream(const void* buff, size_t bufflen, PyObject* callback);
	~ReadStream();
	
	ssize_t Remaining() {return mSSize-mPos;}
	
	void Rewind()
	{
		mPos = 0;
	}
	void Rewind(size_t s)
	{
		mPos -= s;
	}

    bool CheckSpace(int n, size_t esize);
	inline bool CheckSpace(int nbytes) { return CheckSpace(nbytes, 1);}
	inline bool CheckSpace(size_t esize) {return CheckSpace(1, esize);}

	template<class T>
	bool CheckSpace() {
		return CheckSpace(sizeof(T));
	}

	template<class T>
	bool Read(T &r) {
		if (!CheckSpace<T>())
			return false;
		r = *reinterpret_cast<const T*>(mBuff+mPos);
		mPos += sizeof(T);
		return true;
	}

	template<class T>
	bool GetBuffer(const T* &r, int n)
	{
		if (!CheckSpace(n, sizeof(T)))
			return false;
		r = reinterpret_cast<const T*>(mBuff+mPos);
		mPos += n*sizeof(T);
		return true;
	}

	inline bool ReadType(PYTYPES &t);
	inline bool PeekType(PYTYPES &t);
    bool ReadInteger(int &r);
	//corresponds to WriteStream::WriteBuffWoSize
	inline bool GetBuffWoSize(const char * &b, int s) {return GetBuffer(b, s);}
	inline bool ReadBuffWoSize(char *b, int s) {
		const char *source;
		if (!GetBuffWoSize(source, s)) return false;
		return memcpy(b, source, s), true;
	}
	//corresponds to WriteStream::WriteBuff
	inline bool GetBuff(const char* &b, int &s) {return ReadInteger(s) && GetBuffer(b, s);}
	
	inline size_t MarkShared();
	inline bool MarkShared(PyObject *o);
	inline size_t MarkShared_Int(PyObject *o);
	inline bool UpdateShared(size_t ix, PyObject *o);
	inline bool Crc() const;

	//Python file methods
	PyObject *read(PyObject *len);
	PyObject *readline();
	
	//Get unpicklers associated with this stream
	PyObject *GetUnpicklerInt();
	PyObject *GetUnpickler(Marshal *m);
	PyObject *GetOSUnpickler(Marshal *m);


	void SetVersion(int v) {mVersion = v;}
	int GetVersion() const {return mVersion;}

	size_t GetPos() const {return mPos;}

public:
	BluePy mCallback;

	const char* mBuff;
	const size_t mSize;
	size_t mSSize;
	size_t mPos;
		
	std::vector<BluePy> mShared;	//must keep refs to shared objects, since some of them may be temporary
	int* mMapping;
	int mNumShared; //counter when filling shared and mapping
	int mMapCount;	//number of objects in mShared and mMapping

	size_t mCrcPos; //the position right after the crc value
	int mCrc;
	bool mGotCRC;

private:
	BluePy mUnpickler;
	BluePy mOSUnpickler;
	int mVersion;  //version of the pickle
};

//------------------------------------------------------------------------------
const char* WriteStream::GetBuffer( size_t* len ) const
{
	if ( !mFinalized )
	{
		return 0;
	}

	if (len)
	{
		*len = mPos;
	}

	return mBuff;
}

#endif
