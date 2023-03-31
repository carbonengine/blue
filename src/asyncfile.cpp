// This file implements the asyncfile module, which uses stacklessio
#include "StdAfx.h"
#include "stacklessio.h"

#ifdef SLSOCKET

// AsyncFile object
using Ccp::PyObjectPtr;
using Ccp::PyError;
using Ccp::Win32Error;
using Ccp::Handle;


struct PyAsyncFileObject
{	
	PyObject_HEAD
	HANDLE f_fp;
	PyObject *f_name;
	PyObject *f_mode;
	PyObject *weakreflist;
};

extern PyTypeObject PyAsyncFile_Type ;
#define PyAsyncFile_Check(op) PyObject_TypeCheck(op, &PyAsyncFile_Type)


// Special exception translation function which churns out IOErrors.
static PyObject *TranslateException(const std::exception &e, const PyAsyncFileObject *f)
{
	if (dynamic_cast<const Win32Error *>(&e)) {
		const Win32Error &we = dynamic_cast<const Win32Error&>(e);
		PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_IOError, we.GetCode(), f->f_name);
	} else
		Ccp::PyErrFromException(e);
	return 0;
}


//Completion for a ReadFile() call
class IORead : public IOOverlapped
{
public:
	IORead(__int64 size, PyObject *fn) : mFileName(fn, true),
		mResult(Ccp::PyCheck(PyString_FromStringAndSize(0, (Py_ssize_t)size)), false)
	{}
	void *Buffer() const {return PyString_AS_STRING(mResult.p);}

	//Getting the value
	PyObject *GetResult() {
		WaitForCompletion();
		if (GetErrorCode() == ERROR_SUCCESS) {
			if (GetBytesTransfered() != PyString_GET_SIZE(mResult.p)) {
				int fail = _PyString_Resize(& mResult.p, GetBytesTransfered());
				if (fail)
					throw PyError();
			}
			return mResult.Detach();
		} else {
			PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_IOError, GetErrorCode(), mFileName);
			throw PyError();
		}
	}
		
private:
	PyObjectPtr mResult;
	PyObjectPtr mFileName;
};


//Completion for a WriteFile call
class IOWrite : public IOOverlapped
{
public:
	IOWrite(PyObject *fn, PyObject *data) :
		mFileName(fn, true), mData(data, true)
	{}
	void *Buffer() const {return PyString_AS_STRING(mData.p);}
	Py_ssize_t BufLen() const {return PyString_GET_SIZE(mData.p);}
	
	//Getting the value
	PyObject *GetResult() {	
		if (GetErrorCode() == ERROR_SUCCESS) {
			if (GetBytesTransfered() != BufLen()) {
				PyErr_Format(PyExc_IOError, "wrote only %d of %d bytes", GetBytesTransfered(), BufLen());
				throw PyError();
			}
		} else {
			PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_IOError, GetErrorCode(), mFileName);
			throw PyError();
		}
		Py_RETURN_NONE;
	}
	
private:
	PyObjectPtr mFileName;
	PyObjectPtr mData;
};



static PyObject *asfile_close(PyAsyncFileObject *f);


static PyObject *
fill_file_fields(PyAsyncFileObject *f, PyObject *name, char *mode)
{
	assert(name != NULL);
	assert(f != NULL);
	assert(PyAsyncFile_Check(f));
	assert(f->f_fp == INVALID_HANDLE_VALUE);

	Py_DECREF(f->f_name);
	Py_DECREF(f->f_mode);
	
    Py_INCREF(name);
    f->f_name = name;

	f->f_mode = PyString_FromString(mode);
	
	if (f->f_mode == NULL)
		return NULL;
	return (PyObject *) f;
}

bool decode_mode(bool &read, bool&write, bool &end, bool &truncate, const char *mode)
{
	size_t len=strlen(mode);
	if (len<1 || len > 2)
		goto ERR;
	read = write = end = truncate = false;
	switch (mode[0]) {
	case 'r': read = true; break;
	case 'w': write = true; break;
	case 'a': write = true; end = true; break;
	default: goto ERR;
	};
	if (len==2) {
		if (mode[1] != '+')			
			goto ERR;
		if (write)
			truncate = true;
		read = write = true;
	}
	return true;
ERR:
	PyErr_Format(PyExc_IOError, "invalid mode: %s", mode);
	return false;
}

class OpenResult : public IOWorker
{
public:
	OpenResult(const wchar_t *fname, DWORD access, DWORD share, DWORD creation, DWORD flags, bool tail)
		: mFname(fname), mAccess(access), mShare(share), mCreation(creation), mFlags(flags), mTail(tail)
	{
		mErrorCode = 0;
	}

	ULONG GetExecuteFlag() const {return WT_EXECUTELONGFUNCTION;}

	void ThreadFunc()
	{
		try {
			Handle hFile(CreateFileW(PyUnicode_AS_UNICODE(mFname), mAccess, mShare, 0, mCreation, mFlags, 0));
			if (!hFile.Valid())
				throw Win32Error();

			if (mTail) {
				LARGE_INTEGER li;
				li.QuadPart = 0;
				BOOL ok = SetFilePointerEx(hFile.Peek(), li, 0, FILE_END);
				if (!ok)
					throw Win32Error();
			}
					
			//let us be handled by the the thread pool.
			IOOverlapped::Bind(hFile.Peek());
			mHandle.Swap(hFile);
		} catch (const Ccp::Win32Error &e) {
			mErrorCode = e.GetCode();
		}
	}

	HANDLE GetHandle() {
		return mHandle.Detach();
	}

	const wchar_t *mFname;
	DWORD mAccess, mShare, mCreation, mFlags;
	bool mTail;
	Handle mHandle;
	DWORD mErrorCode;
};

		
static PyObject *
open_the_file(PyAsyncFileObject *f)
{
	assert(f != NULL);
	assert(PyAsyncFile_Check(f));
	assert(f->f_name != NULL);
	assert(f->f_fp == INVALID_HANDLE_VALUE);

	try {
		bool read, write, end, trunc;
		if (!decode_mode(read, write, end, trunc, PyString_AS_STRING(f->f_mode)))
			return 0;

		DWORD access = (read?GENERIC_READ:0)|(write?GENERIC_WRITE:0);
		DWORD share  = (read?FILE_SHARE_READ:0)|(write?FILE_SHARE_WRITE:0);
		DWORD creation;
		if (trunc)
			creation = CREATE_ALWAYS;
		else if (write)
			creation = OPEN_ALWAYS;
		else
			creation = OPEN_EXISTING;
		const DWORD flags = FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN;

		IOPtr<OpenResult> result(new OpenResult(
			PyUnicode_AS_UNICODE(f->f_name), access, share, creation, flags, end && !trunc));

		result->ExecuteAndWait();
		if (result->mErrorCode)
			throw Ccp::Win32Error(result->mErrorCode);

		f->f_fp = result->GetHandle();
		return (PyObject *)f;
	} catch (const std::exception &e) {
		return TranslateException(e, f);
	}
}

static PyObject *
asfile_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *self;
	static PyObject *not_yet_string;

	assert(type != NULL && type->tp_alloc != NULL);

	if (not_yet_string == NULL) {
		not_yet_string = PyString_FromString("<uninitialized asyncfile>");
		if (not_yet_string == NULL)
			return NULL;
	}

	self = type->tp_alloc(type, 0);
	if (self != NULL) {
		((PyAsyncFileObject *)self)->f_fp = INVALID_HANDLE_VALUE;
		/* Always fill in the name and mode, so that nobody else
		   needs to special-case NULLs there. */
		Py_INCREF(not_yet_string);
		((PyAsyncFileObject *)self)->f_name = not_yet_string;
		Py_INCREF(not_yet_string);
		((PyAsyncFileObject *)self)->f_mode = not_yet_string;
		((PyAsyncFileObject *)self)->weakreflist = NULL;
	}
	return self;
}

static int
asfile_init(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyAsyncFileObject *foself = (PyAsyncFileObject *)self;
	int ret = 0;
	static char *kwlist[] = {"name", "mode", "buffering", 0};
	char *mode = "r";
	int bufsize = -1;
	PyObject *o_name = 0;

	assert(PyAsyncFile_Check(self));

	/* rexec.py can't stop a user from getting the file() constructor --
	   all they have to do is get *any* file object f, and then do
	   type(f).  Here we prevent them from doing damage with it. */
	if (PyEval_GetRestricted()) {
		PyErr_SetString(PyExc_IOError,
		"file() constructor not accessible in restricted mode");
		return 0;
	}

	if (foself->f_fp != INVALID_HANDLE_VALUE) {
		/* Have to close the existing file first. */
		PyObject *closeresult = asfile_close(foself);
		if (closeresult == NULL)
			return -1;
		Py_DECREF(closeresult);
	}

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!|si:asyncfile",
					kwlist, &PyBaseString_Type, &o_name, &mode, &bufsize))
		return -1;
	o_name = PyUnicode_FromObject(o_name);
	if (!o_name)
		return -1;

	PyObject *ok= fill_file_fields(foself, o_name, mode);
	Py_DECREF(o_name);
			
	if (!ok || !open_the_file(foself))
		return -1;
	return 0;
}

static PyObject *
err_closed(void)
{
	PyErr_SetString(PyExc_ValueError, "I/O operation on closed asyncfile");
	return NULL;
}

static PyObject *
asfile_flush(PyAsyncFileObject *f)
{
	if (f->f_fp == INVALID_HANDLE_VALUE)
		return err_closed();
	BOOL ok = FlushFileBuffers(f->f_fp);
	if (!ok) {
		PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_IOError, 0, f->f_name);
		return 0;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
asfile_close(PyAsyncFileObject *f)
{
	if (f->f_fp != INVALID_HANDLE_VALUE) {
		BOOL ok = CloseHandle(f->f_fp);
		f->f_fp = INVALID_HANDLE_VALUE;
		if (!ok) {
			PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_IOError, 0, f->f_name);
			return 0;
		}
	}
	Py_INCREF(Py_None);
	return Py_None;
}


static void
asfile_dealloc(PyAsyncFileObject *f)
{
	if (f->weakreflist != NULL)
		PyObject_ClearWeakRefs((PyObject *) f);
	if (f->f_fp != INVALID_HANDLE_VALUE) {
		BOOL ok = CloseHandle(f->f_fp);
		if (!ok) 
			PySys_WriteStderr("close failed: [WinErr %d]\n", GetLastError()); 
	}
	Py_XDECREF(f->f_name);
	Py_XDECREF(f->f_mode);
	f->ob_type->tp_free((PyObject *)f);
}


static PyObject *
asfile_seek(PyAsyncFileObject *f, PyObject *args)
{
	int whence;
	LARGE_INTEGER offset;
	
	if (f->f_fp == INVALID_HANDLE_VALUE)
		return err_closed();
	whence = 0;
	if (!PyArg_ParseTuple(args, "L|i:seek", &offset.QuadPart, &whence))
		return NULL;

    BOOL ok = SetFilePointerEx(f->f_fp, offset, 0, whence);
	if (!ok) {
		PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_IOError, 0, f->f_name);
		return 0;
	}
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *
asfile_tell(PyAsyncFileObject *f)
{
	if (f->f_fp == INVALID_HANDLE_VALUE)
		return err_closed();
	
	LARGE_INTEGER offset, pos;
	offset.QuadPart = 0;
	BOOL ok = SetFilePointerEx(f->f_fp, offset, &pos, FILE_CURRENT);
	if (!ok) {
		PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_IOError, 0, f->f_name);
		return 0;
	}
	return PyLong_FromLongLong(pos.QuadPart);
}


static PyObject *
asfile_truncate(PyAsyncFileObject *f, PyObject *args)
{
	LARGE_INTEGER size;
	size.QuadPart = -1;
	if (f->f_fp == INVALID_HANDLE_VALUE)
		return err_closed();
	if (!PyArg_ParseTuple(args, "|K:truncate", &size.QuadPart))
		return NULL;

	LARGE_INTEGER current;
	BOOL ok = TRUE;
	if (size.QuadPart != -1) {
		//get current pos, to return to 
		LARGE_INTEGER offset;
		offset.QuadPart = 0;
		ok = SetFilePointerEx(f->f_fp, offset, &current, FILE_CURRENT);
		//move file pos
		if (ok)
			ok = SetFilePointerEx(f->f_fp, size, 0, FILE_BEGIN);
	}
	if (ok)
		ok = SetEndOfFile(f->f_fp);
	if (size.QuadPart != -1 && ok)
		ok = SetFilePointerEx(f->f_fp, current, 0, FILE_BEGIN);

	if (!ok) {
		PyErr_SetExcFromWindowsErrWithFilenameObject(PyExc_IOError, 0, f->f_name);
		return 0;
	}
	Py_INCREF(Py_None);
	return Py_None;
}


//helper function to raise an error if invoking the method fails.
static inline void OLCheck(const char *errname, BOOL ok)
{
	if (ok)
		return;
	DWORD err = GetLastError();
	if (err == ERROR_IO_PENDING)
		return;
	throw Ccp::Win32Error(err, errname);
}


static PyObject *
asfile_read(PyAsyncFileObject *f, PyObject *args)
{
	try {
		LARGE_INTEGER size, pos;
		size.QuadPart = -1;
		if (f->f_fp == INVALID_HANDLE_VALUE)
			return err_closed();
		if (!PyArg_ParseTuple(args, "|K:read", &size.QuadPart))
			return NULL;

		if (size.QuadPart == -1) {
			if (!GetFileSizeEx(f->f_fp, &size))
				throw Ccp::Win32Error();
		}

		LARGE_INTEGER offset;
		offset.QuadPart = 0;
		if (!SetFilePointerEx(f->f_fp, offset, &pos, FILE_CURRENT))
			throw Ccp::Win32Error();

		IOPtr<IORead> request;
		request = IOPtr<IORead>(new IORead(size.QuadPart, f->f_name));

		request->SetPos(pos.QuadPart);
        {
            IOHandoffHelper handoff(request);
		    BOOL ok = ReadFile(f->f_fp, request->Buffer(), (DWORD)size.QuadPart, 0, request->Overlapped());
		    OLCheck("ReadFile", ok);
            handoff.Success();
        }
		return request->GetResult();
	} catch (const std::exception &e) {
		return TranslateException(e, f);
	}
}


static PyObject *
asfile_write(PyAsyncFileObject *f, PyObject *args)
{
	try {
		if (f->f_fp == INVALID_HANDLE_VALUE)
			return err_closed();
		PyObject *data;
		if (!PyArg_ParseTuple(args, "|S:write", &data))
			return NULL;

		LARGE_INTEGER offset, pos;
		offset.QuadPart = 0;

		//get file pos
		BOOL ok = SetFilePointerEx(f->f_fp, offset, &pos, FILE_CURRENT);
		if (!ok)
			throw Ccp::Win32Error();

		IOPtr<IOWrite> request;
		request = IOPtr<IOWrite> (new IOWrite(f->f_name, data));
		Py_ssize_t len = request->BufLen();

		//Only support dword file transfers at the moment.
		DWORD dwlen = Py_SAFE_DOWNCAST(len, Py_ssize_t, DWORD);
		request->SetPos(pos.QuadPart);
        {
		    IOHandoffHelper handoff(request);
		    ok = WriteFile(f->f_fp, request->Buffer(), dwlen, 0, request->Overlapped());
		    OLCheck("WriteFile", ok);
            handoff.Success();
        }
		
		request->WaitForCompletion();
		//We have to adjust the file pointer now.
		offset.QuadPart = len;
		SetFilePointerEx(f->f_fp, offset, 0, FILE_CURRENT);
		return request->GetResult();

	} catch (const std::exception &e) {
		return TranslateException(e, f);
	}
}


static PyMethodDef asfile_methods[] = {
	{"read",      (PyCFunction)asfile_read,     METH_VARARGS, 0},
	{"write",     (PyCFunction)asfile_write,    METH_VARARGS, 0},
	{"seek",      (PyCFunction)asfile_seek,     METH_VARARGS, 0},
	{"truncate",  (PyCFunction)asfile_truncate, METH_VARARGS, 0},
	{"tell",      (PyCFunction)asfile_tell,     METH_NOARGS,  0},
	{"flush",     (PyCFunction)asfile_flush,    METH_NOARGS,  0},
	{"close",     (PyCFunction)asfile_close,    METH_NOARGS,  0},
	{NULL,	      NULL}		/* sentinel */
};


PyTypeObject PyAsyncFile_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"asyncfile",
	sizeof(PyAsyncFileObject),
	0,
	(destructor)asfile_dealloc,	/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	0,					/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_WEAKREFS,/* tp_flags */
	0,      			/* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	offsetof(PyAsyncFileObject, weakreflist),	/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	asfile_methods,			/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	asfile_init,				/* tp_init */
	PyType_GenericAlloc,/* tp_alloc */
	asfile_new,				/* tp_new */
	PyObject_Del,		/* tp_free */
};

#endif /*SLSOCKET*/
