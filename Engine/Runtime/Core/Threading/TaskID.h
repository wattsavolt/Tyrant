#pragma once

#include "Base/Base.h"

namespace tyr
{
    using TaskID = uint64;

    // Never a value TaskPool::Create() can return (would need an out-of-range pool index),
    // so it's safe to use as a "no task" sentinel, e.g. for a task with no parent.
    constexpr TaskID c_InvalidTaskID = ~TaskID(0);
}