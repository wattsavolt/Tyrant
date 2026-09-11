#include "Renderer.h"
#include "RenderSubmissionThread.h"
#include "GraphicsManager.h"
#include "RenderAPI/GraphicsUtility.h"
#include "RenderAPI/Device.h"
#include "RenderAPI/SwapChain.h"
#include "RenderAPI/Pipeline.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/ShaderModule.h"
#include "RenderAPI/CommandQueue.h"
#include "RenderAPI/CommandAllocator.h"
#include "RenderAPI/CommandList.h"
#include "RenderAPI/DescriptorSet.h"
#include "RenderAPI/Sync.h"
#include "Math/Matrix4.h"
#include "RenderResource/RenderResourceUtil.h"
#include "RenderTransfer/GPUTransferUtil.h"
#include "RenderTransfer/RenderTransferTypes.h"
#include "RenderResource/Shapes.h"
#include "TransferPass.h"
#include "GeometryPass.h"

namespace tyr
{
	bool Renderer::s_Instantiated = false;

	Renderer::Renderer(const RendererConfig& rendererConfig, RenderAPI* renderAPI)
		: m_Config(rendererConfig)
		, m_RenderAPI(renderAPI)
		, m_ShaderCreator(*m_RenderAPI->GetDevice(), rendererConfig.shaderConfig)
		, m_Registry(*m_RenderAPI->GetDevice())
		, m_ViewIdIndexMap(RenderConstants::c_MaxViewsPerFrame)
		, m_SwapChains(WindowConstants::c_MaxWindows)
	{
		TYR_ASSERT(!s_Instantiated);

		m_Ctx.device = m_RenderAPI->GetDevice();

		CreateShaders();
		CreateCommandObjects();
		CreatePipelines();
		CreateBuffers();
		CreateSamplers();
		CreatePasses();

		{
			RenderSubmissionThreadArgs args;
			args.device = m_Ctx.device;
			args.graphicsQueue = m_Ctx.graphicsQueue;
			args.computeQueue = m_Ctx.computeQueue;
			args.transferQueue = m_Ctx.transferQueue;
			m_RenderSubmissionThread = new RenderSubmissionThread(args);
			// TODO: Start the render thread
		}

		s_Instantiated = true;
	}

	Renderer::~Renderer()
	{
		WaitForCompletion();

		// TODO: Stop the thread
		delete m_RenderSubmissionThread;

		DeletePasses();
		DeleteSamplers();
		DeleteBuffers();
		DeletePipelines();
		DeleteCommandObjects();
		DeleteShaders();
		DeleteSwapChains();

		s_Instantiated = false;
	}

	void Renderer::Render(float deltaTime)
	{
		RenderFrame& renderFrame = GetRenderFrame();
		renderFrame.deltaTime = deltaTime;

		if (renderFrame.activeSceneIndex == RenderFrame::c_InvalidSceneIndex)
		{
			return;
		}

		SceneFrame& sceneFrame = renderFrame.sceneFrame;

		uint shaderViewIndex = m_RenderFrameIndex * RenderConstants::c_MaxViewsPerFrame;

		const Scene& scene = m_Data.scenes[renderFrame.activeSceneIndex];

		if (!m_FirstRender)
		{
			const RenderFrame& prevRenderFrame = GetPrevRenderFrame();
			const Scene& prevScene = m_Data.scenes[prevRenderFrame.activeSceneIndex];
			if (scene.id != prevScene.id)
			{
				m_ViewIdIndexMap.Clear();
			}
		}

		ShaderSceneInfo sceneInfo{};
		sceneInfo.ambient = sceneFrame.ambient;
		LocalArray<ShaderView, RenderConstants::c_MaxViewsPerFrame> shaderViews;

		RenderWindowHandle windowHandle = renderFrame.sceneFrame.newWindow ? renderFrame.sceneFrame.newWindow : scene.windowHandle;
		const RenderWindow& renderWindow = m_WindowPool[windowHandle.h];
		const uint swapChainWidth = renderWindow.swapChain->GetWidth();
		const uint swapChainHeight = renderWindow.swapChain->GetHeight();
		for (uint i = 0; i < sceneFrame.views.Size(); ++i)
		{
			const SceneView& sv = sceneFrame.views[i];

			const float aspect = GraphicsUtility::CalculateAspectRatio(sv.viewArea, swapChainWidth, swapChainHeight);

			const Matrix4 view = Matrix4::CreateView(sv.camera.position, sv.camera.forward, sv.camera.up);
			// Do reverse-z for greater floating-point precision
			const Matrix4 projection = Matrix4::CreatePerspective(sv.camera.fov, aspect, sv.camera.farZ, sv.camera.nearZ);

			ShaderView shaderView{};
			shaderView.viewProj = view * projection;
			shaderView.camPos = sv.camera.position;
			shaderView.flags = 0;
			if (const uint* index = m_ViewIdIndexMap.Find(sv.id))
			{
				shaderView.prevViewIndex = *index;
			}
			else
			{
				shaderView.prevViewIndex = i;
			}

			size_t offset;
			UploadBufferAllocation alloc;
			const bool allocSuccess = m_AllocManager.RequestFrameUploadAllocation(sizeof(ShaderSceneInfo), alloc);

			RenderResourceUtil::WriteUploadBuffer(m_Registry.GetBuffer(alloc.buffer), *m_Ctx.device, offset, &sceneInfo, sizeof(ShaderSceneInfo));

			BufferUploadRequest& request = renderFrame.frameBufferUploadRequests.ExpandOne();
			request.srcBuffer = alloc.buffer;
			request.srcOffset = offset;
			request.dstBuffer = m_Resources.sceneInfoBuffer;
			request.dstOffset = sizeof(ShaderSceneInfo);
			request.size = sizeof(ShaderSceneInfo);

			shaderViewIndex++;
		}

		// Clear and update to get rid of old views
		m_ViewIdIndexMap.Clear();
		for (uint i = 0; i < sceneFrame.views.Size(); ++i)
		{
			const SceneView& sv = sceneFrame.views[i];
			m_ViewIdIndexMap[sv.id] = i;
		}

		if (!m_FirstRender)
		{
			m_Ctx.device->WaitForFence(m_Ctx.completionFence, UINT32_MAX);
			m_Ctx.device->ResetFence(m_Ctx.completionFence);
		}

		if (m_FirstRender)
		{
			constexpr uint bindingUpdateCount = 3;
			BufferBindingUpdate bindingUpdates[bindingUpdateCount];
			uint bindingIndex = 0;
			{
				BufferBindingInfo bindingInfo;
				bindingInfo.bufferView = m_Registry.GetBuffer(m_Resources.sceneInfoBuffer).bufferView;
				bindingUpdates[bindingIndex++].bindingIndex = bindingIndex;
			}
			{
				BufferBindingInfo bindingInfo;
				bindingInfo.bufferView = m_Registry.GetBuffer(m_Resources.spotLightBuffer).bufferView;
				bindingUpdates[bindingIndex++].bindingIndex = bindingIndex;
			}
			{
				BufferBindingInfo bindingInfo;
				bindingInfo.bufferView = m_Registry.GetBuffer(m_Resources.materialBuffer).bufferView;
				bindingUpdates[bindingIndex++].bindingIndex = bindingIndex;
			}
			m_Ctx.device->UpdateDescriptorSet(m_Resources.descriptorSet, bindingUpdates, bindingUpdateCount);
		}

		m_AllocManager.SignalResourceUpload(m_Ctx.currentTimelineValue);
		m_AllocManager.SignalFrameUpload(m_Ctx.currentTimelineValue);

		m_Ctx.currentTimelineValue++;

		// TODO: Call this in an async task
		RenderAsync(m_RenderFrameIndex);

		m_FirstRender = false;
	}

	void Renderer::RenderAsync(uint renderFrameIndex)
	{
		RenderRegistry& registry = *RenderRegistry::Instance();

		const RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];
		FrameContext& frameCtx = m_Ctx.frameContexts[renderFrameIndex];

		m_Data.activeSceneIndex = renderFrame.activeSceneIndex;

		m_Data.BeginFrame();

		const SceneFrame& sceneFrame = renderFrame.sceneFrame;
		Scene& scene = m_Data.scenes[m_Data.activeSceneIndex];

		if (sceneFrame.newWindow)
		{
			scene.windowHandle = sceneFrame.newWindow;
		}

		RenderWindow& window = m_WindowPool[scene.windowHandle.h];
		RenderWindowFrame& windowFrame = window.frames[renderFrameIndex];

		bool resized;
		window.swapChainImageIndex = window.swapChain->AcquireNextImage(windowFrame.aquireSwapChainImageSemaphore, resized);

		// TODO: Alert main thread if a resize occurred

		for (const SceneView& sv : sceneFrame.views)
		{
			scene.views.Add(sv);
		}

		for (MeshInstanceHandle handle : sceneFrame.meshInstancesToRemove)
		{
			const uint renderIndex = registry.GetMeshInstance(handle).renderIndex;
			scene.content.meshInstances.SwapAndPopBack(renderIndex);
			if (!scene.content.meshInstances.IsEmpty())
			{
				const MeshInstanceHandle swapped = scene.content.meshInstances[renderIndex];
				registry.GetMeshInstance(swapped).renderIndex = renderIndex;
			}
		}

		for (const MeshInstanceUpdate& update : sceneFrame.meshInstancesToUpdate)
		{
			registry.GetMeshInstance(update.handle).info = update.desc.info;
		}

		scene.content.meshInstances.Reserve(scene.content.meshInstances.Size() + sceneFrame.meshInstancesToAdd.Size());
		for (MeshInstanceHandle handle : sceneFrame.meshInstancesToAdd)
		{
			registry.GetMeshInstance(handle).renderIndex = scene.content.meshInstances.Size();
			scene.content.meshInstances.Add(handle);
		}

		for (DirLightHandle handle : sceneFrame.dirLightsToRemove)
		{
			const uint renderIndex = registry.GetDirectionalLight(handle).renderIndex;
			scene.content.dirLights.SwapAndPopBack(renderIndex);
			if (!scene.content.dirLights.IsEmpty())
			{
				const DirLightHandle swapped = scene.content.dirLights[renderIndex];
				registry.GetDirectionalLight(swapped).renderIndex = renderIndex;
			}
		}

		for (const DirLightUpdate& update : sceneFrame.dirLightsToUpdate)
		{
			registry.GetDirectionalLight(update.handle).info = update.desc.info;
		}

		scene.content.dirLights.Reserve(scene.content.dirLights.Size() + sceneFrame.dirLightsToAdd.Size());
		for (DirLightHandle handle : sceneFrame.dirLightsToAdd)
		{
			registry.GetDirectionalLight(handle).renderIndex = scene.content.dirLights.Size();
			scene.content.dirLights.Add(handle);
		}

		for (PointLightHandle handle : sceneFrame.pointLightsToRemove)
		{
			const uint renderIndex = registry.GetPointLight(handle).renderIndex;
			scene.content.pointLights.SwapAndPopBack(renderIndex);
			if (!scene.content.pointLights.IsEmpty())
			{
				const PointLightHandle swapped = scene.content.pointLights[renderIndex];
				registry.GetPointLight(swapped).renderIndex = renderIndex;
			}
		}

		for (const PointLightUpdate& update : sceneFrame.pointLightsToUpdate)
		{
			registry.GetPointLight(update.handle).info = update.desc.info;
		}

		scene.content.pointLights.Reserve(scene.content.pointLights.Size() + sceneFrame.pointLightsToAdd.Size());
		for (PointLightHandle handle : sceneFrame.pointLightsToAdd)
		{
			registry.GetPointLight(handle).renderIndex = scene.content.pointLights.Size();
			scene.content.pointLights.Add(handle);
		}

		for (SpotLightHandle handle : sceneFrame.spotLightsToRemove)
		{
			const uint renderIndex = registry.GetSpotLight(handle).renderIndex;
			scene.content.spotLights.SwapAndPopBack(renderIndex);
			if (!scene.content.spotLights.IsEmpty())
			{
				const SpotLightHandle swapped = scene.content.spotLights[renderIndex];
				registry.GetSpotLight(swapped).renderIndex = renderIndex;
			}
		}

		for (const SpotLightUpdate& update : sceneFrame.spotLightsToUpdate)
		{
			registry.GetSpotLight(update.handle).info = update.desc.info;
		}

		scene.content.spotLights.Reserve(scene.content.spotLights.Size() + sceneFrame.spotLightsToAdd.Size());
		for (SpotLightHandle handle : sceneFrame.spotLightsToAdd)
		{
			registry.GetSpotLight(handle).renderIndex = scene.content.spotLights.Size();
			scene.content.spotLights.Add(handle);
		}

		m_Data.assetBufferUploadRequests.Reserve(renderFrame.assetBufferUploadRequests.Size());
		for (const BufferUploadRequest& request : renderFrame.assetBufferUploadRequests)
		{
			m_Data.assetBufferUploadRequests.Add(request);
		}

		scene.frameUploadRequests.Reserve(renderFrame.frameBufferUploadRequests.Size());
		for (const BufferUploadRequest& request : renderFrame.frameBufferUploadRequests)
		{
			scene.frameUploadRequests.Add(request);
		}

		m_Data.textureUploadRequests.Reserve(renderFrame.textureUploadRequests.Size());
		for (const TextureUploadRequest& request : renderFrame.textureUploadRequests)
		{
			m_Data.textureUploadRequests.Add(request);
		}

		BuildAndExecuteRenderGraph(renderFrameIndex);
	}

	void Renderer::BuildAndExecuteRenderGraph(uint renderFrameIndex)
	{
		const RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];
		FrameContext& frameCtx = m_Ctx.frameContexts[renderFrameIndex];

		Scene& scene = m_Data.scenes[m_Data.activeSceneIndex];

		RenderWindow& window = m_WindowPool[scene.windowHandle.h];
		RenderWindowFrame& windowFrame = window.frames[renderFrameIndex];

		const uint windowWidth = window.swapChain->GetWidth();
		const uint windowHeight = window.swapChain->GetHeight();
		Viewport viewport;
		viewport.width = windowWidth;
		viewport.height = windowHeight;

		// Temporarily use first one
		CommandList* cmdList = frameCtx.commandLists[0];
		cmdList->Reset(true);
		cmdList->Begin(CommandBufferUsage::COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		// TODO: Render for all views and create render area for each view by calling GraphicsUtility::CreateRenderArea
		RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extents = { windowWidth, windowHeight };
		renderingInfo.viewMask = 0;
		renderingInfo.layerCount = 1;

		RenderingAttachmentInfo colourAttachment;
		colourAttachment.loadOp = AttachmentLoadOp::Clear;
		colourAttachment.storeOp = AttachmentStoreOp::Store;
		colourAttachment.resolveMode = RESOLVE_MODE_NONE;
		colourAttachment.imageLayout = window.swapChain->GetRenderingLayout();
		colourAttachment.clearValue.colour = { 0.0f, 0.0f, 0.0f, 0.0f };
		colourAttachment.imageView = window.swapChain->GetImageViews()[window.swapChainImageIndex];

		renderingInfo.colourAttachmentCount = 1;
		renderingInfo.colourAttachments = &colourAttachment;

		cmdList->BeginRendering(renderingInfo);
		cmdList->SetViewport(&viewport, 1);
		cmdList->SetScissor(&renderingInfo.renderArea, 1);
		cmdList->BindGraphicsPipeline(m_Resources.geometryGraphicsPipeline);
		cmdList->BindDescriptorSet(m_Resources.descriptorSet, m_Resources.geometryGraphicsPipeline);
		cmdList->DrawIndexed(Cube::c_NumIndices, 1, 0, 0, 0);
		cmdList->EndRendering();
		cmdList->End();

		SemaphoreHandle waitSemaphores[1] = { windowFrame.aquireSwapChainImageSemaphore };
		SemaphoreHandle signalSemaphores[2] = { windowFrame.executeCompleteSemaphore, m_Ctx.completionSemaphore };
		// A dummy value needs to be pushed back for the binary semaphore as the count of the number of values must match the number of semaphores
		// (as per the vulkan spec)
		uint64 signalValues[2] = { 0, m_Ctx.currentTimelineValue };
		CommandQueueExecuteArgs executeArgs{};
		executeArgs.waitSemaphoreCount = 1;;
		executeArgs.waitSemaphores = waitSemaphores;
		executeArgs.signalSemaphoreCount = 2;

		// Wait on semaphore used when acquiring next swapchain image
		m_Ctx.graphicsQueue->Execute(&executeArgs, 1, 0, m_Ctx.completionFence);

		// Wait on signal semaphore used in execute
		bool resized;
		window.swapChain->Present(m_Ctx.graphicsQueue, windowFrame.executeCompleteSemaphore, window.swapChainImageIndex, resized);
	}

	void Renderer::PrepareForNextFrame()
	{
		m_RenderFrameIndex = (m_RenderFrameIndex + 1) % RenderConstants::c_BufferedFrameCount;
		RenderFrame& renderFrame = GetRenderFrame();

		//m_Ctx.device->WaitForSemaphore(m_Ctx.completionSemaphore, renderFrame.timelineValue, UINT64_MAX);

		const uint64 completionSemaphoreValue = m_Ctx.device->GetSemaphoreValue(m_Ctx.completionSemaphore);
		// TODO: Resource allocator should have value reclaimed after every submission and not just at end of frame
		m_AllocManager.ReclaimResourceUploadMemory(completionSemaphoreValue);
		m_AllocManager.ReclaimFrameUploadMemory(completionSemaphoreValue);

		for (TextureHandle handle : renderFrame.texturesToDelete)
		{
			m_Registry.DeleteTexture(handle);
		}
		for (MaterialHandle handle : renderFrame.materialsToDelete)
		{
			m_Registry.DeleteMaterial(handle);
		}
		for (MeshHandle handle : renderFrame.meshesToDelete)
		{
			const Mesh& mesh = m_Registry.GetMesh(handle);
			m_AllocManager.FreeMeshLODs(mesh.lodOffset, mesh.lodCount);
			m_Registry.DeleteMesh(handle);
		}
		for (SkeletalMeshHandle handle : renderFrame.skeletalMeshesToDelete)
		{
			const SkeletalMesh& mesh = m_Registry.GetSkeletalMesh(handle);
			m_AllocManager.FreeMeshLODs(mesh.lodOffset, mesh.lodCount);
			m_Registry.DeleteSkeletalMesh(handle);
		}
		for (MeshInstanceHandle handle : renderFrame.meshInstancesToDelete)
		{
			m_Registry.DeleteMeshInstance(handle);
		}
		for (SkeletalMeshInstanceHandle handle : renderFrame.skeletalMeshInstancesToDelete)
		{
			m_Registry.DeleteSkeletalMeshInstance(handle);
		}
		for (DirLightHandle handle : renderFrame.dirLightsToDelete)
		{
			m_Registry.DeleteDirectionalLight(handle);
		}
		for (PointLightHandle handle : renderFrame.pointLightsToDelete)
		{
			m_Registry.DeletePointLight(handle);
		}
		for (SpotLightHandle handle : renderFrame.spotLightsToDelete)
		{
			m_Registry.DeleteSpotLight(handle);
		}
		renderFrame.Clear();
	}

	void Renderer::WaitForCompletion()
	{
		m_Ctx.device->WaitForFence(m_Ctx.completionFence, UINT64_MAX);
		m_Ctx.device->WaitForSemaphore(m_Ctx.completionSemaphore, m_Ctx.currentTimelineValue, UINT64_MAX);
		m_Ctx.device->WaitIdle();
	}

	RenderWindowHandle Renderer::AddWindow(void* osHandle)
	{
		RenderWindowHandle handle(m_WindowPool.Create());
		RenderWindow& renderWindow = m_WindowPool[handle.h.index];

		SwapChainDesc swapChainDesc;
		swapChainDesc.pixelFormat = PixelFormat::PF_B8G8R8A8_SRGB;
		swapChainDesc.colorSpace = ColorSpace::CP_SRGB_NONLINEAR;
		// TODO: Enable later
		swapChainDesc.createDepth = false;
		swapChainDesc.vSyncEnabled = m_Config.vSyncEnabled;
		swapChainDesc.useTripleBuffering = m_Config.useTripleBuffering;

		SwapChain* swapChain = m_SwapChains[handle.h.index];
		if (swapChain)
		{
			swapChain->Recreate(osHandle, swapChainDesc);
		}
		else
		{
			swapChain = m_Ctx.device->CreateSwapChain(osHandle, swapChainDesc);
			m_SwapChains[handle.h.index] = swapChain;
		}

		renderWindow.swapChain = swapChain;
	
		return handle;
	}

	void Renderer::RemoveWindow(RenderWindowHandle window)
	{
		RenderWindow& renderWindow = m_WindowPool[window.h];
		SwapChain* swapChain = renderWindow.swapChain;
		renderWindow = {};
		// Cache the swqap chain for reuse 
		renderWindow.swapChain = swapChain;
		m_WindowPool.Delete(window.h);
	}

	void Renderer::ResizeWindow(RenderWindowHandle window, uint width, uint height)
	{
		RenderWindow& renderWindow = m_WindowPool[window.h];
		SwapChain* swapChain = renderWindow.swapChain;
		RenderWindow& wnd = m_WindowPool[window.h];
		wnd.resizeRequired = true;
		RenderFrame& renderFrame = GetRenderFrame();
		renderFrame.windowResizeRequired = true;
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
			m_Resources.geometryMeshShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "MeshPS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_FRAGMENT_BIT;
			m_Resources.geometryPixelShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
	}

	void Renderer::DeleteShaders()
	{
		m_Ctx.device->DeleteShaderModule(m_Resources.geometryMeshShader);
		m_Ctx.device->DeleteShaderModule(m_Resources.geometryPixelShader);
		ShaderCreator::UnloadCompilerLibs();
	}

	void Renderer::DeleteSwapChains()
	{
		for (SwapChain* swapChain : m_SwapChains)
		{
			if (swapChain)
				delete swapChain;
		}
		m_SwapChains.Clear();
	}

	void Renderer::CreateCommandObjects()
	{
		{
			FenceDesc desc;
			desc.signalled = false;
			desc.debugName = "Fence_0";
			// TODO: Vary timeout depending on expectations
			desc.timeout = static_cast<uint64>(Math::Round((1.0f / 60) * 1000000000)) * 4;
			m_Ctx.completionFence = m_Ctx.device->CreateFence(desc);
		}
		{
			SemaphoreDesc desc;
			desc.type = SemaphoreType::Timeline;
			desc.debugName = "CompletionSemaphore";
			m_Ctx.completionSemaphore = m_Ctx.device->CreateSemaphoreResource(desc);
		}

		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			FrameContext& frameCtx = m_Ctx.frameContexts[i];
			{
				CommandAllocatorDesc desc;
				desc.debugName = "CommandAllocator_" + i;
				// The reset flag is for command buffers that will be reset / re-recorded and transient is for short-lived command buffers 
				desc.flags = CommandAllocatorCreateFlags::COMMAND_ALLOC_CREATE_RESET_COMMAND_BUFFER_BIT;
				desc.queueType = CommandQueueType::CQ_GRAPHICS;
				frameCtx.commandAllocator = m_Ctx.device->CreateCommandAllocator(desc);
			}
			{
				frameCtx.commandLists.Reserve(TaskScheduler::c_MaxWorkers);
				const GDebugString nameStart = GDebugString("CommandList_" + i) + "_";
				CommandListDesc desc;
				desc.allocator = frameCtx.commandAllocator;
				desc.debugName = nameStart + frameCtx.commandLists.Size();
				desc.type = CommandListType::Primary;
				frameCtx.commandLists.Add(m_Ctx.device->CreateCommandList(desc));
			}
		}
		// Need to wait for the swapchain image to be ready for this stage
		//m_ExecuteDesc.waitDstPipelineStages.Add(PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}

	void Renderer::DeleteCommandObjects()
	{
		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			FrameContext& frameCtx = m_Ctx.frameContexts[i];
			for (uint i = 0; i < frameCtx.commandLists.Size(); ++i)
			{
				delete frameCtx.commandLists[i];
			}
			frameCtx.commandLists.Clear();

			delete frameCtx.commandAllocator;
		}
		m_Ctx.device->DeleteFence(m_Ctx.completionFence);
		m_Ctx.device->DeleteSemaphoreResource(m_Ctx.completionSemaphore);
	}

	void Renderer::CreatePipelines()
	{
		GraphicsPipelineDesc desc;
		{
			DescriptorPoolDesc poolDesc;
			poolDesc.maxSets = 1;
			DescriptorPoolSize poolSize;
			poolSize.descriptorCount = 3;
			poolSize.descriptorType = DescriptorType::UniformBuffer;
			poolDesc.poolSizes.Add(std::move(poolSize));
			poolDesc.flags = DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
#if !TYR_FINAL
			poolDesc.debugName = "DescriptorPool";
#endif
			m_Resources.descriptorPool = m_Ctx.device->CreateDescriptorPool(poolDesc);

			DescriptorSetLayoutDesc layoutDesc;
			// Change the flags to add extra functionality.
			layoutDesc.flags = DESCRIPTOR_SET_LAYOUT_UPDATE_AFTER_BIND_POOL_BIT;
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
			{
				DescriptorSetLayoutBinding& binding = layoutDesc.bindings.ExpandOne();
				binding.binding = 3;
				binding.descriptorCount = 1;
				binding.descriptorType = DescriptorType::SampledImage;
				binding.stageFlags = SHADER_STAGE_FRAGMENT_BIT;
			}
			{
				DescriptorSetLayoutBinding& binding = layoutDesc.bindings.ExpandOne();
				binding.binding = 4;
				binding.descriptorCount = 1;
				binding.descriptorType = DescriptorType::Sampler;
				binding.stageFlags = SHADER_STAGE_FRAGMENT_BIT;
			}

			m_Resources.descriptorSetLayout = m_Ctx.device->CreateDescriptorSetLayout(layoutDesc);

			DescriptorSetDesc setDesc;
			setDesc.layout = m_Resources.descriptorSetLayout;
			setDesc.pool = m_Resources.descriptorPool;

#if !TYR_FINAL
			poolDesc.debugName = "DescriptorSet";
#endif
			m_Resources.descriptorSet = m_Ctx.device->CreateDescriptorSet(setDesc);

			desc.pipelineLayoutDesc.descriptorSetLayouts.Add(m_Resources.descriptorSetLayout);
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

		desc.dynamicRendering.colorAttachmentFormats.Add(PF_R8G8B8A8_SRGB); // TODO: Don't hardcode this later?
		desc.dynamicRendering.depthAttachmentFormat = PF_UNKNOWN;
		desc.dynamicRendering.stencilAttachmentFormat = PF_UNKNOWN;
		desc.dynamicRendering.viewMask = 0;

		desc.shaders.Add(m_Resources.geometryMeshShader);
		desc.shaders.Add(m_Resources.geometryPixelShader);
		desc.shaders.Add(m_Resources.geometryPixelShader);

		m_Resources.geometryGraphicsPipeline = m_Ctx.device->CreateGraphicsPipeline(desc);
	}

	void Renderer::DeletePipelines()
	{
		m_Ctx.device->DeleteGraphicsPipeline(m_Resources.geometryGraphicsPipeline);
		m_Ctx.device->DeleteDescriptorSet(m_Resources.descriptorSet);
		m_Ctx.device->DeleteDescriptorSetLayout(m_Resources.descriptorSetLayout);
		m_Ctx.device->DeleteDescriptorPool(m_Resources.descriptorPool);
	}

	void Renderer::CreateBuffers()
	{
		{
			RenderBufferDesc desc{};
			desc.debugName = "Material Buffer";
			desc.size = sizeof(ShaderMaterial) * RenderConstants::c_MaxMaterials;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.materialBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Vertex Buffer";
			desc.size = RenderConstants::c_VertexBufferSize;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.vertexBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Index Buffer";
			desc.size = RenderConstants::c_IndexBufferSize;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.indexBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Meshlet Buffer";
			desc.size = RenderConstants::c_MeshletBufferSize;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.meshletBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Mesh Buffer";
			desc.size = sizeof(ShaderMesh) * RenderConstants::c_MaxMeshes;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.meshBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Mesh LOD Buffer";
			desc.size = sizeof(ShaderMeshLOD) * RenderConstants::c_MaxMeshLODs;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.meshBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Mesh Instance Buffer";
			desc.size = sizeof(uint) * RenderConstants::c_MaxMeshInstances;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.meshInstanceBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc{};
			desc.debugName = "Dir light Buffer";
			desc.size = sizeof(ShaderDirectionalLight) * RenderConstants::c_MaxDirLights;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.directionalLightBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc{};
			desc.debugName = "Point light Buffer";
			desc.size = sizeof(ShaderPointLight) * RenderConstants::c_MaxPointLights;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.pointLightBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc{};
			desc.debugName = "Spot light Buffer";
			desc.size = sizeof(ShaderSpotLight) * RenderConstants::c_MaxSpotLights;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.spotLightBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc{};
			desc.debugName = "Scene Info Buffer";
			desc.size = sizeof(ShaderSceneInfo) * RenderConstants::c_MaxScenes;
			desc.usage = RenderBufferUsage::Uniform;
			m_Resources.sceneInfoBuffer = m_Registry.CreateBuffer(desc);
		}
	}

	void Renderer::DeleteBuffers()
	{
		m_Registry.DeleteBuffer(m_Resources.materialBuffer);
		m_Registry.DeleteBuffer(m_Resources.vertexBuffer);
		m_Registry.DeleteBuffer(m_Resources.indexBuffer);
		m_Registry.DeleteBuffer(m_Resources.meshletBuffer);
		m_Registry.DeleteBuffer(m_Resources.meshBuffer);
		m_Registry.DeleteBuffer(m_Resources.meshLODBuffer);
		m_Registry.DeleteBuffer(m_Resources.meshInstanceBuffer);
		m_Registry.DeleteBuffer(m_Resources.directionalLightBuffer);
		m_Registry.DeleteBuffer(m_Resources.pointLightBuffer);
		m_Registry.DeleteBuffer(m_Resources.spotLightBuffer);
		m_Registry.DeleteBuffer(m_Resources.sceneInfoBuffer);
	}

	void Renderer::CreateSamplers()
	{
		{
			SamplerDesc desc{};
			desc.debugName = "MaterialSampler";
			desc.magFilter = Filter::Linear;
			desc.minFilter = Filter::Linear;
			desc.mipmapMode = SamplerMipmapMode::Linear;
			desc.addressModeU = SamplerAddressMode::Repeat;
			desc.addressModeV = SamplerAddressMode::Repeat;
			desc.addressModeW = SamplerAddressMode::Repeat;
			desc.compareEnable = false;
			desc.compareOp = CompareOp::Always; // ignored when compareEnable = false
			desc.borderColour = BorderColour::FloatTransparentBlack; // irrelevant for Repeat
			desc.minLod = 0.0f;
			desc.maxLod = FLT_MAX; // or mipCount - 1 at bind time
			desc.mipLodBias = 0.0f;
			desc.anisotropyEnable = true;
			desc.maxAnisotropy = 8.0f; // 8 is very common, 16 on high-end PCs
			desc.unnormalisedCoords = false;

			m_Resources.materialSampler = m_Ctx.device->CreateSampler(desc);
		}
	}

	void Renderer::DeleteSamplers()
	{
		m_Ctx.device->DeleteSampler(m_Resources.materialSampler);
	}

	void Renderer::CreatePasses()
	{

	}

	void Renderer::DeletePasses()
	{

	}

	RenderPassHandle Renderer::CreateRenderPass()
	{
		RenderPassDesc desc;
		AttachmentDesc colorAttachment{};
		colorAttachment.format = PixelFormat::PF_R8G8B8A8_SRGB;
		colorAttachment.samples = SampleCount::OneBit;
		colorAttachment.loadOp = AttachmentLoadOp::Clear;
		colorAttachment.storeOp = AttachmentStoreOp::Store;
		colorAttachment.stencilLoadOp = AttachmentLoadOp::DontCare;
		colorAttachment.stencilStoreOp = AttachmentStoreOp::DontCare;
		colorAttachment.initialLayout = IMAGE_LAYOUT_UNKNOWN;
		colorAttachment.finalLayout = IMAGE_LAYOUT_PRESENT_SRC;

		// References above attachment

		SubpassDesc subpassDesc;
		subpassDesc.pipelineType = PipelineType::Graphics;

		AttachmentReference& colorAttachmentRef = subpassDesc.colorAttachments.ExpandOne();
		colorAttachmentRef.attachmentIndex = 0;
		colorAttachmentRef.layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		desc.attachments.Add(std::move(colorAttachment));
		desc.subpasses.Add(std::move(subpassDesc));
		// Add render pass dependencies later when needed.

		return m_Ctx.device->CreateRenderPass(desc);
	}
}