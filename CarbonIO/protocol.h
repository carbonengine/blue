/**
 * CCP Network protocol related definitions shared between CarbonIO and StacklessIO
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "dll_exports.h"

// bits defined in the 4-byte packet header
enum EHeaderBits
{
    ceHeaderBitsMask = 0xF0000000,
    ceHeaderSizeMask = 0x0FFFFFFF,

    ceHeaderExpectPayloadOffset = 1<<28, // if defined in a header, tells where the OOB data ends
    ceHeaderBitZlibCompressed = 1<<29, // was compressed with zlib
    ceHeaderBitSnappyCompressed = 1<<30, // was compressed with snappy
    ceHeaderExtraHeaderBitsFollow = 1<<31, // in case it needs to be extended, reserve this bit [unimplemented]
};

struct SCallbackEntry
{
    CioDataCallback callback;
    SCallbackEntry *next;
};

extern SCallbackEntry* g_packetCallbackChainPostDecompress;
#endif //PROTOCOL_H
