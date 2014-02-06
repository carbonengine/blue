#include "windows.h"

#define EVEFILEDESC "CCP Blue Framework\0"
#ifndef _DEBUG
#define EVEINTFILENAME "blue\0"
#define EVEFILENAME "blue.dll\0"
#else
#define EVEINTFILENAME "blueD\0"
#define EVEFILENAME "blueD.dll\0"
#endif
#define EVEFILETYPE VFT_DLL

#include "autoversion.h"
//standard file version thing
//#include "../version/evebuildver.h"
