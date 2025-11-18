#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "Rendering/RendererConfig.h"
#include "RenderAPI/RenderAPI.h"

namespace tyr
{
	class Renderer;
	class Device;
	struct RenderUpdateData;

	class TYR_RENDERER_EXPORT RendererModule final : public IModule
	{
	public:
		RendererModule();
		~RendererModule();

		void InitializeModule() override;

		void ShutdownModule() override;

		void UpdateModule(float deltaTime) override;
	
		Device* GetDevice() const;

		Renderer* GetRenderer() const { return m_Renderer; }

	private:
		Ref<RenderAPI> m_RenderAPI;
		Renderer* m_Renderer;
	};
	
}