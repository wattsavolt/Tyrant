#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RendererConfig.h"
#include "RenderAPI/RenderAPI.h"

namespace tyr
{
	class SceneRenderer;
	class Device;
	struct RenderUpdateData;

	class TYR_RENDERER_EXPORT Renderer final : INonCopyable
	{
	public:
		Renderer();
		~Renderer();

		void Initialize(const RendererConfig& rendererConfig, uint& surfaceWidth, uint& surfaceHeight);
		void Shutdown();
		void Render(double deltaTime, const RenderUpdateData& updateData);
		bool IsInitialized();
	
		Device* GetDevice() const;

	private:
		static bool s_Instantiated;
		bool m_Initialized;
		Ref<RenderAPI> m_RenderAPI;
		URef<SceneRenderer> m_SceneRenderer;
	};
	
}