#pragma once

#include "Shader.h"
#include "RendererMacros.h"

namespace tyr
{
    struct ShaderCreatorConfig
    {
        // Absolute path to the root directory containing the source of the built-in shaders
        Path builtInSourceRootDirPath;
        // Absolute path to the root directory containing the source of the app shaders
        Path appSourceRootDirPath;
        // Absolute path to shader include directory
        Path includeDirPath;
        // Absolute path to the root directory containing the byte code of both built-in and app shaders
        Path byteCodeRootDirPath;
        ShaderModel shaderModel = { 6, 2 };
    };

    struct ShaderCompileConfig
    {
        static constexpr uint8 c_MaxDefineSize = 24;
        LocalArray<LocalString<c_MaxDefineSize>, 8> defines;
    };

    class LibraryLoader;
    class Device;

    // Class that compiles shaders (if necessary), loads shader byte code and creates shader modules
	class TYR_RENDERER_EXPORT ShaderCreator final : public INonCopyable
	{
	public:
        ShaderCreator(Device& device, const ShaderCreatorConfig& config);
        ~ShaderCreator();

        void CompileShader(const ShaderCompileConfig& compileConfig, const ShaderDesc& shaderDesc, bool isBuiltIn = true);
        ShaderModuleHandle CreateShader(const ShaderDesc& shaderDesc, bool isBuiltIn = true);
        ShaderModuleHandle CompileAndCreateShader(const ShaderCompileConfig& compileConfig, const ShaderDesc& shaderDesc, bool isBuiltIn = true);

        static void LoadCompilerLibs();
        static void UnloadCompilerLibs();

    private:
        Device& m_Device;
        ShaderCreatorConfig m_Config;

#define TYR_USE_DXCOMPILER 1
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
        static constexpr const char* c_CompilerLibraryName = "dxcompiler.dll";
        static constexpr const char* c_SignatureLibraryName = "dxil.dll";
#elif TYR_PLATFORM == TYR_PLATFORM_LINUX
        static constexpr const char* c_CompilerLibraryName = "libdxcompiler.so";
        static constexpr const char* c_SignatureLibraryName = "libdxil.so";
#elif TYR_PLATFORM == TYR_PLATFORM_OSX || TYR_PLATFORM == TYR_PLATFORM_IOS
        static constexpr const char* c_CompilerLibraryName = "libdxcompiler.dylib";
        static constexpr const char* c_SignatureLibraryName = "libdxil.dylib";
#else
        #define TYR_USE_DXCOMPILER 0
#endif
#ifdef TYR_USE_DXCOMPILER
        static constexpr const char* c_CompilerCreationFunctionName = "DxcCreateInstance";
#endif
        static Handle s_CompilerLibrary;
        static Handle s_SignatureLibrary;
	};
}
