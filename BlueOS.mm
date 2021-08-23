#include "BlueOS.h"

#if __APPLE__

void BlueOS::PumpOS()
{
    @autoreleasepool
    {
        PumpOSInternal();
    }
}

#endif