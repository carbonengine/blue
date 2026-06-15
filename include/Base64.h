// Copyright © 2013 CCP ehf.

// http://www.experts-exchange.com/Programming/System/Windows__Programming/A_3216-Fast-Base64-Encode-and-Decode.html
#pragma once
#ifndef Base64_h
#define Base64_h

size_t ToBase64( const uint8_t* pSrc, size_t nLenSrc, char* pDst, size_t nLenDst );
size_t FromBase64( const uint8_t* pSrc, size_t nLenSrc, char* pDst, size_t nLenDst );

#endif // Base64_h

