// Copyright © 2003 CCP ehf.

/* 
	*************************************************************************

	BlueSmartVar.h

	Project:   Blue

	Description:   

		Simple wrapping for arbitrary objects using Pythons CObject type

	Dependencies:

		Python


	*************************************************************************
*/
#ifndef _BLUEPYSWRAP_H
#define _BLUEPYSWRAP_H

#if BLUE_WITH_PYTHON

#include <Python.h>

#if PY_VERSION_HEX >= 0x02070000

//////////////////////////////////////////////////////////////////////////
// This is for Python 2.7

template<class T>
class BluePySWrap : public T {
	typedef BluePySWrap<T> Class;
public:
	typedef T Base;
	
	static PyObject *Wrap(const T &payload);


	static Class *Unwrap(PyObject *wrapped){
		if (!PyCapsule_CheckExact(wrapped) || PyCapsule_GetDestructor(wrapped) != Destructor)
			return 0;
		return reinterpret_cast<Class*>(PyCapsule_GetPointer(wrapped, 0));
	}

private:
	BluePySWrap(const T &payload) : T(payload) {}
	static void Destructor(PyObject *destructor);

	// no copying!
	BluePySWrap(const BluePySWrap &other);
	BluePySWrap &operator=(const BluePySWrap &other);
};


// A simple class that wraps the type in a struct.  Useful if we want to wrap
// a non-struct, like integers and the like.
template<typename T>
struct Structify {
	Structify<T>(const T& item) : mItem(item) {}
	operator T () const {return mItem;}
	T mItem;
};


template<class T>
PyObject *BluePySWrap<T>::Wrap(const T &payload)
{
	Class *wrapper = new Class(payload);
	if (!wrapper)
		return PyErr_NoMemory();
	
	/* it would be cool if we could get the name of this type here */
	PyObject *result = PyCapsule_New((void*)wrapper, 0, Destructor);
	if (!result) {
		delete wrapper;
		return 0;
	}
	return result;
}

//It's imperative that there be only one instance of this function (i.e. the linker does
//its job) since we use it for type identification.
template<class T>
void BluePySWrap<T>::Destructor(PyObject *obj)
{
	Class* cobj = reinterpret_cast<Class*>(PyCapsule_GetPointer(obj, 0));
	delete cobj;
}


#else

//////////////////////////////////////////////////////////////////////////
// This is for Python 2.6

template<class T>
class BluePySWrap : public T {
	typedef BluePySWrap<T> Class;
public:
	typedef T Base;

	static PyObject *Wrap(const T &payload);

	static Class *Unwrap(PyObject *wrapped){
		if (!PyCObject_Check(wrapped) || PyCObject_GetDesc(wrapped) != (void*)DeletionThunker)
			return 0;
		return reinterpret_cast<Class*>(PyCObject_AsVoidPtr(wrapped));
	}

private:
	BluePySWrap(const T &payload) : T(payload) {}
	static void DeletionThunker(void *obj, void *cookie);

	// no copying!
	BluePySWrap(const BluePySWrap &other);
	BluePySWrap &operator=(const BluePySWrap &other);
};


// A simple class that wraps the type in a struct.  Useful if we want to wrap
// a non-struct, like integers and the like.
template<typename T>
struct Structify {
	Structify<T>(const T& item) : mItem(item) {}
	operator T () const {return mItem;}
	T mItem;
};


template<class T>
PyObject *BluePySWrap<T>::Wrap(const T &payload)
{
	Class *wrapper = new Class(payload);
	if (!wrapper)
		return PyErr_NoMemory();

	PyObject *result = PyCObject_FromVoidPtrAndDesc((void*)wrapper, DeletionThunker,
		DeletionThunker);
	if (!result) {
		delete wrapper;
		return 0;
	}
	return result;
}

//It's imperative that there be only one instance of this function (i.e. the linker does
//its job) since we use it for type identification.
template<class T>
void BluePySWrap<T>::DeletionThunker(void *obj, void *cookie)
{
	CCP_ASSERT(cookie == (void*)DeletionThunker);
	delete reinterpret_cast<Class*>(obj);
}

#endif

#endif

#endif //_BLUEPYSWRAP_H