/* 
	*************************************************************************

	Blue.h

	Author:    Matthias Gudmundsson
	Created:   Oct. 2000
	OS:        Win32
	Project:   Blue

	Description:   

		Inclusion of all blue's main header files


	Dependencies:

		Blue

	(c) CCP 2000

	*************************************************************************
*/

#ifndef _BLUE_H_
#define _BLUE_H_

#ifndef __cplusplus
	#error Blue requires C++ compilation (use a .cpp suffix moron)
#endif

#define _BLUE_H_INCLUDING

// don't have any declspec guid and __uuidof, so grrr...
#include "BlueId.h"

#undef _BLUE_H_INCLUDING

#endif

