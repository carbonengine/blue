// Copyright © 2005 CCP ehf.

/******************************************************************************
 * PyRowset.h
 * Header for the PyRowset.cpp file, which defines python implemented rowsets.
 *****************************************************************************/

#ifndef PYROWSET_H
#define PYROWSET_H

#include "PyTemplates.h"
#include "Marshal.h"

bool DBRowsetInit(PyObject *module);

struct DBRowDescriptor;
struct ColumnDescriptor;


//We need the DBRow declared here, so that Marshal can access it for our custom
//marshalling.  DBRowDescriptor et al are defined in the PyRowset.cpp

//////////////////////////////////////////////////////////////////////////////
// DBRow
//
class DBRow :
	public PyXObject2<DBRow>
{
public:
	DBRow() : mRD(0) {}
	~DBRow();

	//initialization 
	bool Init(const DBRow *row);
	bool Init(DBRowDescriptor *rd, PyObject *data, PyObject *reuse);
	bool InitSeq(PyObject *t, PyObject *reuse);
	bool InitDB(PyObject *t, PyObject *reuse);
	static bool Reuse(PyObject *reuse, PyObject* &r);
	
	
	PYTHON_CLASS("blue.DBRow");
	PYTHON_METHODS_BEGIN()
		METHOD_O(__reduce_ex__, "Pickling support")
		METHOD_NOARGS(__getstate__, "Pickling support") 
		METHOD_O(__setstate__, "Pickling support") 
	PYTHON_METHODS_END()

	PYTHON_GETSET_BEGIN()
		PYTHON_GET(__keys__, "get key names")
		PYTHON_GET(__columns__, "get key names") //alias for __keys__
		PYTHON_GETSET(__data__, "raw data")
		PYTHON_GETSET(__cdata__, "raw data, compressed")
		PYTHON_GETSET(__header__, "The row descriptor")
	PYTHON_GETSET_END()

	PYTHON_MEMBERS_BEGIN()
	PYTHON_MEMBERS_END()

	static bool InitType(PyTypeObject *tp);
	
	//Object constructor - destructor
	static PyObject *_New(PyTypeObject *type, PyObject *args, PyObject *kwds);
	static PyObject * CreateFromRowDescriptor( PyObject *rowDescriptor );

	//Comparison
	static PyObject* Compare(PyObject *a, PyObject *b, int op);
	static Py_hash_t Hash(PyObject *obj);

	//Reduce to a tuple
	PyObject *__reduce_ex__(PyObject *protocol);
	PyObject *__getstate__();
	PyObject *__setstate__(PyObject *state);

	//get keys
	PyObject *Get___keys__();
	PyObject *Get___columns__();

	//getting the header
	PyObject *Get___header__();
	bool Set___header__(PyObject *h);

	//direct data access
	PyObject *Get___data__();
	bool Set___data__(PyObject *v);
	PyObject *Get___cdata__();
	bool Set___cdata__(PyObject *v);

	//attribute access override
	PyObject *GetAttr(PyObject *key);
	static int SetAttr(PyObject *self, PyObject *key, PyObject *val);
	
	//Repr
	PyObject *Repr(PyObject *args);

	//Garbage collection
	static int GCTraverse(DBRow *self, visitproc visit, void *arg);
	static int GCClear(DBRow *self);

	//Sequence protocol
	static Py_ssize_t SequenceLength(DBRow *row);
	static PyObject *SequenceGet(DBRow *row, Py_ssize_t i);
	static int SequenceSet(DBRow *row, Py_ssize_t i, PyObject *val);
	static PyObject *SequenceGetSlice(DBRow *row, Py_ssize_t ilow, Py_ssize_t ihigh);
	static int SequenceSetSlice(DBRow *row, Py_ssize_t ilow, Py_ssize_t ihigh, PyObject *v);

	//mapping protocol
	static PyObject *MappingSubscript(DBRow *row, PyObject *key);
	static int MappingAssSubscript(DBRow *row, PyObject *key, PyObject *val);

	//Accessing the columns
	const ColumnDescriptor *GetCD(int &idx, PyObject *key, PyObject *exc);
	PyObject *Get(const ColumnDescriptor &c, Py_ssize_t idx) const;
	bool Set(const ColumnDescriptor &c, Py_ssize_t idx, PyObject *o);
	bool SetNotNull(const ColumnDescriptor &c, PyObject *o);

	//Getting packed data
	void PackData(std::vector<char> &packed);
	bool UnpackData(const char *data, Py_ssize_t datalen);

	//Custom blue marshaling
	bool Write(Marshal &m, WriteStream &s);
	static PyObject *Read(Marshal &m, ReadStream &s);

	//inlines to access data
	inline void *Data(Py_ssize_t offset) const {
		return (void*)((char*)mData + offset);
	};
	inline PyObject* &Object(Py_ssize_t idx) const;
	inline char *BitPtr(int &bit, Py_ssize_t bitoffset) const;
    void SetBit(Py_ssize_t bitoffset, bool val=true);
	inline void ClrBit(Py_ssize_t bitoffset) {SetBit(bitoffset, false);}
    bool GetBit(Py_ssize_t bitoffset) const;

	//C API
	//getting and setting integral values from C.  Doesn't support strings or python objects.
	//return false on failure, with no exception set.
public:
	size_t GetColumnCount() const;
	//we don't want to expose the DB types here, but the enums correspond to those.
	enum datatypes {
		SI1=16,
		UI1=17,
		SI2=2,
		UI2=18,
		SI4=3,
		UI4=19,
		SI8=20,
		UI8=21,
		R4=4,
		R8=5,
		CY=6,
		BOOL=11,
		FILETIME=64,
		STR=129,
		WSTR=130,
		BYTES=128
	};

	Py_ssize_t GetDataOffset(size_t i, datatypes &type, Py_ssize_t &nullOffset) const;

	//raw Access to the Null
	inline bool GetNull(Py_ssize_t nullIndex);
	inline void SetNull(Py_ssize_t nullIndex, bool isNull);

	//raw Access to the data
	template<class T>
	inline bool GetValue(datatypes type, Py_ssize_t dataOffset, T &v);

	
	//combined access to the data
	template<class T>
	bool GetValue(size_t idx, T &val, bool &isNull) const;

public:
	DBRowDescriptor *mRD;
	int64_t mData[1];
};

//Template functions defined

inline bool DBRow::GetNull(Py_ssize_t nullIndex)
{
	return GetBit(nullIndex);
}
inline void DBRow::SetNull(Py_ssize_t nullIndex, bool isNull)
{
	SetBit(nullIndex, isNull);
}

template<class T>
inline bool DBRow::GetValue(datatypes type, Py_ssize_t dataOffset, T &val)
{
	#define GETIT(TP) val = T(*(TP*)Data(dataOffset)); break;
	switch (type) {
	case SI1:	GETIT(int8_t);
	case UI1:	GETIT(uint8_t);
	case SI2:	GETIT(int16_t);
	case UI2:	GETIT(uint16_t);
	case SI4:	GETIT(int32_t);
	case UI4:	GETIT(uint32_t);
	case SI8:	GETIT(int64_t);
	case FILETIME:
	case UI8:	GETIT(uint64_t);
	case R4:	GETIT(float);
	case R8:	GETIT(double);
#undef GETIT
	case CY:
		val = T(double(*(int64_t*)mData) / 10000.0); break;
	case BOOL:
		val = T(GetBit(dataOffset));
	default:
		return false;
	}
	return true;
}


template<class T>
bool DBRow::GetValue(size_t i, T &val, bool &isNull) const
{
	datatypes type;
	Py_ssize_t nullOffset;
	Py_ssize_t dataOffset = GetDataOffset(i, type, nullOffset);
	if (dataOffset<0) return false;
	isNull = GetBit(nullOffset);
	if (isNull)
		return true;
	return GetValue(type, dataOffset, val);
}

#endif //PYROWSET_H
