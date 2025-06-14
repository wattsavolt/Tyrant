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
		uint multiSamplingCount = 4;
		uint refreshRate = 60;
		PixelFormat outputPixelFormat = PF_R8G8B8A8_SRGB;
		ColorSpace outputColorSpace = CP_SRGB_NONLINEAR;
		String appName = "";
		bool vSyncEnabled = false;
		// Should enable when using v-sync
		bool useTripleBuffering = false;
		bool windowed = true;
		bool multiSamplingEnabled = true;
		Handle windowHandle = nullptr;
		Handle moduleHandle = nullptr;
	};

	class Device;
	class SwapChain;

	/// Class repesenting the rendering API 
	class TYR_GRAPHICS_EXPORT RenderAPI
	{
	public:
		void Initialize(const RenderAPIConfig& config);
		void Shutdown();

		virtual inline Matrix4 CreateShaderMatrix(const Matrix4& mat) const = 0;

		Device* GetDevice() { return m_Device; }

		SwapChain* GetSwapChain() { return m_SwapChain; }

		static const bool m_ValidationLayersEnabled;

	protected:
		friend class GraphicsManager;
		RenderAPI();
		virtual ~RenderAPI();	

		virtual void InitializeAPI() = 0;
		virtual void ShutdownAPI() = 0;

		RenderAPIConfig m_Config;
		// Wrapper around the logical device.
		Device* m_Device;
		SwapChain* m_SwapChain;
		bool m_Initialized;
	};
}
