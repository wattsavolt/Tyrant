#include "Renderer.h"
#include "GraphicsManager.h"
#include "RenderAPI/GraphicsUtility.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/SwapChain.h"
#include "RenderAPI/Pipeline.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/ShaderModule.h"
#include "RenderAPI/CommandAllocator.h"
#include "RenderAPI/CommandList.h"
#include "RenderAPI/DescriptorSetGroup.h"
#include "RenderAPI/Sync.h"
#include "Math/Matrix4.h"

namespace tyr
{
	bool Renderer::s_Instantiated = false;

	Renderer::Renderer(const RendererConfig& rendererConfig, Ref<RenderAPI>& renderAPI)
		: m_Config(rendererConfig)
		, m_SwapChainImageIndex(0)
		, m_FirstRender(true)
		, m_SceneUpdated(false)
		, m_RenderAPI(renderAPI)
		// Value must be greater than the initial value (0) the semaphore was created with 
		, m_CompletionSemaphoreSignalValue(1)
		, m_SemaphoreIndex(0)
		, m_Device(m_RenderAPI->GetDevice())
		, m_SwapChain(m_RenderAPI->GetSwapChain())
		, m_ShaderCreator(*m_Device, rendererConfig.shaderConfig)
	{
		TYR_ASSERT(!s_Instantiated);

		CreateShaders();
		CreatePipeline();
		CreateBuffers();
		CreateCommandAllocators();
		CreateCommandLists();

		m_SceneInfo.viewProj = Matrix4::c_Identity;
		m_SceneInfo.camPos = Vector3::c_Zero;
		m_SceneInfo.ambient = 0.0f;

		m_RenderingInfo.renderArea.offset = { 0, 0 };
		m_RenderingInfo.renderArea.extents = { static_cast<uint>(m_SwapChain->GetWidth()),
			static_cast<uint>(m_SwapChain->GetHeight()) };
		m_RenderingInfo.viewMask = 0;
		m_RenderingInfo.layerCount = 1;

		RenderingAttachmentInfo colourAttachment;
		colourAttachment.loadOp = AttachmentLoadOp::Clear;
		colourAttachment.storeOp = AttachmentStoreOp::Store;
		colourAttachment.resolveMode = RESOLVE_MODE_NONE;
		colourAttachment.imageLayout = m_SwapChain->GetRenderingLayout();
		colourAttachment.clearValue.colour = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_RenderingInfo.colourAttachments.Add(std::move(colourAttachment));

		s_Instantiated = true;
	}

	Renderer::~Renderer()
	{
		WaitForCompletion();

		TYR_SAFE_DELETE(m_CommandList);
		TYR_SAFE_DELETE(m_CommandAllocator);
		m_Device->DeleteGraphicsPipeline(m_Pipeline);
		m_Device->DeleteDescriptorSetGroup(m_DescriptorSetGroup);
		m_Device->DeleteDescriptorSetLayout(m_DescriptorSetLayout);
		m_Device->DeleteDescriptorPool(m_DescriptorPool);
		for (uint i = 0; i < 6; ++i)
		{
			RenderBufferUtil::DeleteBuffer(m_TransferBuffers[i], *m_Device);
		}
		RenderBufferUtil::DeleteBuffer(m_VertexBuffer, *m_Device);
		RenderBufferUtil::DeleteBuffer(m_IndexBuffer, *m_Device);
		RenderBufferUtil::DeleteBuffer(m_InstanceBuffer, *m_Device);
		RenderBufferUtil::DeleteBuffer(m_SpotLightBuffer, *m_Device);
		RenderBufferUtil::DeleteBuffer(m_MaterialBuffer, *m_Device);
		RenderBufferUtil::DeleteBuffer(m_SceneInfoBuffer, *m_Device);
		for (uint i = 0; i < 3; ++i)
		{
			m_Device->DeleteSemaphoreResource(m_AquireSwapChainImageSemaphores[i]);
			m_Device->DeleteSemaphoreResource(m_ExecuteCompleteSemaphores[i]);
		}
		m_Device->DeleteSemaphoreResource(m_CompletionSemaphore);
		m_Device->DeleteFence(m_Fence);
		m_Device->DeleteShaderModule(m_VertexShader);
		m_Device->DeleteShaderModule(m_PixelShader);
		ShaderCreator::UnloadCompilerLibs();
		s_Instantiated = false;
	}

	void Renderer::Render(double deltaTime)
	{
		m_SwapChainImageIndex = m_SwapChain->AcquireNextImage(m_AquireSwapChainImageSemaphores[m_SemaphoreIndex]);

		const float windowWidth = m_SwapChain->GetWidth();
		const float windowHeight = m_SwapChain->GetHeight();
		m_Viewport.width = windowWidth;
		m_Viewport.height = windowHeight;

		Optional<const RenderFrame*> rf = m_FrameUpdateQueue.ReadOnly();
		if (rf)
		{
			const RenderFrame& renderFrame = *rf.value();
			const SceneFrame& sceneFrame = renderFrame.sceneFrames[0];
			const SceneView& sceneView = sceneFrame.view;
			const Matrix4 view = Matrix4::CreateView(sceneView.camera.position, sceneView.camera.forward, sceneView.camera.up);
			// Do reverse-z for greater floating-point precision
			const Matrix4 projection = Matrix4::CreatePerspective(sceneView.camera.fov, windowWidth / windowHeight, sceneView.camera.farZ, sceneView.camera.nearZ);
			m_SceneInfo.viewProj = view * projection;
			m_SceneInfo.camPos = sceneView.camera.position;
			m_RenderingInfo.renderArea.offset = 
			{ 
				static_cast<int>(sceneView.viewArea.x * windowWidth),
				static_cast<int>(sceneView.viewArea.y * windowHeight)
			};
			m_RenderingInfo.renderArea.extents =
			{
				static_cast<uint>(sceneView.viewArea.width * windowWidth),
				static_cast<uint>(sceneView.viewArea.height * windowHeight)
			};
			m_SceneUpdated = true;
		}
		else
		{
			m_SceneUpdated = false;
		}

		if (!m_FirstRender)
		{
			m_Device->WaitForFence(m_Fence, UINT32_MAX);
			m_Device->ResetFence(m_Fence);
		}

		m_RenderingInfo.colourAttachments[0].imageView = m_SwapChain->GetImageViews()[m_SwapChainImageIndex];

		m_CommandList->Reset(true);
		m_CommandList->Begin(CommandBufferUsage::COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		
		if (m_FirstRender)
		{
			BufferBindingUpdate bindingUpdates[3];
			uint bindingIndex = 0;
			CreateBufferBindingUpdate(bindingUpdates[bindingIndex], m_SceneInfoBuffer.bufferView, 0, bindingIndex);
			bindingIndex++;
			CreateBufferBindingUpdate(bindingUpdates[bindingIndex], m_SpotLightBuffer.bufferView, 0, bindingIndex);
			bindingIndex++;
			CreateBufferBindingUpdate(bindingUpdates[bindingIndex], m_MaterialBuffer.bufferView, 0, bindingIndex);
			m_Device->UpdateDescriptorSetGroup(m_DescriptorSetGroup, bindingUpdates, 3);
			PerformStaticTransfers();
		}
		PeformDynamicTransfers();
		AddRenderBarriers();

		m_CommandList->BeginRendering(m_RenderingInfo);
		m_CommandList->SetViewport(&m_Viewport, 1);	
		m_CommandList->SetScissor(&m_RenderingInfo.renderArea, 1);
		m_CommandList->BindGraphicsPipeline(m_Pipeline);
		m_CommandList->BindVertexBuffers(m_VertexBuffers.Data(), m_VertexBuffers.Size());
		m_CommandList->BindIndexBuffer(m_IndexBuffer.bufferView);
		m_CommandList->BindDescriptorSet(m_DescriptorSetGroup, m_Pipeline);
		m_CommandList->DrawIndexed(Cube::c_NumIndices, 1, 0, 0, 0);
		m_CommandList->EndRendering();
		m_CommandList->End();

		m_ExecuteDesc.waitSemaphores.Clear();
		m_ExecuteDesc.waitSemaphores.Add(m_AquireSwapChainImageSemaphores[m_SemaphoreIndex]);
		m_ExecuteDesc.signalSemaphores.Clear();
		m_ExecuteDesc.signalSemaphores.Add(m_ExecuteCompleteSemaphores[m_SemaphoreIndex]);
		m_ExecuteDesc.signalSemaphores.Add(m_CompletionSemaphore);
		m_ExecuteDesc.signalValues.Clear();
		// A dummy value needs to be pushed back for the binary semaphore as the count of the number of values must match the number of semaphores
		// (as per the vulkan spec)
		m_ExecuteDesc.signalValues.Add(0);
		m_ExecuteDesc.signalValues.Add(m_CompletionSemaphoreSignalValue);

		// Wait on semaphore used when acquiring next swapchain image
		m_CommandList->Execute(&m_ExecuteDesc, 1, 0, m_Fence);
		// Wait on signal semaphore used in execute
		m_SwapChain->Present(m_CommandList, m_ExecuteCompleteSemaphores[m_SemaphoreIndex], m_SwapChainImageIndex);

		m_SemaphoreIndex = (m_SemaphoreIndex + 1) % 3;
		m_CompletionSemaphoreSignalValue++;

		m_FirstRender = false;
	}

	void Renderer::PerformStaticTransfers()
	{
		constexpr uint bufferCount = 5;
		constexpr uint bufferBarrierCount = bufferCount * 2;
		BufferBarrier bufferBarriers[bufferBarrierCount];
		RenderBuffer* buffers[bufferCount] = { &m_VertexBuffer,  &m_IndexBuffer, &m_InstanceBuffer, &m_SpotLightBuffer, &m_MaterialBuffer };
		uint index = 0;
		for (uint i = 0; i < bufferCount; ++i)
		{
			BufferBarrier& upload = bufferBarriers[index++];
			BufferBarrier& gpu = bufferBarriers[index++];
			RenderBufferUtil::CreateTransferReadBarrier(upload, m_TransferBuffers[i].buffer);
			RenderBufferUtil::CreateTransferWriteBarrier(gpu , buffers[i]->buffer);
		}
		m_CommandList->AddBarriers(bufferBarriers, bufferBarrierCount);
		for (uint i = 0; i < bufferCount; ++i)
		{
			m_CommandList->CopyBuffer(m_TransferBuffers[i].buffer, buffers[i]->buffer);
		}
	}

	void Renderer::PeformDynamicTransfers()
	{
		if (!m_SceneUpdated)
		{
			return;
		}
		BufferBarrier bufferBarriers[2];
		uint index = 0;
		BufferBarrier& upload = bufferBarriers[index++];
		BufferBarrier& gpu = bufferBarriers[index];
		m_Device->WriteBuffer(m_TransferBuffers[5].buffer, reinterpret_cast<const void*>(&m_SceneInfo), 0, sizeof(ShaderSceneInfo));
		RenderBufferUtil::CreateTransferReadBarrier(upload, m_TransferBuffers[5].buffer);
		RenderBufferUtil::CreateTransferWriteBarrier(gpu, m_SceneInfoBuffer.buffer);
		m_CommandList->AddBarriers(bufferBarriers, 2);
		m_CommandList->CopyBuffer(m_TransferBuffers[5].buffer, m_SceneInfoBuffer.buffer);
	}

	void Renderer::AddRenderBarriers()
	{
		uint bufferBarrierCount = m_FirstRender ? 6 : 1;
		BufferBarrier* bufferBarriers = StackNew<BufferBarrier>(bufferBarrierCount);
		{
			uint index = 0;
			if (m_FirstRender)
			{
				bufferBarriers[index].buffer = m_VertexBuffer.buffer;
				bufferBarriers[index].srcAccess = BARRIER_ACCESS_TRANSFER_WRITE_BIT;
				bufferBarriers[index].dstAccess = BARRIER_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
				bufferBarriers[index].srcStage = PIPELINE_STAGE_TRANSFER_BIT;
				bufferBarriers[index++].dstStage = PIPELINE_STAGE_VERTEX_INPUT_BIT;

				bufferBarriers[index].buffer = m_IndexBuffer.buffer;
				bufferBarriers[index].srcAccess = BARRIER_ACCESS_TRANSFER_WRITE_BIT;
				bufferBarriers[index].dstAccess = BARRIER_ACCESS_INDEX_READ_BIT;
				bufferBarriers[index].srcStage = PIPELINE_STAGE_TRANSFER_BIT;
				bufferBarriers[index++].dstStage = PIPELINE_STAGE_VERTEX_INPUT_BIT;

				bufferBarriers[index].buffer = m_InstanceBuffer.buffer;
				bufferBarriers[index].srcAccess = BARRIER_ACCESS_TRANSFER_WRITE_BIT;
				bufferBarriers[index].dstAccess = BARRIER_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
				bufferBarriers[index].srcStage = PIPELINE_STAGE_TRANSFER_BIT;
				bufferBarriers[index++].dstStage = PIPELINE_STAGE_VERTEX_INPUT_BIT;

				bufferBarriers[index].buffer = m_SpotLightBuffer.buffer;
				bufferBarriers[index].srcAccess = BARRIER_ACCESS_TRANSFER_WRITE_BIT;
				bufferBarriers[index].dstAccess = BARRIER_ACCESS_UNIFORM_READ_BIT;
				bufferBarriers[index].srcStage = PIPELINE_STAGE_TRANSFER_BIT;
				bufferBarriers[index++].dstStage = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

				bufferBarriers[index].buffer = m_MaterialBuffer.buffer;
				bufferBarriers[index].srcAccess = BARRIER_ACCESS_TRANSFER_WRITE_BIT;
				bufferBarriers[index].dstAccess = BARRIER_ACCESS_UNIFORM_READ_BIT;
				bufferBarriers[index].srcStage = PIPELINE_STAGE_TRANSFER_BIT;
				bufferBarriers[index++].dstStage = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			
			bufferBarriers[index].buffer = m_SceneInfoBuffer.buffer;
			bufferBarriers[index].srcAccess = BARRIER_ACCESS_TRANSFER_WRITE_BIT;
			bufferBarriers[index].dstAccess = BARRIER_ACCESS_UNIFORM_READ_BIT;
			bufferBarriers[index].srcStage = PIPELINE_STAGE_TRANSFER_BIT;
			bufferBarriers[index].dstStage = PIPELINE_STAGE_VERTEX_SHADER_BIT;
		}
		ImageBarrier imageBarriers[2];
		{
			m_SwapChain->CreateRenderingImageBarrier(imageBarriers[0], m_SwapChainImageIndex);
			m_SwapChain->CreatePresentingImageBarrier(imageBarriers[1], m_SwapChainImageIndex);
		}	
		m_CommandList->AddBarriers(bufferBarriers, bufferBarrierCount, imageBarriers, 2);
		StackDelete(bufferBarriers, bufferBarrierCount);
	}

	void Renderer::CreateShaders()
	{
		ShaderCreator::LoadCompilerLibs();
		ShaderCompileConfig shaderCompileConfig;
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "MeshVS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_VERTEX_BIT;
			m_VertexShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "MeshPS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_FRAGMENT_BIT;
			m_PixelShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
	}

	RenderPassHandle Renderer::CreateRenderPass()
	{
		RenderPassDesc desc;
		AttachmentDesc colorAttachment{};
		colorAttachment.format = m_RenderAPI->GetSwapChain()->GetDesc().pixelFormat;
		colorAttachment.samples = SampleCount::OneBit;
		colorAttachment.loadOp = AttachmentLoadOp::Clear;
		colorAttachment.storeOp = AttachmentStoreOp::Store;
		colorAttachment.stencilLoadOp = AttachmentLoadOp::DontCare;
		colorAttachment.stencilStoreOp = AttachmentStoreOp::DontCare;
		colorAttachment.initialLayout = IMAGE_LAYOUT_UNKNOWN;
		colorAttachment.finalLayout = IMAGE_LAYOUT_PRESENT_SRC;

		// References above attachment
		AttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachmentIndex = 0;
		colorAttachmentRef.layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		SubpassDesc subpassDesc;
		subpassDesc.pipelineType = PipelineType::Graphics;
		subpassDesc.colorAttachments.Add(std::move(colorAttachmentRef));

		desc.attachments.Add(std::move(colorAttachment));
		desc.subpasses.Add(std::move(subpassDesc));
		// Add render pass dependencies later when needed.

		return m_Device->CreateRenderPass(desc);
	}

	void Renderer::CreatePipeline()
	{
		GraphicsPipelineDesc desc;
		{
			DescriptorPoolDesc poolDesc;
			poolDesc.maxSets = 1;
			DescriptorPoolSize poolSize;
			poolSize.descriptorCount = 3;
			poolSize.descriptorType = DescriptorType::UniformBuffer;
			poolDesc.poolSizes.Add(std::move(poolSize));
#if !TYR_FINAL
			poolDesc.debugName = "DescriptorPool";
#endif
			m_DescriptorPool = m_Device->CreateDescriptorPool(poolDesc);

			DescriptorSetLayoutDesc layoutDesc;
			// Change the flags to add extra functionality.
			layoutDesc.flags = DESCRIPTOR_SET_LAYOUT_NONE;
			{
				DescriptorSetLayoutBinding& binding = layoutDesc.bindings.ExpandOne();
				binding.binding = 0;
				// Only more than one for array of same type.
				binding.descriptorCount = 1;
				binding.descriptorType = DescriptorType::UniformBuffer;
				binding.stageFlags = static_cast<ShaderStage>(SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT);
			}
			{
				DescriptorSetLayoutBinding& binding = layoutDesc.bindings.ExpandOne();
				binding.binding = 1;
				// Only more than one for array of same type.
				binding.descriptorCount = 1;
				binding.descriptorType = DescriptorType::UniformBuffer;
				binding.stageFlags = SHADER_STAGE_FRAGMENT_BIT;
			}
			{
				DescriptorSetLayoutBinding& binding = layoutDesc.bindings.ExpandOne();
				binding.binding = 2;
				binding.descriptorCount = 1;
				binding.descriptorType = DescriptorType::UniformBuffer;
				binding.stageFlags = SHADER_STAGE_FRAGMENT_BIT;
			}

			DescriptorSetGroupDesc groupDesc;
			m_DescriptorSetLayout = m_Device->CreateDescriptorSetLayout(layoutDesc);
			groupDesc.layouts.Add(m_DescriptorSetLayout);
			groupDesc.pool = m_DescriptorPool;
#if !TYR_FINAL
			poolDesc.debugName = "DescriptorSetGroup";
#endif
			m_DescriptorSetGroup = m_Device->CreateDescriptorSetGroup(groupDesc);

			desc.pipelineLayoutDesc.descriptorSetLayouts.Add(groupDesc.layouts[0]);
		}

		desc.topology = PrimitiveTopology::TriangeList;

		// Vertex layout
		desc.vertexInputStateDesc.attributeDescriptions.Resize(6);
		// Position
		desc.vertexInputStateDesc.attributeDescriptions[0].binding = 0;
		desc.vertexInputStateDesc.attributeDescriptions[0].location = 0;
		desc.vertexInputStateDesc.attributeDescriptions[0].format = PF_R32G32B32_SFLOAT;
		desc.vertexInputStateDesc.attributeDescriptions[0].offset = 0;
		// Normal
		desc.vertexInputStateDesc.attributeDescriptions[1].binding = 0;
		desc.vertexInputStateDesc.attributeDescriptions[1].location = 1;
		desc.vertexInputStateDesc.attributeDescriptions[1].format = PF_R32G32B32_SFLOAT;
		desc.vertexInputStateDesc.attributeDescriptions[1].offset = 12;
		// World Matrix
		desc.vertexInputStateDesc.attributeDescriptions[2].binding = 1;
		desc.vertexInputStateDesc.attributeDescriptions[2].location = 2;
		desc.vertexInputStateDesc.attributeDescriptions[2].format = PF_R32G32B32A32_SFLOAT;
		desc.vertexInputStateDesc.attributeDescriptions[2].offset = 0;
		desc.vertexInputStateDesc.attributeDescriptions[3].binding = 1;
		desc.vertexInputStateDesc.attributeDescriptions[3].location = 3;
		desc.vertexInputStateDesc.attributeDescriptions[3].format = PF_R32G32B32A32_SFLOAT;
		desc.vertexInputStateDesc.attributeDescriptions[3].offset = 16;
		desc.vertexInputStateDesc.attributeDescriptions[4].binding = 1;
		desc.vertexInputStateDesc.attributeDescriptions[4].location = 4;
		desc.vertexInputStateDesc.attributeDescriptions[4].format = PF_R32G32B32A32_SFLOAT;
		desc.vertexInputStateDesc.attributeDescriptions[4].offset = 32;
		desc.vertexInputStateDesc.attributeDescriptions[5].binding = 1;
		desc.vertexInputStateDesc.attributeDescriptions[5].location = 5;
		desc.vertexInputStateDesc.attributeDescriptions[5].format = PF_R32G32B32A32_SFLOAT;
		desc.vertexInputStateDesc.attributeDescriptions[5].offset = 48;

		VertexInputBindingDesc& vertBindingDesc = desc.vertexInputStateDesc.bindingDescriptions.ExpandOne();
		vertBindingDesc.binding = 0;
		vertBindingDesc.stride = sizeof(Vertex);
		vertBindingDesc.inputRate = VertexInputRate::Vertex;

		VertexInputBindingDesc& instBindingDesc = desc.vertexInputStateDesc.bindingDescriptions.ExpandOne();
		instBindingDesc.binding = 1;
		instBindingDesc.stride = sizeof(Matrix4);
		instBindingDesc.inputRate = VertexInputRate::Instance;

		// Blend description
		desc.blendStateDesc.srcColorBlendFactor = BlendFactor::One;
		desc.blendStateDesc.destColorBlendFactor = BlendFactor::Zero;
		desc.blendStateDesc.colorBlendOp = BlendOp::Add;
		desc.blendStateDesc.srcAlphaBlendFactor = BlendFactor::One;
		desc.blendStateDesc.destAlphaBlendFactor = BlendFactor::Zero;
		desc.blendStateDesc.alphaBlendOp = BlendOp::Add;
		desc.blendStateDesc.colorWriteMask = static_cast<ColorComponent>(COLOR_COMPONENT_R_BIT | COLOR_COMPONENT_G_BIT | COLOR_COMPONENT_B_BIT | COLOR_COMPONENT_A_BIT);
		desc.blendStateDesc.blendEnabled = false;

		// Rasterization description
		desc.rasterizerStateDesc.depthBiasConstantFactor = 0.0f;
		desc.rasterizerStateDesc.depthBiasClamp = 0.0f;
		desc.rasterizerStateDesc.depthBiasSlopeFactor = 0.0f;
		desc.rasterizerStateDesc.cullMode = CullMode::Back;
		desc.rasterizerStateDesc.polygonMode = PolygonMode::Fill;
		desc.rasterizerStateDesc.frontFace = FrontFace::Clockwise;
		desc.rasterizerStateDesc.depthClampEnabled = false;
		desc.rasterizerStateDesc.depthBiasEnabled = false;

		// Depth stencil description
		desc.depthStencilStateDesc.depthCompareOp = CompareOp::Never;
		desc.depthStencilStateDesc.minDepthBounds = 0.0f;
		desc.depthStencilStateDesc.maxDepthBounds = 1.0f;
		desc.depthStencilStateDesc.depthTestEnable = false;
		desc.depthStencilStateDesc.depthWriteEnable = false;
		desc.depthStencilStateDesc.depthBoundsTestEnable = false;
		desc.depthStencilStateDesc.stencilTestEnable = false;
		desc.depthStencilStateDesc.front.failOp = StencilOp::Zero;
		desc.depthStencilStateDesc.front.passOp = StencilOp::Zero;
		desc.depthStencilStateDesc.front.depthFailOp = StencilOp::Zero;
		desc.depthStencilStateDesc.front.compareOp = CompareOp::Never;
		desc.depthStencilStateDesc.front.compareMask = 0;
		desc.depthStencilStateDesc.front.writeMask = 0;
		desc.depthStencilStateDesc.front.reference = 0;
		desc.depthStencilStateDesc.back = desc.depthStencilStateDesc.front;

		// Multisampling description
		desc.multiSampleDesc.rasterizationSamples = SampleCount::OneBit;
		desc.multiSampleDesc.minSampleShading = 1.0f;
		desc.multiSampleDesc.sampleShadingEnable = false;
		desc.multiSampleDesc.alphaToCoverageEnable = false;
		desc.multiSampleDesc.alphaToOneEnable = false;

		desc.dynamicRendering.colorAttachmentFormats.Add(m_RenderAPI->GetSwapChain()->GetDesc().pixelFormat);
		desc.dynamicRendering.depthAttachmentFormat = PF_UNKNOWN;
		desc.dynamicRendering.stencilAttachmentFormat = PF_UNKNOWN;
		desc.dynamicRendering.viewMask = 0; 

		desc.shaders.Add(m_VertexShader);
		desc.shaders.Add(m_PixelShader);
		
		m_Pipeline = m_Device->CreateGraphicsPipeline(desc);
	}

	void Renderer::CreateBuffers()
	{
		m_VertexBuffers.Reserve(2);
		{
			{
				TransferBufferDesc desc;
				desc.debugName = "Vertex Transfer Buffer";
				desc.size = sizeof(Cube::c_Vertices);
				RenderBufferUtil::CreateTransferBuffer(m_TransferBuffers[0], *m_Device, desc);
				m_Device->WriteBuffer(m_TransferBuffers[0].buffer, static_cast<const void*>(Cube::c_Vertices), 0, desc.size);
			}
			{
				GpuBufferDesc desc;
				desc.debugName = "Vertex GPU Buffer";
				desc.size = sizeof(Cube::c_Vertices);
				desc.usage = GpuBufferUsage::Vertex;
				RenderBufferUtil::CreateGpuBuffer(m_VertexBuffer, *m_Device, desc);
				m_VertexBuffers.Add(m_VertexBuffer.buffer);
			}		
		}
		{
			{
				TransferBufferDesc desc;
				desc.debugName = "Index Transfer Buffer";
				desc.size = sizeof(Cube::c_Indices);
				RenderBufferUtil::CreateTransferBuffer(m_TransferBuffers[1], *m_Device, desc);
				m_Device->WriteBuffer(m_TransferBuffers[1].buffer, static_cast<const void*>(Cube::c_Indices), 0, desc.size);
			}
			{
				GpuBufferDesc desc;
				desc.debugName = "Index GPU Buffer";
				desc.size = sizeof(Cube::c_Indices);
				desc.usage = GpuBufferUsage::Index;
				desc.stride = 4;
				RenderBufferUtil::CreateGpuBuffer(m_IndexBuffer, *m_Device, desc);
			}
		}
		{
			{
				TransferBufferDesc desc;
				desc.debugName = "Instance Transfer Buffer";
				desc.size = sizeof(Matrix4);
				RenderBufferUtil::CreateTransferBuffer(m_TransferBuffers[2], *m_Device, desc);
				// Hard code for now
				Matrix4 transform = Matrix4::CreateTRS({ 0,0,15 }, Quaternion::c_Identity, { 3,3,3 });
				m_Device->WriteBuffer(m_TransferBuffers[2].buffer, static_cast<const void*>(&transform), 0, desc.size);
			}
			{
				GpuBufferDesc desc;
				desc.debugName = "Instance GPU Buffer";
				desc.size = sizeof(Matrix4);
				desc.usage = GpuBufferUsage::Vertex;	
				RenderBufferUtil::CreateGpuBuffer(m_InstanceBuffer, *m_Device, desc);
				m_VertexBuffers.Add(m_InstanceBuffer.buffer);
			}
		}
		{
			{
				TransferBufferDesc desc;
				desc.debugName = "Spotlight Transfer Buffer";
				desc.size = sizeof(ShaderSpotLight);
				RenderBufferUtil::CreateTransferBuffer(m_TransferBuffers[3], *m_Device, desc);
				ShaderSpotLight light;
				light.pos = { 0, 5, 10 };
				light.cone = 1.0f;
				light.dir = { 0, -1, 0 };
				light.att = { 1, 1, 1 };
				light.colour = { 1, 1, 1 };
				m_Device->WriteBuffer(m_TransferBuffers[3].buffer, static_cast<const void*>(&light), 0, desc.size);
			}
			{
				GpuBufferDesc desc;
				desc.debugName = "Spotlight GPU Buffer";
				desc.size = sizeof(ShaderSpotLight);
				desc.usage = GpuBufferUsage::Uniform;
				RenderBufferUtil::CreateGpuBuffer(m_SpotLightBuffer, *m_Device, desc);
			}
		}
		{
			{
				TransferBufferDesc desc;
				desc.debugName = "Material Transfer Buffer";
				desc.size = sizeof(MaterialData);
				RenderBufferUtil::CreateTransferBuffer(m_TransferBuffers[4], *m_Device, desc);
				MaterialData material;
				material.albedo = { 0, 0, 1 };
				material.roughness = 0.1f;
				material.metallic = 1.0f;
				m_Device->WriteBuffer(m_TransferBuffers[4].buffer, static_cast<const void*>(&material), 0, desc.size);
			}
			{
				GpuBufferDesc desc;
				desc.debugName = "Material GPU Buffer";
				desc.size = sizeof(MaterialData);
				desc.usage = GpuBufferUsage::Uniform;
				RenderBufferUtil::CreateGpuBuffer(m_MaterialBuffer, *m_Device, desc);
			}
		}
		{
			{
				TransferBufferDesc desc;
				desc.debugName = "SceneInfo Transfer Buffer";
				desc.size = sizeof(ShaderSceneInfo);
				RenderBufferUtil::CreateTransferBuffer(m_TransferBuffers[5], *m_Device, desc);
			}
			{
				GpuBufferDesc desc;
				desc.debugName = "SceneInfo GPU Buffer";
				desc.size = sizeof(ShaderSceneInfo);
				desc.usage = GpuBufferUsage::Uniform;
				RenderBufferUtil::CreateGpuBuffer(m_SceneInfoBuffer, *m_Device, desc);
			}
		}
	}

	void Renderer::CreateCommandAllocators()
	{
		CommandAllocatorDesc desc;
		desc.debugName = "CommandAllocator_0";
		// The reset flag is for command buffers that will be reset / re-recorded and transient is for short-lived command buffers 
		desc.flags = CommandAllocatorCreateFlags::COMMAND_ALLOC_CREATE_RESET_COMMAND_BUFFER_BIT;
		desc.queueType = CommandQueueType::CQ_GRAPHICS;
		m_CommandAllocator = m_Device->CreateCommandAllocator(desc);
	}

	void Renderer::CreateCommandLists()
	{
		{
			FenceDesc desc;
			desc.signalled = false;
			desc.debugName = "Fence_0";
			// TODO: Vary timeout depending on expectations
			desc.timeout = static_cast<uint64>(Math::Round((1.0f / 60) * 1000000000)) * 4;
			m_Fence = m_Device->CreateFence(desc);
		}
		{
			SemaphoreDesc desc;		
			desc.type = SemaphoreType::Binary;
			for (uint8 i = 0; i < 3; ++i)
			{
				desc.debugName = "ExecuteWaitSemaphore " + i;
				m_AquireSwapChainImageSemaphores[i] = m_Device->CreateSemaphoreResource(desc);
			}		
		}
		{
			SemaphoreDesc desc;
			desc.type = SemaphoreType::Binary;
			for (uint8 i = 0; i < 3; ++i)
			{
				desc.debugName = "ExecuteSignalSemaphore " + i;
				m_ExecuteCompleteSemaphores[i] = m_Device->CreateSemaphoreResource(desc);
			}
		}
		{
			SemaphoreDesc desc;
			desc.type = SemaphoreType::Timeline;
			desc.debugName = "CompletionSemaphore";
			m_CompletionSemaphore = m_Device->CreateSemaphoreResource(desc);
		}
		{
			CommandListDesc desc;
			desc.allocator = m_CommandAllocator;
			desc.debugName = "CommandList_0";
			desc.type = CommandListType::Primary;
			m_CommandList = m_Device->CreateCommandList(desc);
		}
		m_ExecuteDesc.commandLists.Add(m_CommandList);
		// Need to wait for the swapchain image to be ready for this stage
		m_ExecuteDesc.waitDstPipelineStages.Add(PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}

	void Renderer::CreateBufferBindingUpdate(BufferBindingUpdate& bindingUpdate, BufferViewHandle bufferView, uint descriptorIndex, uint bindingIndex)
	{
		BufferBindingInfo bindingInfo;
		bindingInfo.bufferView = bufferView;
		bindingUpdate.bufferBindingInfos.Add(bindingInfo);
		bindingUpdate.descriptorIndex = descriptorIndex;
		bindingUpdate.bindingIndex = bindingIndex;
	}

	bool Renderer::TryAddFrame(const RenderFrame* frame)
	{
		return m_FrameUpdateQueue.Enqueue(frame);
	}

	void Renderer::WaitForCompletion()
	{
		// Commented code not needed but keeping it for future reference
		m_Device->WaitForFence(m_Fence, UINT64_MAX);
		if (!m_ExecuteDesc.signalValues.IsEmpty())
		{
			m_Device->WaitForSemaphore(m_CompletionSemaphore, m_ExecuteDesc.signalValues.Back(), UINT64_MAX);
		}
		m_Device->WaitIdle();
	}
}