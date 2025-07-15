#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderUpdate/RenderUpdate.h"
#include "RenderDataTypes/RenderDataTypes.h"
#include "Shaders/ShaderTypes.h"
#include "RendererConfig.h"
#include "RenderAPI/CommandList.h"

namespace tyr
{
	class RenderAPI;
	class Device;
	class GraphicsPipeline;
	class RenderPass;
	class Buffer;
	class BufferView;
	class GpuBuffer;
	class Shader;
	class CommandAllocator;
	class DescriptorPool;
	class DescriptorSetLayout;
	class DescriptorSetGroup;
	class Fence;
	class Semaphore;
	struct BufferBindingUpdate;

	struct MaterialData
	{
		Vector3 albedo;
		float roughness;
		float metallic;
	};

	class TYR_RENDERER_EXPORT SceneRenderer final : INonCopyable
	{
	public:
		SceneRenderer(const RendererConfig& rendererConfig, Ref<RenderAPI> renderAPI);
		~SceneRenderer();

		void Render(double deltaTime, const RenderUpdateData& updateData);

		// Wait for all rendering operations to be complete
		void WaitForCompletion();
	
	private:
		void Shutdown();
		Ref<RenderPass> CreateRenderPass();
		void CreateShaders();
		void CreatePipeline();
		void CreateBuffers();
		void PerformStaticTransfers();
		void PeformDynamicTransfers();
		void AddRenderBarriers();
		void CreateCommandAllocators();
		void CreateCommandLists();
		void CreateBufferBindingUpdate(BufferBindingUpdate& bindingUpdate, ORef<BufferView>& bufferView, uint descriptorIndex, uint bindingIndex);

		static bool s_Instantiated;

		Ref<RenderAPI> m_RenderAPI;
		SwapChain* m_SwapChain;
		Device* m_Device;
		Ref<GraphicsPipeline> m_Pipeline;
		Ref<GpuBuffer> m_VertexBuffer;
		Ref<GpuBuffer> m_IndexBuffer;
		Ref<GpuBuffer> m_InstanceBuffer;
		Array<Buffer*> m_VertexBuffers;
		Ref<GpuBuffer> m_SpotLightBuffer;
		Ref<GpuBuffer> m_MaterialBuffer;
		Ref<GpuBuffer> m_SceneInfoBuffer;
		Ref<Shader> m_VertexShader;
		Ref<Shader> m_PixelShader;
		Ref<DescriptorPool> m_DescriptorPool;
		Ref<DescriptorSetLayout> m_DescriptorSetLayout;
		Ref<DescriptorSetGroup> m_DescriptorSetGroup;
		Ref<CommandAllocator> m_CommandAllocator;
		Ref<CommandList> m_CommandList;
		MaterialTextureData m_MaterialTextureData;
		RendererConfig m_Config;
		ShaderSceneInfo m_SceneInfo;
		RenderingInfo m_RenderingInfo;
		CommndListExecuteDesc m_ExecuteDesc;
		Ref<Fence> m_Fence;
		Ref<Semaphore> m_AquireSwapChainImageSemaphores[3];
		Ref<Semaphore> m_ExecuteCompleteSemaphores[3];
		Ref<Semaphore> m_CompletionSemaphore;
		uint64 m_CompletionSemaphoreSignalValue;
		uint8 m_SemaphoreIndex;
		Viewport m_Viewport;
		uint m_SwapChainImageIndex;
		bool m_FirstRender;
		bool m_SceneUpdated;
	};
	
}