// Copyright © 2014 CCP ehf.

//header for the win32 file
#ifndef _WIN32_H_
#define _WIN32_H_

bool initwin32(PyObject* blueModule);
#if BLUE_WITH_PYTHON
PyObject *PyWin32Error(const char *msg = 0, DWORD code = 0);
#endif

void GetWindowsVersion( OSVERSIONINFO &info );
void GetWindowsVersion( OSVERSIONINFOEX &info );

#endif
