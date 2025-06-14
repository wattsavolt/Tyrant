#pragma once

#include "Base/Base.h"

namespace tyr
{
    struct Guid
    {
        uint data1;
        uint16 data2;
        uint16 data3;
        uint8 data4[8];
    };

}
