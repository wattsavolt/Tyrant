
#include "Shader.h"
#include "Platform/Platform.h"
#include "IO/FileStream.h"
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#include <windows.h>
#endif
#include "dxcapi.h"

namespace tyr
{
	const wchar_t* GethaderStageString(ShaderStage stage)
	{
		switch (stage)
		{
		case SHADER_STAGE_VERTEX_BIT:
			return L"vs";
		case SHADER_STAGE_FRAGMENT_BIT:
			return L"ps";
		case SHADER_STAGE_GEOMETRY_BIT:
			return L"gs";
		case SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return L"hs";
		case SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return L"ds";
		case SHADER_STAGE_COMPUTE_BIT:
			return L"cs";
		default:
			TYR_ASSERT(false);
			return L"";
		}
	}

	const char* GetShaderBinaryExtension(ShaderBinaryLanguage language)
	{
		switch (language)
		{
		case ShaderBinaryLanguage::SpirV:
			return ".spv";
		case ShaderBinaryLanguage::Dxil:
			return ".cso";
		default:
			TYR_ASSERT(false);
			return "";
		}
	}

	uint Shader::s_RefCount = 0;
	Handle Shader::s_CompilerLibrary = nullptr;
	Handle Shader::s_SignatureLibrary = nullptr;

	Shader::Shader(const ShaderDesc& shaderDesc)
		: m_ByteCodeSize(0)
	{
		m_Desc = shaderDesc;
		s_RefCount++;

		if (!m_Desc.neverCompile)
		{
			if (!s_CompilerLibrary)
			{
				s_CompilerLibrary = Platform::OpenLibrary(c_CompilerLibraryName, false);
				TYR_ASSERT(s_CompilerLibrary);
			}
			if (!s_SignatureLibrary && c_SignatureLibraryName && c_SignatureLibraryName != "")
			{
				s_SignatureLibrary = Platform::OpenLibrary(c_SignatureLibraryName, false);
				TYR_ASSERT(s_SignatureLibrary);
			}
		}
	}

	Shader::~Shader()
	{
		s_RefCount--;

		if (s_RefCount == 0)
		{
			if (s_CompilerLibrary)
			{
				Platform::CloseLibrary(s_CompilerLibrary);
			}
			if (s_SignatureLibrary)
			{
				Platform::CloseLibrary(s_SignatureLibrary);
			}
		}
	}

	void Shader::CreateByteCode()
	{
		if (m_Desc.neverCompile)
		{
			LoadBinaryFile();
		}
		else
		{
			Compile();
			// TODO : Just copy the binary to byte code vector after compiling
			LoadBinaryFile();
		}
	}

	void Shader::Compile()
	{
		const ShaderCompileArguments& compileArgs = m_Desc.compileArgs;
		const String filePath = compileArgs.shaderDirPath + compileArgs.fileName;
		const String fileNameWithoutExt = StringUtil::GetFileNameWithoutExtension(compileArgs.fileName);
		const String binaryDirPath = Platform::c_BinaryDirectory + "/" + m_Desc.binaryDirPath;
		const ShaderBinaryLanguage binaryLanguage = GetShaderBinaryLanguage();
		const String shaderExt = GetShaderBinaryExtension(binaryLanguage);
		const String destFilePath = binaryDirPath + "/" + fileNameWithoutExt + shaderExt;

		// Only recompile if necessary
		if (std::filesystem::exists(destFilePath) && std::filesystem::exists(filePath) &&
			std::filesystem::last_write_time(destFilePath) > std::filesystem::last_write_time(filePath))
		{
			return;
		}

#ifdef TYR_USE_DXCOMPILER
		DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)Platform::GetProcessAddress(s_CompilerLibrary, c_CompilerCreationFunctionName);
		TYR_ASSERT(DxcCreateInstance);

		IDxcCompiler3* compiler = nullptr;
		TYR_GASSERT(DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), reinterpret_cast<void**>(&compiler)));

		IDxcUtils* utils = nullptr;
		TYR_GASSERT(DxcCreateInstance(CLSID_DxcUtils, __uuidof(IDxcUtils), reinterpret_cast<void**>(&utils)));

		IDxcContainerReflection* containerReflection = nullptr;
		TYR_GASSERT(DxcCreateInstance(CLSID_DxcContainerReflection, __uuidof(IDxcContainerReflection), reinterpret_cast<void**>(&containerReflection)));

		if (compiler && utils)
		{
			IDxcIncludeHandler* includeHandler = nullptr;
			TYR_GASSERT(utils->CreateDefaultIncludeHandler(&includeHandler));

			IDxcBlobEncoding* sourceBlob = nullptr;
			TYR_GASSERT(utils->LoadFile(StringUtil::ToWString(filePath).c_str(), nullptr, &sourceBlob));

			if (includeHandler && sourceBlob)
			{
				DxcBuffer source;
				source.Ptr = sourceBlob->GetBufferPointer();
				source.Size = sourceBlob->GetBufferSize();
				source.Encoding = DXC_CP_ACP;

				const WString entryPoint = StringUtil::ToWString(compileArgs.entryPoint);
				const WString shaderProfile = WString(GethaderStageString(compileArgs.stage)) + L"_" + std::to_wstring(compileArgs.shaderModel.majorVer) 
					+ L"_" + std::to_wstring(compileArgs.shaderModel.minorVer);
				const WString dxcDestFilePath = StringUtil::ToWString(destFilePath);
				const WString includeDirPath = StringUtil::ToWString(compileArgs.shaderIncludeDirPath).c_str();

				Array<const wchar_t*> args;
				args.Add(L"-Fo");
				args.Add(dxcDestFilePath.c_str());
				args.Add(L"-E");
				args.Add(entryPoint.c_str());
				args.Add(L"-T");
				args.Add(shaderProfile.c_str());
				// Include directory.
				args.Add(L"-I");
				args.Add(includeDirPath.c_str());
				//args.push_back(L"-Qstrip_debug");
				
				args.Add(DXC_ARG_WARNINGS_ARE_ERRORS);
				args.Add(DXC_ARG_DEBUG);

				for (const String& define : compileArgs.defines)
				{
					args.Add(L"-D");
					args.Add(StringUtil::ToWString(define).c_str());
				}

				switch (binaryLanguage)
				{
				case ShaderBinaryLanguage::SpirV:
					args.Add(L"-D");
					args.Add(L"TYR_VULKAN");
					if (compileArgs.stage == SHADER_STAGE_FRAGMENT_BIT)
					{
						args.Add(L"-D");
						args.Add(L"TYR_PIXEL_SHADER");
					}
					args.Add(L"-spirv");
					args.Add(L"-fspv-target-env=vulkan1.3");			
					args.Add(L"-fvk-use-dx-position-w");
					args.Add(L"-fvk-use-dx-layout");
					args.Add(L"-enable-16bit-types");
					args.Add(L"-Zpr");
					break;
				case ShaderBinaryLanguage::Dxil:
					args.Add(L"-D");
					args.Add(L"TYR_D3D12");
					// Doesn't work with spir-v
					args.Add(L"-Qstrip_reflect");
					args.Add(L"-Zpc");
					break;
				default:
					TYR_ASSERT(false);
				}

				IDxcResult* results = nullptr;
				TYR_GASSERT(compiler->Compile(&source, args.Data(), args.Size(), includeHandler, __uuidof(IDxcResult), reinterpret_cast<void**>(&results)));

				IDxcBlobUtf8* errors = nullptr;
				IDxcBlobUtf16* errorsName = nullptr;
				results->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), reinterpret_cast<void**>(&errors), &errorsName);
				if (errors != nullptr && errors->GetStringLength() != 0)
				{
					char* s = (char*)errors->GetStringPointer();
					TYR_ALERT(s);
					TYR_ASSERT(false);
					return;
				}
				TYR_SAFE_RELEASE(errorsName);
				TYR_SAFE_RELEASE(errors);

				HRESULT status = S_OK;
				TYR_GASSERT(results->GetStatus(&status));
				if (status != S_OK)
				{
					TYR_ALERT("Failed to Compile shader.");
					TYR_ASSERT(false);
					return;
				}

				IDxcBlob* shaderBinary = nullptr;
				IDxcBlobUtf16* shaderNameWide = nullptr;
				TYR_GASSERT(results->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob), reinterpret_cast<void**>(&shaderBinary), &shaderNameWide));
				if (shaderBinary && shaderNameWide)
				{
					const String shaderName = StringUtil::ToString((const wchar_t*)shaderNameWide->GetStringPointer());
					std::filesystem::create_directory(binaryDirPath);
					FileStream::WriteFile(shaderName.c_str(), shaderBinary->GetBufferPointer(), shaderBinary->GetBufferSize());
				}			
				TYR_SAFE_RELEASE(shaderNameWide);
				TYR_SAFE_RELEASE(shaderBinary);

				if (results->HasOutput(DXC_OUT_PDB))
				{
					IDxcBlob* pdbBinary = nullptr;
					IDxcBlobUtf16* pdbName = nullptr;
					TYR_GASSERT(results->GetOutput(DXC_OUT_PDB, __uuidof(IDxcBlob), reinterpret_cast<void**>(&pdbBinary), &pdbName));
					if (pdbBinary && pdbName)
					{
						FileStream::WriteFile((const char*)pdbName->GetStringPointer(), pdbBinary->GetBufferPointer(), pdbBinary->GetBufferSize());
					}
					TYR_SAFE_RELEASE(pdbName);
					TYR_SAFE_RELEASE(pdbBinary);
				}
				TYR_SAFE_RELEASE(results);
			}

			TYR_SAFE_RELEASE(sourceBlob);
			TYR_SAFE_RELEASE(includeHandler);
		}

		TYR_SAFE_RELEASE(containerReflection);
		TYR_SAFE_RELEASE(utils);
		TYR_SAFE_RELEASE(compiler);
#endif
	}

	void Shader::LoadBinaryFile()
	{
		const String binaryDirPath = Platform::c_BinaryDirectory + "/" + m_Desc.binaryDirPath;
		const String fileNameWithoutExt = StringUtil::GetFileNameWithoutExtension(m_Desc.compileArgs.fileName);
		const String shaderExt = GetShaderBinaryExtension(GetShaderBinaryLanguage());
		const String shaderBinFilePath = binaryDirPath + "/" + fileNameWithoutExt + shaderExt;
		m_ByteCodeSize = FileStream::ReadAllFile(shaderBinFilePath.c_str(), m_ByteCode);
	}
	
	void Shader::Recompile()
	{
		Compile();
		Construct();
	}
}