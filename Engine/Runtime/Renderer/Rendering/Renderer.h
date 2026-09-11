#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "Containers/SPSCRingBuffer.h"
#include "Shaders/ShaderTypes.h"
#include "RendererConfig.h"
#include "RenderAPI/CommandList.h"
#include "Shader/ShaderCreator.h"
#include "RenderResource/RenderBuffer.h"
#include "RenderFrame.h"
#include "RenderWindow.h"
#include "RenderContext.h"
#include "RenderResources.h"
#include "RenderData.h"
#include "RenderRegistry.h"
#include "RenderAllocationManager.h"
#include "Window/WindowConstants.h"

namespace tyr
{
	class RenderAPI;
	class Device;
	class SwapChain;
	class CommandAllocator;
	class TransferPass;
	class GeometryPass;
	class RenderSubmissionThread;
	struct BufferBindingUpdate;

	struct RenderSyncData
	{
		// Used for signalling resource upload allocators and these uploads done on transfer queue
		uint64 resourceTransferTimelineValue = 0;
		// The last timeline value when this frame was rendered
		// Also used for signalling frame upload allocator as frame uploads done on graphics queue
		uint64 graphicsTimelineValue = 0;
	};

	class Renderer final : INonCopyable
	{
	public:
		Renderer(const RendererConfig& rendererConfig, RenderAPI* renderAPI);
		~Renderer();

		// Needs to be called each frame
		void Render(float deltaTime);

		// Needs to be called at end of each frame
		void PrepareForNextFrame();

		// Wait for all rendering operations to be complete
		void WaitForCompletion();

		RenderWindowHandle AddWindow(void* osHandle);

		void RemoveWindow(RenderWindowHandle window);

		void ResizeWindow(RenderWindowHandle window, uint width, uint height);

		ShaderCreator& GetShaderCreator() { return m_ShaderCreator; }

		RenderFrame& GetRenderFrame()
		{
			return m_RenderFrames[m_RenderFrameIndex];
		}

		RenderFrame& GetPrevRenderFrame()
		{
			const uint index = Utility::GetPrevCircularIndex(m_RenderFrameIndex, RenderConstants::c_BufferedFrameCount);
			return m_RenderFrames[index];
		}

		RenderResources& GetRenderResources() { return m_Resources; }

		RenderData& GetRenderData() { return m_Data; }

		RenderAllocationManager& GetAllocationManager() { return m_AllocManager; }

	private:
		void RenderAsync(uint renderFrameIndex);
		void BuildAndExecuteRenderGraph(uint renderFrameIndex);

		void CreateShaders();
		void DeleteShaders();
		// No CreateSwapChains function as they are created as needed but reused so need to be deleted at the end
		void DeleteSwapChains();
		void CreateCommandObjects();
		void DeleteCommandObjects();
		void CreatePipelines();
		void DeletePipelines();
		void CreateBuffers();
		void DeleteBuffers();
		void CreateSamplers();
		void DeleteSamplers();
		void CreatePasses();
		void DeletePasses();

		// Probably only ever useful if supporting mobile devices. Unused but kept as an example
		RenderPassHandle CreateRenderPass();

		static bool s_Instantiated;

		RenderAPI* m_RenderAPI;
		RenderSubmissionThread* m_RenderSubmissionThread;
		ShaderCreator m_ShaderCreator;	
		ShaderMaterial m_ShaderMaterial;
		RendererConfig m_Config;
		RenderRegistry m_Registry;
		RenderAllocationManager m_AllocManager{};
		RenderFrame m_RenderFrames[RenderConstants::c_BufferedFrameCount];
		RenderSyncData m_SyncDatas[RenderConstants::c_BufferedFrameCount];
		LocalObjectPool<RenderWindow, WindowConstants::c_MaxWindows> m_WindowPool;
		Array<SwapChain*> m_SwapChains;
		HashMap<uint, uint> m_ViewIdIndexMap;
		RenderContext m_Ctx{};
		RenderResources m_Resources{};
		RenderData m_Data{};
		uint m_RenderFrameIndex = 0;
		bool m_FirstRender = false;
	};
	
}