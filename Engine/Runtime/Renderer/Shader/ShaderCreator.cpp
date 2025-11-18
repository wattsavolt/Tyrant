#include "ShaderCreator.h"
#include "Platform/Platform.h"
#include "IO/FileStream.h"
#include "RenderAPI/Device.h"
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

	constexpr const char* GetShaderBinaryExtension(ShaderBinaryLanguage language)
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

	Handle ShaderCreator::s_CompilerLibrary = nullptr;
	Handle ShaderCreator::s_SignatureLibrary = nullptr;

	ShaderCreator::ShaderCreator(Device& device, const ShaderCreatorConfig& config)
		: m_Device(device)
		, m_Config(config)
	{

	}

	ShaderCreator::~ShaderCreator()
	{

	}

	void ShaderCreator::CompileShader(const ShaderCompileConfig& compileConfig, const ShaderDesc& shaderDesc, bool isBuiltIn)
	{
		const ShaderBinaryLanguage binaryLanguage = m_Device.GetShaderBinaryLanguage();
		const char* shaderExt = GetShaderBinaryExtension(binaryLanguage);

		const char* sourceDirPath = isBuiltIn ? m_Config.builtInSourceRootDirPath.CStr() : m_Config.appSourceRootDirPath.CStr();

		char sourceFilePath[PathConstants::c_MaxPathTotalSize];
		snprintf(sourceFilePath, sizeof(sourceFilePath), "%s/%s/%s%s", sourceDirPath, shaderDesc.dirPath.CStr(), shaderDesc.fileName.CStr(), ".hlsl");

		char byteCodeDirPath[PathConstants::c_MaxPathTotalSize];
		snprintf(byteCodeDirPath, sizeof(byteCodeDirPath), "%s/%s", m_Config.byteCodeRootDirPath.CStr(), shaderDesc.dirPath.CStr());

		char byteCodeFilePath[PathConstants::c_MaxPathTotalSize];
		snprintf(byteCodeFilePath, sizeof(byteCodeFilePath), "%s/%s%s", byteCodeDirPath, shaderDesc.fileName.CStr(), shaderExt);
		
		if (!fs::exists(sourceFilePath))
		{
			TYR_ASSERT(false);
			return;
		}

		// Only compile if necessary
		if (fs::exists(byteCodeFilePath) && fs::last_write_time(byteCodeFilePath) > fs::last_write_time(sourceFilePath))
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

			wchar_t sourceFilePathW[PathConstants::c_MaxAssetPathTotalSize];
			StringUtil::ToWString(sourceFilePath, sourceFilePathW);
			TYR_GASSERT(utils->LoadFile(sourceFilePathW, nullptr, &sourceBlob));

			if (includeHandler && sourceBlob)
			{
				DxcBuffer source;
				source.Ptr = sourceBlob->GetBufferPointer();
				source.Size = sourceBlob->GetBufferSize();
				source.Encoding = DXC_CP_ACP;

				wchar_t entryPoint[ShaderModuleDesc::c_MaxEntryPointSize];
				StringUtil::ToWString(shaderDesc.entryPoint.CStr(), entryPoint);

				wchar_t shaderProfile[ShaderModuleDesc::c_MaxEntryPointSize];
				swprintf(shaderProfile, ShaderModuleDesc::c_MaxEntryPointSize, L"%s_%u_%u", GethaderStageString(shaderDesc.stage), m_Config.shaderModel.majorVer, m_Config.shaderModel.minorVer);

				wchar_t includeDirPath[PathConstants::c_MaxPathTotalSize];
				StringUtil::ToWString(m_Config.includeDirPath.CStr(), includeDirPath);

				wchar_t byteCodeFilePathW[PathConstants::c_MaxPathTotalSize];
				StringUtil::ToWString(byteCodeFilePath, byteCodeFilePathW);

				LocalArray<const wchar_t*, 32> args;
				args.Add(L"-Fo");
				args.Add(byteCodeFilePathW);
				args.Add(L"-E");
				args.Add(entryPoint);
				args.Add(L"-T");
				args.Add(shaderProfile);
				// Include directory.
				args.Add(L"-I");
				args.Add(includeDirPath);
				//args.push_back(L"-Qstrip_debug");
				
				args.Add(DXC_ARG_WARNINGS_ARE_ERRORS);
				args.Add(DXC_ARG_DEBUG);

				for (const auto& define : compileConfig.defines)
				{
					args.Add(L"-D");
					wchar_t defineW[ShaderCompileConfig::c_MaxDefineSize];
					StringUtil::ToWString(define.CStr(), defineW);
					args.Add(defineW);
				}

				// Use row-major layout
				args.Add(L"-Zpr");

				switch (binaryLanguage)
				{
				case ShaderBinaryLanguage::SpirV:
					args.Add(L"-D");
					args.Add(L"TYR_VULKAN");
					if (shaderDesc.stage == SHADER_STAGE_FRAGMENT_BIT)
					{
						args.Add(L"-D");
						args.Add(L"TYR_PIXEL_SHADER");
					}
					args.Add(L"-spirv");
					args.Add(L"-fspv-target-env=vulkan1.3");			
					args.Add(L"-fvk-use-dx-position-w");
					args.Add(L"-fvk-use-dx-layout");
					args.Add(L"-enable-16bit-types");
					break;
				case ShaderBinaryLanguage::Dxil:
					args.Add(L"-D");
					args.Add(L"TYR_D3D12");
					// Doesn't work with spir-v
					args.Add(L"-Qstrip_reflect");
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
					fs::create_directory(byteCodeDirPath);
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

	ShaderModuleHandle ShaderCreator::CreateShader(const ShaderDesc& shaderDesc, bool isBuiltIn)
	{
		const ShaderBinaryLanguage binaryLanguage = m_Device.GetShaderBinaryLanguage();
		const char* shaderExt = GetShaderBinaryExtension(binaryLanguage);

		const char* sourceDirPath = isBuiltIn ? m_Config.builtInSourceRootDirPath.CStr() : m_Config.appSourceRootDirPath.CStr();

		char sourceFilePath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(sourceFilePath, sizeof(sourceFilePath), "%s/%s/%s%s", sourceDirPath, shaderDesc.dirPath.CStr(), shaderDesc.fileName.CStr(), ".hlsl");

		char byteCodeDirPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(byteCodeDirPath, sizeof(byteCodeDirPath), "%s/%s", m_Config.byteCodeRootDirPath.CStr(), shaderDesc.dirPath.CStr());

		char byteCodeFilePath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(byteCodeFilePath, sizeof(byteCodeFilePath), "%s/%s%s", byteCodeDirPath, shaderDesc.fileName.CStr(), shaderExt);

		static constexpr uint c_MaxByteCodeSize = 102400;
		static TYR_THREADLOCAL uint8 byteCode[c_MaxByteCodeSize];

		const size_t byteCodeSize = FileStream::ReadAllFile(byteCodeFilePath, byteCode);

		ShaderModuleDesc desc;
		desc.byteCode = byteCode;
		desc.size = byteCodeSize;
		desc.entryPoint = shaderDesc.entryPoint;
		desc.stage = shaderDesc.stage;
#if !TYR_FINAL
		desc.debugName = shaderDesc.fileName.CStr();
#endif
		return m_Device.CreateShaderModule(desc);
	}

	ShaderModuleHandle ShaderCreator::CompileAndCreateShader(const ShaderCompileConfig& compileConfig, const ShaderDesc& shaderDesc, bool isBuiltIn)
	{
		CompileShader(compileConfig, shaderDesc, isBuiltIn);
		return CreateShader(shaderDesc, isBuiltIn);
	}

	void ShaderCreator::LoadCompilerLibs()
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

	void ShaderCreator::UnloadCompilerLibs()
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