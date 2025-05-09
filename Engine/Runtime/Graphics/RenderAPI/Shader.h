

#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"

namespace tyr
{
    struct ShaderModel
    {
        uint8 majorVer : 6;
        uint8 minorVer : 2;
    };

    struct ShaderCompileArguments
    {
        // Absolute path to shader directory.
        String shaderDirPath;
        String shaderIncludeDirPath;
        String fileName;
        String entryPoint = "main";
        ShaderStage stage;
        Array<String> defines;
        ShaderModel shaderModel = { 6, 0 };
    };

    struct ShaderDesc
    {
        ShaderCompileArguments compileArgs;
        // Path relative to current working directory where shader binaries are saved to and loaded from.
        String binaryDirPath = "ShaderBin";
        String debugName;
        // Never compiles shader and expects the binary file to be present. Should always be false on PC.
        bool neverCompile = false;
    };

    enum class ShaderBinaryLanguage : uint
    {
        Dxil = 0,
        SpirV
    };

    class LibraryLoader;
	/// Class representing a shader
	class TYR_GRAPHICS_EXPORT Shader
	{
	public:
        Shader(const ShaderDesc& shaderDesc);
        virtual ~Shader();

        void Recompile();

        const char* GetEntryPoint() const { return m_Desc.compileArgs.entryPoint.c_str(); }
        ShaderStage GetStage() const { return m_Desc.compileArgs.stage; }
        const uint8* GetByteCode() const { return m_ByteCode.Data(); }

    protected:
        virtual void CreateByteCode();
        virtual void Compile();
        virtual void LoadBinaryFile();
        virtual void Construct() = 0;
        virtual ShaderBinaryLanguage GetShaderBinaryLanguage() const = 0;

        ShaderDesc m_Desc;
        // Size may be bigger than the current binary code size to minimize allocations when recompiling
        Array<uint8> m_ByteCode;
        size_t m_ByteCodeSize;

        static uint s_RefCount;

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
