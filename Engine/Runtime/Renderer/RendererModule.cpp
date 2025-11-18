#include "RendererModule.h"
#include "BuildConfig.h"
#include "GraphicsManager.h"
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

	void RendererModule::InitializeModule()
	{
		WindowModule* windowModule;
		TYR_GET_MODULE(WindowModule, windowModule);
		RendererConfig rendererConfig;
		rendererConfig.renderAPIConfig.windowHandle = windowModule->GetPrimaryWindow()->GetHandle();
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
	}

	void RendererModule::UpdateModule(float deltaTime)
	{
		// This runs on main thread

		// TODO: This won't need to be called as the renderer will be running on its own thread
		m_Renderer->Render(deltaTime);
	}

	void RendererModule::ShutdownModule()
	{
		TYR_SAFE_DELETE(m_Renderer);
		m_RenderAPI->Shutdown();
	}

	Device* RendererModule::GetDevice() const
	{
		return m_RenderAPI->GetDevice();
	}
}