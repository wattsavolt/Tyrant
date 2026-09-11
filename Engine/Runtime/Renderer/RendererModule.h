#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "Rendering/RendererConfig.h"
#include "RenderAPI/RenderAPI.h"

namespace tyr
{
	class Renderer;
	class RendererAPI;
	class Device;
	class RenderAPI;
	class WindowModule;
	struct RenderUpdateData;

	class TYR_RENDERER_API RendererModule final : public IModule
	{
	public:
		RendererModule();
		~RendererModule();

		void Initialize() override;

		void Shutdown() override;

		void Update(float deltaTime) override;

		void EndFrame() override;
	
		Device* GetDevice() const;

		RendererAPI* GetRendererAPI() const { return m_RendererAPI; }

	private:
		RenderAPI* m_RenderAPI{};
		Renderer* m_Renderer{};
		RendererAPI* m_RendererAPI{};
		WindowModule* m_WindowModule{};
	};
	
}