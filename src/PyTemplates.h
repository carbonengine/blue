// Copyright © 2003 CCP ehf.

/*
	*************************************************************************

	PyTemplates.h

	Project:   Python template helpers

	Description:

		Simplifies declarations of python classes


	Dependencies:

		Python

	*************************************************************************
*/

#ifndef _PYTEMPLATES_H_
#define _PYTEMPLATES_H_

#include <Python.h>
#include <structmember.h>

// -------------------------------------------------------------
// Error reporting helpers
namespace PyErr
{
	PyObject* SetErr32(int err, const char* format, ...);
};

/// <summary>
/// this is a replacement for:
/// `typedef PyObject* ( *PyNoArgsFunction )( PyObject* )`
/// which was removed from the CPython API in Python3.9
/// </summary>
/// 
using BlueNoArgsFunction = std::add_pointer<PyObject*(PyObject*)>::type;

// -------------------------------------------------------------
// Macro which declares METH_NOARGS python method definition
#define METHOD_NOARGS(_fn, _doc) \
	{ #_fn, (PyCFunction)(BlueNoArgsFunction)PyCFuncNoArgs<&_ClassType::_fn>, METH_NOARGS, _doc },

// -------------------------------------------------------------
// Macro which declares METH_O python method definition
#define METHOD_O(_fn, _doc) \
	{#_fn, PyCFuncArgs<&_ClassType::_fn>, METH_O, _doc},

// -------------------------------------------------------------
// Macro which declares METH_VARARGS python method definition
#define METHOD_VARARGS(_fn, _doc) \
	{#_fn, PyCFuncArgs<&_ClassType::_fn>, METH_VARARGS, _doc},

// -------------------------------------------------------------
// Macro which declares METH_KEYWORDS python method definition
#define METHOD_KEYWORDS(_fn, _doc) \
	{ #_fn, (PyCFunction)(PyCFunctionWithKeywords)PyCFuncKeywords<&_ClassType::_fn>, \
		METH_VARARGS | METH_KEYWORDS, _doc},


// -------------------------------------------------------------
// Macros to construct PyMethodDef chain
#define PYTHON_METHODS_BEGIN() \
	static const PyMethodDef* _MethodDefs() \
	{ \
		static const PyMethodDef defs[] = \
		{

#define PYTHON_METHODS_END() \
			{0, 0} \
		}; \
		return defs;\
	}

// -------------------------------------------------------------
// Macros to construct PyGetSetDef chain
#define PYTHON_GETSET_BEGIN() \
	static const PyGetSetDef* _GetSetDefs() \
	{ \
		static const PyGetSetDef defs[] = \
		{

#define PYTHON_GETSET(_propname, _descr) \
	{(char*)#_propname, PyCFuncGetter<&_ClassType::Get_##_propname>, PyCFuncSetter<&_ClassType::Set_##_propname>, (char*)_descr},

#define PYTHON_GET(_propname, _descr) \
	{(char*)#_propname, PyCFuncGetter<&_ClassType::Get_##_propname>, NULL, (char*)_descr},

#define PYTHON_GETLIST(_propname, _variable, _descr, _listclass) \
	{_propname, _listclass::_getList<offsetof(_ClassType, _variable)>, NULL, _descr},
	//{"listeners", ListenerList::_getList<offsetof(_ClassType, mListeners)>, NULL, "hi there"},

#define PYTHON_GETINT64(_propname, _variable, _descr) \
	{_propname, PyCFuncGetInt64< &_ClassType::_variable>, NULL, _descr},

#define PYTHON_GETSETINT64(_propname, _variable, _descr) \
	{_propname, PyCFuncGetInt64< &_ClassType::_variable>, PyCFuncSetInt64< offsetof(_ClassType, _variable)>, _descr},

#define PYTHON_GETSET_END() PYTHON_METHODS_END()

// -------------------------------------------------------------
// Macros to construct PyMemberDef chain
#define PYTHON_MEMBERS_BEGIN() \
	static const PyMemberDef* _MemberDefs() \
	{ \
		static const PyMemberDef defs[] = \
		{

#define PYTHON_MEMBER(_propname, _type, _variable, _flags) \
	{(char*)_propname, _type, (int)_spesoffs(_ClassType, _variable), _flags},

#define PYTHON_MEMBERS_END() PYTHON_METHODS_END()

// -------------------------------------------------------------
// Macros to uhm... give the class a name
#define PYTHON_CLASS(_classname) \
	static char* _ClassName() { return (char*)_classname; }

#define _spesoffs(s, m) (BLUE_MEMBEROFFSET(s, m) - (size_t)static_cast<PyObject*>(reinterpret_cast<s*>(0)))



// -------------------------------------------------------------
// This class wraps a python type, so we can define it without the silliness of huge static initializers.
class PyXTypeObject : public PyTypeObject
{
public:
	PyXTypeObject(char *name) {
		static const PyTypeObject def = {PyObject_HEAD_INIT(0)}; //the default, empty dude.
		*static_cast<PyTypeObject*>(this) = def;
		tp_name = name;
		tp_flags = Py_TPFLAGS_DEFAULT;
		tp_doc = "";
	}
	bool IsReady() {return !!(tp_flags & Py_TPFLAGS_READY);}
	bool Ready() {return PyType_Ready(this) == 0;}
};


// -------------------------------------------------------------
// A thunker class.  Defines template thunker instances to thunk away the "self" thing
template<class T>
class PyXThunker
{
public:
	// Thunker for no argument PyCFunction
	typedef PyObject* (T::*PYCFUNCNOARGS)();
	template <PYCFUNCNOARGS meth>
	static PyObject* PyCFuncNoArgs(PyObject* self)
	{
		T* pThis = static_cast<T*>(self);
		return (pThis->*meth)();
	}

	// Thunker for single argument PyCFunction
	typedef PyObject* (T::*PYCFUNCARGS)(PyObject*);
	template <PYCFUNCARGS meth>
	static PyObject* PyCFuncArgs(PyObject* self, PyObject* args)
	{
		T* pThis = static_cast<T*>(self);
		return (pThis->*meth)(args);
	}

	// Thunker for double argument PyCFunction
	typedef PyObject* (T::*PYCFUNCKEYWORDS)(PyObject*, PyObject*);
	template <PYCFUNCKEYWORDS meth>
	static PyObject* PyCFuncKeywords(PyObject* self, PyObject* args, PyObject* kw)
	{
		T* pThis = static_cast<T*>(self);
		return (pThis->*meth)(args, kw);
	}

	// Thunker for no arguments proc
	typedef PyObject* (T::*PYCPROCNOARGS)();
	template <PYCPROCNOARGS meth>
	static int PyCProcNoArgs(PyObject* self)
	{
		T* pThis = static_cast<T*>(self);
		return (pThis->*meth)();
	}

	// Thunker for single argument proc
	typedef PyObject* (T::*PYCPROCARGS)(PyObject*);
	template <PYCPROCARGS meth>
	static int PyCProcArgs(PyObject* self, PyObject* args)
	{
		T* pThis = static_cast<T*>(self);
		return (pThis->*meth)(args);
	}

	// Thunker for getter
	typedef PyObject* (T::*PYCFUNCGETTER)();
	template <PYCFUNCGETTER meth>
	static PyObject* PyCFuncGetter(PyObject* self, void*)
	{
		T* pThis = static_cast<T*>(self);
		return (pThis->*meth)();
	}

	// Thunker for setter
	typedef bool (T::*PYCFUNCSETTER)(PyObject*);
	template <PYCFUNCSETTER meth>
	static int PyCFuncSetter(PyObject* self, PyObject* v, void*)
	{
		T* pThis = static_cast<T*>(self);
		return (pThis->*meth)(v) ? 0 : -1;
	}

	// Help thunk for get/setting int64
	template <int64_t T::*P>
	static PyObject* PyCFuncGetInt64(PyObject* self, void*)
	{
		T* pThis = static_cast<T*>(self);
		return PyLong_FromLongLong(pThis->*P);
	}

	template <int64_t T::*P>
	static int PyCFuncSetInt64(PyObject* self, PyObject* v, void*)
	{
		int64_t tmp = PyLong_AsLongLong(v);
		if (tmp == -1 && PyErr_Occurred())
			return -1;

		T* pThis = static_cast<T*>(self);
		pThis->*P = tmp;
		return 0;
	}
};


// -------------------------------------------------------------
// A pythonized class.  Use with impunity
template <class T>
struct PyXObject : public PyObject, public PyXThunker<T>
{
	PyXObject(PyTypeObject* type = const_cast<PyTypeObject*>(GetType()))
	{
		PyObject_Init(this, type);
	}

	typedef T _ClassType;

	static void _Dealloc(PyObject* self)
	{
		delete (T*)self;
	}

	static PyObject* _Repr(PyObject* self)
	{
		return ((T*)self)->Repr();
	}

	static PyObject* _Str(PyObject* self)
	{
		return ((T*)self)->Str();
	}

	static const PyMethodDef* _MethodDefs() { return NULL; }
	static const PyMemberDef* _MemberDefs() { return NULL; }
	static const PyGetSetDef* _GetSetDefs() { return NULL; }

	static const PyTypeObject* GetType()
	{
		static PyTypeObject type =
		{
			PyVarObject_HEAD_INIT(&PyType_Type, 0)
			T::_ClassName(),
			0,
			0,
			T::_Dealloc,        /* tp_dealloc */
			0,                  /* tp_print */
			0,                  /* tp_getattr */
			0,                  /* tp_setattr */
			0,                  /* tp_compare */
			0,                  /* tp_repr */
			0,                  /* tp_as_number */
			0,                  /* tp_as_sequence */
			0,                  /* tp_as_mapping */
			0,                  /* tp_hash */
			0,					/* tp_call */
			0,                  /* tp_str */
			PyObject_GenericGetAttr,        /* tp_getattro */
			PyObject_GenericSetAttr,        /* tp_setattro */
			0,                  /* tp_as_buffer */
			Py_TPFLAGS_DEFAULT, /* tp_flags */
			0,				    /* tp_doc */
			0,                  /* tp_traverse */
			0,                  /* tp_clear */
			0,                  /* tp_richcompare */
			0,					/* tp_weaklistoffset */
			0,                  /* tp_iter */
			0,                  /* tp_iternext */
			const_cast<PyMethodDef*>(T::_MethodDefs()),      /* tp_methods */
			const_cast<PyMemberDef*>(T::_MemberDefs()),      /* tp_members */
			const_cast<PyGetSetDef*>(T::_GetSetDefs()),      /* tp_getset */
			0,                  /* tp_base */
			0,                  /* tp_dict */
			0,                  /* tp_descr_get */
			0,                  /* tp_descr_set */
			0,                  /* tp_dictoffset */
			0,                  /* tp_init */
			0,                  /* tp_alloc */
			0,					/* tp_new */
			nullptr,                  /* tp_free */
		};

		static bool postInitialized = false;
		if (!postInitialized)
		{
			T::InitializeType(&type);
			postInitialized = true;
		}

		return &type;
	}

	static void InitializeType(PyTypeObject* type){}

};


// -------------------------------------------------------------
// An improved version.  Uses a different type sceme to inherit from "object"
// It achieves this using PyXTypeObject which has sensible defaults for most stuff.
template <class T>
struct PyXObject2 : public PyObject, public PyXThunker<T>
{
	typedef T _ClassType;

	//Define this object's type
	static PyTypeObject* GetType()
	{
		static PyXTypeObject type(T::_ClassName());
		if (!type.IsReady()) {
			type.tp_basicsize = sizeof(T);
			type.tp_new = T::_New;
			type.tp_dealloc = T::_Dealloc;
			type.tp_methods = const_cast<PyMethodDef*>(T::_MethodDefs());
			type.tp_members = const_cast<PyMemberDef*>(T::_MemberDefs());
			type.tp_getset  = const_cast<PyGetSetDef*>(T::_GetSetDefs());
			if (!T::InitType(&type)) //type specific custom additions
				return 0;
			if (!type.Ready())
				return 0;
		}
		return &type;
	}
	static bool InitType(PyTypeObject *type) {return true;} //default implementation

	static void *_Alloc(PyTypeObject *subtype, int n = 0)
	{
		return subtype->tp_alloc(subtype, n);
	}

	static PyObject *_New(PyTypeObject *subtype, PyObject *args, PyObject *kw)
	{
		void *raw = _Alloc(subtype);
		if (!raw)
			return 0;
		T *obj = new(raw) T;
		return obj;
	}
	static void _Dealloc(PyObject *self)
	{
		T* obj = static_cast<T*>(self);
		obj->~T();
		self->ob_type->tp_free(self);
	}

	static const PyMethodDef* _MethodDefs() { return NULL; }
	static const PyMemberDef* _MemberDefs() { return NULL; }
	static const PyGetSetDef* _GetSetDefs() { return NULL; }
};


template <class T>
struct SequenceObject
{
	static PySequenceMethods* GetSequenceMethods()
	{
		static PySequenceMethods seqmeth = {
            T::Seq_Length == Seq_Length_ ? NULL : Seq_Length_,
			T::Seq_Concat_ == Seq_Concat_ ? NULL : Seq_Concat_,
			T::Seq_Repeat_ == Seq_Repeat_ ? NULL : Seq_Repeat_,
			Seq_Item_, // mandatory
			T::Seq_Slice_ == Seq_Slice_ ? NULL : Seq_Slice_,
			T::Seq_AssItem_ == Seq_AssItem_ ? NULL : Seq_AssItem_,
			T::Seq_AssSlice_ == Seq_AssSlice_ ? NULL : Seq_AssSlice_,
			T::Seq_Contains_ == Seq_Contains_ ? NULL : Seq_Contains_,
		};

		return &seqmeth;
	}

	int Seq_Length(){return 0;}
	static ssize_t Seq_Length_(PyObject* self)
	{
		return ((T*)self)->Seq_Length();
	}

	PyObject* Seq_Concat(PyObject* o) { return NULL; }
	static PyObject* Seq_Concat_(PyObject* self, PyObject* o)
	{
		return ((T*)self)->Seq_Concat(o);
	}

	PyObject* Seq_Repeat(int i) { return NULL; }
	static PyObject* Seq_Repeat_(PyObject* self, ssize_t i)
	{
		return ((T*)self)->Seq_Repeat(i);
	}

	static PyObject* Seq_Item_(PyObject* self, ssize_t i)
	{
		return 0;//((T*)self)->Seq_Item(i);
	}

	PyObject* Seq_Slice(int i, int j) { return NULL; }
	static PyObject* Seq_Slice_(PyObject* self, ssize_t i, ssize_t j)
	{
		return ((T*)self)->Seq_Slice(i, j);
	}

	int Seq_AssItem(int i, PyObject* o) { return -1; }
	static int Seq_AssItem_(PyObject* self, ssize_t i, PyObject* o)
	{
		return ((T*)self)->Seq_AssItem(i, o);
	}

	int Seq_AssSlice(int i,  int j, PyObject* o) { return -1; }
	static int Seq_AssSlice_(PyObject* self, ssize_t i,  ssize_t j, PyObject* o)
	{
		return ((T*)self)->Seq_AssSlice(i, j, o);
	}

	int Seq_Contains(PyObject* o) { return -1; }
	static int Seq_Contains_(PyObject* self, PyObject* o)
	{
		return ((T*)self)->Seq_Contains(o);
	}
};



template <class T>
struct PyWeakrefSupport
{
	PyObject* mWeakrefList;

	PyWeakrefSupport() :
		mWeakrefList(NULL)
	{
	}

	~PyWeakrefSupport()
	{
		if (mWeakrefList != NULL)
			PyObject_ClearWeakRefs((T*)this);
	}
};




// Ad-hoc doubly linked list
template <class LI>
struct DList
{
	typedef typename LI::_TClass T;
	typedef DList<LI> _Class;
	
	DList() :
		mFirst(NULL),
		mSize(0)
	{
	}

	void Lock(){}
	void Unlock(){}

	PyObject* GetList()
	{
		PyObject* list = PyList_New(mSize);
		if (list == NULL)
			return NULL;

		int i = 0;
		for (typename LI::_TClass* o = mFirst; o != NULL; o = o->LI::mNext)
		{
			PyList_SET_ITEM(list, i++, o);
			Py_INCREF(o);
		}

		return list;
	}

	// For python get map
	template <int offs>
	static PyObject* _getList(PyObject* self, void*)
	{
		_Class* pThis = (_Class*)((char*)self+offs);
		return pThis->GetList();
	}


	T* mFirst;
	int mSize;
};


#endif
