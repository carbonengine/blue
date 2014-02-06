/* 
	*************************************************************************

	Manifest.h

	Author:    Kristjan Valur Jonsson
	Created:   Aug. 2007
	OS:        Win32
	Project:   Blue

	Description:   

		Declerations of functions for the reading and wrinting of
		packaging manifests.

	Dependencies:

		Blue

	(c) CCP 2007

	*************************************************************************
*/
#ifndef _MANIFEST_H_
#define _MANIFEST_H_

#include <wincrypt.h>

PyObject *PyPackManifest_Impl(PyObject *seq, const char *timeStamp, HCRYPTPROV hProv, ALG_ID algid);
#endif //_MANIFEST_H_
