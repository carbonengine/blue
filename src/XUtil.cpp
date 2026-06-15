// Copyright © 2013 CCP ehf.

#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "BluePython.h"
#if CCP_STACKLESS
#include "PyRowset.h"
#endif

// The functions in here were moved out of BluePython.cpp to reduce the size
// of that file. The functions should probably be moved out of BluePyOS - I
// don't see any particular reason they should be there, but I'm taking it
// one step at a time.

PyObject *BluePyOS::PyXUtil_Filter(PyObject *args)
{
#if CCP_STACKLESS

	// takes in the set to filter, the list with the ordinals (indices) to filter by, a corresponding list of values to filter on, and a set to put the results in.
    PyObject* rows;
    PyObject* indices;
    PyObject* condvalues;
    PyObject* retset;
	struct {
		Py_ssize_t dataIdx;
		int64_t cond;
		DBRow::datatypes type;
	} myvec[4];
    
    if (!PyArg_ParseTuple(args, "O!O!O!O!:XUtil_Filter", &PySet_Type, &rows, &PyList_Type, &indices, &PyList_Type, &condvalues, &PySet_Type, &retset))
        return NULL;
        
    // special case if list is empty or if no conditions
    if (PySet_Size(rows) == 0)
    {
        Py_INCREF(retset);
        return retset;
    }
    
    // Error out if indices and condvalues don't match up or if we get more then we're ready for
    if (PyList_GET_SIZE(indices) != PyList_GET_SIZE(condvalues))
        return PyErr_SetString(PyExc_RuntimeError, "XUtil_Filter: Length of index list value list don't match up"), nullptr;

	//Get initial dude.  We assume all of them are the same, so we use this for info on how to index the data
    PyObject *first;
    Py_ssize_t setpos = 0;
	Py_hash_t hash;
    _PySet_NextEntry(rows, &setpos, &first, &hash);
    if (!PyObject_IsInstance(first, (PyObject*)DBRow::GetType()))
		return PyErr_Format(PyExc_RuntimeError, "XUtil_Filter: Row in rowset must be of type DBRow");

	// build standard vectors of these guys so we don't have to use python getters inside the loop.
	int numConds = 0;
	for (Py_ssize_t j = 0; j < PyList_GET_SIZE(indices); j++)
    {
		//ignore NULL conditions
		PyObject *idxO = PyList_GET_ITEM(indices, j);
		PyObject *cndO = PyList_GET_ITEM(condvalues, j);
		if (idxO == Py_None || cndO == Py_None)
			continue;
		
		if (numConds >= sizeof(myvec) / sizeof(myvec[0]))
			 return PyErr_SetString(PyExc_RuntimeError, "XUtil_Filter: Too many conditions to Filter on."), nullptr;
		
        int idx = int( PyLong_AS_LONG(idxO) );
        Py_ssize_t nullOffset; //throwaway
		myvec[numConds].dataIdx = static_cast<DBRow*>( first )->GetDataOffset(idx, myvec[numConds].type, nullOffset);
		if (myvec[numConds].dataIdx<0)
			return PyErr_SetString(PyExc_ValueError, "invalid column"), nullptr;
		myvec[numConds].cond = PyLong_AsLongLong(cndO);
		if (myvec[numConds].cond == (int64_t)(-1) && PyErr_Occurred())
			return PyErr_SetString(PyExc_OverflowError, "XUtil_Filter: 64 bit value overflowed! "), nullptr;
		++numConds;
    }
	
     // go through the whole set and check each condition, continuing on any failure
    setpos = 0;
    PyObject* row;
    while (_PySet_NextEntry(rows, &setpos, &row, &hash))
    {
        //  Error out if passed non-DBRows
        if (!PyObject_IsInstance(row, (PyObject*)DBRow::GetType()))
            return PyErr_Format(PyExc_RuntimeError, "XUtil_Filter: Row in rowset must be of type DBRow");
            
        // check each condition before adding to return list
        bool include = true;
        for (long j = 0; j < numConds; j++)
        {  
            int64_t value;
			if ( ! static_cast<DBRow*>( row )->GetValue(myvec[j].type, myvec[j].dataIdx, value) )
                return PyErr_SetString(PyExc_RuntimeError, "XUtil_Filter: Unsupported data type or index out of range for DBRow"), nullptr;
            if (myvec[j].cond != value)
            {
                include = false;
                break;
            }
        }
        if (!include)
            continue;
        // All conditions hold, add to return set
        if ( 0 != PySet_Add(retset, row) )
            return 0; 
    }

    Py_INCREF(retset);
    return retset;

#else

	return nullptr;

#endif
}


//--------------------------------------------------------------------
PyObject *BluePyOS::PyXUtil_Index(PyObject *args)
{
#if CCP_STACKLESS

	/* C++ version of dbutil.Index()

		def Index(rows, keyName):

		d = {}
		c = 0
	    
		if (rows):
			keyIdx = rows[0].__keys__.index(keyName)
	    
		for row in rows:
			d[row[keyIdx]] = row
			c += 1
			if c == 25000:
				c = 0
				blue.pyos.BeNice()

		return d
	*/
	
	PyObject* rows;
	PyObject* keyName;
	PyObject* dict;

	if (!PyArg_ParseTuple(args, "O!O!O!:XUtil_Index", &PyList_Type, &rows, &PyUnicode_Type, &keyName, &PyDict_Type, &dict))
		return NULL;

	// To get the offset of the key column, we need at least one row in the rowset.
	// If the rowset is empty, we simply return the empty dict.
	if (PyList_GET_SIZE(rows) == 0)
	{
		Py_INCREF(dict);
		return dict;
	}

	PyObject* row = PyList_GET_ITEM(rows, 0);
	if (!PyObject_IsInstance(row, (PyObject*)DBRow::GetType()))
		return PyErr_Format(PyExc_RuntimeError, "XUtil_Index: Row in rowset must be of type DBRow");

	PyObject* keys = PyObject_GetAttrString(row, "__keys__");
	if (!keys)
		return 0;
		
	Py_ssize_t keyColumn = PySequence_Index(keys, keyName);
	Py_DECREF(keys);
	if (keyColumn == -1)
		return PyErr_Format(PyExc_RuntimeError, "XUtil_Index: Invalid column name '%s'.", PyUnicode_AsUTF8(keyName));

	// See if we can yield
	float timeSlice = 0.001f * mBeNiceSlice;
	bool yield = CanYield();

	// Go through the whole list carefully. It should not be possible to blow up this
	// code, except if it's mutated during the Yield call.
	for (Py_ssize_t i = 0; i < PyList_GET_SIZE(rows); i++)
	{
		PyObject* row = PyList_GetItem(rows, i);
		if (!row)
			return 0;
		if (!PyObject_IsInstance(row, (PyObject*)DBRow::GetType()))
			continue; // Should we report this?

		PyObject* keyObject = static_cast<DBRow*>( row )->SequenceGet(static_cast<DBRow*>( row ), keyColumn);
		if (!keyObject)
			return 0;
		int fail = PyDict_SetItem(dict, keyObject, row);
		Py_DECREF(keyObject);
		if (fail)
			return 0;
		if (yield && (i+1) % 25000 == 0)
		{
			// "Inline" BeNice() code.
			float elapsed = GetTaskletTimer()->GetElapsed();
			if (elapsed >= timeSlice) 
			{
				if( !Yield() )
				{
					return 0;
				}
			}
		}				
	}
	
	Py_INCREF(dict);
	return dict;

#else

	return nullptr;

#endif
}

#endif
