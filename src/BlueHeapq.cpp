// Copyright © 2015 CCP ehf.

#include "StdAfx.h"
#include "BlueHeapq.h"


#if BLUE_WITH_PYTHON

namespace
{

//heapq stuff, here for the time being
PyObject *PyHeapPush(PyObject *self, PyObject *args);
PyObject *PyHeapPop(PyObject *self, PyObject *args);
PyObject *PyHeapify(PyObject *self, PyObject *args);
PyObject *PyHeapReplace(PyObject *self, PyObject *args);
PyObject *PyHeapSort(PyObject *self, PyObject *args);
PyObject *PyHeapCheck(PyObject *self, PyObject *args);

PyMethodDef heapqmethods[] = {
	{
		"heappush", PyHeapPush, METH_VARARGS, 
		"push an element onto heap\n"
		":param heap: heap list\n"
		":type heap: list\n"
		":param element: element to push\n"
		":type element: any\n"
		":param cmp: optional compare function\n"
		":type cmp: Optional[(any, any)->int]"
	},
	{
		"heappop", PyHeapPop, METH_VARARGS, 
		"pop an element off heap\n"
		":param heap: heap list\n"
		":type heap: list\n"
		":param cmp: optional compare function\n"
		":type cmp: Optional[(any, any)->int]" 
	},
	{
		"heapify", PyHeapify, METH_VARARGS, 
		"put list inn heap order\n"
		":param heap: heap list\n"
		":type heap: list\n"
		":param cmp: optional compare function\n"
		":type cmp: Optional[(any, any)->int]" 
	},
	{
		"heapreplace", PyHeapReplace, METH_VARARGS, 
		"pop and subsequently push an item\n"
		":param heap: heap list\n"
		":type heap: list\n"
		":param element: element to push\n"
		":type element: any\n"
		":param cmp: optional compare function\n"
		":type cmp: Optional[(any, any)->int]" 
	},
	{
		"heapsort", PyHeapSort, METH_VARARGS, 
		"sort a list in heap order (large to small)\n"
		":param heap: heap list\n"
		":type heap: list\n"
		":param cmp: optional compare function\n"
		":type cmp: Optional[(any, any)->int]" 
	},
	{
		"heapcheck", PyHeapCheck, METH_VARARGS, 
		"Test the heap property\n"
		":param heap: heap list\n"
		":type heap: list\n"
		":param cmp: optional compare function\n"
		":type cmp: Optional[(any, any)->int]" 
	},
	{0}
};

//heapq methods
inline bool CallCompare(bool &r, PyObject *cmp, PyObject *a, PyObject *b)
{
	PyObject *res = PyObject_CallFunctionObjArgs(cmp, a, b, 0);
	if (!res) return false;
	int t = PyObject_IsTrue(res);
	Py_DECREF(res);
	if (t<0) return false;
	r = !!t;
	return true;
}

static bool HeapPercolateUp(PyListObject *l, Py_ssize_t ci, PyObject *cmp = 0)
{
	PyObject *element = PyList_GET_ITEM(l, ci);
	if (!cmp) {
		while (ci>0) {
			Py_ssize_t pi = (ci-1)>>1;
			PyObject *parent = PyList_GET_ITEM(l, pi);
			int cmp = PyObject_RichCompareBool(parent, element, Py_LE);
			if (cmp<0)
				return false; //exception
			if (cmp)
				break; //parent is less or equal to child, break.
			PyList_SET_ITEM(l, ci, parent);
			ci = pi;
		}
	} else {
		while (ci>0) {
			Py_ssize_t pi = (ci-1)>>1;
			PyObject *parent = PyList_GET_ITEM(l, pi);
			bool t;
			if (!CallCompare(t, cmp, parent, element)) return 0;
			if (t)
				break; //heap property satisfied.
			PyList_SET_ITEM(l, ci, parent);
			ci = pi;
		}
	}
	PyList_SET_ITEM(l, ci, element);
	return true;
}


bool HeapPercolateDown(PyListObject *l, Py_ssize_t size, Py_ssize_t pi, PyObject *cmp = 0)
{
	PyObject *element = PyList_GET_ITEM(l, pi);
	if (!cmp) {
		while (pi < size/2) {
			Py_ssize_t ci = 2*pi+1;
			PyObject *child = PyList_GET_ITEM(l, ci);
			if (ci+1 < size) {
				//pick smaller child
				PyObject *child1 = PyList_GET_ITEM(l, ci+1);
				int cmp = PyObject_RichCompareBool(child, child1, Py_LE);
				if (cmp<0)
					return false;
				if (!cmp){
					//child1 is smaller
					child = child1;
					ci++;
				}
			}
			int cmp = PyObject_RichCompareBool(element, child, Py_LE);
			if (cmp<0)
				return false;
			if (cmp)
				break; //element is less or equal to child.
			PyList_SET_ITEM(l, pi, child);
			pi = ci;
		}
	} else {
		while (pi < size/2) {
			Py_ssize_t ci = 2*pi+1;
			PyObject *child = PyList_GET_ITEM(l, ci);
			bool t;
			if (ci+1 < size) {
				//pick child according to heap property
				PyObject *child1 = PyList_GET_ITEM(l, ci+1);
				if (!CallCompare(t, cmp, child, child1)) return 0;
				if (!t){
					//must pick other child
					child = child1;
					ci++;
				}
			}
			if (!CallCompare(t, cmp, element, child)) return 0;
			if (t)
				break; //heap property satisfied
			PyList_SET_ITEM(l, pi, child);
			pi = ci;
		}
	}
	PyList_SET_ITEM(l, pi, element);
	return true;
}


PyObject *PyHeapPush(PyObject *self, PyObject *args)
{
	PyObject *list, *element, *cmp=0;
	if (!PyArg_ParseTuple(args, "OO|O", &list, &element, &cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), nullptr;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), nullptr;
	
	if (PyList_Append(list, element))
		return 0;
	if (!HeapPercolateUp((PyListObject*)list, PyList_GET_SIZE(list)-1, cmp))
		return 0;
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyHeapPop(PyObject *self, PyObject *args)
{
	PyObject *list, *cmp=0;
	if (!PyArg_ParseTuple(args, "O|O", &list, cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), nullptr;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), nullptr;
	
	Py_ssize_t end = PyList_GET_SIZE(list)-1; //tail element
	PyObject *result;
	if (end>=0) {//list is one or more elements
		//get head element
		result = PyList_GET_ITEM(list, 0);
		Py_INCREF(result);
		if (end) { //more than one element
			//swap first and last
			PyList_SET_ITEM(list, 0, PyList_GET_ITEM(list, end));
			PyList_SET_ITEM(list, end, result);
			//delete last element
			PyList_SetSlice(list, end, end+1, 0);
			if (!HeapPercolateDown((PyListObject*)list, end, 0, cmp)) {
				Py_DECREF(result);
				return 0;
			}
		} else  //we had only one element
			PyList_SetSlice(list, 0, 1, 0); //delete the element
	} else
		return PyErr_SetString(PyExc_IndexError, "heap is empty"), nullptr;
	return result;
}


PyObject *PyHeapify(PyObject *self, PyObject *args)
{
	PyObject *list, *cmp=0;
	if (!PyArg_ParseTuple(args, "O|O", &list, &cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), nullptr;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), nullptr;

	Py_ssize_t s = PyList_GET_SIZE(list);
	for(Py_ssize_t i = s/2-1; i>=0; i--)
		if (!HeapPercolateDown((PyListObject*)list, s, i, cmp))
			return 0;
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyHeapReplace(PyObject *self, PyObject *args)
{
	PyObject *list, *element, *cmp=0;
	if (!PyArg_ParseTuple(args, "OO|O", &list, &element, &cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), nullptr;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), nullptr;
	
	Py_ssize_t s = PyList_GET_SIZE(list);
	if (!s)
		return PyErr_SetString(PyExc_IndexError, "heap is empty"), nullptr;

	//swap the stuff.
	PyObject *result = PyList_GET_ITEM(list, 0); //take the list's reference of the head
	PyList_SET_ITEM(list, 0, element);
	Py_INCREF(element);
	if (!HeapPercolateDown((PyListObject*)list, s, 0, cmp)) {
		Py_DECREF(result);
		return 0;
	}
	return result;
}

	
PyObject *PyHeapSort(PyObject *self, PyObject *args)
{
	PyObject *list, *cmp=0;
	if (!PyArg_ParseTuple(args, "O|O", &list, &cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), nullptr;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), nullptr;
	
	Py_ssize_t s = PyList_GET_SIZE(list);
	for(Py_ssize_t i = s-1; i>0; i--) {
		//swap head and tail of current list
		PyObject *tmp = PyList_GET_ITEM(list, i);
		PyList_SET_ITEM(list, i, PyList_GET_ITEM(list, 0));
		PyList_SET_ITEM(list, 0, tmp);
		if (!HeapPercolateDown((PyListObject*)list, i, 0, cmp))
			return 0;
	}
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyHeapCheck(PyObject *self, PyObject *args)
{
	PyObject *list, *cmp=0;
	if (!PyArg_ParseTuple(args, "O|O", &list, &cmp))
		return 0;
	if (!PyList_Check(list))
		return PyErr_SetString(PyExc_TypeError, "list object expected"), nullptr;
	if (cmp && !PyCallable_Check(cmp))
		return PyErr_SetString(PyExc_TypeError, "calalble object expected"), nullptr;
	
	for (Py_ssize_t i = PyList_GET_SIZE(list)-1; i>0; i--) {
		Py_ssize_t pi = (i-1)>>1;
		bool ok;
		if (cmp) {
			if (!CallCompare(ok, cmp, PyList_GET_ITEM(list, pi), PyList_GET_ITEM(list, i))) return 0;
		} else {
			int r = PyObject_RichCompareBool(PyList_GET_ITEM(list, pi), PyList_GET_ITEM(list, i), Py_LE);
			if (r<0) return 0;
			ok = !!r;
		}
		if (!ok) { //heap property not satisfied.
			Py_INCREF(Py_False);
			return Py_False;
		}
	}
	Py_INCREF(Py_True);
	return Py_True;
}

}

PyMODINIT_FUNC InitHeapq(void)
{
    static struct PyModuleDef moduleDef = {
        PyModuleDef_HEAD_INIT,
        "blue.heapq",
        "",
        -1,
        heapqmethods,
    };
    auto module = PyModule_Create(&moduleDef);
    if ( ! module ) {
        CCP_LOGERR("Failed to create blue.heapq module");
		return nullptr;
    }

	return module;
}

#endif
