#pragma once

#include "RenderAPI/RenderAPITypes.h"
#include "RenderAPI/ShaderModule.h"

namespace tyr
{
    struct ShaderModel
    {
        uint8 majorVer : 6;
        uint8 minorVer : 2;
    };

    struct ShaderDesc
    {
        // Relative path to the directory containing the source shader hlsl or byte code
        RelativePath dirPath = "";
        FileName fileName;
        LocalString<ShaderModuleDesc::c_MaxEntryPointSize> entryPoint = "main";
        ShaderStage stage;
    };
}
