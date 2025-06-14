#pragma once

namespace tyr
{
#define TYR_REFL_PRIVATE_ACCESS(type) friend struct type##MetaClass;
}


