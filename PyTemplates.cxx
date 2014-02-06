//
// PyTemplates.cxx
//
// Matthias Gudmundsson
// (c) CCP Jan 2003
//

#include "PyTemplates.h"


//--------------------------------------------------------------------
// SetErr32
//--------------------------------------------------------------------
PyObject* PyErr::SetErr32(int err, const char* format, ...)
{
	if (format != NULL)
	{
		va_list args;
		va_start(args, format);
		char buff[256];
		_vsnprintf_s(buff, 256, _TRUNCATE, format, args);
		va_end(args);
		PyErr_SetFromWindowsErrWithFilename(err, buff);
	}
	else
	{
		PyErr_SetFromWindowsErr(err);
	}

	return NULL;
}