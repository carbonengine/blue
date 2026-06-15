// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "Base64.h"


static uint8_t LookupDigits[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, //gap: ctrl chars
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, //gap: ctrl chars
	0,0,0,0,0,0,0,0,0,0,0,           //gap: spc,!"#$%'()*
	62,                   // +
	0, 0, 0,             // gap ,-.
	63,                   // /
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, // 0-9
	0, 0, 0,             // gap: :;<
	99,                   //  = (end padding)
	0, 0, 0,             // gap: >?@
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,
	17,18,19,20,21,22,23,24,25, // A-Z
	0, 0, 0, 0, 0, 0,    // gap: [\]^_`
	26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,
	43,44,45,46,47,48,49,50,51, // a-z
	0, 0, 0, 0,          // gap: {|}~ (and the rest...)
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char Base64Digits[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


//
size_t ToBase64( const uint8_t* pSrc, size_t nLenSrc, char* pDst, size_t nLenDst )
{
	size_t nLenOut= 0;
	while ( (int)nLenSrc > 0 ) {
		if (nLenOut+4 > nLenDst) return(0); // error

		// read three source bytes (24 bits) 
		uint8_t s1= pSrc[0];   // (but avoid reading past the end)
		uint8_t s2= 0; if (nLenSrc>1) s2=pSrc[1]; //------ corrected, thanks to  jprichey
		uint8_t s3= 0; if (nLenSrc>2) s3=pSrc[2];

		uint32_t n;
		n =  s1;    // xxx1
		n <<= 8;    // xx1x
		n |= s2;    // xx12  
		n <<= 8;    // x12x
		n |= s3;    // x123  

		//-------------- get four 6-bit values for lookups
		uint8_t m4= n & 0x3f;  n >>= 6;
		uint8_t m3= n & 0x3f;  n >>= 6;
		uint8_t m2= n & 0x3f;  n >>= 6;
		uint8_t m1= n & 0x3f;  

		//------------------ lookup the right digits for output
		uint8_t b1 = Base64Digits[m1];
		uint8_t b2 = Base64Digits[m2];
		uint8_t b3 = Base64Digits[m3];
		uint8_t b4 = Base64Digits[m4];

		//--------- end of input handling
		*pDst++ = b1;
		*pDst++ = b2;
		if ( nLenSrc >= 3 ) {  // 24 src bits left to encode, output xxxx
			*pDst++ = b3;
			*pDst++ = b4;
		}
		if ( nLenSrc == 2 ) {  // 16 src bits left to encode, output xxx=
			*pDst++ = b3;
			*pDst++ = '=';
		}
		if ( nLenSrc == 1 ) {  // 8 src bits left to encode, output xx==
			*pDst++ = '=';
			*pDst++ = '=';
		}
		pSrc    += 3;
		nLenSrc -= 3;
		nLenOut += 4;
	}
	// Could optionally append a NULL byte like so:
	// *pDst++= 0; nLenOut++;
	return( nLenOut );  
}


size_t FromBase64( const uint8_t* pSrc, size_t nLenSrc, char* pDst, size_t nLenDst )
{
	size_t nLenOut= 0;
	for( size_t j=0; j<nLenSrc; j+=4 ) {
		if ( nLenOut > nLenDst ) {
			return( 0 ); // error, buffer too small
		}
		uint8_t s1= LookupDigits[ *pSrc++ ];
		uint8_t s2= LookupDigits[ *pSrc++ ];
		uint8_t s3= LookupDigits[ *pSrc++ ];
		uint8_t s4= LookupDigits[ *pSrc++ ];

		uint8_t d1= ((s1 & 0x3f) << 2) | ((s2 & 0x30) >> 4);
		uint8_t d2= ((s2 & 0x0f) << 4) | ((s3 & 0x3c) >> 2);
		uint8_t d3= ((s3 & 0x03) << 6) | ((s4 & 0x3f) >> 0);

		*pDst++ = d1;  nLenOut++;
		if (s3==99) break;      // end padding found
		*pDst++ = d2;  nLenOut++;
		if (s4==99) break;      // end padding found
		*pDst++ = d3;  nLenOut++;
	}
	return( nLenOut );
}

