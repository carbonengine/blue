// Copyright © 2021 CCP ehf.

#include "StdAfx.h"
#include "BlueClipboard.h"


static CBlueClipboard s_blueClipboard;
BlueClipboard* blueClipboard = &s_blueClipboard;

BLUE_REGISTER_GLOBAL_AS_MODULE_OBJECT( "clipboard", blueClipboard );


BlueClipboard& BlueClipboard::GetInstance()
{
	return s_blueClipboard;
}

PyObject* BlueClipboard::PyGetString() const
{
	std::string str;
	auto result = GetData( str );
    switch( result )
    {
    case CLIPBOARD_OK:
        return PyUnicode_FromStringAndSize( str.c_str(), str.length() );
        break;
    case CLIPBOARD_INCOMPATIBLE_FORMAT:
        Py_RETURN_NONE;
    default:
        PyErr_SetString( PyExc_OSError, "failed to get data from clipboard" );
        return nullptr;
    }
}

PyObject* BlueClipboard::PyGetUnicode() const
{
	std::wstring unicode;
	auto result = GetData( unicode );
    switch( result )
    {
    case CLIPBOARD_OK:
        return PyUnicode_FromWideChar( unicode.c_str(), unicode.length() );
        break;
    case CLIPBOARD_INCOMPATIBLE_FORMAT:
        Py_RETURN_NONE;
    default:
        PyErr_SetString( PyExc_OSError, "failed to get data from clipboard" );
        return nullptr;
    }
	return nullptr;
}

PyObject* BlueClipboard::PySetData( PyObject* data )
{
	if( !PyUnicode_Check( data ) )
	{
		PyErr_SetString( PyExc_TypeError, "String object required" );
		return nullptr;
	}

	OperationResult result;
        Py_ssize_t len = PyUnicode_GET_LENGTH( data );
        std::wstring unicode;
        unicode.resize( size_t( len ) );
        PyUnicode_AsWideChar( data, &unicode[0], len );

        result = SetData( unicode );
	if( result == CLIPBOARD_OK )
	{
		Py_RETURN_NONE;
	}
        PyErr_SetString( PyExc_OSError, "failed to write data to clipboard" );
	return nullptr;
}



BLUE_DEFINE_ABSTRACT( BlueClipboard );

const Be::ClassInfo* BlueClipboard::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueClipboard, "" )
		MAP_INTERFACE( BlueClipboard )

#if BLUE_WITH_PYTHON
		MAP_METHOD_AND_WRAP(
			"GetClipboardString",
			PyGetString,
			"Returns data from clipboard as text. If the data is not representable as text\n"
			"then the function returns None.\n"
			":rtype: str|None\n"
			":raises OSError: if there is an error communicating with clipboard\n"
			)

		MAP_METHOD_AND_WRAP(
			"GetClipboardUnicode",
			PyGetUnicode,
			"Returns data from clipboard as unicode. If the data is not representable as unicode\n"
			"then the function returns None.\n"
			":rtype: unicode|None\n"
			":raises OSError: if there is an error communicating with clipboard\n"
			)

		MAP_METHOD_AND_WRAP(
			"SetClipboardData",
			PySetData,
			"Sends data to the clipboard. Only text is supported.\n"
			":param text: text to send to the clipboard\n"
			":type text: str|unicode\n"
			":rtype: None\n"
			":raises OSError: if there is an error communicating with clipboard\n"
			)
#endif
	EXPOSURE_END()
}
