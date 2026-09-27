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
#include "TransferPass.h"
#include "GeometryPass.h"
#include "GUIPass.h"
#include "RenderGraph.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphAllocation.h"
namespace tyr
{
	bool Renderer::s_Instantiated = false;

	Renderer::Renderer(const RendererConfig& rendererConfig, RenderAPI* renderAPI)
		: m_Config(rendererConfig)
		, m_RenderAPI(renderAPI)
		, m_ShaderCreator(*m_RenderAPI->GetDevice(), rendererConfig.shaderConfig)
		, m_Registry(*m_RenderAPI->GetDevice())
		, m_ViewIdIndexMap(RenderConstants::c_MaxViewsPerFrame)
	{
		TYR_ASSERT(!s_Instantiated);

		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			m_RenderAsyncTasks[i] = c_InvalidTaskID;
			m_RenderFrameNumbers[i] = 0;
		}

		m_Ctx.device = m_RenderAPI->GetDevice();

		RenderGraphAllocator::Create();

		CreateQueues();
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
		}

		s_Instantiated = true;
	}

	void Renderer::Shutdown()
	{
		if (!m_RenderSubmissionThread)
		{
			// Already shut down - called explicitly during engine shutdown, and the destructor
			// calls this again as a safety net.
			return;
		}

		WaitForCompletion();

		// Stops the thread, which drains and executes anything still queued as its last
		// action before joining.
		delete m_RenderSubmissionThread;
		m_RenderSubmissionThread = nullptr;

		// Wait again now that the thread has definitely submitted everything, so nothing else
		// (a module removing a window/scene, or this object's own destructor) deletes a resource
		// the GPU might still be using.
		m_Ctx.device->WaitIdle();
	}

	Renderer::~Renderer()
	{
		// Safety net - the real call is expected to have already happened explicitly, early in
		// engine shutdown (see Shutdown()'s own comment). Idempotent, so this is a no-op then.
		Shutdown();

		DeletePasses();
		DeleteSamplers();
		DeleteBuffers();
		DeletePipelines();
		DeleteCommandObjects();
		DeleteShaders();
		// Safe now (unlike inside Shutdown()) - every module has finished calling
		// RemoveWindow/RemoveScene/DeleteMesh/etc by this point, and nothing async is left
		// running that could still touch their data, so whatever's still sitting in any
		// RenderFrame slot's *ToDelete lists (queued during module shutdown, after the main
		// loop - and so PrepareForNextFrame - had already stopped running) can finally be
		// flushed.
		DeleteRemainingFrameResources();
		DeleteQueues();

		RenderGraphAllocator::Destroy();

		s_Instantiated = false;
	}

	void Renderer::Render(float deltaTime)
	{
		// Must run before this frame's own frame upload allocations further down, so an
		// older frame's report can never end up covering this frame's allocations too.
		DrainSubmissionCompletions();

		while (Optional<RenderNotification> notification = m_RenderSubmissionThread->DequeueRenderNotification())
		{
			ResizeWindow(notification->window);
		}

		RenderFrame& renderFrame = GetRenderFrame();
		renderFrame.deltaTime = deltaTime;

		if (!renderFrame.activeScene)
		{
			return;
		}

		SceneFrame& sceneFrame = renderFrame.sceneFrame;

		const Scene& scene = m_Data.scenes[renderFrame.activeScene.h];

		if (!m_FirstRender)
		{
			const RenderFrame& prevRenderFrame = GetPrevRenderFrame();
			// The previous slot might have had no active scene at all (e.g. its tick was
			// skipped - see Render()'s own early return above) - treat that the same as a
			// changed scene rather than indexing scenes with an invalid handle.
			if (!prevRenderFrame.activeScene || m_Data.scenes[prevRenderFrame.activeScene.h].id != scene.id)
			{
				m_ViewIdIndexMap.Clear();
			}
		}

		ShaderSceneInfo sceneInfo{};
		sceneInfo.ambient = sceneFrame.ambient;
		// scene.content.dirLights/pointLights/spotLights are each packed contiguously from
		// index 0 (swap-and-pop-back on removal - see the merge loop in RenderAsync), so their
		// sizes are exactly how many of each light buffer's entries, from the start, are real -
		// see SceneInfo::dirLightCount's own comment on why the shader needs these rather than
		// a fixed loop.
		sceneInfo.dirLightCount = scene.content.dirLights.Size();
		sceneInfo.pointLightCount = scene.content.pointLights.Size();
		sceneInfo.spotLightCount = scene.content.spotLights.Size();

		const RenderWindowHandle windowHandle = renderFrame.sceneFrame.newWindow ? renderFrame.sceneFrame.newWindow : scene.windowHandle;
		if (!windowHandle)
		{
			// scene.windowHandle is only ever persisted by RenderAsync (see its own newWindow
			// handling) once it has actually processed a frame that carried a window change -
			// until that catches up on the worker thread, there's nothing to render to yet.
			return;
		}

		const RenderWindow& renderWindow = m_WindowPool[windowHandle.h];
		const uint swapChainWidth = renderWindow.swapChain->GetWidth();
		const uint swapChainHeight = renderWindow.swapChain->GetHeight();
		// TODO: Extend to multiple views per scene - only the first view's data reaches the
		// shader for now (see ShaderSceneInfo's own comment).
		if (!sceneFrame.views.IsEmpty())
		{
			const SceneView& sv = sceneFrame.views[0];

			// Before EditorViewport has ever requested a render target (e.g. the very first
			// few frames), fall back to the swap chain's own size instead of dividing by zero.
			const uint viewportWidth = m_Resources.viewportWidth != 0 ? m_Resources.viewportWidth : swapChainWidth;
			const uint viewportHeight = m_Resources.viewportHeight != 0 ? m_Resources.viewportHeight : swapChainHeight;
			const float aspect = GraphicsUtility::CalculateAspectRatio(sv.viewArea, viewportWidth, viewportHeight);

			const Matrix4 view = Matrix4::CreateView(sv.camera.position, sv.camera.forward, sv.camera.up);
			// Do reverse-z for greater floating-point precision
			const Matrix4 projection = Matrix4::CreatePerspective(sv.camera.fov, aspect, sv.camera.farZ, sv.camera.nearZ);

			sceneInfo.viewProj = view * projection;
			sceneInfo.camPos = sv.camera.position;
		}

		{
			UploadBufferAllocation alloc;
			const bool allocSuccess = m_AllocManager.RequestFrameUploadAllocation(sizeof(ShaderSceneInfo), alloc);
			TYR_ASSERT(allocSuccess);

			RenderResourceUtil::WriteUploadBuffer(m_Registry.GetBuffer(alloc.buffer), *m_Ctx.device, alloc.offset, &sceneInfo, sizeof(ShaderSceneInfo));

			BufferUploadRequest& request = renderFrame.frameBufferUploadRequests.ExpandOne();
			request.srcBuffer = alloc.buffer;
			request.srcOffset = alloc.offset;
			request.dstBuffer = m_Resources.sceneInfoBuffer;
			// The whole buffer is bound as a single cbuffer at offset 0 - only one scene ever
			// renders at a time (see Scene's own comment), so there's no per-scene slot to
			// index into here.
			request.dstOffset = 0;
			request.size = sizeof(ShaderSceneInfo);
		}

		// Clear and update to get rid of old views
		m_ViewIdIndexMap.Clear();
		for (uint i = 0; i < sceneFrame.views.Size(); ++i)
		{
			const SceneView& sv = sceneFrame.views[i];
			m_ViewIdIndexMap[sv.id] = i;
		}

		if (m_FirstRender)
		{
			constexpr uint bindingCount = 12;
			BufferBindingInfo bindingInfos[bindingCount];
			BufferBindingUpdate bindingUpdates[bindingCount];

			auto SetBinding = [&](uint i, uint bindingIndex, RenderBufferHandle buffer)
			{
				bindingInfos[i].bufferView = m_Registry.GetBuffer(buffer).bufferView;
				bindingUpdates[i].bindingIndex = bindingIndex;
				bindingUpdates[i].bufferBindingInfos = &bindingInfos[i];
				bindingUpdates[i].infoCount = 1;
			};

			SetBinding(0, TYR_BINDING_SCENE_INFO, m_Resources.sceneInfoBuffer);
			SetBinding(1, TYR_BINDING_MESH, m_Resources.meshBuffer);
			SetBinding(2, TYR_BINDING_MESH_LOD, m_Resources.meshLODBuffer);
			SetBinding(3, TYR_BINDING_MESHLET, m_Resources.meshletBuffer);
			SetBinding(4, TYR_BINDING_VERTEX, m_Resources.vertexBuffer);
			SetBinding(5, TYR_BINDING_INDEX, m_Resources.indexBuffer);
			SetBinding(6, TYR_BINDING_MESH_INSTANCE, m_Resources.meshInstanceBuffer);
			SetBinding(7, TYR_BINDING_MATERIAL, m_Resources.materialBuffer);
			SetBinding(8, TYR_BINDING_DIR_LIGHT, m_Resources.directionalLightBuffer);
			SetBinding(9, TYR_BINDING_POINT_LIGHT, m_Resources.pointLightBuffer);
			SetBinding(10, TYR_BINDING_SPOT_LIGHT, m_Resources.spotLightBuffer);
			SetBinding(11, TYR_BINDING_GUI_VERTEX, m_Resources.guiVertexBuffer);

			m_Ctx.device->UpdateDescriptorSet(m_Resources.descriptorSet, bindingUpdates, bindingCount);

			// The one material sampler every texture is read with (see CreateSamplers) - bound
			// once here, like the buffers above, rather than per-texture, since MeshPS.hlsl
			// always indexes samplers[0] regardless of which texture it's sampling. Unlike
			// CreateTexture's own descriptor write (TYR_BINDING_TEXTURES, one call per texture
			// as each is created), nothing was ever writing this binding at all - samplers[0]
			// sat permanently unwritten, and reading an unwritten bindless descriptor is what
			// was producing black/zero samples regardless of how correct everything upstream of
			// the pixel shader was.
			ImageBindingInfo samplerBindingInfo{};
			samplerBindingInfo.sampler = m_Resources.materialSampler;
			samplerBindingInfo.hasSampler = true;

			ImageBindingUpdate samplerUpdate{};
			samplerUpdate.bindingIndex = TYR_BINDING_SAMPLERS;
			samplerUpdate.imageBindingInfos = &samplerBindingInfo;
			samplerUpdate.infoCount = 1;

			m_Ctx.device->UpdateDescriptorSet(m_Resources.descriptorSet, nullptr, 0, &samplerUpdate, 1);
		}

		// The number of images a swap chain lets an app hold acquired-without-presenting is
		// (imageCount - surfaceMinImageCount + 1), which is only guaranteed to be at least 1 -
		// not the c_BufferedFrameCount depth the rest of this pacing is built around (drivers
		// commonly report a surface minImageCount of 2, which with our 3-image swap chain
		// leaves room for exactly 2). RenderAsync tasks are chained one-to-the-next, not
		// three-to-the-next, so without this, tasks for frame-1 and frame could both already
		// hold an acquired image by the time frame+1 tries to acquire a third. Waiting for the
		// previous frame's present to have been issued (not completed - vkQueuePresentKHR
		// itself is what the validation layer's acquire/present accounting cares about) before
		// creating this one's task keeps at most one frame's image "acquired but not yet
		// presented" at a time, which every implementation is guaranteed to allow.
		if (m_FrameNumber > 0)
		{
			const uint64 requiredPresentedFrame = m_FrameNumber - 1;
			// GetLastPresentedFrame() is sentinel-initialized to ~uint64(0) ("nothing presented
			// yet") rather than 0, since 0 is also a legitimate real frame number - treat the
			// sentinel as "still waiting" rather than letting it satisfy the < comparison below.
			uint64 lastPresentedFrame = m_RenderSubmissionThread->GetLastPresentedFrame();
			while (lastPresentedFrame == ~uint64(0) || lastPresentedFrame < requiredPresentedFrame)
			{
				TYR_THREAD_SLEEP_MS(0);
				lastPresentedFrame = m_RenderSubmissionThread->GetLastPresentedFrame();
			}
		}

		const uint renderFrameIndex = m_RenderFrameIndex;
		const uint64 frameNumber = m_FrameNumber++;
		const TaskID task = TaskScheduler::Instance().CreateTask([this, renderFrameIndex, frameNumber]()
		{
			RenderAsync(renderFrameIndex, frameNumber);
		}, TaskLifetime::ManualRelease);

		if (m_PrevRenderAsyncTask != c_InvalidTaskID)
		{
			TaskScheduler::Instance().AddDependency(task, m_PrevRenderAsyncTask);
		}
		m_PrevRenderAsyncTask = task;

		// Whatever's still sitting in this slot is guaranteed already finished - by the time
		// m_RenderFrameIndex reaches renderFrameIndex again, PrepareForNextFrame's wait for it
		// has already run at least once - but may not have been released yet if it was still
		// m_PrevRenderAsyncTask at that point (see its comment). Release it now that it
		// definitely isn't anymore, so it isn't overwritten below without ever being released.
		if (m_RenderAsyncTasks[renderFrameIndex] != c_InvalidTaskID)
		{
			TaskScheduler::Instance().ReleaseTask(m_RenderAsyncTasks[renderFrameIndex]);
		}
		m_RenderAsyncTasks[renderFrameIndex] = task;
		m_RenderFrameNumbers[renderFrameIndex] = frameNumber;

		TaskScheduler::Instance().Enqueue(task);

		m_FirstRender = false;
	}

	void Renderer::RenderAsync(uint renderFrameIndex, uint64 frameNumber)
	{
		RenderRegistry& registry = *RenderRegistry::Instance();

		const RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];
		FrameContext& frameCtx = m_Ctx.frameContexts[renderFrameIndex];

		m_Data.activeScene = renderFrame.activeScene;

		m_Data.BeginFrame();

		const SceneFrame& sceneFrame = renderFrame.sceneFrame;
		Scene& scene = m_Data.scenes[m_Data.activeScene.h];

		if (sceneFrame.newWindow)
		{
			scene.windowHandle = sceneFrame.newWindow;
		}

		RenderWindow& window = m_WindowPool[scene.windowHandle.h];
		RenderWindowFrame& windowFrame = window.frames[renderFrameIndex];

		// A swap chain's acquire/present calls must be externally synchronized against each
		// other. Present() is issued from RenderSubmissionThread, and this function can run on
		// any worker-pool thread, so acquiring here directly would let the two race on the same
		// VkSwapchainKHR - instead, ask RenderSubmissionThread to do the acquire too, keeping
		// every call touching this swap chain on that one thread, and spin-wait for its result
		// (RenderAsync calls are already serialized, so only ever one request in flight).
		RenderAcquireRequest acquireRequest;
		acquireRequest.swapChain = window.swapChain;
		acquireRequest.semaphore = windowFrame.aquireSwapChainImageSemaphore;
		acquireRequest.window = scene.windowHandle;
		m_RenderSubmissionThread->EnqueueRenderAcquireRequest(acquireRequest);

		Optional<RenderAcquireResult> acquireResult = m_RenderSubmissionThread->DequeueAcquireResult();
		while (!acquireResult)
		{
			TYR_THREAD_SLEEP_MS(0);
			acquireResult = m_RenderSubmissionThread->DequeueAcquireResult();
		}

		// A failed acquire (VK_ERROR_OUT_OF_DATE_KHR) leaves no valid image - swapChainImageIndex
		// must not be touched in that case (see BuildAndExecuteRenderGraph, which skips the
		// geometry pass and present entirely when this is false). Either way, resizeNeeded was
		// already turned into a RenderNotification by RenderSubmissionThread - Render() drains
		// and resizes from those itself, so nothing further is needed here.
		const bool hasValidSwapChainImage = acquireResult->valid;
		if (hasValidSwapChainImage)
		{
			window.swapChainImageIndex = acquireResult->imageIndex;
		}

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

		BuildAndExecuteRenderGraph(renderFrameIndex, frameNumber, hasValidSwapChainImage);
	}

	void Renderer::RecordTransferPass(CommandList& cmdList)
	{
		m_TransferPass->Execute(cmdList);
	}

	void Renderer::RecordGeometryPass(CommandList& cmdList, uint renderFrameIndex)
	{
		// EditorViewport hasn't requested a render target size yet (e.g. the very first few
		// frames, before any ImGui layout has happened) - nothing to render into.
		if (m_Resources.viewportWidth == 0 || m_Resources.viewportHeight == 0)
		{
			return;
		}

		const Texture& viewportTexture = m_Registry.GetTexture(m_Resources.viewportColourTexture);

		Viewport viewport;
		viewport.width = m_Resources.viewportWidth;
		viewport.height = m_Resources.viewportHeight;

		// A freshly (re)created viewport texture's image is undefined until this transition -
		// unlike the swap chain image (see RecordGUIPass), this one isn't cycled every frame,
		// so the transition must only run once per (re)creation, not every frame.
		if (m_Resources.viewportTextureIsNew)
		{
			ImageBarrier barrier{};
			barrier.image = viewportTexture.image;
			barrier.srcAccess = BARRIER_ACCESS_NONE;
			barrier.dstAccess = BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.srcLayout = IMAGE_LAYOUT_UNKNOWN;
			barrier.dstLayout = viewportTexture.imageLayout;
			barrier.subresourceRange.aspect = SUBRESOURCE_ASPECT_COLOUR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.mipCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.arrayLayerCount = 1;
			barrier.srcStage = PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.dstStage = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			cmdList.AddBarriers(nullptr, 0, &barrier, 1);
			m_Resources.viewportTextureIsNew = false;
		}

		// TODO: Render for all views and create render area for each view by calling GraphicsUtility::CreateRenderArea
		RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extents = { m_Resources.viewportWidth, m_Resources.viewportHeight };
		renderingInfo.viewMask = 0;
		renderingInfo.layerCount = 1;

		RenderingAttachmentInfo colourAttachment;
		colourAttachment.loadOp = AttachmentLoadOp::Clear;
		colourAttachment.storeOp = AttachmentStoreOp::Store;
		colourAttachment.resolveMode = RESOLVE_MODE_NONE;
		colourAttachment.imageLayout = viewportTexture.imageLayout;
		colourAttachment.clearValue.colour = { 0.0f, 0.0f, 0.0f, 0.0f };
		colourAttachment.imageView = viewportTexture.imageView;

		renderingInfo.colourAttachmentCount = 1;
		renderingInfo.colourAttachments = &colourAttachment;

		cmdList.BeginRendering(renderingInfo);
		cmdList.SetViewport(&viewport, 1);
		cmdList.SetScissor(&renderingInfo.renderArea, 1);
		m_GeometryPass->Execute(cmdList);
		cmdList.EndRendering();
	}

	void Renderer::RecordGUIPass(CommandList& cmdList, uint renderFrameIndex)
	{
		Scene& scene = m_Data.scenes[m_Data.activeScene.h];
		RenderWindow& window = m_WindowPool[scene.windowHandle.h];

		const uint windowWidth = window.swapChain->GetWidth();
		const uint windowHeight = window.swapChain->GetHeight();
		Viewport viewport;
		viewport.width = windowWidth;
		viewport.height = windowHeight;

		const ImageHandle swapChainImage = window.swapChain->GetImages()[window.swapChainImageIndex];

		// A newly acquired swap chain image's layout is undefined - transition it to whatever
		// layout rendering needs before using it as a colour attachment below. This used to
		// happen in RecordGeometryPass, back when it drew straight into the swap chain image -
		// now that GeometryPass renders into its own offscreen viewport texture instead (see
		// EditorViewport), this pass is the first (and only) one touching the swap chain image
		// each frame.
		{
			ImageBarrier barrier{};
			window.swapChain->CreateRenderingImageBarrier(barrier, swapChainImage);
			cmdList.AddBarriers(nullptr, 0, &barrier, 1);
		}

		RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extents = { windowWidth, windowHeight };
		renderingInfo.viewMask = 0;
		renderingInfo.layerCount = 1;

		RenderingAttachmentInfo colourAttachment;
		// Clear, not Load - GeometryPass no longer draws into this image; the 3D scene now
		// lives in its own offscreen texture, shown inside the Viewport panel via
		// ImGui::Image() (EditorViewport).
		colourAttachment.loadOp = AttachmentLoadOp::Clear;
		colourAttachment.storeOp = AttachmentStoreOp::Store;
		colourAttachment.resolveMode = RESOLVE_MODE_NONE;
		colourAttachment.imageLayout = window.swapChain->GetRenderingLayout();
		colourAttachment.clearValue.colour = { 0.0f, 0.0f, 0.0f, 0.0f };
		colourAttachment.imageView = window.swapChain->GetImageViews()[window.swapChainImageIndex];

		renderingInfo.colourAttachmentCount = 1;
		renderingInfo.colourAttachments = &colourAttachment;

		cmdList.BeginRendering(renderingInfo);
		cmdList.SetViewport(&viewport, 1);
		cmdList.SetScissor(&renderingInfo.renderArea, 1);
		m_GUIPass->Execute(cmdList);
		cmdList.EndRendering();

		// Presenting requires the image to be in the present-source layout, not whatever
		// rendering left it in - GUI is always the last thing drawn into this image each frame.
		{
			ImageBarrier barrier{};
			window.swapChain->CreatePresentingImageBarrier(barrier, swapChainImage);
			cmdList.AddBarriers(nullptr, 0, &barrier, 1);
		}
	}

	void Renderer::BuildAndExecuteRenderGraph(uint renderFrameIndex, uint64 frameNumber, bool hasValidSwapChainImage)
	{
		RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];
		FrameContext& frameCtx = m_Ctx.frameContexts[renderFrameIndex];

		Scene& scene = m_Data.scenes[m_Data.activeScene.h];

		TransferPassArgs transferArgs;
		transferArgs.device = m_Ctx.device;
		transferArgs.data = &m_Data;
		transferArgs.registry = &m_Registry;
		transferArgs.resources = &m_Resources;
		transferArgs.renderFrame = &renderFrame;
		m_TransferPass->Recreate(transferArgs);

		GeometryPassArgs geometryArgs;
		geometryArgs.device = m_Ctx.device;
		geometryArgs.registry = &m_Registry;
		geometryArgs.allocManager = &m_AllocManager;
		geometryArgs.resources = &m_Resources;
		geometryArgs.scene = &scene;
		geometryArgs.pipeline = m_Resources.geometryGraphicsPipeline;
		geometryArgs.descriptorSet = m_Resources.descriptorSet;
		m_GeometryPass->Recreate(geometryArgs);

		GUIPassArgs guiArgs;
		guiArgs.registry = &m_Registry;
		guiArgs.pipeline = m_Resources.guiPipeline;
		guiArgs.descriptorSet = m_Resources.descriptorSet;
		guiArgs.vertexBuffer = m_Resources.guiVertexBuffer;
		guiArgs.indexBuffer = m_Resources.guiIndexBuffer;
		guiArgs.renderFrame = &renderFrame;
		m_GUIPass->Recreate(guiArgs);

		RenderWindow& window = m_WindowPool[scene.windowHandle.h];
		RenderWindowFrame& windowFrame = window.frames[renderFrameIndex];

		// Temporarily use first one
		CommandList* cmdList = frameCtx.commandLists[CommandQueueType::CQ_GRAPHICS][0];
		cmdList->Reset(true);
		cmdList->Begin(CommandBufferUsage::COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		RenderGraphAllocator::NextFrame();
		RenderGraph graph;

		// Every buffer TransferPass might write and GeometryPass reads - both passes run on
		// the graphics queue for now (see TransferPass/GeometryPass), so BuildBarriers just
		// inserts a same-queue pipeline barrier between them; no cross-queue semaphore wait
		// is needed yet.
		const RenderBufferHandle graphBuffers[] = {
			m_Resources.meshBuffer, m_Resources.meshLODBuffer, m_Resources.meshletBuffer,
			m_Resources.vertexBuffer, m_Resources.indexBuffer, m_Resources.meshInstanceBuffer,
			m_Resources.materialBuffer, m_Resources.directionalLightBuffer, m_Resources.pointLightBuffer,
			m_Resources.spotLightBuffer, m_Resources.sceneInfoBuffer
		};
		constexpr uint bufferCount = (uint)(sizeof(graphBuffers) / sizeof(graphBuffers[0]));

		for (uint i = 0; i < bufferCount; ++i)
		{
			graph.RegisterBuffer(&m_Registry.GetBuffer(graphBuffers[i]));
		}

		// Tracked separately from graphBuffers above - GeometryPass doesn't touch these, only
		// TransferPass (writes, via SubmitGUIDrawData's upload requests) and GUIPass (reads).
		const RenderBufferHandle guiGraphBuffers[] = { m_Resources.guiVertexBuffer, m_Resources.guiIndexBuffer };
		constexpr uint guiBufferCount = (uint)(sizeof(guiGraphBuffers) / sizeof(guiGraphBuffers[0]));

		for (uint i = 0; i < guiBufferCount; ++i)
		{
			graph.RegisterBuffer(&m_Registry.GetBuffer(guiGraphBuffers[i]));
		}

		graph.AddPass("Transfer",
			[this](RenderGraphBuilder& builder) { m_TransferPass->Setup(builder); },
			[this](CommandList& cl) { RecordTransferPass(cl); },
			RenderGraphPhase::Transfer, CommandQueueType::CQ_GRAPHICS);

		// Nothing to draw into or present without a valid acquired image (see RenderAsync -
		// this only happens when acquisition genuinely failed, VK_ERROR_OUT_OF_DATE_KHR). The
		// Transfer pass above still runs - it's not tied to the window - so this frame's uploads
		// aren't skipped, just its geometry/GUI/present.
		if (hasValidSwapChainImage)
		{
			graph.AddPass("Geometry",
				[this](RenderGraphBuilder& builder) { m_GeometryPass->Setup(builder); },
				[this, renderFrameIndex](CommandList& cl) { RecordGeometryPass(cl, renderFrameIndex); },
				RenderGraphPhase::Geometry, CommandQueueType::CQ_GRAPHICS);

			// Always added, even on a frame with nothing to draw - it's what transitions the
			// swap chain image to the present-source layout now (see RecordGUIPass), so it has
			// to run whether or not any GUIDrawData was actually submitted this frame.
			graph.AddPass("GUI",
				[this](RenderGraphBuilder& builder) { m_GUIPass->Setup(builder); },
				[this, renderFrameIndex](CommandList& cl) { RecordGUIPass(cl, renderFrameIndex); },
				RenderGraphPhase::Output, CommandQueueType::CQ_GRAPHICS);
		}

		graph.Compile();

		CommandList* cmdLists[CommandQueueType::CQ_COUNT] = {};
		cmdLists[CommandQueueType::CQ_GRAPHICS] = cmdList;
		graph.Execute(cmdLists);

		cmdList->End();

		// Indexed by the acquired swap chain image, not renderFrameIndex - see
		// RenderWindow::executeCompleteSemaphores' comment. Only meaningful if there's actually
		// an image this frame.
		const SemaphoreHandle executeCompleteSemaphore = hasValidSwapChainImage
			? window.executeCompleteSemaphores[window.swapChainImageIndex]
			: SemaphoreHandle{};

		RenderSubmissionRequest submissionRequest;
		submissionRequest.queueType = CommandQueueType::CQ_GRAPHICS;
		submissionRequest.commandLists.Add(cmdList);
		if (hasValidSwapChainImage)
		{
			// Wait on the semaphore used when acquiring the next swapchain image
			submissionRequest.waitSemaphores.Add(windowFrame.aquireSwapChainImageSemaphore);
			submissionRequest.waitValues.Add(0);
			submissionRequest.waitDstPipelineStages.Add(PipelineStage::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
			submissionRequest.signalSemaphores.Add(executeCompleteSemaphore);
			// A dummy value needs to be pushed back for the binary semaphore as the count of the number of values must match the number of semaphores
			// (as per the vulkan spec)
			submissionRequest.signalValues.Add(0);
		}
		submissionRequest.reportCompletion = true;
		submissionRequest.renderFrameIndex = renderFrameIndex;
		submissionRequest.frameNumber = frameNumber;
		m_RenderSubmissionThread->EnqueueRenderSubmissionRequest(submissionRequest);

		// Always enqueued, even without a valid image - RenderSubmissionThread still needs to
		// record this frameNumber as "handled" (see RenderPresentRequest::present's comment) so
		// Render()'s pacing wait doesn't stall waiting for a present that was never going to
		// happen. It just skips the actual Present() call when present is false.
		RenderPresentRequest presentRequest;
		presentRequest.swapChain = window.swapChain;
		presentRequest.queue = m_Ctx.graphicsQueue;
		// Wait on the signal semaphore used in the submission above
		presentRequest.waitSemaphore = executeCompleteSemaphore;
		presentRequest.imageIndex = window.swapChainImageIndex;
		presentRequest.frameNumber = frameNumber;
		presentRequest.window = scene.windowHandle;
		presentRequest.present = hasValidSwapChainImage;
		m_RenderSubmissionThread->EnqueueRenderPresentRequest(presentRequest);
	}

	void Renderer::DrainSubmissionCompletions()
	{
		while (Optional<RenderFrameCompletion> completion = m_RenderSubmissionThread->DequeueFrameCompletion())
		{
			RenderSyncData& syncData = m_SyncDatas[completion->renderFrameIndex];
			syncData.completionTimelineValue = completion->timelineValue;
			syncData.frameNumber = completion->frameNumber;
			m_AllocManager.SignalFrameUpload(completion->timelineValue);

			// This slot's asset/texture upload requests are exactly what RenderAsync merged into
			// m_Data and handed to TransferPass for the submission this completion reports on -
			// PrepareForNextFrame won't clear (and so can't yet be reusing) this slot until it has
			// drained this exact completion first (it spin-drains via this same function while
			// waiting on frameNumber to match), so reading them here is safe. Each request whose
			// data came from a resource upload allocation (see BufferUploadRequest::resourceId)
			// gets signalled now so ResourceUploadAllocator can reclaim it once the GPU catches up.
			const RenderFrame& renderFrame = m_RenderFrames[completion->renderFrameIndex];
			for (const BufferUploadRequest& request : renderFrame.assetBufferUploadRequests)
			{
				if (request.resourceId)
				{
					m_AllocManager.SignalResourceUpload(request.resourceId, completion->timelineValue);
				}
			}
			for (const TextureUploadRequest& request : renderFrame.textureUploadRequests)
			{
				if (request.resourceId)
				{
					m_AllocManager.SignalResourceUpload(request.resourceId, completion->timelineValue);
				}
			}
		}
	}

	void Renderer::PrepareForNextFrame()
	{
		m_RenderFrameIndex = (m_RenderFrameIndex + 1) % RenderConstants::c_BufferedFrameCount;
		RenderFrame& renderFrame = GetRenderFrame();

		// RenderAsync reads this slot's RenderFrame by reference on a worker thread. If the
		// main thread has been producing frames faster than RenderAsync can consume them, that
		// task might not even have started yet - wait for it here before this slot's data gets
		// touched below/cleared at the end of this function. The GPU semaphore wait further
		// down can't substitute for this: its completion value is still 0 (same as "nothing
		// submitted yet") until the task has actually run and submitted something.
		TaskID& slotTask = m_RenderAsyncTasks[m_RenderFrameIndex];
		// Captured before the release logic below can clear slotTask back to c_InvalidTaskID -
		// this is the one true way to know whether this slot was ever actually used, since a
		// completionTimelineValue of 0 is ambiguous (see the wait further down).
		const bool slotWasUsed = (slotTask != c_InvalidTaskID);

		if (slotWasUsed)
		{
			TaskScheduler::Instance().WaitOnTask(slotTask);

			// Render() skips a tick (and so skips creating a task) whenever there's nothing
			// ready to render yet - e.g. at startup, before RenderAsync has caught up on
			// resolving the active scene's window. Skipped ticks still land here every time
			// m_RenderFrameIndex cycles back around, though, so a long enough run of them can
			// bring us back to this exact slot while its task is still the one m_PrevRenderAsyncTask
			// points to (the next real Render() call needs to depend on it) - releasing it here
			// regardless would leave that dependency pointing at a freed/reused TaskID. Only
			// release once a newer task has taken over as m_PrevRenderAsyncTask; otherwise leave
			// it recorded and try again next time this slot comes back around.
			if (slotTask != m_PrevRenderAsyncTask)
			{
				TaskScheduler::Instance().ReleaseTask(slotTask);
				slotTask = c_InvalidTaskID;
			}
		}

		const SemaphoreHandle graphicsTimelineSemaphore = m_Ctx.graphicsQueue->GetTimelineSemaphore();

		// This slot was last used 3 frames ago - wait for that frame's GPU work to be done
		// before touching anything tied to it below (its RenderAsync task's CPU-side work is
		// already known finished, from the wait above). completionTimelineValue is reported
		// asynchronously by RenderSubmissionThread, so the report for this slot's most recent
		// use might not have arrived yet even though an older, stale report (from an earlier
		// use of the same slot) is sitting there with a real, nonzero value - checking against
		// m_RenderFrameNumbers (the frame this exact use was submitted under) is what tells
		// a fresh report apart from a stale one, instead of just checking for nonzero.
		RenderSyncData* syncData = &m_SyncDatas[m_RenderFrameIndex];
		if (slotWasUsed)
		{
			const uint64 expectedFrameNumber = m_RenderFrameNumbers[m_RenderFrameIndex];
			while (syncData->frameNumber != expectedFrameNumber)
			{
				TYR_THREAD_SLEEP_MS(0);
				DrainSubmissionCompletions();
			}
			m_Ctx.device->WaitForSemaphore(graphicsTimelineSemaphore, syncData->completionTimelineValue, UINT64_MAX);
		}

		// Resource uploads share the graphics queue for now too, in the absence of a
		// transfer queue. Every resource upload allocation still live at this point was already
		// signalled by DrainSubmissionCompletions (above) as soon as its submission's timeline
		// value was known - nothing left to drain here.
		const uint64 graphicsCompletedValue = m_Ctx.device->GetSemaphoreValue(graphicsTimelineSemaphore);
		m_AllocManager.ReclaimResourceUploadMemory(graphicsCompletedValue);
		m_AllocManager.ReclaimFrameUploadMemory(graphicsCompletedValue);

		ProcessFrameDeleteLists(renderFrame);
		renderFrame.Clear();
	}

	void Renderer::ProcessFrameDeleteLists(RenderFrame& renderFrame)
	{
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
		for (const PendingWindowDelete& pending : renderFrame.windowsToDelete)
		{
			DeleteWindowResources(pending);
		}
		for (SceneHandle handle : renderFrame.scenesToDelete)
		{
			m_Data.scenes.Delete(handle.h);
		}
	}

	void Renderer::WaitForCompletion()
	{
		// The last RenderAsync task might still be running - wait for it to actually finish
		// before waiting on the GPU/tearing anything down, since it still uses this Renderer.
		// Every earlier task depended on the one before it, so this alone guarantees all of
		// them (including every entry still sitting in m_RenderAsyncTasks below) are done too.
		if (m_PrevRenderAsyncTask != c_InvalidTaskID)
		{
			TaskScheduler::Instance().WaitOnTask(m_PrevRenderAsyncTask);

			m_PrevRenderAsyncTask = c_InvalidTaskID;
		}

		// PrepareForNextFrame only releases a slot's task once that slot cycles back around -
		// release whatever's left here so nothing leaks its ManualRelease task slot on shutdown.
		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			if (m_RenderAsyncTasks[i] != c_InvalidTaskID)
			{
				TaskScheduler::Instance().ReleaseTask(m_RenderAsyncTasks[i]);
				m_RenderAsyncTasks[i] = c_InvalidTaskID;
			}
		}

		// No WaitIdle() here: the last RenderAsync task having finished only means its work was
		// enqueued to RenderSubmissionThread, not that the thread has actually issued it yet (or
		// stopped). Calling vkDeviceWaitIdle() from this thread while that one might still be
		// mid-vkQueueSubmit/vkQueuePresentKHR is a real Vulkan threading violation - the caller
		// (the destructor) already waits properly, after that thread is torn down.
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
		// The swap chain must have as many images as the renderer keeps frames in flight, or
		// it runs out of images to acquire before earlier ones are done with (see
		// SwapChainDesc::minImageCount's comment).
		swapChainDesc.minImageCount = RenderConstants::c_BufferedFrameCount;

		// Always a fresh swap chain - never reuses one from a just-removed window at the same
		// pool slot. RemoveWindow only queues its old swap chain for deferred deletion (see its
		// own comment), so one could still be pending here; recreating it in place instead of
		// creating a new one would race against that deferred delete. Recreate() stays reserved
		// for ResizeWindow, where the window was never removed in the first place.
		SwapChain* swapChain = m_Ctx.device->CreateSwapChain(osHandle, swapChainDesc);
		renderWindow.swapChain = swapChain;

		// Binary semaphores used to sync swapchain image acquisition/presentation with command
		// submission. RenderWindow is reset whenever its pool slot is freed in RemoveWindow, so
		// these always need recreating here regardless of whether the swap chain itself was
		// fresh or reused.
		SemaphoreDesc semaphoreDesc;

		// Indexed by frame-in-flight - needed before AcquireNextImage returns an image index.
		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
#if !TYR_FINAL
			semaphoreDesc.debugName = "AcquireSwapChainImageSemaphore";
#endif
			renderWindow.frames[i].aquireSwapChainImageSemaphore = m_Ctx.device->CreateSemaphoreResource(semaphoreDesc);
		}

		// Indexed by swap chain image index instead (see RenderWindow::executeCompleteSemaphores'
		// comment for why frame-in-flight indexing isn't safe for this one).
		for (uint i = 0; i < SwapChain::c_MaxImages; ++i)
		{
#if !TYR_FINAL
			semaphoreDesc.debugName = "ExecuteCompleteSemaphore";
#endif
			renderWindow.executeCompleteSemaphores[i] = m_Ctx.device->CreateSemaphoreResource(semaphoreDesc);
		}

		return handle;
	}

	void Renderer::RemoveWindow(RenderWindowHandle window)
	{
		// The swap chain/semaphores can't be deleted right here - RenderAsync or
		// RenderSubmissionThread might still be using them from up to c_BufferedFrameCount
		// frames ago. Queue a copy of the resource handles, and the pool handle itself, on this
		// tick's RenderFrame instead; PrepareForNextFrame deletes/frees them for real once this
		// slot cycles back around and its GPU work is confirmed done (same pattern as every
		// other *ToDelete list), or - if that never happens because the app is shutting down -
		// Shutdown()/the destructor flush them once nothing async is left running (see
		// DeleteWindows' comment).
		//
		// Deferring the pool slot's own deletion (not just the GPU resources) means this
		// function only ever *reads* the live pool slot - never writes it - so it's safe to call
		// even while a worker thread might still be concurrently reading (or, for
		// swapChainImageIndex specifically, writing - but DeleteWindowResources never uses that
		// field, so a stale/torn copy of it here is harmless) the same RenderWindow.
		GetRenderFrame().windowsToDelete.Add(PendingWindowDelete{ window, m_WindowPool[window.h] });
	}

	void Renderer::ResizeWindow(RenderWindowHandle window)
	{
		// Resizing isn't frequent enough in practice for anything cleverer to be worth it -
		// stall the main thread until every already-created RenderAsync task (CPU-side
		// recording), everything RenderSubmissionThread has queued for them (GPU submit/
		// present), and the GPU itself have all finished, so nothing can still be touching this
		// window's swap chain when it's recreated below. Mirrors Shutdown()'s quiesce, minus
		// stopping the submission thread - rendering carries on normally once this returns.
		WaitForCompletion();
		if (m_FrameNumber > 0)
		{
			const uint64 lastFrameNumber = m_FrameNumber - 1;
			while (m_RenderSubmissionThread->GetLastPresentedFrame() != lastFrameNumber)
			{
				TYR_THREAD_SLEEP_MS(0);
			}
		}
		m_Ctx.device->WaitIdle();

		// No width/height parameter - SwapChain::Resize() reads the current extent straight from
		// the surface itself (vkGetPhysicalDeviceSurfaceCapabilitiesKHR).
		RenderWindow& renderWindow = m_WindowPool[window.h];
		SwapChain* swapChain = renderWindow.swapChain;
		swapChain->Resize();
		// Safe to destroy the retired swap chain immediately instead of deferring it (unlike
		// Resize()'s usual oldSwapchain-kept-around pattern) - everything's confirmed idle above,
		// so nothing can still be using it.
		swapChain->DestroyOldSwapChain();
	}

	void Renderer::CreateQueues()
	{
		m_Ctx.graphicsQueue = m_Ctx.device->CreateCommandQueue(CommandQueueType::CQ_GRAPHICS, 0, "GraphicsQueue");
		TYR_ASSERT(m_Ctx.graphicsQueue);

		// Dedicated compute/transfer queues aren't guaranteed to exist - CreateCommandQueue
		// returns null when they don't, and the rest of the renderer already falls back to
		// the graphics queue wherever that happens.
		m_Ctx.computeQueue = m_Ctx.device->CreateCommandQueue(CommandQueueType::CQ_COMPUTE, 0, "ComputeQueue");
		m_Ctx.transferQueue = m_Ctx.device->CreateCommandQueue(CommandQueueType::CQ_TRANSFER, 0, "TransferQueue");
	}

	void Renderer::DeleteQueues()
	{
		delete m_Ctx.graphicsQueue;
		m_Ctx.graphicsQueue = nullptr;
		delete m_Ctx.computeQueue;
		m_Ctx.computeQueue = nullptr;
		delete m_Ctx.transferQueue;
		m_Ctx.transferQueue = nullptr;
	}

	void Renderer::CreateShaders()
	{
		ShaderCreator::LoadCompilerLibs();
		ShaderCompileConfig shaderCompileConfig;
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "MeshAS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_TASK_BIT;
			m_Resources.geometryTaskShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "MeshMS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_MESH_BIT;
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
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "GUIVS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_VERTEX_BIT;
			m_Resources.guiVertexShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "GUIPS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_FRAGMENT_BIT;
			m_Resources.guiPixelShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
	}

	void Renderer::DeleteShaders()
	{
		m_Ctx.device->DeleteShaderModule(m_Resources.geometryTaskShader);
		m_Ctx.device->DeleteShaderModule(m_Resources.geometryMeshShader);
		m_Ctx.device->DeleteShaderModule(m_Resources.geometryPixelShader);
		m_Ctx.device->DeleteShaderModule(m_Resources.guiVertexShader);
		m_Ctx.device->DeleteShaderModule(m_Resources.guiPixelShader);
		ShaderCreator::UnloadCompilerLibs();
	}

	void Renderer::DeleteWindowResources(const PendingWindowDelete& pending)
	{
		const RenderWindow& window = pending.window;
		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			m_Ctx.device->DeleteSemaphoreResource(window.frames[i].aquireSwapChainImageSemaphore);
		}
		for (uint i = 0; i < SwapChain::c_MaxImages; ++i)
		{
			m_Ctx.device->DeleteSemaphoreResource(window.executeCompleteSemaphores[i]);
		}
		delete window.swapChain;
		// Freed here rather than immediately in RemoveWindow - see its own comment.
		m_WindowPool.Delete(pending.handle.h);
	}

	void Renderer::DeleteRemainingFrameResources()
	{
		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			ProcessFrameDeleteLists(m_RenderFrames[i]);
			m_RenderFrames[i].Clear();
		}
	}

	void Renderer::CreateCommandObjects()
	{
		auto CreateQueueCommandObjects = [this](FrameContext& frameCtx, uint frameIndex, CommandQueueType queueType)
		{
			CommandAllocatorDesc allocDesc;
			allocDesc.debugName = GDebugString("CommandAllocator_") + (int)frameIndex;
			// The reset flag is for command buffers that will be reset / re-recorded and transient is for short-lived command buffers
			allocDesc.flags = CommandAllocatorCreateFlags::COMMAND_ALLOC_CREATE_RESET_COMMAND_BUFFER_BIT;
			allocDesc.queueType = queueType;
			frameCtx.commandAllocators[queueType] = m_Ctx.device->CreateCommandAllocator(allocDesc);

			Array<CommandList*>& cmdLists = frameCtx.commandLists[queueType];
			cmdLists.Reserve(TaskScheduler::c_MaxWorkers);
			const GDebugString nameStart = GDebugString("CommandList_") + (int)frameIndex + "_";
			CommandListDesc listDesc;
			listDesc.allocator = frameCtx.commandAllocators[queueType];
			listDesc.debugName = nameStart + cmdLists.Size();
			listDesc.type = CommandListType::Primary;
			cmdLists.Add(m_Ctx.device->CreateCommandList(listDesc));
		};

		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			FrameContext& frameCtx = m_Ctx.frameContexts[i];
			CreateQueueCommandObjects(frameCtx, i, CommandQueueType::CQ_GRAPHICS);

			if (m_Ctx.transferQueue)
			{
				CreateQueueCommandObjects(frameCtx, i, CommandQueueType::CQ_TRANSFER);
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
			for (uint q = 0; q < CommandQueueType::CQ_COUNT; ++q)
			{
				if (!frameCtx.commandAllocators[q])
				{
					continue;
				}

				for (uint c = 0; c < frameCtx.commandLists[q].Size(); ++c)
				{
					delete frameCtx.commandLists[q][c];
				}
				frameCtx.commandLists[q].Clear();

				delete frameCtx.commandAllocators[q];
				frameCtx.commandAllocators[q] = nullptr;
			}
		}
	}

	void Renderer::CreatePipelines()
	{
		GraphicsPipelineDesc desc;
		{
			DescriptorPoolDesc poolDesc;
			poolDesc.maxSets = 1;
			{
				DescriptorPoolSize& poolSize = poolDesc.poolSizes.ExpandOne();
				poolSize.descriptorType = DescriptorType::UniformBuffer;
				poolSize.descriptorCount = 1;
			}
			{
				DescriptorPoolSize& poolSize = poolDesc.poolSizes.ExpandOne();
				poolSize.descriptorType = DescriptorType::StorageBuffer;
				poolSize.descriptorCount = 11;
			}
			{
				DescriptorPoolSize& poolSize = poolDesc.poolSizes.ExpandOne();
				poolSize.descriptorType = DescriptorType::SampledImage;
				poolSize.descriptorCount = RenderConstants::c_MaxTextures;
			}
			{
				DescriptorPoolSize& poolSize = poolDesc.poolSizes.ExpandOne();
				poolSize.descriptorType = DescriptorType::Sampler;
				poolSize.descriptorCount = Device::c_MaxSamplers;
			}
			poolDesc.flags = DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
#if !TYR_FINAL
			poolDesc.debugName = "DescriptorPool";
#endif
			m_Resources.descriptorPool = m_Ctx.device->CreateDescriptorPool(poolDesc);

			DescriptorSetLayoutDesc layoutDesc;
#if !TYR_FINAL
			layoutDesc.debugName = "DescriptorSetLayout";
#endif
			layoutDesc.flags = DESCRIPTOR_SET_LAYOUT_UPDATE_AFTER_BIND_POOL_BIT;

			const ShaderStage meshPipelineStages = static_cast<ShaderStage>(SHADER_STAGE_TASK_BIT | SHADER_STAGE_MESH_BIT | SHADER_STAGE_FRAGMENT_BIT);

			auto AddBinding = [&](uint bindingIndex, DescriptorType type, uint count, ShaderStage stageFlags, DescriptorBindingFlags bindingFlags = DESCRIPTOR_BINDING_NONE)
			{
				DescriptorSetLayoutBinding& binding = layoutDesc.bindings.ExpandOne();
				binding.binding = bindingIndex;
				binding.descriptorType = type;
				binding.descriptorCount = count;
				binding.stageFlags = stageFlags;
				binding.bindingFlags = bindingFlags;
			};

			AddBinding(TYR_BINDING_SCENE_INFO, DescriptorType::UniformBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_MESH, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_MESH_LOD, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_MESHLET, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_VERTEX, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_INDEX, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_MESH_INSTANCE, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_MATERIAL, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_DIR_LIGHT, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_POINT_LIGHT, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_SPOT_LIGHT, DescriptorType::StorageBuffer, 1, meshPipelineStages);

			const DescriptorBindingFlags bindlessFlags = static_cast<DescriptorBindingFlags>(DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
			AddBinding(TYR_BINDING_TEXTURES, DescriptorType::SampledImage, RenderConstants::c_MaxTextures, meshPipelineStages, bindlessFlags);
			AddBinding(TYR_BINDING_SAMPLERS, DescriptorType::Sampler, Device::c_MaxSamplers, meshPipelineStages, bindlessFlags);
			// GUIVS.hlsl reads this directly (vertex-pulling via SV_VertexID) - a vertex stage,
			// not one of the mesh pipeline's task/mesh/fragment stages. Declared last, matching
			// its binding number (13) being the highest - VulkanDescriptorSet.cpp's
			// bindingDescriptorTypes lookup assumes bindings are declared in ascending binding-
			// number order with no gaps, so this has to stay last as long as that holds.
			AddBinding(TYR_BINDING_GUI_VERTEX, DescriptorType::StorageBuffer, 1, SHADER_STAGE_VERTEX_BIT);

			m_Resources.descriptorSetLayout = m_Ctx.device->CreateDescriptorSetLayout(layoutDesc);

			DescriptorSetDesc setDesc;
			setDesc.layout = m_Resources.descriptorSetLayout;
			setDesc.pool = m_Resources.descriptorPool;
#if !TYR_FINAL
			setDesc.debugName = "DescriptorSet";
#endif
			m_Resources.descriptorSet = m_Ctx.device->CreateDescriptorSet(setDesc);

			desc.pipelineLayoutDesc.descriptorSetLayouts.Add(m_Resources.descriptorSetLayout);

			// Which mesh instance this draw call is for.
			PushConstantRange& pushConstantRange = desc.pipelineLayoutDesc.pushConstantRanges.ExpandOne();
			pushConstantRange.stageFlags = meshPipelineStages;
			pushConstantRange.offset = 0;
			pushConstantRange.size = sizeof(uint);
		}

		desc.topology = PrimitiveTopology::TriangeList;

		// Mesh shaders read geometry data themselves - no vertex input state needed.

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

		desc.shaders.Add(m_Resources.geometryTaskShader);
		desc.shaders.Add(m_Resources.geometryMeshShader);
		desc.shaders.Add(m_Resources.geometryPixelShader);

		m_Resources.geometryGraphicsPipeline = m_Ctx.device->CreateGraphicsPipeline(desc);

		// GUI pipeline - reuses the same bindless descriptor set layout (its font/UI textures
		// are just more entries in the same textures[] array), but is otherwise a completely
		// separate pipeline: a plain vertex+pixel shader pair (vertex-pulling via
		// TYR_BINDING_GUI_VERTEX, not fixed-function vertex input) with alpha blending on and
		// no depth testing, drawn over whatever GeometryPass already rendered.
		GraphicsPipelineDesc guiDesc;
		guiDesc.topology = PrimitiveTopology::TriangeList;

		guiDesc.blendStateDesc.srcColorBlendFactor = BlendFactor::SrcAlpha;
		guiDesc.blendStateDesc.destColorBlendFactor = BlendFactor::InvSrcAlpha;
		guiDesc.blendStateDesc.colorBlendOp = BlendOp::Add;
		guiDesc.blendStateDesc.srcAlphaBlendFactor = BlendFactor::One;
		guiDesc.blendStateDesc.destAlphaBlendFactor = BlendFactor::InvSrcAlpha;
		guiDesc.blendStateDesc.alphaBlendOp = BlendOp::Add;
		guiDesc.blendStateDesc.colorWriteMask = static_cast<ColorComponent>(COLOR_COMPONENT_R_BIT | COLOR_COMPONENT_G_BIT | COLOR_COMPONENT_B_BIT | COLOR_COMPONENT_A_BIT);
		guiDesc.blendStateDesc.blendEnabled = true;

		guiDesc.rasterizerStateDesc.depthBiasConstantFactor = 0.0f;
		guiDesc.rasterizerStateDesc.depthBiasClamp = 0.0f;
		guiDesc.rasterizerStateDesc.depthBiasSlopeFactor = 0.0f;
		guiDesc.rasterizerStateDesc.cullMode = CullMode::None;
		guiDesc.rasterizerStateDesc.polygonMode = PolygonMode::Fill;
		guiDesc.rasterizerStateDesc.frontFace = FrontFace::Clockwise;
		guiDesc.rasterizerStateDesc.depthClampEnabled = false;
		guiDesc.rasterizerStateDesc.depthBiasEnabled = false;

		guiDesc.depthStencilStateDesc = desc.depthStencilStateDesc;
		guiDesc.multiSampleDesc = desc.multiSampleDesc;

		guiDesc.dynamicRendering.colorAttachmentFormats.Add(PF_R8G8B8A8_SRGB);
		guiDesc.dynamicRendering.depthAttachmentFormat = PF_UNKNOWN;
		guiDesc.dynamicRendering.stencilAttachmentFormat = PF_UNKNOWN;
		guiDesc.dynamicRendering.viewMask = 0;

		guiDesc.pipelineLayoutDesc.descriptorSetLayouts.Add(m_Resources.descriptorSetLayout);

		const ShaderStage guiPipelineStages = static_cast<ShaderStage>(SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT);
		PushConstantRange& guiPushConstantRange = guiDesc.pipelineLayoutDesc.pushConstantRanges.ExpandOne();
		guiPushConstantRange.stageFlags = guiPipelineStages;
		guiPushConstantRange.offset = 0;
		guiPushConstantRange.size = sizeof(float) * 4 + sizeof(uint); // scale, translate, textureIndex - see GUIPass.cpp's GUIPushConstants

		guiDesc.shaders.Add(m_Resources.guiVertexShader);
		guiDesc.shaders.Add(m_Resources.guiPixelShader);

		m_Resources.guiPipeline = m_Ctx.device->CreateGraphicsPipeline(guiDesc);
	}

	void Renderer::DeletePipelines()
	{
		m_Ctx.device->DeleteGraphicsPipeline(m_Resources.geometryGraphicsPipeline);
		m_Ctx.device->DeleteGraphicsPipeline(m_Resources.guiPipeline);
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
			m_Resources.meshLODBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Mesh Instance Buffer";
			desc.size = sizeof(ShaderMeshInstance) * RenderConstants::c_MaxMeshInstances;
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
		{
			RenderBufferDesc desc;
			desc.debugName = "GUI Vertex Buffer";
			desc.size = RenderConstants::c_GUIVertexBufferSize;
			// Storage, not Vertex - GUIVS.hlsl pulls its own vertex via a StructuredBuffer
			// binding (TYR_BINDING_GUI_VERTEX) instead of fixed-function vertex input.
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.guiVertexBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "GUI Index Buffer";
			desc.size = RenderConstants::c_GUIIndexBufferSize;
			desc.usage = RenderBufferUsage::Index;
			desc.stride = sizeof(uint16);
			m_Resources.guiIndexBuffer = m_Registry.CreateBuffer(desc);
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
		m_Registry.DeleteBuffer(m_Resources.guiVertexBuffer);
		m_Registry.DeleteBuffer(m_Resources.guiIndexBuffer);
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
		TransferPassArgs transferArgs;
		transferArgs.device = m_Ctx.device;
		transferArgs.data = &m_Data;
		transferArgs.registry = &m_Registry;
		transferArgs.resources = &m_Resources;
		transferArgs.renderFrame = nullptr;
		m_TransferPass = new TransferPass(transferArgs);

		GeometryPassArgs geometryArgs;
		geometryArgs.device = m_Ctx.device;
		geometryArgs.registry = &m_Registry;
		geometryArgs.allocManager = &m_AllocManager;
		geometryArgs.resources = &m_Resources;
		geometryArgs.scene = nullptr;
		geometryArgs.pipeline = m_Resources.geometryGraphicsPipeline;
		geometryArgs.descriptorSet = m_Resources.descriptorSet;
		m_GeometryPass = new GeometryPass(geometryArgs);

		GUIPassArgs guiArgs;
		guiArgs.registry = &m_Registry;
		guiArgs.pipeline = m_Resources.guiPipeline;
		guiArgs.descriptorSet = m_Resources.descriptorSet;
		guiArgs.vertexBuffer = m_Resources.guiVertexBuffer;
		guiArgs.indexBuffer = m_Resources.guiIndexBuffer;
		guiArgs.renderFrame = nullptr;
		m_GUIPass = new GUIPass(guiArgs);
	}

	void Renderer::DeletePasses()
	{
		delete m_TransferPass;
		m_TransferPass = nullptr;
		delete m_GeometryPass;
		m_GeometryPass = nullptr;
		delete m_GUIPass;
		m_GUIPass = nullptr;
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