// Copyright © 2021 CCP ehf.

#ifndef _ERRORMESSAGE_H_
#define _ERRORMESSAGE_H_


#include <string>

#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#define IDS_VERIFYFAIL_M 106
#define IDS_VERIFYFAIL_C 107
#define IDS_INVALIDWINDOWS 108
#define IDS_VERIFYFAIL_M1 109
#define IDS_VERIFYFAIL_NOTFOUND 110
#define IDS_VERIFYFAIL_UNKNOWNFOUND 111
#define IDS_VERIFYFAIL_INCORRECTCRC 112
#define IDS_INVALIDMACOS 113
#define IDS_INVALIDOS 114

// Exposed so invalid os dialogs can be requested externally to blue
extern "C" BLUEIMPORT void BlueShowInvalidOSVersionError();

// Translate an error message to an appropriate language for the user - returns the original string on failure.
std::string TranslateErrorMessage( const std::string& original, unsigned messageID );

void DisplayErrorMessageBox( const char* title, const char* message );

#endif // _ERRORMESSAGE_H_
