#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"
#include "Math/Matrix4.h"

namespace tyr
{
	struct RenderAPICreateConfig
	{
		RenderAPIBackend backend;
	};

	struct RenderAPIConfig
	{
		String appName = "";
	};

	class Device;
	class SwapChain;

	/// Class repesenting the rendering API 
	class TYR_GRAPHICS_API RenderAPI
	{
	public:
		void Initialize(const RenderAPIConfig& config);
		void Shutdown();

		Device* GetDevice() { return m_Device; }

		static const bool m_ValidationLayersEnabled;

	protected:
		friend class GraphicsManager;
		RenderAPI();
		virtual ~RenderAPI();	

		virtual void InitializeAPI() = 0;
		virtual void ShutdownAPI() = 0;

		RenderAPIConfig m_Config;
		Device* m_Device;
		bool m_Initialized;
	};
}
