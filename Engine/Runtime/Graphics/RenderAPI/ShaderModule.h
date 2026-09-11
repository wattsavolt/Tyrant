#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"

namespace tyr
{
    TYR_CREATE_HANDLE_TYPE(ShaderModuleHandle);
   
    enum class ShaderBinaryLanguage : uint
    {
        Dxil = 0,
        SpirV
    };

    struct ShaderModuleDesc
    {
        static constexpr uint8 c_MaxEntryPointSize = 16;
        TYR_DECLARE_GDEBUGNAME(debugName);
        void* byteCode = nullptr;
        size_t size;
        LocalString<c_MaxEntryPointSize> entryPoint = "main";
        ShaderStage stage;
    };
}
