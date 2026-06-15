// Copyright © 2014 CCP ehf.

#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "Logger/Logger.h"


//A few thunker objects
template <class C, PyObject *(C::*pFunc)() >
PyObject *ThunkNone(PyObject *self)
{
	C *obj = static_cast<C*>(self);
	return (obj->*pFunc)();
}

template <class C, PyObject *(C::*pFunc)(PyObject*args) >
PyObject *ThunkArgs(PyObject *self, PyObject *args)
{
	C *obj = static_cast<C*>(self);
	return (obj->*pFunc)(args);
}

template <class C, PyObject *(C::*pFunc)(PyObject*args, PyObject *kw) >
PyObject *ThunkArgsKw(PyObject *self, PyObject *args, PyObject *kw)
{
	C *obj = static_cast<C*>(self);
	return (obj->*pFunc)(args, kw);
}


class CPyTypeObject : public PyTypeObject
{
public:
	CPyTypeObject( const char *name )
	{
		static const PyTypeObject def = {PyObject_HEAD_INIT(0)};
		*static_cast<PyTypeObject*>(this) = def;
		tp_name = name;
		tp_flags = Py_TPFLAGS_DEFAULT;
		tp_doc = "";
	}
	bool IsReady() {return !!(tp_flags & Py_TPFLAGS_READY);}
	bool Ready() {return PyType_Ready(this) == 0;}
};


// log support
struct LogChannel : 
	public PyObject, 
	public CcpLogChannel_t
{
	PyObject* mFacilityStr;
	PyObject* mObjectStr;
	
	LogChannel(PyObject *args, PyObject *kwds) : mFacilityStr(0), mObjectStr(0)
	{
		channel = 0;
		source = 0;
		oktocall = false;
		if (!PyArg_ParseTuple(args, "O!O!", &PyUnicode_Type, &mFacilityStr, &PyUnicode_Type, &mObjectStr))
			return;

		Py_INCREF(mFacilityStr);
		Py_INCREF(mObjectStr);
		facility = PyUnicode_AsUTF8(mFacilityStr);
		object = PyUnicode_AsUTF8(mObjectStr);
		oktocall = true;
	}

	~LogChannel()
	{
		Py_XDECREF(mFacilityStr);
		Py_XDECREF(mObjectStr);
	}

	static PyObject *New(PyTypeObject *type, PyObject *args, PyObject *kwds)
	{
		void *obj = type->tp_alloc(type, 0);
		LogChannel *lc = new(obj) LogChannel(args, kwds);
		if (!lc->oktocall) {
			Py_DECREF(lc);
			return 0;
		}
		return lc;
	}

	static void Dealloc(PyObject *self)
	{
		static_cast<LogChannel*>(self)->~LogChannel();
		self->ob_type->tp_free(self);
	}
		
	
	PyObject* Repr()
	{
		return PyUnicode_FromFormat("log.Channel(\"%s\",\"%s\")", facility, object);
	}

	PyObject* Log(PyObject* args)
	{
		PyObject *textO;
		TLOGFLAG flag = LGINFO;
		int backstack = 0;
		if (!PyArg_ParseTuple(args, "O|ii", &textO, &flag, &backstack))
			return nullptr;
		if (!PyUnicode_Check(textO))
		{
			PyErr_SetString(PyExc_TypeError, "expected string or unicode object");
			return nullptr;
		}

		CCP::LogType logType = TLOGFLAGToLogType( flag );

		if (CCP::IsLogging( logType ) && oktocall) {
			CCP::LogFuncChannel(*this, logType, flag, "%s", PyUnicode_AsUTF8(textO));
		}

		Py_INCREF(Py_None);
		return Py_None;
	}


	PyObject* LogCounter(PyObject* args)
	{
		PyObject *text;
		if (!PyArg_ParseTuple(args, "O", &text))
			return nullptr;
		if (!PyUnicode_Check(text))
		{
			PyErr_SetString(PyExc_TypeError, "expected string or unicode");
			return nullptr;
		}
		return PyObject_CallMethod(this, const_cast<char*>("Log"), const_cast<char*>("Oi"), text, CCP::LOGTYPE_NOTICE);
	}


	PyObject* IsOpen(PyObject* args)
	{
		int flags = -1;
		if (!PyArg_ParseTuple(args, "|i", &flags))
			return NULL;
		return PyLong_FromLong(CCP::IsLogging( TLOGFLAGToLogType( TLOGFLAG(flags) ) ) && oktocall);
	}


	static PyMemberDef *GetMembers()
	{
		static PyMemberDef members[] = 
		{
			{const_cast<char*>("facility"), T_OBJECT_EX, BLUE_MEMBEROFFSET(LogChannel, mFacilityStr), READONLY},
			{const_cast<char*>("object"), T_OBJECT_EX, BLUE_MEMBEROFFSET(LogChannel, mObjectStr), READONLY},
			{const_cast<char*>("channelOpen"), T_INT, BLUE_MEMBEROFFSET(LogChannel, oktocall), READONLY},
			{NULL}
		};
		return members;
	}


	static PyMethodDef *GetMethods()
	{
		static PyMethodDef methods[] = 
		{
			{"Log",			(PyCFunction)ThunkArgs<LogChannel, &LogChannel::Log>,			METH_VARARGS},
			{"LogCounter",	(PyCFunction)ThunkArgs<LogChannel, &LogChannel::LogCounter>, METH_VARARGS},
			{"IsOpen",		(PyCFunction)ThunkArgs<LogChannel, &LogChannel::IsOpen>,		METH_VARARGS},
			{0}
		};
		return methods;
	}

	static PyObject *GetAttribs()
	{
		PyObject *i, *dict = PyDict_New();
		PyDict_SetItemString(dict, "INFO", i = PyLong_FromLong(LGINFO)); Py_DECREF(i);
		PyDict_SetItemString(dict, "NOTICE", i = PyLong_FromLong(LGPERF)); Py_DECREF(i);
		PyDict_SetItemString(dict, "WARN", i = PyLong_FromLong(LGWARN)); Py_DECREF(i);
		PyDict_SetItemString(dict, "ERR", i = PyLong_FromLong(LGERR)); Py_DECREF(i);
		return dict;
	}

	//Define this object's type
	static PyTypeObject* GetType()
	{
		static CPyTypeObject type("blue.LogChannel");
		if (!type.IsReady()) {
			// initialize type
			type.tp_basicsize = sizeof(LogChannel);
			type.tp_flags |= Py_TPFLAGS_BASETYPE;
			type.tp_new = New;
			type.tp_dealloc = Dealloc;
			type.tp_methods = GetMethods();
			type.tp_members = GetMembers();
			type.tp_repr = (reprfunc)ThunkNone<LogChannel, &LogChannel::Repr>;
			type.tp_dict = PyDict_New();
			PyObject *dict = GetAttribs();
			PyDict_SetItemString(type.tp_dict, "flags", dict);
			Py_DECREF(dict);
			if (!type.Ready())
				return 0;
		}
		return &type;
	}

	private:
		// no assignment
		LogChannel &operator=(const LogChannel &);
};

//Show the type outside
PyTypeObject *LogChannelType()
{
	return LogChannel::GetType();
}

#endif
