

#include "Renderer.h"
#include "GraphicsManager.h"
#include "SceneRenderer.h"
#include "RenderUpdate/RenderUpdate.h"
#include "RenderAPI/SwapChain.h"

namespace tyr
{
	bool Renderer::s_Instantiated = false;

	Renderer::Renderer()
		: m_Initialized(false)
	{
		TYR_ASSERT(!s_Instantiated);
		s_Instantiated = true;
	}

	Renderer::~Renderer()
	{
		s_Instantiated = false;
	}

	void Renderer::Initialize(const RendererConfig& rendererConfig, uint& surfaceWidth, uint& surfaceHeight)
	{
		TYR_ASSERT(!m_Initialized);

		m_RenderAPI = GraphicsManager::CreateRenderAPI(rendererConfig.renderAPICreateConfig);
		m_RenderAPI->Initialize(rendererConfig.renderAPIConfig);

		m_SceneRenderer = MakeURef<SceneRenderer>(rendererConfig, m_RenderAPI);

		surfaceWidth = m_RenderAPI->GetSwapChain()->GetWidth();
		surfaceHeight = m_RenderAPI->GetSwapChain()->GetHeight();

		m_Initialized = true;
	}

	void Renderer::Shutdown()
	{
		TYR_ASSERT(m_Initialized);

		m_SceneRenderer->WaitForCompletion();
		m_SceneRenderer.reset(nullptr);
		m_RenderAPI->Shutdown();

		m_Initialized = false;
	}

	bool Renderer::IsInitialized()
	{
		return m_Initialized;
	}

	void Renderer::Render(double deltaTime, const RenderUpdateData& updateData)
	{
		m_SceneRenderer->Render(deltaTime, updateData);
	}


}