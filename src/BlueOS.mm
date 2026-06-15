// Copyright © 2021 CCP ehf.

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