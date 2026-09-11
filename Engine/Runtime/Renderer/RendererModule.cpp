#include "RendererModule.h"
#include "BuildConfig.h"
#include "GraphicsManager.h"
#include "Rendering/RendererAPI.h"
#include "Rendering/Renderer.h"
#include "RenderAPI/SwapChain.h"
#include "Window/Window.h"
#include "Window/WindowModule.h"

namespace tyr
{
	RendererModule::RendererModule()
	{
		
	}

	RendererModule::~RendererModule()
	{

	}

	void RendererModule::Initialize()
	{
		TYR_GET_MODULE(WindowModule, m_WindowModule);
		RendererConfig rendererConfig;
		rendererConfig.renderAPIConfig.appName = c_AppName;
		rendererConfig.renderAPICreateConfig.backend = RenderAPIBackend::Vulkan;

		rendererConfig.shaderConfig.builtInSourceRootDirPath = c_EngineShadersDir;
		rendererConfig.shaderConfig.appSourceRootDirPath = c_AssetsDir;
		rendererConfig.shaderConfig.includeDirPath = c_EngineShadersDir;
		// TODO: If is final, the path should be to the asset bundle
		char byteCodeRootDirPath[PathConstants::c_MaxPathTotalSize];
		char binaryDirectoryPath[PathConstants::c_MaxPathTotalSize];
		Platform::GetBinaryDirectoryPath(binaryDirectoryPath);
		snprintf(byteCodeRootDirPath, sizeof(byteCodeRootDirPath), "%s/%s", binaryDirectoryPath, "Shaders");
		rendererConfig.shaderConfig.byteCodeRootDirPath = byteCodeRootDirPath;
		rendererConfig.shaderConfig.shaderModel = { 6, 2 };

		m_RenderAPI = GraphicsManager::CreateRenderAPI(rendererConfig.renderAPICreateConfig);
		m_RenderAPI->Initialize(rendererConfig.renderAPIConfig);

		m_Renderer = new Renderer(rendererConfig, m_RenderAPI);
		m_RendererAPI = new RendererAPI(*m_Renderer);
	}

	void RendererModule::Update(float deltaTime)
	{
		m_Renderer->Render(deltaTime);
	}

	void RendererModule::EndFrame()
	{
		m_Renderer->PrepareForNextFrame();
	}

	void RendererModule::Shutdown()
	{
		TYR_SAFE_DELETE(m_RendererAPI);
		TYR_SAFE_DELETE(m_Renderer);
		m_RenderAPI->Shutdown();
		GraphicsManager::DestroyRenderAPI(m_RenderAPI);
		m_WindowModule = nullptr;
	}

	Device* RendererModule::GetDevice() const
	{
		return m_RenderAPI->GetDevice();
	}
}