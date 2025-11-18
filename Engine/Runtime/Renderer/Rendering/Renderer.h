#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "Containers/SPSCRingBuffer.h"
#include "RenderDataTypes/RenderDataTypes.h"
#include "Shaders/ShaderTypes.h"
#include "RendererConfig.h"
#include "RenderAPI/CommandList.h"
#include "Shader/ShaderCreator.h"
#include "Resources/RenderBuffer.h"
#include "RenderUpdate/RenderFrame.h"

namespace tyr
{
	class RenderAPI;
	class Device;
	class CommandAllocator;
	struct BufferBindingUpdate;
	struct MaterialData
	{
		Vector3 albedo;
		float roughness;
		float metallic;
	};

	class TYR_RENDERER_EXPORT Renderer final : INonCopyable
	{
	public:
		Renderer(const RendererConfig& rendererConfig, Ref<RenderAPI>& renderAPI);
		~Renderer();

		void Render(double deltaTime);

		bool TryAddFrame(const RenderFrame* frame);

		// Wait for all rendering operations to be complete
		void WaitForCompletion();
	
	private:
		RenderPassHandle CreateRenderPass();
		void CreateShaders();
		void CreatePipeline();
		void CreateBuffers();
		void PerformStaticTransfers();
		void PeformDynamicTransfers();
		void AddRenderBarriers();
		void CreateCommandAllocators();
		void CreateCommandLists();
		void CreateBufferBindingUpdate(BufferBindingUpdate& bindingUpdate, BufferViewHandle bufferView, uint descriptorIndex, uint bindingIndex);

		static bool s_Instantiated;

		Ref<RenderAPI> m_RenderAPI;
		SwapChain* m_SwapChain;
		Device* m_Device;
		ShaderCreator m_ShaderCreator;
		SPSCRingBuffer<const RenderFrame*, RenderFrame::c_MaxRenderFrames> m_FrameUpdateQueue;
		GraphicsPipelineHandle m_Pipeline;
		RenderBuffer m_VertexBuffer;
		RenderBuffer m_IndexBuffer;
		RenderBuffer m_InstanceBuffer;
		RenderBuffer m_SpotLightBuffer;
		RenderBuffer m_MaterialBuffer;
		RenderBuffer m_SceneInfoBuffer;
		RenderBuffer m_TransferBuffers[6];
		Array<BufferHandle> m_VertexBuffers;
		ShaderModuleHandle m_VertexShader;
		ShaderModuleHandle m_PixelShader;
		DescriptorPoolHandle m_DescriptorPool;
		DescriptorSetLayoutHandle m_DescriptorSetLayout;
		DescriptorSetGroupHandle m_DescriptorSetGroup;
		CommandAllocator* m_CommandAllocator{ nullptr };
		CommandList* m_CommandList{ nullptr };
		MaterialTextureData m_MaterialTextureData;
		RendererConfig m_Config;
		ShaderSceneInfo m_SceneInfo;
		RenderingInfo m_RenderingInfo;
		CommndListExecuteDesc m_ExecuteDesc;
		FenceHandle m_Fence;
		SemaphoreHandle m_AquireSwapChainImageSemaphores[3];
		SemaphoreHandle m_ExecuteCompleteSemaphores[3];
		SemaphoreHandle m_CompletionSemaphore;
		uint64 m_CompletionSemaphoreSignalValue;
		uint8 m_SemaphoreIndex;
		Viewport m_Viewport;
		uint m_SwapChainImageIndex;
		bool m_FirstRender;
		bool m_SceneUpdated;
	};
	
}