#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"

namespace tyr
{
    using ShaderModuleHandle = ResourceHandle;

    enum class ShaderBinaryLanguage : uint
    {
        Dxil = 0,
        SpirV
    };

    struct ShaderModuleDesc
    {
        static constexpr uint8 c_MaxEntryPointSize = 16;
#if !TYR_FINAL
        GDebugString debugName;
#endif
        void* byteCode = nullptr;
        size_t size;
        LocalString<c_MaxEntryPointSize> entryPoint = "main";
        ShaderStage stage;
    };
}
