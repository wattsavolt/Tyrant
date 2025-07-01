#pragma once

#include "Base/Base.h"

namespace tyr
{
    // Ensure size is 16 bytes
    #pragma pack(push, 1)
    struct Guid
    {
        uint data1;
        uint16 data2;
        uint16 data3;
        uint8 data4[8];
    };
    #pragma pack(pop)
}
