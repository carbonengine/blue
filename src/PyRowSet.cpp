// Copyright © 2014 CCP ehf.

/*
 * Here we write a custom C implementation of a python rowset.  Essentially a table.
 */

	
#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "PyRowset.h"

#include "PyTemplates.h"

#include <new>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

#ifdef _WIN32
//#define ATLTRACE2 __noop
#include <atldbcli.h>
#else
typedef uint32_t DBTYPE;
enum DBTYPEENUM {
    // The following values exactly match VARENUM
    // in Automation and may be used in VARIANT.
    DBTYPE_EMPTY = 0,
    DBTYPE_NULL = 1,
    DBTYPE_I2 = 2,
    DBTYPE_I4 = 3,
    DBTYPE_R4 = 4,
    DBTYPE_R8 = 5,
    DBTYPE_CY = 6,
    DBTYPE_DATE = 7,
    DBTYPE_BSTR = 8,
    DBTYPE_IDISPATCH = 9,
    DBTYPE_ERROR = 10,
    DBTYPE_BOOL = 11,
    DBTYPE_VARIANT = 12,
    DBTYPE_IUNKNOWN = 13,
    DBTYPE_DECIMAL = 14,
    DBTYPE_UI1 = 17,
    DBTYPE_ARRAY = 0x2000,
    DBTYPE_BYREF = 0x4000,
    DBTYPE_I1 = 16,
    DBTYPE_UI2 = 18,
    DBTYPE_UI4 = 19,
    
    // The following values exactly match VARENUM
    // in Automation but cannot be used in VARIANT.
    DBTYPE_I8 = 20,
    DBTYPE_UI8 = 21,
    DBTYPE_GUID = 72,
    DBTYPE_VECTOR = 0x1000,
    DBTYPE_FILETIME = 64,
    DBTYPE_RESERVED = 0x8000,
    
    // The following values are not in VARENUM in OLE.
    DBTYPE_BYTES = 128,
    DBTYPE_STR = 129,
    DBTYPE_WSTR = 130,
    DBTYPE_NUMERIC = 131,
    DBTYPE_UDT = 132,
    DBTYPE_DBDATE = 133,
    DBTYPE_DBTIME = 134,
    DBTYPE_DBTIMESTAMP = 135,
    DBTYPE_HCHAPTER = 136,
    DBTYPE_PROPVARIANT = 138,
    DBTYPE_VARNUMERIC = 139,
};
#endif

struct StringOrder
{
	bool operator() (const char *a, const char *b) const {
		return strcmp(a, b) < 0;
	}
};

//use this for the hash
struct HashStringOrder
{
	bool operator() (const char *a, const char *b) const {
		return strcmp(a, b) == 0;
	}

	size_t operator() (const char *a) const {
        size_t val = 2166136261;
        while( *a )
        {
            val = 16777619 * val ^ (size_t)*a++;
        }
        return val;
	}
};


///////////////////////////////////
// Column descriptor
// Note, this structure is replicated more or less in DB.dll
struct ColumnDescriptor
{
	ColumnDescriptor(const char *name) : mName(name), mType(0), mOffset(0), mSize(0){}
	static bool TypeSize(const DBTYPE &type, int &size)
	{
		switch(type & 0xff) {
		case DBTYPE_BOOL:
			size = 0; break;
		case DBTYPE_I1:
		case DBTYPE_UI1:
			size = 1; break;
		case DBTYPE_I2:
		case DBTYPE_UI2:
			size = 2; break;
		case DBTYPE_I4:
		case DBTYPE_UI4:
		case DBTYPE_R4:
			size = 3; break;
		case DBTYPE_I8:
		case DBTYPE_UI8:
		case DBTYPE_R8:
		case DBTYPE_CY:
		case DBTYPE_FILETIME:
			size = 4; break;
		case DBTYPE_DBTIMESTAMP:
			size = 4; break;
		case DBTYPE_STR:
		case DBTYPE_WSTR:
		case DBTYPE_BYTES:
			size = 5; break;  //signal an object
		case DBTYPE_EMPTY:
			size = -1; break;
		default:
			PyErr_Format(PyExc_TypeError, "DBRowDescriptor doesn't support data type %d", type);
			return false;
		}
		return true;
	}
	bool IsObject() const {return mType == DBTYPE_STR ||  mType == DBTYPE_WSTR || mType == DBTYPE_BYTES;}
	bool IsVirtual() const {return mType == DBTYPE_EMPTY;}
	bool IsBool() const {return mType == DBTYPE_BOOL;}

	std::string mName;
	int mOffset;
	DBTYPE mType;
	unsigned char mSize;
};

bool operator == (const ColumnDescriptor &a, const ColumnDescriptor &b)
{
	return a.mName == b.mName && a.mOffset == b.mOffset && a.mType == b.mType && a.mSize == b.mSize;
}

//A row descriptor object that isn't python.  We can't always create python objects
class RowDescriptor
{
public:
	RowDescriptor() {
		mSNull = mTotalLen = mDataLen = mNObjects = mSObjects = mNRealCols = 0 ;
	}
	typedef std::vector<ColumnDescriptor> columnList_t;
	typedef std::unordered_map<const char *, std::pair<const ColumnDescriptor *, int>, HashStringOrder, HashStringOrder> columnMap_t;
	
	bool Init(PyObject *a);
	bool InitFromTypedList();
	bool InitDB(const columnList_t *l);
	
	//members.  First a list of descriptors
	columnList_t mColumnList;
	columnMap_t mColumnMap;
	std::vector<DBTYPE> mObjectTypes;

	int mSNull; //start of null flags (in bits)
	int mDataLen; //length of integer data only, and flags (in bytes)
	int mSObjects;  //offset into the object array. (index in pointer sizes)
	int mNObjects; //number of objects trailing data.
	int mTotalLen; //total length of data in bytes (including object pointers)
	int mNRealCols; //number of non-virtual objects
};

bool operator == (const RowDescriptor &a, const RowDescriptor &b)
{
	return a.mColumnList == b.mColumnList;
}

//////////////////////////////////////////////////////////////////////////////
// PyRowDescriptor
//
struct DBRowDescriptor :
	public PyXObject2<DBRowDescriptor>,
	public RowDescriptor
{
	PYTHON_CLASS("blue.DBRowDescriptor");
	PYTHON_METHODS_BEGIN()
		METHOD_O(__reduce_ex__, "Pickling support")
		METHOD_NOARGS(__getstate__, "Pickling support")
		METHOD_O(__setstate__, "Pickling support")
		METHOD_NOARGS(Keys, "keys")
		METHOD_O(Index, "index of a given key")
		METHOD_O(CompareDB, "DB internal use")
		METHOD_NOARGS(Size, "return size of data, and number of extra columns")
	PYTHON_METHODS_END()

	PYTHON_GETSET_BEGIN()
		PYTHON_GETSET(virtual, "extra virtual columns at the end")
	PYTHON_GETSET_END()

	static bool InitType(PyTypeObject *tp)
	{
		static PySequenceMethods sequenceMethods = {
			(lenfunc)SequenceLength,
			0,
			0,
			(ssizeargfunc)SequenceGet
		};
		tp->tp_flags |= Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC;
		tp->tp_traverse = (traverseproc)GCTraverse;
		tp->tp_clear = (inquiry)GCClear;
		tp->tp_repr = (reprfunc)(PyCFunction)PyCFuncArgs<&DBRowDescriptor::Repr>;
		tp->tp_as_sequence = &sequenceMethods;
		tp->tp_richcompare = Compare;
		tp->tp_hash = Hash;
		return true;
	}

	static PyObject* Compare(PyObject *a, PyObject *b, int op);
	static Py_hash_t Hash(PyObject *obj);

	static PyObject *_New(PyTypeObject*type, PyObject *args, PyObject *kw);
	PyObject *Repr(PyObject *args);
	PyObject *__reduce_ex__(PyObject *proto);
	PyObject *__getstate__();
	PyObject *__setstate__(PyObject *state);
	PyObject *Keys();
	PyObject *Index(PyObject *key);

	//sequence protocol
	static Py_ssize_t SequenceLength(DBRowDescriptor *row);
	static PyObject *SequenceGet(DBRowDescriptor *row, Py_ssize_t i);

	PyObject *Get_virtual();
	bool Set_virtual(PyObject *v);

	//setting and getting virtual properties
	PyObject *VirtualGet(int n, const PyObject *row) const;
	bool VirtualSet(int n, PyObject *row, PyObject *val);

	//Garbage collection
	static int GCTraverse(DBRowDescriptor *self, visitproc visit, void *arg);
	static int GCClear(DBRowDescriptor *self);

	//to compare with stuff from DB
	PyObject *CompareDB(PyObject *c); 
	PyObject *Size();

//private:
	BluePy mVirtualGetSet;
};
	
	
////////////////////////
// RowDescriptor methods
bool RowDescriptor::Init(PyObject *a)
{
	if (PyCapsule_CheckExact(a)) {
		//init from DB
		const columnList_t *src = reinterpret_cast<const columnList_t*>(PyCapsule_GetPointer(a, "DBRowDescriptor"));
		if (!src) {
			if (PyErr_Occurred())
				return false;
			return PyErr_SetString(PyExc_RuntimeError, "unexpected null pointer"), false;
		}
		return InitDB(src);
	}

	const char typeError[] = "expected a tuple of tuples of name and type";
	if (!PyTuple_Check(a))
		return PyErr_SetString(PyExc_TypeError, typeError), false;
	
	//first pass.  Build column list, count sizes;
	mColumnList.clear();
	for(int i = 0; i<PyTuple_GET_SIZE(a); i++) {
		PyObject *t = PyTuple_GET_ITEM(a, i);
		const char *name;
#ifdef PY27_COMPATIBILITY_MODE
		Py_ssize_t size;
		int type;
		PyObject *dummy = 0;
		if (!PyArg_ParseTuple(t, "s#i|O:DBRowDescriptor", &name, &size, &type, &dummy))
			return false;
#else
		int type;
		PyObject *dummy = 0;
		if (!PyArg_ParseTuple(t, "si|O:DBRowDescriptor", &name, &type, &dummy))
			return false;
#endif
		mColumnList.push_back(ColumnDescriptor(name));
		mColumnList.back().mType = (DBTYPE)type;
	}
	return InitFromTypedList();	
}

bool RowDescriptor::InitFromTypedList()
{
	//first pass.  Build column list, count sizes;
	int sizes[5] = {0};
	mNObjects = 0;
	mNRealCols = 0;
	mObjectTypes.clear();
	size_t i;
	for(i = 0; i<mColumnList.size(); i++) {
		ColumnDescriptor &cd = mColumnList[i];

		const auto& name = cd.mName;

		// It is possible to do all sorts of strange things by allowing columns that have the same
		// name as Python's magic double-underscore attributes. For example, it is possible to use
		// `__class__` as a column name in combination with one of the `PyObject*` storing column
		// types to make Python think that a `blue.DBRow` is actually some other class.
		// Therefore, columns must not have a name that is also a valid Python magic attribute.
		// However, since that list can change over time, it is not feasible to do an exhaustive
		// check on whether a column name conflicts with a Python magic attribute.
		// All names starting with a double underscore are thus disallowed.
		if ( name.length() > 1 && name[0] == '_' && name[1] == '_')
		{
			PyErr_SetString(PyExc_ValueError, "Column names cannot begin with a double underscore");
			return false;
		}

		int size;  //logarithm of size plus 1
		if (!ColumnDescriptor::TypeSize(cd.mType, size))
			return false;
		if (cd.IsObject()) {
			//python obj
			mNObjects++;
			mNRealCols++;
			mObjectTypes.push_back(cd.mType);
		} else if (cd.IsVirtual()) {
			; //virtual column
		} else {
			//regular columns and bools
			mNRealCols++;
			sizes[size]++;
		}
		cd.mSize = size;
	}

	//now, compute offsets, starting with the largest size
	int offsets[5];
	int offset = 0; //totaloffset to pyobjects
	for(i=4; i>0; i--) { //regular ints
		offsets[i] = offset;
		offset += sizes[i]*(1<<(i-1));
	}
	//bools
	offset *= 8; //bools and nullflags offsets are in bits
	offsets[0] = offset;
	offset += sizes[0];

	//null flags
	mSNull = offset;
	offset += mNRealCols;

	offset = (offset+7)/8; //back to bytes
	mDataLen = offset; //this is the length of the data.

	//objects
	if (mNObjects) {
		offset = (offset + (sizeof(PyObject*)-1)) & ~(sizeof(PyObject*)-1); //round up in size.
		mSObjects = offset / sizeof(PyObject*); //get offset for the object counter.
		mTotalLen = offset + mNObjects*sizeof(PyObject*);
	} else {
		mSObjects = 0;
		mTotalLen = mDataLen;
	}

	//second pass, compute offsets and modify sizes
	mColumnMap.clear();
	int nVirtuals = 0;
	for(i = 0; i<mColumnList.size(); i++) {
		ColumnDescriptor &cd = mColumnList[i];
		if (cd.IsObject()) {
			//python object
			cd.mOffset = offset;
			cd.mSize = sizeof(PyObject*);
			offset += cd.mSize;
		} else if (cd.IsVirtual()) {
			//virtual object
			cd.mOffset = nVirtuals++;
			cd.mSize = 0;
		} else if (cd.IsBool()) {
			cd.mOffset = offsets[0]++; //in bits
			cd.mSize = -1;
		} else {
			//regular integer fields
			cd.mOffset = offsets[cd.mSize];
			int size = 1<<(cd.mSize-1);
			offsets[cd.mSize] += size;
			cd.mSize = size;
		}
		//Also initialize the map, while we're at it.
		mColumnMap.insert(columnMap_t::value_type(cd.mName.c_str(), std::pair<ColumnDescriptor*, int>(&cd, (int)i)));
	}

	return true;
}
	

bool RowDescriptor::InitDB(const columnList_t *src)
{
	mColumnList.clear();
	mColumnList = *src;
	return InitFromTypedList();
}



///////////////////////////////
//DBRowDescriptor methods
	
//Object constructor - destructor
PyObject *DBRowDescriptor::_New(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
	void *raw = _Alloc(subtype);
	if (!raw)
		return 0;
	DBRowDescriptor *obj = new(raw) DBRowDescriptor;
	PyObject *stuff;
	if (!PyArg_ParseTuple(args, "O", &stuff)) {
		Py_DECREF(obj);
		return 0;
	}
	if (!obj->Init(stuff)) {
		Py_DECREF(obj);
		return 0;
	}
	return obj;
}

PyObject* DBRowDescriptor::Compare(PyObject *a, PyObject *b, int op)
{
	auto lhs = static_cast<RowDescriptor *>(static_cast<DBRowDescriptor *>(a));
	auto rhs = static_cast<RowDescriptor *>(static_cast<DBRowDescriptor *>(b));

	switch(op) {
	case Py_EQ:
		if ( *lhs == *rhs )
			Py_RETURN_TRUE;
		Py_RETURN_FALSE;
	case Py_LT:
		if ( lhs < rhs )
			Py_RETURN_TRUE;
		Py_RETURN_FALSE;
	case Py_GT:
		if ( lhs > rhs )
			Py_RETURN_TRUE;
		Py_RETURN_FALSE;
	default:
		Py_RETURN_NOTIMPLEMENTED;
	}
}

Py_hash_t DBRowDescriptor::Hash(PyObject *obj)
{
	std::hash<PyObject *> hash;
	return static_cast<Py_hash_t>(hash(obj));
}

PyObject *DBRowDescriptor::Repr(PyObject *args)
{
	PyObject *l = PyList_New(mColumnList.size());
	if (!l)
		return 0;
	for(Py_ssize_t i = 0; i<(Py_ssize_t)mColumnList.size(); i++) {
		PyObject *tpl = PyTuple_New(3);
		if (!tpl) {
			Py_DECREF(l);
			return 0;
		}
		PyTuple_SET_ITEM(tpl, 0, PyUnicode_FromString(mColumnList[i].mName.c_str()));
		PyTuple_SET_ITEM(tpl, 1, PyLong_FromLong(mColumnList[i].mType));
		PyTuple_SET_ITEM(tpl, 2, PyLong_FromLong(mColumnList[i].mSize));
		PyList_SET_ITEM(l, i, tpl);
	}
	PyObject *result = PyUnicode_FromFormat("<DBRowDescriptor object %R %d>", l, mTotalLen);
	Py_DECREF(l);
	return result;
}


PyObject *DBRowDescriptor::CompareDB(PyObject *a)
{
	//DB calls us here to see if we are the same as the one that it intents to start building
	if (!PyCapsule_CheckExact(a))
		return PyErr_SetString(PyExc_TypeError, "expected Capsule"), nullptr;

	const columnList_t *src = reinterpret_cast<const columnList_t*>(PyCapsule_GetPointer(a, "DBRowDescriptor"));
	if (!src) {
		if (PyErr_Occurred())
			return 0;
		return PyErr_SetString(PyExc_RuntimeError, "unexpected null pointer"), nullptr;
	}
	if (src->size() != mNRealCols || mColumnList.size()<src->size())
		goto FAIL;
	for(size_t i = 0; i<src->size(); i++)
		if ((*src)[i].mType != mColumnList[i].mType)
			goto FAIL;
	Py_INCREF(Py_True);
	return Py_True;
FAIL:
	Py_INCREF(Py_False);
	return Py_False;
}


PyObject *DBRowDescriptor::Size()
{
	return Py_BuildValue("(ii)", mDataLen, mNObjects);
}

PyObject *DBRowDescriptor::__reduce_ex__(PyObject *proto)
{
	//construct the tuple of arguments
	Py_ssize_t virtualsize = 0;
	if (mVirtualGetSet)
		virtualsize = PyList_GET_SIZE(mVirtualGetSet.o);
	Py_ssize_t realsize = mColumnList.size()-virtualsize;
	PyObject *r = PyTuple_New(realsize);
	if (!r)
		return 0;
	for(int i = 0; i<realsize; i++) {
		const char *name  = mColumnList[i].mName.c_str();
		int type = mColumnList[i].mType;

		PyObject *st = PyTuple_New(2);
		if (!st) {
			Py_DECREF(r); return 0;
		}
		PyObject *o = PyUnicode_FromString(name);
		if (!o) {
			Py_DECREF(st); Py_DECREF(r); return 0;
		}
		PyTuple_SET_ITEM(st, 0, o);
		o = PyLong_FromLong(type);
		if (!o) {
			Py_DECREF(st); Py_DECREF(r); return 0;
		}
		PyTuple_SET_ITEM(st, 1, o);
		PyTuple_SET_ITEM(r, i, st);
	}
	PyObject *result;
	if (mVirtualGetSet)
		result = Py_BuildValue("O(O)O", GetType(), r, mVirtualGetSet.o);
	else
		result = Py_BuildValue("O(O)", GetType(), r);
	Py_DECREF(r);
	return result;
}


PyObject *DBRowDescriptor::__getstate__()
{
	return Get_virtual();
}


PyObject *DBRowDescriptor::__setstate__(PyObject *state)
{
	if (!Set_virtual(state))
		return 0;
	Py_INCREF(Py_None);
	return Py_None;
}


//Garbage collection
int DBRowDescriptor::GCTraverse(DBRowDescriptor *self, visitproc visit, void *arg)
{
	if (self->mVirtualGetSet && visit(self->mVirtualGetSet, arg)<0)
		return -1;
	return 0;
}


int DBRowDescriptor::GCClear(DBRowDescriptor *self)
{
	self->mVirtualGetSet.Release();
	return 0;
}


PyObject *DBRowDescriptor::Keys()
{
	BluePyList t(mColumnList.size());
	if (!t)
		return 0;
	for (size_t i = 0 ; i<mColumnList.size(); i++) {
		BluePyStr k(mColumnList[i].mName.c_str());
		if (!k)
			return 0;
		t.Set(i, k);
	}
	return t.Detach();
}


PyObject *DBRowDescriptor::Index(PyObject *key)
{
	if (!PyUnicode_Check(key))
		return PyErr_SetString(PyExc_TypeError, "string key required"), nullptr;
	const char *c = PyUnicode_AsUTF8(key);
	columnMap_t::iterator i = mColumnMap.find(c);
	if (i==mColumnMap.end())
		return PyErr_Format(PyExc_KeyError, "key %s not found", c), nullptr;
	const ColumnDescriptor *cd = (*i).second.first;

	return PyLong_FromLong((int)(cd - &mColumnList[0]));
}


Py_ssize_t DBRowDescriptor::SequenceLength(DBRowDescriptor *row)
{
	return row->mColumnList.size();
}


//Return a shallow copy of the dude
PyObject *DBRowDescriptor::Get_virtual()
{
	BluePyList newList(0);
	if (!newList)
		return 0;
	if (mVirtualGetSet)
		for(int i = 0; i<PyList_GET_SIZE(mVirtualGetSet.o); i++) {
			if (!newList.Append(PyList_GET_ITEM(mVirtualGetSet.o, i)))
				return 0;
		}
	return newList.Detach();
}


bool DBRowDescriptor::Set_virtual(PyObject *l)
{
	//make sure we have a list of tuples at least three
	if (!PyList_Check(l))
		return PyErr_SetString(PyExc_TypeError, "list required"), false;
		
	//Remove old stuff from the end
	Py_ssize_t oldLen = mVirtualGetSet? PyList_GET_SIZE(mVirtualGetSet.o) : 0;
	Py_ssize_t basicLen = mColumnList.size() - oldLen;
	columnList_t oldlist = mColumnList; //store this to keep
	mColumnList.erase(mColumnList.begin() + basicLen, mColumnList.end());
	
	Py_ssize_t newLen = PyList_GET_SIZE(l);
	BluePyList newList(0);
	if (!newList)
		goto ERR1;
	for (Py_ssize_t i = 0; i<newLen; i++) {
		PyObject *t = PyList_GET_ITEM(l, i);
		if (!PyTuple_Check(t) || (PyTuple_GET_SIZE(t) != 2))
			goto ERR1;
		BluePy name(PyObject_Str(PyTuple_GET_ITEM(t, 0)));
		if (!name)
			goto ERR2;
		ColumnDescriptor cd(PyUnicode_AsUTF8(name.o));
		cd.mType = DBTYPE_EMPTY;
		mColumnList.push_back(cd);
		if (!newList.Append(t))
			goto ERR2;
	}
	if ( ! InitFromTypedList() )
	{
		return false;
	}
	if (newLen)
		mVirtualGetSet = newList;
	return true;
ERR1:
	mColumnList = oldlist;
	if ( ! InitFromTypedList() )
	{
		return false;
	}
	return PyErr_SetString(PyExc_TypeError, "expected list of tuples of size two"), false;
ERR2:
	mColumnList = oldlist;
	if ( ! InitFromTypedList() )
	{
		return false;
	}
	return false;
}


PyObject *DBRowDescriptor::VirtualGet(int n, const PyObject *row) const
{
	if (!mVirtualGetSet)
        return PyErr_SetString(PyExc_RuntimeError, "Internal error in VirtualGet; no mVirtualGetSet."), nullptr;
    if (!PyList_Check(mVirtualGetSet.o))
        return PyErr_SetString(PyExc_RuntimeError, "Internal error in VirtualGet;  mVirtualGetSet.o isn't a list."), nullptr;
	
	if (n < 0 || n >= PyList_GET_SIZE(mVirtualGetSet.o))
		return PyErr_SetString(PyExc_RuntimeError, "Internal error in VirtualGet; GetSet list has funky size."), nullptr;

	PyObject *t = PyList_GET_ITEM(mVirtualGetSet.o, n);
	if (!PyTuple_Check(t) || PyTuple_GET_SIZE(t) < 2) 
		return PyErr_SetString(PyExc_RuntimeError, "Internal error in VirtualGet; Virtual column wrongly specified."), nullptr;

	return PyObject_CallFunctionObjArgs(PyTuple_GET_ITEM(t, 1), row, 0);
}

bool DBRowDescriptor::VirtualSet(int n, PyObject *row, PyObject *val)
{
	PyErr_SetString(PyExc_RuntimeError, "Virtual columns are read-only");
	return false;
}


PyObject *DBRowDescriptor::SequenceGet(DBRowDescriptor *row, Py_ssize_t i)
{
	if (i<0 || i>=(Py_ssize_t)row->mColumnList.size()) 
		return PyErr_Format(PyExc_IndexError, "index %d out of range", int(i));
	const ColumnDescriptor &cd = row->mColumnList[i];
	BluePyTuple r(3);
	if (!r) return 0;
	r.SET(0, PyUnicode_InternFromString(cd.mName.c_str()));
	r.SET(1, PyLong_FromLong(cd.mType));
	r.SET(2, PyLong_FromLong(cd.mSize));
	return r.Detach();
}


//////////////////////////////////////////////////////////////////////////////
// DBRow
//

bool operator == (const DBRow &a, const DBRow &b)
{
	if (!(*a.mRD == *b.mRD))
		return false;

	for (size_t i = 0; i < a.GetColumnCount(); ++i)
	{
		const auto &cd = a.mRD->mColumnList[i];
		auto del = [](PyObject *p) {
			Py_XDECREF(p);
		};

		auto lhs = std::unique_ptr<PyObject, decltype(del)>(a.Get(cd, i), del);
		auto rhs = std::unique_ptr<PyObject, decltype(del)>(b.Get(cd, i), del);

		if ( PyObject_RichCompareBool( lhs.get(), rhs.get(), Py_EQ ) != 1 ) // FIXME this swallows errors, which are indicated by a -1 return code
			return false;
	}

	return true;
}

bool DBRow::InitType(PyTypeObject *tp)
{
		static PySequenceMethods sequenceMethods = {
			(lenfunc)SequenceLength,
			0,
			0,
			(ssizeargfunc)SequenceGet,
			nullptr,
			(ssizeobjargproc)SequenceSet,
			nullptr,
		};

		static PyMappingMethods mappingMethods = {
			(lenfunc)SequenceLength,
			(binaryfunc)MappingSubscript,
			(objobjargproc)MappingAssSubscript
		};
		tp->tp_basicsize = BLUE_MEMBEROFFSET(DBRow, mData);
		tp->tp_itemsize = 1;
		tp->tp_flags |= Py_TPFLAGS_HAVE_GC; //support GC but not subclassing (because of our variable size)
										    //note to self:  Fix this, since strings allow it.
		tp->tp_as_sequence = &sequenceMethods;
		tp->tp_as_mapping = &mappingMethods;
		tp->tp_traverse = (traverseproc)GCTraverse;
		tp->tp_clear = (inquiry)GCClear;
		tp->tp_repr = (reprfunc)(PyCFunction)PyCFuncArgs<&DBRow::Repr>;
		tp->tp_getattro = (PyCFunction)PyCFuncArgs<&DBRow::GetAttr>;
		tp->tp_setattro = SetAttr;
		tp->tp_richcompare = Compare;
		tp->tp_hash = Hash;
		return true;
}


//Object constructor - destructor
//can be called with
// O or (O,)  -  Called with a single argument, a row descriptor or another Row
// (O, D) - init from Rowdescriptor and sequence of values
// (O, D, R) - init from Rowdescriptor, raw DB data, and reuse dict
PyObject *DBRow::_New(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	//we can be called with an argument directly, not a tuple, in args (optimization for marshaling)
	Py_ssize_t tsize;
	PyObject *firstarg;
	if (PyTuple_Check(args)) {
		tsize = PyTuple_GET_SIZE(args);
		if (!tsize)
			return PyErr_SetString(PyExc_ValueError, "expected at least one argument"), nullptr;
		firstarg = PyTuple_GET_ITEM(args, 0);
	} else {
		tsize = 1;
		firstarg = args;
	}
	
	//first try copy constructor
	if (PyObject_IsInstance(firstarg, (PyObject*)DBRow::GetType())) {
		//we have at least one DBRow object there at the start
		PyObject *otherO;
		if (PyArg_ParseTuple(args, "O:DBRow", &otherO)) {
			DBRow *other = reinterpret_cast<DBRow*>(otherO);
			void *raw = _Alloc(type, other->mRD->mTotalLen);
			DBRow *row = new(raw) DBRow(); //c++ constructor
			if (!row->Init(other)) {
				Py_DECREF(row);
				return 0;
			}
			return row;
		} else
			return 0;
	}
	//dont use PyArg_ParseTuple.  This is performance critical when instancing large
	//rowsets.
	PyObject *descriptorO, *data=0, *reuse=0;
	descriptorO = firstarg;
	if (!PyObject_IsInstance(descriptorO, (PyObject*)DBRowDescriptor::GetType()))
		return PyErr_SetString(PyExc_TypeError, "expected DBRow or DBRowDescriptor as first argument"), nullptr;
	if (tsize > 1)
		data = PyTuple_GET_ITEM(args, 1);
	if (tsize > 2)
		reuse = PyTuple_GET_ITEM(args, 2);
	if (tsize > 3)
		return PyErr_SetString(PyExc_ValueError, "expected at most two arguments"), nullptr;
	/*
	if (!PyArg_ParseTuple(args, "O!|OO", DBRowDescriptor::GetType(), &descriptorO, &data, &reuse))
		return 0;
	*/
	
	DBRowDescriptor *rd = static_cast<DBRowDescriptor*>( descriptorO );
	void *raw = _Alloc(type, rd->mTotalLen);
	if (!raw)
		return 0;
	DBRow *row = new(raw) DBRow; //c++ constructor
	if (!row->Init(rd, data, reuse)) {
		Py_DECREF(row);
		return 0;
	}
	return row;
}

PyObject * DBRow::CreateFromRowDescriptor( PyObject *rowDescriptor )
{
	DBRowDescriptor *rd = static_cast<DBRowDescriptor*>(rowDescriptor);
	void *raw = _Alloc( GetType(), rd->mTotalLen );
	if( !raw )
	{
		return 0;
	}

	DBRow *row = new(raw)DBRow(); //c++ constructor
	if( !row->Init( rd, 0, 0 ) )
	{
		Py_DECREF( row );
		return 0;
	}
	return row;
}

DBRow::~DBRow()
{
	if (!mRD)
		return;
	for(int i = 0; i<mRD->mNObjects; i++)
		Py_XDECREF(Object(i));
	Py_DECREF(mRD);
}


bool DBRow::Init(const DBRow *other)
{
	mRD = other->mRD;
	Py_INCREF(mRD);
	memcpy(Data(0), other->Data(0), mRD->mDataLen);
	for(int i = 0; i<mRD->mNObjects; i++) {
		Object(i) = other->Object(i);
		Py_XINCREF(Object(i));
	}
	return true;
}


bool DBRow::Init(DBRowDescriptor *rd, PyObject *data, PyObject *reuse)
{
	mRD = rd;
	Py_INCREF(rd);

	if (!data) {
		//okay, just null the stuff
		memset(Data(0), 0, mRD->mDataLen);
		for(int i = 0; i<mRD->mNObjects; i++) {
			Py_INCREF(Py_None);
			Object(i) = Py_None;
		}
		return true;
	}
	if (PyCapsule_CheckExact(data))
		return InitDB(data, reuse);
	return InitSeq(data, reuse);
}


//init from sequence
bool DBRow::InitSeq(PyObject *s, PyObject *reuse)
{
	Py_ssize_t l = PySequence_Size(s);

	if (l<0)
		return false;
	Py_ssize_t v = mRD->mVirtualGetSet ? PyList_GET_SIZE(mRD->mVirtualGetSet.o) : 0;
	Py_ssize_t rl = mRD->mColumnList.size() - v;

	if (l > rl)
		return PyErr_Format(PyExc_TypeError, "Sequence must be at most %d elements", int(rl)), false;
	if (l < rl) {
		if (PyErr_WarnEx(PyExc_UserWarning, "sequence is too short", 1))
			return false;
	}
	Py_ssize_t i;
	for(i = 0; i<mRD->mNObjects; i++)
		Object(i) = 0;
	for(i = 0; i<l; i++) {
		BluePy item(PySequence_GetItem(s, i));
		if (!item)
			return false;
		if (!Set(mRD->mColumnList[i], i, item))
			return false;
	}
	// set any remaining columns to None
	for(; i < rl; i++)
		if (!Set(mRD->mColumnList[i], i, Py_None))
			return false;
	return true;
}
	

//we are being initialized from DB
bool DBRow::InitDB(PyObject *dataO, PyObject *reuse)
{
	void *src = PyCapsule_GetPointer(dataO, "DBRow");
	if (!src) {
		if (PyErr_Occurred())
			return false;
		return PyErr_SetString(PyExc_RuntimeError, "unexpected null ptr"), false;
	}
	memcpy(&mData[0], src, mRD->mTotalLen);
	//Add references to the Python objects provided.
	for(int i = 0; i<mRD->mNObjects; i++) {
		PyObject *data = Object(i);
		if (!data) {
			data = Py_None;
			Object(i) = data;
			Py_INCREF(data);
			continue;
		}
		Py_INCREF(data);
		if (reuse) {
			if (!Reuse(reuse, data))
				return false;
			Object(i) = data;
		}
	}
	return true;
}

PyObject* DBRow::Compare(PyObject *a, PyObject *b, int op)
{
	// Object a is guaranteed to be of type blue.DBRow, second object can be NoneType
	if( a->ob_type != b->ob_type )
	{
		switch( op )
		{
		case Py_EQ:
			Py_RETURN_FALSE;
		case Py_NE:
			Py_RETURN_TRUE;
		default:
			Py_RETURN_NOTIMPLEMENTED;
		}
	}

	auto lhs = static_cast<DBRow*>( a );
	auto rhs = static_cast<DBRow*>( b );

	switch( op )
	{
	case Py_EQ:
		if( *lhs == *rhs )
			Py_RETURN_TRUE;
		Py_RETURN_FALSE;
	case Py_LT:
		if( lhs < rhs )
			Py_RETURN_TRUE;
		Py_RETURN_FALSE;
	case Py_GT:
		if( lhs > rhs )
			Py_RETURN_TRUE;
		Py_RETURN_FALSE;
	default:
		Py_RETURN_NOTIMPLEMENTED;
	}
}

Py_hash_t DBRow::Hash(PyObject *obj)
{
	std::hash<PyObject *> hash;
	return static_cast<Py_hash_t>(hash(obj));
}

//store Reuse a python object being set into a row.  Look it up in a dict and replace, or insert it.
bool DBRow::Reuse(PyObject *reuse, PyObject* &r)
{
	PyObject *found = PyDict_GetItem(reuse, r);
	if (found) {
		Py_INCREF(found);
		Py_DECREF(r);
		r = found;
		return true;
	} else {
		if (PyErr_Occurred())
			return false;
		if (PyDict_SetItem(reuse, r, r))
			return false;
		return true;
	}
}
	

PyObject *DBRow::Repr(PyObject *args)
{
	PyObject *l = PyList_New(mRD->mColumnList.size());
	if (!l)
		return 0;
	for(size_t i = 0; i<mRD->mColumnList.size(); i++) {
		PyObject *v = Get(mRD->mColumnList[i], i);
		if (!v) {
			Py_DECREF(l);
			return 0;
		}
		PyList_SET_ITEM(l, i, v); //steal ref to v
	}
	PyObject *result = PyUnicode_FromFormat("<DBRow object %R>", l);
	Py_DECREF(l);
	return result;
}


PyObject *DBRow::Get(const ColumnDescriptor &c, Py_ssize_t i) const
{
	if (i<mRD->mNRealCols && GetBit(mRD->mSNull + i)) {
		//field is null
		Py_INCREF(Py_None);
		return Py_None;
	}

	void *data = Data(c.mOffset);
	switch (c.mType) {
	case DBTYPE_I1:
		return PyLong_FromLong(*(int8_t*)data);
	case DBTYPE_UI1:
		return PyLong_FromLong(*(uint8_t*)data);
	case DBTYPE_BOOL: {
		PyObject *result = GetBit(c.mOffset)?Py_True:Py_False;
		Py_INCREF(result);
		return result; }
	case DBTYPE_I2:
		return PyLong_FromLong(*(int16_t*)data);
	case DBTYPE_UI2:
		return PyLong_FromLong(*(uint16_t*)data);
	case DBTYPE_I4:
		return PyLong_FromLong(*(int32_t*)data);
	case DBTYPE_UI4:
		return PyLong_FromLong(*(uint32_t*)data);
	case DBTYPE_R4:
		return PyFloat_FromDouble(*(float*)data);
	case DBTYPE_I8:
		return PyLong_FromLongLong(*(int64_t*)data);
	case DBTYPE_UI8:
	case DBTYPE_FILETIME:
		return PyLong_FromUnsignedLongLong(*(uint64_t*)data);
	case DBTYPE_R8:
		return PyFloat_FromDouble(*(double*)data);
	case DBTYPE_CY:
		return PyFloat_FromDouble(double(*(int64_t*)data) / 10000.0);
	case DBTYPE_STR:
	case DBTYPE_WSTR:
	case DBTYPE_BYTES: {
		//a python object
		PyObject *result = *(PyObject**)data;
		Py_XINCREF(result);
		return result;}
	case DBTYPE_EMPTY:
		// A virtual column!
		return mRD->VirtualGet(c.mOffset, this);
	default:
		PyErr_Format(PyExc_RuntimeError, "Unexpected db column type encountered: %d", c.mType);
		return nullptr;
	}
}


template<class T>
static bool SetInt(T* dest, PyObject *o)
{
	long l = PyLong_AsLong(o);
	if (l == -1 && PyErr_Occurred())
		return false;
	*dest = (T)l;
	if (sizeof(*dest)<sizeof(l) && *dest != l) {
		if (PyErr_WarnEx(PyExc_UserWarning, "integer was truncated", 1))
			return false;
	}
	return true;
}


template<class T>
static bool SetFloat(T* dest, PyObject *o)
{
	double d = PyFloat_AsDouble(o);
	if (d == -1.0 && PyErr_Occurred())
		return false;
	*dest = (T)d;
	if (sizeof(*dest)<sizeof(d) && *dest != d) {
		if (PyErr_WarnEx(PyExc_UserWarning, "float was truncated", 1))
			return false;
	}
	return true;
}


//Set wrapper that handles PyNone by setting flag to null
bool DBRow::Set(const ColumnDescriptor &c, Py_ssize_t i, PyObject *o)
{
	if (i >= mRD->mNRealCols)
		//virtual columns have no special null handling
		return SetNotNull(c, o);

	if (o != Py_None) {
		//non null
		bool r = SetNotNull(c, o);
		if (r)
			//clear NULL flag
			ClrBit(mRD->mSNull + i);
		return r;
	} else {
		void *data = Data(c.mOffset);
		switch (c.mType) {
		case DBTYPE_STR:
		case DBTYPE_WSTR:
		case DBTYPE_BYTES:
			//a python object
			Py_XDECREF(*(PyObject**)data);
			Py_INCREF(Py_None);
			*(PyObject**)data = Py_None;
		}
		SetBit(mRD->mSNull + i);
		return true;
	}
}


//Actual setting of the column, given that we are not setting None
bool DBRow::SetNotNull(const ColumnDescriptor &c, PyObject *o)
{
	void *data = Data(c.mOffset);
	switch (c.mType) {
	case DBTYPE_I1:
		return SetInt((int8_t*)data, o);
	case DBTYPE_UI1:
		return SetInt((uint8_t*)data, o);
	case DBTYPE_BOOL: {
		if (PyObject_IsTrue(o))
			SetBit(c.mOffset);
		else
			ClrBit(c.mOffset);
		return true; }
	case DBTYPE_I2:
		return SetInt((int16_t*)data, o);
	case DBTYPE_UI2:
		return SetInt((uint16_t*)data, o);
	case DBTYPE_I4:
		return SetInt((int32_t*)data, o);
	case DBTYPE_UI4:
		return SetInt((uint32_t*)data, o);
	case DBTYPE_R4:
		return SetFloat((float*)data, o);
		
	case DBTYPE_I8: {
		int64_t tmp = PyLong_AsLongLong(o);
		if (tmp == -1 && PyErr_Occurred())
			return false;
		*(int64_t*)data = tmp;
		return true; }		
	case DBTYPE_FILETIME:
	case DBTYPE_UI8: {
		uint64_t tmp = PyLong_AsUnsignedLongLong(o);
		if (tmp == (uint64_t)-1 && PyErr_Occurred())
			return false;
		*(uint64_t*)data = tmp;
		return true; }
	case DBTYPE_R8:
		return SetFloat((double*)data, o);
	case DBTYPE_CY: {
		double d = PyFloat_AsDouble(o);
		if (d == -1.0 && PyErr_Occurred())
			return false;
		*(int64_t*)data = (int64_t)(floor(d * 100.0 + 0.5) * 100.0);
		return true; }

	case DBTYPE_STR:
	case DBTYPE_WSTR:
	case DBTYPE_BYTES:
		//a python object
		Py_INCREF(o);
		Py_XDECREF(*(PyObject**)data);
		*(PyObject**)data = o;
		return true;

	case DBTYPE_EMPTY:
		PyErr_SetString(PyExc_RuntimeError, "Virtual columns are read-only");
		return false;
	default:
		PyErr_Format(PyExc_RuntimeError, "Unexpected db column type encountered: %d", c.mType);
		return false;
	}
}


//Data field accessors
PyObject* &DBRow::Object(Py_ssize_t idx) const {
	PyObject ** p = ((PyObject**)mData)+mRD->mSObjects+idx;
	return *p;
}

//return a byte containing a bit
char *DBRow::BitPtr(int &bit, Py_ssize_t bitoffset) const 
{
	Py_ssize_t byte = bitoffset/8;
	CCP_ASSERT(byte >= 0 && byte < mRD->mDataLen);
	bit = (int)(bitoffset % 8);
	return (char*)mData + byte;
}


void DBRow::SetBit(Py_ssize_t bitoffset, bool val)
{
	int bit;
	char *byte = BitPtr(bit, bitoffset);
    if (val)
		*byte |= 1<<bit;
	else
		*byte &= ~(1<<bit);
}


bool DBRow::GetBit(Py_ssize_t bitoffset) const
{
	int bit;
	char *byte = BitPtr(bit, bitoffset);
	return ((*byte) & (1<<bit)) != 0;
}


//Sequence interface
Py_ssize_t DBRow::SequenceLength(DBRow *row)
{
	return (int)row->mRD->mColumnList.size();
}

PyObject *DBRow::SequenceGet(DBRow *row, Py_ssize_t i)
{
	if (i<0 || i>= (int)row->mRD->mColumnList.size())
		return PyErr_Format(PyExc_IndexError, "index out of range: %d", int(i)), nullptr;
	return row->Get(row->mRD->mColumnList[i], i);
}

int DBRow::SequenceSet(DBRow *row, Py_ssize_t index, PyObject *val)
{
	Py_ssize_t i = index;
	if (i<0)
		i += row->mRD->mColumnList.size();
	if (i<0 || i>= (Py_ssize_t)row->mRD->mColumnList.size())
		return PyErr_Format(PyExc_IndexError, "index out of range: %d", int(index)), -1;
	if (!val)
		return PyErr_SetString(PyExc_RuntimeError, "cannot delete column"), -1;
	if (!row->Set(row->mRD->mColumnList[i], i, val))
		return -1;
	return 0;
}


PyObject *DBRow::SequenceGetSlice(DBRow *row, Py_ssize_t ilow, Py_ssize_t ihigh)
{
	Py_ssize_t mysize = row->mRD->mColumnList.size();
	if (ilow<0)
		ilow = 0;
	else if (ilow > mysize)
		ilow = mysize;
	if (ihigh < ilow)
		ihigh = ilow;
	else if (ihigh > mysize)
		ihigh = mysize;
	Py_ssize_t n = ihigh-ilow;
	BluePyList r(n);
	if (!r) return 0;
	for(Py_ssize_t i = 0; i<n; i++) {
		const ColumnDescriptor &cd = row->mRD->mColumnList[i+ilow];
		BluePy e(row->Get(cd, i+ilow));
		if (!e) return 0;
		if (!r.Set(i, e)) return 0;
	}
	return r.Detach();
}


int DBRow::SequenceSetSlice(DBRow *row, Py_ssize_t ilow, Py_ssize_t ihigh, PyObject *v)
{
	//see implementation for list in the python source.  We must postpone decrefs until list
	//is ready.
	BluePy s;
	if (v) {
		s = BluePy(PySequence_Fast(v, "must assign a sequence"));
		if (!s) return 0;
	}
	Py_ssize_t n = s?PySequence_Fast_GET_SIZE(s.o):0;
	Py_ssize_t mysize = row->mRD->mColumnList.size();
	if (ilow<0)
		ilow = 0;
	else if (ilow > mysize)
		ilow = mysize;
	if (ihigh < ilow)
		ihigh = ilow;
	else if (ihigh > mysize)
		ihigh = mysize;

	if (ihigh > mysize || ihigh-ilow != n)
		return PyErr_SetString(PyExc_RuntimeError, "Cannot modify row length"), -1;
	BluePyList garbage;
	for (int i=0; i<n; i++) {
		PyObject *e = PySequence_Fast_GET_ITEM(s.o, i);
		const ColumnDescriptor &cd = row->mRD->mColumnList[i+ilow];
		if (cd.IsObject()) {
			BluePy old(row->Get(cd, i+ilow));
			if (!old) return -1;
			if (!garbage)
				garbage = BluePyList(0);
			if (!garbage ||!garbage.Append(old)) return -1;
		}
		if (!row->Set(cd, i+ilow, e)) return -1;
	}
	return 0;
}


////////////////////////
// Mapping interface
const ColumnDescriptor *DBRow::GetCD(int &idx, PyObject *key, PyObject *exception)
{
	if (PyUnicode_Check(key)) {
		const char *c = PyUnicode_AsUTF8(key);
		RowDescriptor::columnMap_t::iterator i = mRD->mColumnMap.find(c);
		if (i==mRD->mColumnMap.end()) {
			if (exception)
				return PyErr_Format(exception, "Row has no field %s", c), nullptr;
			return 0;
		}
		idx = (*i).second.second;
		return (*i).second.first;
	}
	if (PyLong_Check(key)) {
		int c = int(PyLong_AS_LONG(key));
		int oc = c;
		if (c < 0)
			c =(int) mRD->mColumnList.size()+c;
		if (c<0 || c>=(int)mRD->mColumnList.size())
			return PyErr_Format(PyExc_IndexError, "index %d out of range", oc), nullptr;
		idx = c;
		return &mRD->mColumnList[c];
	}
	PyErr_SetString(PyExc_TypeError, "only integers or strings supported");
	return 0;
}


PyObject* DBRow::MappingSubscript( DBRow* row, PyObject* key )
{
	const int columnCount = static_cast<int>( row->mRD->mColumnList.size() );

	if( PyUnicode_Check( key ) )
	{
		const char* unicodeKey = PyUnicode_AsUTF8( key );
		const auto iterator = row->mRD->mColumnMap.find( unicodeKey );

		if( iterator == row->mRD->mColumnMap.end() )
			return PyErr_Format( PyExc_KeyError, "Row has no field %s", unicodeKey ), nullptr;

		const auto index = iterator->second.second;
		return SequenceGet( row, index );
	}

	if( PyIndex_Check( key ) )
	{
		Py_ssize_t index = PyNumber_AsSsize_t( key, PyExc_IndexError );

		if( index == -1 && PyErr_Occurred() )
			return nullptr;

		if( index < 0 )
			index += columnCount;

		return SequenceGet( row, index );
	}

	if( PySlice_Check( key ) )
	{
		Py_ssize_t start, stop, step;

		if( PySlice_Unpack( key, &start, &stop, &step ) )
			return nullptr;

		PySlice_AdjustIndices( columnCount, &start, &stop, step );

		if( step != 1 )
		{
			PyErr_SetString( PyExc_NotImplementedError, "Supported for slices with a step size other than 1 is not implemented." );
			return nullptr;
		}

		return SequenceGetSlice( row, start, stop );
	}

	PyErr_Format( PyExc_TypeError, "list indices must be integers or slices, not %.200s", Py_TYPE( key )->tp_name );
	return nullptr;
}


int DBRow::MappingAssSubscript(DBRow *row, PyObject *key, PyObject *value)
	{
	if (!value)
		return PyErr_SetString(PyExc_RuntimeError, "cannot delete column"), -1;
	int idx;
	const ColumnDescriptor *cd = row->GetCD(idx, key, PyExc_KeyError);
	if (!cd)
		return -1;
	if(!row->Set(*cd, idx, value))
		return -1;
	return 0;
}

/////////////////////
//Garbage collection
int DBRow::GCTraverse(DBRow *self, visitproc visit, void *arg)
{
	if (!self->mRD)
		return 0;
	if (visit(self->mRD, arg)<0)
		return -1;
	for(int i = 0; i<self->mRD->mNObjects; i++)
		if (self->Object(i)) {
			if (visit(self->Object(i), arg)<0)
				return -1;
		}
	return 0;
}

int DBRow::GCClear(DBRow *self)
{
	//careful here.  the docs maintain we should keep things valid, but we assume non-zero mRD and
	//object pointers.
	if (!self->mRD)
		return 0;
	
	for(int i = 0; i<self->mRD->mNObjects; i++) {
		Py_XDECREF(self->Object(i));
		self->Object(i) = 0;
	}
	Py_DECREF(self->mRD);
	self->mRD = 0;
	return 0;
}

//////////////////////
// Regular methods

//Reduce.
//We must use setstate when unreducing, since the object columns may hold circular references to ourselves.
PyObject *DBRow::__reduce_ex__(PyObject *protocol)
{
	//special case for null objects
	if (!mRD->mTotalLen)
		return Py_BuildValue("O(O)", DBRow::GetType(), mRD);

	PyObject *state = __getstate__();
	if (!state) return 0;
	return Py_BuildValue("O(O)N", DBRow::GetType(), mRD, state);
}


//returns either a tuple of string and list, or a string, or a list.
PyObject *DBRow::__getstate__()
{
	BluePy str;
	if (mRD->mDataLen) {
		//str = BluePy(PyString_FromStringAndSize((char*)Data(0), mRD->mDataLen));
		std::vector<char> packed;
		PackData(packed);
		str = BluePy(PyBytes_FromStringAndSize(&packed[0], packed.size()));
		if (!str) return 0;
	}
	BluePy objs;
	if (mRD->mNObjects) {
		BluePyList l(mRD->mNObjects);
		if (!l) return 0;
		for(int i = 0; i<mRD->mNObjects; i++)
			if (!l.Set(i, Object(i))) return 0;
		objs = l;
	}
	if (str && objs)
		return Py_BuildValue("OO", str.o, objs.o);
	if (str)
		return str.Detach();
	return objs.Detach();
}


PyObject *DBRow::__setstate__(PyObject *state)
{
	PyObject *str=0, *objs=0;
	if (PyTuple_Check(state)) {
		if (PyTuple_GET_SIZE(state)<2)
			return PyErr_SetString(PyExc_TypeError, "expected a tuple of size 2"), nullptr;
		str = PyTuple_GET_ITEM(state, 0);
		objs = PyTuple_GET_ITEM(state, 1);
		if (!PyBytes_Check(str) || !PyList_Check(objs))
			return PyErr_SetString(PyExc_TypeError, "expected a tuple bytes and list"), nullptr;
	} else if (PyBytes_Check(state))
		str = state;
	else if (PyList_Check(state))
		objs = state;
	else
		return PyErr_SetString(PyExc_TypeError, "expected tuple, list or string"), nullptr;

	if (str) {
		const char *data;
		Py_ssize_t datalen;
		if (PyBytes_AsStringAndSize(str, (char**)&data, &datalen) || !UnpackData(data, datalen))
			return nullptr;
		/*
		int l = PyString_GET_SIZE(str);
		if (l > mRD->mDataLen)
			l = mRD->mDataLen;
		memcpy(Data(0), PyString_AS_STRING(str), l);
		*/
	}
	if (objs) {
		Py_ssize_t l = PyList_GET_SIZE(objs);
		if (l > mRD->mNObjects)
			l = mRD->mNObjects;
		for(Py_ssize_t i = 0; i<l; i++) {
			Object(i) = PyList_GET_ITEM(objs, i);
			Py_INCREF(Object(i));
		}
	}
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *DBRow::Get___keys__()
{
	return mRD->Keys();
}


PyObject *DBRow::Get___columns__()
{
	return mRD->Keys();
}


//Access to the header (row descriptor)
PyObject *DBRow::Get___header__()
{
	Py_INCREF(mRD);
	return mRD;
}


//Setting the header requires it to have the same dataLen
bool DBRow::Set___header__(PyObject *h)
{
	if (!PyObject_IsInstance(h, reinterpret_cast<PyObject*>( DBRowDescriptor::GetType() )))
		return PyErr_SetString(PyExc_TypeError, "require a DBRowDescriptor instance"), false;
	DBRowDescriptor *rd = static_cast<DBRowDescriptor*>( h );
	if (rd->mDataLen != mRD->mDataLen || rd->mNObjects != mRD->mNObjects)
		return PyErr_SetString(PyExc_ValueError, "header.Size() doesn't match"), false;

	Py_INCREF(rd);
	PyObject *old = mRD;
	mRD=rd;
	Py_DECREF(old);
	return true;
}


PyObject *DBRow::Get___data__()
{
	int offset = mRD->mDataLen ? 1 : 0; //is there blob data there?
	PyObject *r = PyTuple_New(mRD->mNObjects+offset);
	if (!r) return 0;
	if (offset)
		PyTuple_SET_ITEM(r, 0, PyBytes_FromStringAndSize((char*)Data(0), mRD->mDataLen));
	for (int i = 0; i<mRD->mNObjects; i++) {
		PyObject *o = Object(i);
		Py_XINCREF(o);
		PyTuple_SET_ITEM(r, i+offset, o);
	}
	return r;
}


bool DBRow::Set___data__(PyObject *v)
{
	int offset = mRD->mDataLen ? 1 : 0; //is there blob data there?
	if (!PyTuple_Check(v) || PyTuple_GET_SIZE(v) != mRD->mNObjects + offset)
		return PyErr_Format(PyExc_ValueError, "argument must be tuple of %d elements", mRD->mNObjects+offset), false;
	if (offset) {
		PyObject *s = PyTuple_GET_ITEM(v, 0);
		if (!PyBytes_Check(s) || PyBytes_GET_SIZE(s) != mRD->mDataLen)
			return PyErr_Format(PyExc_ValueError, "incorrect blob size, must be %d bytes", mRD->mDataLen), false;
		memcpy(Data(0), PyBytes_AS_STRING(s), mRD->mDataLen);
	}
	for (int i = 0; i<mRD->mNObjects; i++) {
		PyObject *o PyTuple_GET_ITEM(v, i+offset);
		Py_XINCREF(o);
		Object(i) = o;
	}
	return true;
}


//same, but return compressed blob data
PyObject *DBRow::Get___cdata__()
{
	int offset = mRD->mDataLen ? 1 : 0; //is there blob data there?
	PyObject *r = PyTuple_New(mRD->mNObjects+offset);
	if (!r) return 0;
	if (offset) {
		std::vector<char> data;
		PackData(data);
		PyTuple_SET_ITEM(r, 0, PyBytes_FromStringAndSize(data.size()?&data[0]:0, data.size()));
	}
	for (int i = 0; i<mRD->mNObjects; i++) {
		PyObject *o = Object(i);
		Py_XINCREF(o);
		PyTuple_SET_ITEM(r, i+offset, o);
	}
	return r;
}


//set data, with compressed blob
bool DBRow::Set___cdata__(PyObject *v)
{
	int offset = mRD->mDataLen ? 1 : 0; //is there blob data there?
	if (!PyTuple_Check(v) || PyTuple_GET_SIZE(v) != mRD->mNObjects + offset)
		return PyErr_Format(PyExc_ValueError, "argument must be tuple of %d elements", mRD->mNObjects+offset), false;
	if (offset) {
		PyObject *s = PyTuple_GET_ITEM(v, 0);
		if (!PyBytes_Check(s))
			return PyErr_SetString(PyExc_TypeError, "blob must be string"), false;
		if (!UnpackData(PyBytes_AS_STRING(s), PyBytes_GET_SIZE(s)))
			return false;
	}
	for (int i = 0; i<mRD->mNObjects; i++) {
		PyObject *o PyTuple_GET_ITEM(v, i+offset);
		Py_XINCREF(o);
		Object(i) = o;
	}
	return true;
}


PyObject *DBRow::GetAttr(PyObject *key)
{
	// First try looking at our columns
	int idx;
	const ColumnDescriptor *cd = GetCD(idx, key, NULL);
	if (cd != 0)
		return Get(*cd, idx);
	// If that fails, try standard python attribute lookup
	return PyObject_GenericGetAttr(this, key);
}


int DBRow::SetAttr(PyObject *self, PyObject *key, PyObject *val)
{
	int idx;
	DBRow *tis = reinterpret_cast<DBRow*>(self);
	const ColumnDescriptor *cd = tis->GetCD(idx, key, NULL);
	if (cd != 0)
	{
		// Guard against `del DBRow.column` / `delattr(DBRow, "column")`
		if (!val)
		{
			PyErr_SetString(PyExc_RuntimeError, "Cannot delete a column from a DBRow");
			return -1;
		}
		if (!tis->Set(*cd, idx, val))
			return -1;
		return 0;
	}
	// not a 'column' attribute, try normal python attributes
	return PyObject_GenericSetAttr(self, key, val);
}



//magic Run length encoding.  Encode runs of zeroes, and non-zero bytes.  a nibble (four bits)
//for each run.
void RLEPack(std::vector<char> &out, const char *data, size_t datalen)
{
	int zerorun = 0;
	int nibble = 0;
	size_t marker;
	for(size_t i = 0; i<datalen; ) {
		if (!nibble) {
			//start a new dual batch
			marker = out.size();
		out.push_back(0);
		}
		size_t count;
		size_t start = i;
		size_t end = i+8;//how far to scan
		if (end > datalen)
			end = datalen;
		if (!data[i]) {
			//zeroes.  Count the run
			zerorun++;
			i+=1;
			for(; i<end && !data[i]; i++)
				;
			count = i-start-1;
		} else {
			zerorun = 0;
			out.push_back(data[i++]);
			for(; i<end && data[i]; i++)
				out.push_back(data[i]);
			count = start-i;
			}
		count += 8; //offset it
		if (nibble)
			count <<= 4;
		out[marker] |= count;
		nibble = !nibble;
	}
	//remove trailing zeroes
	if (nibble && zerorun)
		zerorun++;  //implicit last nibble is also zero
	while(zerorun>=2) {
		out.pop_back();
		zerorun -= 2;
	}
}


bool RLEUnPack(char *out, size_t outlen, const char *data, size_t datalen)
{
	int nibble = 0;
	int run;
	size_t outc = 0;
	for(size_t i = 0; i<datalen && outc<outlen; ) {
		int count;
		if (!nibble) {
			run = data[i++];
			count = run & 0xf;
			} else
			count = (run & 0xf0)>>4;
		nibble = !nibble;
		count -= 8;
		if (count >= 0) {
			if (outc + count + 1 > outlen) 
				goto ERR;
			while(count-- >= 0) {
				out[outc++] = 0;
			}
		} else {
			if (outc - count > outlen)
				goto ERR;
			while(count++ && i<datalen)
				out[outc++] = data[i++];
		}
	}
	//fill the end
	while (outc < outlen)
		out[outc++] = 0;
	return true;
ERR:
	return PyErr_SetString(PyExc_RuntimeError, "Invalid RLE string"), false;
}
			

//Getting packed data
void DBRow::PackData(std::vector<char> &res)
{
	RLEPack(res, (const char *)Data(0), mRD->mDataLen);
}
	
bool DBRow::UnpackData(const char *data, Py_ssize_t datalen){
	//Todo: don't use vector, unpack directly into object.
	return RLEUnPack((char*)Data(0), mRD->mDataLen, data, datalen);
}



//Custom marshaling
bool DBRow::Write(Marshal &m, WriteStream &s)
{
	if (!m.WriteObject(&s, mRD)) return false;

	s.mScratch.resize(0); //use always the same scratch vector
	PackData(s.mScratch);
	size_t sz = s.mScratch.size();
	if (!s.WriteBuff(sz ? &s.mScratch[0] : 0, sz)) return false;

	for(int i = 0; i<mRD->mNObjects; i++)
		if (!m.WriteObject(&s, Object(i))) return false;
	return true;
}


PyObject *DBRow::Read(Marshal &m, ReadStream &s)
{
	BluePy rd(m.ReadObject(&s)); //read row descriptor
	if (!rd) return 0;

	//Check object is of correct DBRowDescriptorType
	if( DBRowDescriptor::GetType() != Py_TYPE( rd.o ) )
	{
		PyErr_Format( PyExc_TypeError, "Hacker Warning! Invalid type in marshal data, expected blue.DBRowDescriptor." );
		return 0;
	}

	//Create object
	BluePy rowO(DBRow::CreateFromRowDescriptor( rd ) ); 
	if (!rowO) return 0;
	DBRow *row = static_cast<DBRow*>(rowO.o);

	//get data
	const char *data;
	int datalen;
	if (!s.GetBuff(data, datalen)) return 0;

	//Todo: RLE directly into data.
	if (!row->UnpackData(data, datalen)) return 0;
	
	//get objects
	for(int i = 0; i<row->mRD->mNObjects; i++) {
		BluePy obj(m.ReadObject(&s));
		if (!obj) return 0;
		row->Object(i) = obj.Detach();
	}
	return rowO.Detach();
}


///////////////////////////////////////////////////////////////////
// C access api
size_t DBRow::GetColumnCount() const
{
	return mRD->mColumnList.size();
}

Py_ssize_t DBRow::GetDataOffset(size_t i, datatypes &type, Py_ssize_t &nullOffset) const
{
	const ColumnDescriptor &c = mRD->mColumnList[i];
	type = (datatypes)c.mType;
	if ((int)i >= mRD->mNRealCols)
		return -1; //can't deal with virtual columns
	nullOffset = mRD->mSNull + i;
	return c.mOffset;
}


///////////////////////////////////////////////////////////////////
// Initialize the blue module
bool DBRowsetInit(PyObject *module)
{
	Py_INCREF(DBRowDescriptor::GetType());
	if (PyModule_AddObject(module, "DBRowDescriptor", (PyObject*)DBRowDescriptor::GetType()))
		return false;
	if (PyModule_AddObject(module, "DBRow", (PyObject*)DBRow::GetType()))
		return false;

	//create a blue.db module, load it with classes and constants.  Way to go.
	PyObject *db = PyModule_New("blue.db");
	if (PyModule_AddObject(db, "RowDescriptor", (PyObject*)DBRowDescriptor::GetType()))
		return false;
	if (PyModule_AddObject(db, "Row", (PyObject*)DBRow::GetType()))
		return false;

#define ADD(C) if (PyModule_AddIntConstant(db, #C, C)) return false;

	ADD(DBTYPE_I1)
	ADD(DBTYPE_UI1)
	ADD(DBTYPE_BOOL)
	ADD(DBTYPE_I2)
	ADD(DBTYPE_UI2)
	ADD(DBTYPE_I4)
	ADD(DBTYPE_UI4)
	ADD(DBTYPE_R4)
	ADD(DBTYPE_I8)
	ADD(DBTYPE_FILETIME)
	ADD(DBTYPE_UI8)
	ADD(DBTYPE_R8)
	ADD(DBTYPE_CY)
	ADD(DBTYPE_STR)
	ADD(DBTYPE_WSTR)
	ADD(DBTYPE_BYTES)

	return true;
}

#endif
