//header for the win32 file
#ifndef _WIN32_H_
#define _WIN32_H_

void initwin32(void);
PyObject *PyWin32Error(const char *msg = 0, DWORD code = 0);

#endif