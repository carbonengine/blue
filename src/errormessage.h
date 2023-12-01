#ifndef _ERRORMESSAGE_H_
#define _ERRORMESSAGE_H_

////////////////////////////////////////////////////////////
//
//  Created:		Feb 2021
//  Copyright:		CCP 2023
//  Documentation:	https://ccpgames.atlassian.net/wiki/spaces/TC/pages/177930592/Localizing+Early+Error+Messages
//

#include <string>

#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#define IDS_VERIFYFAIL_M 106
#define IDS_VERIFYFAIL_C 107
#define IDS_INVALIDOS 108
#define IDS_VERIFYFAIL_M1 109
#define IDS_VERIFYFAIL_NOTFOUND 110
#define IDS_VERIFYFAIL_UNKNOWNFOUND 111
#define IDS_VERIFYFAIL_INCORRECTCRC 112

struct BlueErrorMessage
{
	std::string de;
	std::string en;
	std::string es;
	std::string fr;
	std::string ja;
	std::string ko;
	std::string ru;
	std::string zh;
};

// Exposed to allow altering of error message at runtime, useful for altertering message based on system spec.
extern "C" BLUEIMPORT void BlueSetErrorMessage(const BlueErrorMessage & message, unsigned int messageId);

// Exposed so that dialog can be requested externally to blue
extern "C" BLUEIMPORT void BlueShowMessageBox(const std::string & title, unsigned int titleId, const std::string & message, unsigned int messageId);

// Translate an error message to an appropriate language for the user - returns the original string on failure.
std::string TranslateErrorMessage( const std::string& original, unsigned messageID );

void DisplayErrorMessageBox( const char* title, const char* message );

#endif // _ERRORMESSAGE_H_
