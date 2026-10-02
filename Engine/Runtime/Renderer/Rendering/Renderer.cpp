#include "Renderer.h"
#include <chrono>
#include "RenderDebug.h"
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
#include "Memory/StackAllocation.h"
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
	namespace
	{
		// Fixed, worst-case-sized byte regions within the ray-tracing culling staging buffer,
		// one contiguous block per buffered render frame slot, indexed directly by renderFrameIndex.
		constexpr size_t c_ActiveInstanceIndicesBytes = sizeof(uint) * RenderConstants::c_MaxMeshInstances;
		constexpr size_t c_DrawCountResetBytes = sizeof(uint);
		constexpr size_t c_TlasInstancesBytes = RenderConstants::c_TLASInstanceBufferSize;
		constexpr size_t c_RTCullingStagingSlotSize = c_ActiveInstanceIndicesBytes + c_DrawCountResetBytes + c_TlasInstancesBytes;
	}

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
		CreateAccelerationStructures();
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
		// deletes a resource the GPU might still be using.
		m_Ctx.device->WaitIdle();

		// The normal signal-and-reclaim path stops running once the main loop does, so an asset
		// upload submitted in one of the last few frames before shutdown can reach this point
		// still marked unsignalled, even though WaitIdle above confirms it was already consumed.
		m_AllocManager.ReclaimAllResourceUploadMemory();
	}

	Renderer::~Renderer()
	{
		// Safety net - the real call is expected to have already happened explicitly, early in
		// engine shutdown. Idempotent, so this is a no-op then.
		Shutdown();

		DeletePasses();
		DeleteSamplers();
		DeleteAccelerationStructures();
		DeleteBuffers();
		DeletePipelines();
		DeleteCommandObjects();
		DeleteShaders();
		// Safe now, unlike inside Shutdown() - every module has finished removing its own
		// windows/scenes/meshes by this point, so whatever's still queued in any RenderFrame
		// slot's delete lists can finally be flushed.
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

		// One-time per-mesh BLAS build requests, drained into a persistent queue here (main
		// thread, same as where they were added) rather than in RenderAsync, since this doesn't
		// need scene.content.meshInstances to have been merged first.
		for (MeshHandle handle : renderFrame.meshesToBuildBLAS)
		{
			m_PendingBLASBuilds.Add(handle);
		}

		// Decide and create this frame's BLAS batch here, on the main thread - acceleration
		// structure/buffer creation must never happen on a worker thread. The worker thread only
		// issues the actual build commands from already-created data.
		{
			size_t accumulatedBytes = 0;
			bool builtAny = false;
			uint processedCount = 0;
			size_t maxScratchSize = 0;

			for (uint i = 0; i < m_PendingBLASBuilds.Size(); ++i)
			{
				Mesh& mesh = m_Registry.GetMesh(m_PendingBLASBuilds[i]);
				if (mesh.blas)
				{
					// Already built (requested more than once)
					processedCount++;
					continue;
				}

				const MeshLODAllocInfo& lodAlloc = m_AllocManager.GetMeshLODAllocInfo(mesh.lodOffset);
				const size_t meshBytes = lodAlloc.vertexBufferAllocation.size + lodAlloc.indexBufferAllocation.size;

				// Always build at least one real mesh, even if it alone exceeds the budget -
				// otherwise an oversized mesh at the front of this FIFO (never-reordered, for
				// streaming priority) queue would stall every mesh behind it forever.
				if (builtAny && accumulatedBytes + meshBytes > RenderConstants::c_MaxBLASBuildBytesPerFrame)
				{
					break;
				}

				AccelerationStructureGeometryDesc geomDesc;
				geomDesc.vertexBuffer = m_Registry.GetBuffer(m_Resources.vertexBuffer).buffer;
				geomDesc.vertexOffset = lodAlloc.vertexBufferAllocation.offset;
				geomDesc.vertexStride = sizeof(ShaderVertex);
				geomDesc.maxVertexCount = (uint)(lodAlloc.vertexBufferAllocation.size / sizeof(ShaderVertex));
				geomDesc.indexBuffer = m_Registry.GetBuffer(m_Resources.indexBuffer).buffer;
				geomDesc.indexOffset = lodAlloc.indexBufferAllocation.offset;
				geomDesc.maxPrimitiveCount = (uint)(lodAlloc.indexBufferAllocation.size / sizeof(uint) / 3);

				AccelerationStructureDesc blasDesc;
				blasDesc.debugName = "Mesh BLAS";
				blasDesc.type = AccelerationStructureType::BottomLevel;
				blasDesc.geometries.Add(geomDesc);

				size_t asSize = 0;
				size_t buildScratchSize = 0;
				m_Ctx.device->GetAccelerationStructureSize(blasDesc, asSize, buildScratchSize);

				if (!m_AllocManager.RequestBLASStorageAllocation(asSize, mesh.blasStorageAllocation))
				{
					// Out of shared BLAS storage - leave this mesh pending (it stays at the front
					// of the queue) rather than asserting; it simply doesn't cast/receive
					// ray-traced shadows until room frees up.
					break;
				}

				mesh.blas = m_Ctx.device->CreateAccelerationStructureAt(blasDesc, m_Registry.GetBuffer(m_Resources.blasStorageBuffer).buffer, mesh.blasStorageAllocation.offset);
				maxScratchSize = maxScratchSize > buildScratchSize ? maxScratchSize : buildScratchSize;

				BLASBuildRecord& record = renderFrame.blasBuildsToRecord.ExpandOne();
				record.blas = mesh.blas;
				// scratchOffset is filled in below, once this frame's scratch capacity (and so
				// the per-slot byte offset) is finalized - growing blasScratchBuffer mid-batch
				// would otherwise shift every record's offset that was already computed above it.

				accumulatedBytes += meshBytes;
				builtAny = true;
				processedCount++;
			}

			m_PendingBLASBuilds.EraseFromFront(processedCount);

			if (maxScratchSize > 0)
			{
				// One call for the whole batch, after every BLAS above is created, since growing
				// mid-batch would shift already-computed offsets.
				EnsureBLASScratchCapacity(maxScratchSize);

				const size_t scratchOffset = (size_t)m_RenderFrameIndex * m_BLASScratchCapacity;
				for (BLASBuildRecord& record : renderFrame.blasBuildsToRecord)
				{
					record.scratchOffset = scratchOffset;
				}
			}
		}

		// Scene-dependent setup only. Not an early return out of Render() - this frame's
		// already-queued asset/frame buffer uploads still need the transfer pass to run
		// regardless of whether a scene happens to be active this tick.
		if (renderFrame.activeScene)
		{
			SceneFrame& sceneFrame = renderFrame.sceneFrame;

			const Scene& scene = m_Data.scenePool[renderFrame.activeScene.h];

			// Read via the main-thread-only mirror, not Scene's own fields directly, to avoid
			// racing RenderAsync's (worker thread) writes to the real Scene.
			const ImmediateSceneData& immediateSceneData = GetImmediateSceneData(renderFrame.activeScene);

			if (!m_FirstRender)
			{
				const RenderFrame& prevRenderFrame = GetPrevRenderFrame();
				// The previous slot might have had no active scene at all - treat that the
				// same as a changed scene rather than indexing scenes with an invalid handle.
				if (!prevRenderFrame.activeScene || m_Data.scenePool[prevRenderFrame.activeScene.h].id != scene.id)
				{
					m_ViewIdIndexMap.Clear();
				}
			}

			// A scene can be active without being the one on screen right now - e.g. a
			// level-editing scene once a play scene takes over. Nothing below is needed unless
			// this scene actually renders/uploads this tick.
			if (immediateSceneData.visible)
			{
				ShaderSceneInfo sceneInfo{};
				sceneInfo.ambient = immediateSceneData.ambient;
				sceneInfo.dirLightCount = immediateSceneData.dirLightCount;
				sceneInfo.pointLightCount = immediateSceneData.pointLightCount;
				sceneInfo.spotLightCount = immediateSceneData.spotLightCount;

				const RenderWindowHandle windowHandle = immediateSceneData.windowHandle;
				if (!windowHandle)
				{
					// Nothing has reached this scene's ImmediateSceneData yet (e.g. the very first
					// tick a scene is ever made active, before WorldManager::SetActiveWorld's
					// SetSceneWindow call this same tick has run) - there's nothing to render to yet.
					// A narrower, separate case from "no active scene at all" (see this block's own
					// guard) - left as a full early return here rather than folded into that, since
					// RenderAsync's scene-content merge still needs scene.content to be handled
					// consistently either way and hasn't been audited for running safely without a
					// dispatch on a tick like this.
					return;
				}

				const RenderWindow& renderWindow = m_WindowPool[windowHandle.h];
				const uint swapChainWidth = renderWindow.swapChain->GetWidth();
				const uint swapChainHeight = renderWindow.swapChain->GetHeight();

				// Resolve this scene's own RenderViewport (see Scene::renderViewport/RenderViewport's
				// own comment) and bring its current buffered slot up to date before anything below
				// reads its width/height or texture handles.
				const RenderViewportHandle viewportHandle = immediateSceneData.renderViewport;
				// Before anything has ever requested a render target size yet (e.g. the very first
				// few frames, or no viewport handle has reached this scene yet), fall back to the
				// swap chain's own size instead of leaving these at zero.
				uint viewportWidth = swapChainWidth;
				uint viewportHeight = swapChainHeight;
				if (viewportHandle)
				{
					RenderViewport& viewport = m_RenderViewportPool[viewportHandle.h];
					RenderViewportTextureData& targets = viewport.textureData[m_RenderFrameIndex];

					// Apply this slot's own pending resize, if any - see RenderViewport's own comment
					// on resize propagation. A no-op (just clears the flag) if this slot already
					// matches the requested size, whether because a direct request already resized it
					// this very tick or because it just happened to already be that size.
					if (viewport.pendingResize[m_RenderFrameIndex])
					{
						if (targets.width != viewport.requestedWidth || targets.height != viewport.requestedHeight)
						{
							ResizeRenderViewportSlot(viewport, m_RenderFrameIndex, "Viewport", viewport.requestedWidth, viewport.requestedHeight);
						}
						viewport.pendingResize[m_RenderFrameIndex] = false;
					}

					// Keep this slot's lighting-output descriptor pointed at whatever colour texture
					// is actually here now - see EnsureLightingOutputBound's own comment.
					EnsureLightingOutputBound(m_RenderFrameIndex, targets.colourTexture);

					if (targets.width != 0 && targets.height != 0)
					{
						viewportWidth = targets.width;
						viewportHeight = targets.height;
					}
				}

				// TODO: Extend to multiple views per scene - only the first view's data reaches the
				// shader for now (see ShaderSceneInfo's own comment).
				if (!sceneFrame.views.IsEmpty())
				{
					const SceneView& sv = sceneFrame.views[0];
					const float aspect = GraphicsUtility::CalculateAspectRatio(sv.viewArea, viewportWidth, viewportHeight);

					const Matrix4 view = Matrix4::CreateView(sv.camera.position, sv.camera.forward, sv.camera.up);
					// Do reverse-z for greater floating-point precision
					const Matrix4 projection = Matrix4::CreatePerspective(sv.camera.fov, aspect, sv.camera.farZ, sv.camera.nearZ);

					sceneInfo.viewProj = view * projection;
					// Deferred lighting pass reconstructs world position from depth with this - see
					// DeferredLightingCS.hlsl.
					sceneInfo.invViewProj = sceneInfo.viewProj.Inverse();
					sceneInfo.camPos = sv.camera.position;

					// Gribb-Hartmann frustum plane extraction, adapted for this engine's row-vector
					// convention (v * M, not M * v) - planes come from viewProj's COLUMNS, not rows.
					// Each resulting Vector4(a,b,c,d) satisfies dot(worldPos,abc)+d >= 0 for "inside",
					// which holds regardless of the reverse-Z projection used here (Vulkan's clip-space
					// constraint 0<=z<=w that this derivation relies on is convention-invariant). Used
					// by CullInstancesCS.hlsl.
					{
						const Vector4 c0 = sceneInfo.viewProj.GetColumn4D(0);
						const Vector4 c1 = sceneInfo.viewProj.GetColumn4D(1);
						const Vector4 c2 = sceneInfo.viewProj.GetColumn4D(2);
						const Vector4 c3 = sceneInfo.viewProj.GetColumn4D(3);

						sceneInfo.frustumPlanes[0] = c3 + c0; // Left
						sceneInfo.frustumPlanes[1] = c3 - c0; // Right
						sceneInfo.frustumPlanes[2] = c3 + c1; // Bottom
						sceneInfo.frustumPlanes[3] = c3 - c1; // Top
						sceneInfo.frustumPlanes[4] = c2;      // Near
						sceneInfo.frustumPlanes[5] = c3 - c2; // Far

						for (Vector4& plane : sceneInfo.frustumPlanes)
						{
							const float invLength = 1.0f / Vector3(plane.x, plane.y, plane.z).Length();
							plane = plane * invLength;
						}
					}

					// GBufferPS.hlsl's motion vectors - see ShaderSceneInfo::prevViewProj's own comment.
					// On the very first frame there is no real previous frame, so use this frame's own
					// viewProj (motion vectors come out exactly zero rather than reading identity/garbage).
					sceneInfo.prevViewProj = m_FirstRender ? sceneInfo.viewProj : m_PrevViewProj;
					m_PrevViewProj = sceneInfo.viewProj;
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
			}
		}

		if (m_FirstRender)
		{
			constexpr uint bindingCount = 16;
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
			SetBinding(12, TYR_BINDING_ACTIVE_INSTANCE_INDICES, m_Resources.activeMeshInstanceIndexBuffer);
			SetBinding(13, TYR_BINDING_VISIBLE_INSTANCE_INDICES, m_Resources.visibleInstanceIndexBuffer);
			SetBinding(14, TYR_BINDING_INDIRECT_DRAW_COMMANDS, m_Resources.indirectDrawCommandBuffer);
			SetBinding(15, TYR_BINDING_DRAW_COUNT, m_Resources.drawCountBuffer);

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

		// Everything RendererAPI queued this tick (CreateTexture's bindless write,
		// EnsureLightingOutputBound's per-slot write, etc. - see QueueImageBindingUpdate and
		// PendingDescriptorUpdates' own comments) goes to the device in one batched call here,
		// before RenderAsync is dispatched below - CreateTask's task can start executing on a
		// worker thread the moment it's created, so every queued write needs to have already
		// landed before that point, not after.
		FlushDescriptorUpdates();

		// Safe to dispatch RenderAsync straight from here: every module that writes into this
		// tick's RenderFrame slot has already run its Update() by the time RendererModule::Update()
		// (this function) runs - RendererModule is registered so that it executes late in each
		// phase (see EngineLoop.cpp), specifically so everyone else can feed it first. That
		// includes GUIModule, whose Update() (not EndFrame() - see its own comment on why) submits
		// ImGui's/Nuklear's draw data into this exact slot via RendererAPI::SubmitGUIDrawData,
		// appending to frameBufferUploadRequests. Dispatching here used to run into a real crash
		// (a BufferUploadRequest read back completely default-constructed, from a worker thread
		// already inside GpuTransferUtil::UploadToBuffers while the main thread's ExpandOne()
		// reallocated the same array out from under it) back when GUIModule submitted from
		// EndFrame() instead, which ran *after* this dispatch - see GUIModule::Update's own
		// comment before moving GUI submission again.
		//
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

		// Snapshots this tick's own frame-upload head, tagged with frameNumber, now that every
		// RequestFrameUploadAllocation call this tick is going to make has already happened
		// (SceneInfo above, GUI vertex/index uploads from other modules' earlier Update() calls -
		// see this function's own comment on module ordering) and before the next tick gets a
		// chance to allocate anything more. See FrameUploadAllocator::RecordAllocationCheckpoint/
		// Signal's own comments for why this precise pairing matters.
		m_AllocManager.RecordFrameUploadCheckpoint(frameNumber);

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

	void Renderer::QueueBufferBindingUpdate(uint bindingIndex, uint descriptorArrayIndex, const BufferBindingInfo& info)
	{
		m_PendingDescriptorUpdates.bufferInfos.Add(info);
		BufferBindingUpdate& update = m_PendingDescriptorUpdates.bufferUpdates.ExpandOne();
		update.bindingIndex = bindingIndex;
		update.descriptorArrayIndex = descriptorArrayIndex;
		// bufferBindingInfos/infoCount are left at their defaults here and only set in
		// FlushDescriptorUpdates, right before this array is actually read - see
		// PendingDescriptorUpdates' own comment on why.
	}

	void Renderer::QueueImageBindingUpdate(uint bindingIndex, uint descriptorArrayIndex, const ImageBindingInfo& info)
	{
		m_PendingDescriptorUpdates.imageInfos.Add(info);
		ImageBindingUpdate& update = m_PendingDescriptorUpdates.imageUpdates.ExpandOne();
		update.bindingIndex = bindingIndex;
		update.descriptorArrayIndex = descriptorArrayIndex;
	}

	void Renderer::QueueAccelerationStructureBindingUpdate(uint bindingIndex, uint descriptorArrayIndex, const AccelerationStructureBindingInfo& info)
	{
		m_PendingDescriptorUpdates.accelerationStructureInfos.Add(info);
		AccelerationStructureBindingUpdate& update = m_PendingDescriptorUpdates.accelerationStructureUpdates.ExpandOne();
		update.bindingIndex = bindingIndex;
		update.descriptorArrayIndex = descriptorArrayIndex;
	}

	void Renderer::FlushDescriptorUpdates()
	{
		PendingDescriptorUpdates& pending = m_PendingDescriptorUpdates;
		if (pending.bufferUpdates.IsEmpty() && pending.imageUpdates.IsEmpty() && pending.accelerationStructureUpdates.IsEmpty())
		{
			return;
		}

		// Point each update at its matching info entry now that that info array is done growing
		// for this tick - Add()'s own reallocate-on-grow would have left an earlier-computed
		// pointer dangling had this been done inside Queue*BindingUpdate instead, since a later
		// call in the same tick can still trigger another reallocation.
		for (uint i = 0; i < pending.bufferUpdates.Size(); ++i)
		{
			pending.bufferUpdates[i].bufferBindingInfos = &pending.bufferInfos[i];
			pending.bufferUpdates[i].infoCount = 1;
		}
		for (uint i = 0; i < pending.imageUpdates.Size(); ++i)
		{
			pending.imageUpdates[i].imageBindingInfos = &pending.imageInfos[i];
			pending.imageUpdates[i].infoCount = 1;
		}
		for (uint i = 0; i < pending.accelerationStructureUpdates.Size(); ++i)
		{
			pending.accelerationStructureUpdates[i].accelerationStructureBindingInfos = &pending.accelerationStructureInfos[i];
			pending.accelerationStructureUpdates[i].infoCount = 1;
		}

		m_Ctx.device->UpdateDescriptorSet(m_Resources.descriptorSet,
			pending.bufferUpdates.Data(), pending.bufferUpdates.Size(),
			pending.imageUpdates.Data(), pending.imageUpdates.Size(),
			pending.accelerationStructureUpdates.Data(), pending.accelerationStructureUpdates.Size());

		pending.bufferInfos.Clear();
		pending.bufferUpdates.Clear();
		pending.imageInfos.Clear();
		pending.imageUpdates.Clear();
		pending.accelerationStructureInfos.Clear();
		pending.accelerationStructureUpdates.Clear();
	}

	void Renderer::RenderAsync(uint renderFrameIndex, uint64 frameNumber)
	{
		RenderRegistry& registry = *RenderRegistry::Instance();

		// Not const - the active-instance-index upload below is scheduled here rather than on
		// the submission thread (see its own comment), since scene.content.meshInstances isn't
		// finalized until the merge loop just below has run. Safe to mutate: PrepareForNextFrame
		// already waits for this slot's previous RenderAsync task before it's reused, so nothing
		// else touches this renderFrameIndex's frameBufferUploadRequests concurrently.
		RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];
		FrameContext& frameCtx = m_Ctx.frameContexts[renderFrameIndex];

		m_Data.activeScene = renderFrame.activeScene;

		if (!m_Data.activeScene)
		{
			// Nothing to merge/cull/draw, and no window to acquire/present to (only ever known
			// via the active scene's windowHandle - see below) - but this slot's already-queued
			// asset/frame buffer uploads (e.g. from AssetModule::Update, which runs before
			// RendererModule::Update this same tick regardless of whether a scene is active -
			// see Render()'s own comment) still need to reach the GPU, so still run the
			// transfer-only path rather than stranding them unsubmitted and never signalled -
			// see BuildAndExecuteRenderGraph's hasActiveScene parameter.
			//
			// No merge needed for any of that: TransferPass::Execute reads RenderFrame's upload
			// request lists directly (see its own comment), and BuildAndExecuteRenderGraph always
			// points TransferPassArgs.renderFrame at this slot regardless of hasActiveScene.
			m_Data.BeginFrame();

			BuildAndExecuteRenderGraph(renderFrameIndex, frameNumber, false, false);
			return;
		}

		m_Data.BeginFrame();

		const SceneFrame& sceneFrame = renderFrame.sceneFrame;
		Scene& scene = m_Data.scenePool[m_Data.activeScene.h];

		if (sceneFrame.newWindow)
		{
			scene.windowHandle = sceneFrame.newWindow;
		}

		if (sceneFrame.newRenderViewport)
		{
			scene.renderViewport = sceneFrame.newRenderViewport;
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

#if TYR_RENDER_DEBUG
		// TEMP DEBUG - flicker investigation. Revert after.
		{
			const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			TYR_LOG_WARNING("[DBG] RenderAsync: ms=%lld frameNumber=%llu renderFrameIndex=%u valid=%d imageIndex=%u",
				(long long)nowMs, (unsigned long long)frameNumber, renderFrameIndex, (int)hasValidSwapChainImage,
				hasValidSwapChainImage ? acquireResult->imageIndex : ~0u);
		}
#endif

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

		// GPU-driven instance culling (RecordCullingPass) needs to know exactly which pool
		// slots belong to this frame's active scene - the mesh instance pool is shared across
		// every scene (up to RenderConstants::c_MaxScenes in the editor), but only one scene
		// ever renders at a time, so a naive "cull the whole pool" dispatch would draw other
		// scenes' instances too. Upload the just-finalized active list as plain pool indices.
		// Scheduled here (not on the submission thread, unlike every other upload request) since
		// scene.content.meshInstances only becomes final once the merge above has run. The 3
		// upload requests this generates go into m_Data.workerUploadRequests, not
		// renderFrame.frameBufferUploadRequests - RenderFrame may only ever be written from the
		// main thread (this runs on a RenderAsync worker thread), and RenderData is already
		// worker-owned (see its own comment). Written straight into this renderFrameIndex's own
		// fixed slot of rtCullingStagingBuffer, not allocated from RenderAllocationManager - that
		// allocator is main-thread-only (see FrameUploadAllocator's own comment); a worker thread
		// may only write into already-allocated memory, never allocate or free it.
		const size_t stagingSlotBase = renderFrameIndex * c_RTCullingStagingSlotSize;
		const size_t activeIndicesOffset = stagingSlotBase;
		const size_t drawCountOffset = stagingSlotBase + c_ActiveInstanceIndicesBytes;
		const size_t tlasInstancesOffset = drawCountOffset + c_DrawCountResetBytes;
		RenderBuffer& stagingBuffer = m_Registry.GetBuffer(m_Resources.rtCullingStagingBuffer);

		{
			const uint activeInstanceCount = scene.content.meshInstances.Size();
			if (activeInstanceCount > 0)
			{
				const size_t uploadSize = sizeof(uint) * activeInstanceCount;

				const SmartStack<uint> indexStack = SmartStackAlloc<uint>(activeInstanceCount);
				uint* const indices = indexStack;
				for (uint i = 0; i < activeInstanceCount; ++i)
				{
					indices[i] = scene.content.meshInstances[i].h.index;
				}

				RenderResourceUtil::WriteUploadBuffer(stagingBuffer, *m_Ctx.device, activeIndicesOffset, indices, uploadSize);

				BufferUploadRequest& request = m_Data.workerUploadRequests.ExpandOne();
				request.srcBuffer = m_Resources.rtCullingStagingBuffer;
				request.srcOffset = activeIndicesOffset;
				request.dstBuffer = m_Resources.activeMeshInstanceIndexBuffer;
				request.dstOffset = 0;
				request.size = uploadSize;
			}

			// The culling compute shader atomically increments this from 0 - without resetting
			// it every frame, a frame with fewer visible instances than the last would still
			// read the previous frame's higher count via the indirect draw, redrawing stale
			// entries the shader never touched this frame.
			uint zero = 0;
			RenderResourceUtil::WriteUploadBuffer(stagingBuffer, *m_Ctx.device, drawCountOffset, &zero, sizeof(uint));

			BufferUploadRequest& countRequest = m_Data.workerUploadRequests.ExpandOne();
			countRequest.srcBuffer = m_Resources.rtCullingStagingBuffer;
			countRequest.srcOffset = drawCountOffset;
			countRequest.dstBuffer = m_Resources.drawCountBuffer;
			countRequest.dstOffset = 0;
			countRequest.size = sizeof(uint);
		}

		// Ray-traced shadows (Phase 3) - rebuild the TLAS's instance buffer from this same
		// just-finalized active list. An instance whose mesh hasn't finished its one-time BLAS
		// build yet (see RequestBLASBuild) is skipped - it simply doesn't cast/receive shadows
		// for its first few frames.
		{
			const uint activeInstanceCount = scene.content.meshInstances.Size();
			uint tlasInstanceCount = 0;

			if (activeInstanceCount > 0)
			{
				const SmartStack<AccelerationStructureInstance> instanceStack = SmartStackAlloc<AccelerationStructureInstance>(activeInstanceCount);
				AccelerationStructureInstance* const instances = instanceStack;

				for (MeshInstanceHandle handle : scene.content.meshInstances)
				{
					const MeshInstance& instance = registry.GetMeshInstance(handle);
					const Mesh& mesh = registry.GetMesh(instance.info.mesh);
					if (!mesh.blas)
					{
						continue;
					}

					AccelerationStructureInstance& out = instances[tlasInstanceCount++];
					// VkAccelerationStructureInstanceKHR's transform is row-major 3x4 for a
					// COLUMN-vector transform (transformed = M * pos) - the transpose of this
					// engine's row-vector Matrix4 (pos * M). GetColumn4D(r) of the engine matrix
					// gives exactly VK row r's 4 components (column c is defined as
					// (m[0][c],m[1][c],m[2][c],m[3][c]), so fixing the column index at r and
					// reading its 4 components sweeps m[c][r] for c=0..3 - exactly VK row r,
					// including the translation landing in the last component as expected).
					const Matrix4& transform = instance.info.transform;
					for (uint r = 0; r < 3; ++r)
					{
						const Vector4 row = transform.GetColumn4D(r);
						out.transform[r][0] = row.x;
						out.transform[r][1] = row.y;
						out.transform[r][2] = row.z;
						out.transform[r][3] = row.w;
					}
					out.instanceCustomIndex = 0;
					out.mask = 0xFF;
					out.shaderBindingTableRecordOffset = 0;
					out.flags = 0;
					out.accelerationStructureReference = m_Ctx.device->GetAccelerationStructureDeviceAddress(mesh.blas);
				}

				if (tlasInstanceCount > 0)
				{
					const size_t uploadSize = sizeof(AccelerationStructureInstance) * tlasInstanceCount;
					RenderResourceUtil::WriteUploadBuffer(stagingBuffer, *m_Ctx.device, tlasInstancesOffset, instances, uploadSize);

					BufferUploadRequest& request = m_Data.workerUploadRequests.ExpandOne();
					request.srcBuffer = m_Resources.rtCullingStagingBuffer;
					request.srcOffset = tlasInstancesOffset;
					request.dstBuffer = m_Resources.tlasInstanceBuffer;
					// One physical buffer, c_BufferedFrameCount slots big - confine this frame's
					// upload to its own slot (see RenderResources::tlasInstanceBuffer's own
					// comment), same pattern as the GUI vertex/index buffers.
					request.dstOffset = (size_t)renderFrameIndex * RenderConstants::c_TLASInstanceBufferSize;
					request.size = uploadSize;
				}
			}

			m_TLASInstanceCount = tlasInstanceCount;
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

		// No merge needed for the upload request lists - TransferPass::Execute reads them straight
		// off RenderFrame (see its own comment).
		BuildAndExecuteRenderGraph(renderFrameIndex, frameNumber, true, hasValidSwapChainImage);
	}

	void Renderer::RecordTransferPass(CommandList& cmdList, uint renderFrameIndex)
	{
		m_TransferPass->Execute(cmdList, m_RenderFrames[renderFrameIndex], m_Data);
	}

	void Renderer::SetupCullingPass(RenderGraphBuilder& builder)
	{
		const PipelineStage cullingStage = PIPELINE_STAGE_COMPUTE_SHADER_BIT;

		builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.sceneInfoBuffer), cullingStage, BARRIER_ACCESS_UNIFORM_READ_BIT);
		builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.meshBuffer), cullingStage, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.meshLODBuffer), cullingStage, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.meshInstanceBuffer), cullingStage, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.activeMeshInstanceIndexBuffer), cullingStage, BARRIER_ACCESS_SHADER_READ_BIT);

		builder.WriteBuffer(m_Registry.GetBuffer(m_Resources.visibleInstanceIndexBuffer), cullingStage, BARRIER_ACCESS_SHADER_WRITE_BIT);
		builder.WriteBuffer(m_Registry.GetBuffer(m_Resources.indirectDrawCommandBuffer), cullingStage, BARRIER_ACCESS_SHADER_WRITE_BIT);
		builder.WriteBuffer(m_Registry.GetBuffer(m_Resources.drawCountBuffer), cullingStage, BARRIER_ACCESS_SHADER_WRITE_BIT);
	}

	void Renderer::RecordCullingPass(CommandList& cmdList, uint renderFrameIndex)
	{
		Scene& scene = m_Data.scenePool[m_Data.activeScene.h];
		const uint activeInstanceCount = scene.content.meshInstances.Size();

		cmdList.BindComputePipeline(m_Resources.cullingPipeline);
		cmdList.BindDescriptorSet(m_Resources.descriptorSet, m_Resources.cullingPipeline);
		cmdList.PushConstants(m_Resources.cullingPipeline, SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint), &activeInstanceCount);

		// Matches CullInstancesCS.hlsl's [numthreads(64, 1, 1)] - a zero count still dispatches
		// zero groups correctly rather than needing a special case here.
		const uint groupCount = (activeInstanceCount + 63) / 64;
		cmdList.Dispatch(groupCount, 1, 1);
	}

	RenderViewportTextureData* Renderer::GetActiveViewportTextureData(uint renderFrameIndex)
	{
		const Scene& scene = m_Data.scenePool[m_Data.activeScene.h];
		if (!scene.renderViewport)
		{
			return nullptr;
		}
		return &m_RenderViewportPool[scene.renderViewport.h].textureData[renderFrameIndex];
	}

	void Renderer::RecordGeometryPass(CommandList& cmdList, uint renderFrameIndex)
	{
		// Read the active scene's own RenderViewport directly, in place, rather than a per-frame
		// copy - see RenderViewport's own comment on why this is safe from a worker thread (nothing
		// else touches this slot's data between the main thread's own tick and this call).
		RenderViewportTextureData* viewportData = GetActiveViewportTextureData(renderFrameIndex);
		if (!viewportData)
		{
#if TYR_RENDER_DEBUG
			// TEMP DEBUG - flicker investigation. Revert after.
			TYR_LOG_WARNING("[DBG] RecordGeometryPass: SKIPPED (no viewport) renderFrameIndex=%u", renderFrameIndex);
#endif
			// Scene has no RenderViewport yet (e.g. World hasn't finished setting one up) -
			// nothing to render into.
			return;
		}

		const TextureHandle gbufferAlbedoAOHandle = viewportData->gbufferAlbedoAO;
		const TextureHandle gbufferNormalRoughMetalHandle = viewportData->gbufferNormalRoughMetal;
		const TextureHandle gbufferMotionHandle = viewportData->gbufferMotion;
		const TextureHandle depthBufferHandle = viewportData->depthBuffer;
		const uint viewportWidth = viewportData->width;
		const uint viewportHeight = viewportData->height;
		const bool viewportTextureIsNew = viewportData->isNew;
		// Consumed once, right here - unlike the old ViewportSnapshot-era code, this resets the
		// live struct itself (this worker thread owns this slot exclusively until it cycles back
		// around - see RenderViewportTextureData::isNew's own comment).
		viewportData->isNew = false;

		// EditorViewport hasn't requested a render target size yet (e.g. the very first few
		// frames, before any ImGui layout has happened) - nothing to render into.
		if (viewportWidth == 0 || viewportHeight == 0)
		{
#if TYR_RENDER_DEBUG
			// TEMP DEBUG - flicker investigation. Revert after.
			TYR_LOG_WARNING("[DBG] RecordGeometryPass: SKIPPED (zero size) renderFrameIndex=%u width=%u height=%u isNew=%d",
				renderFrameIndex, viewportWidth, viewportHeight, (int)viewportTextureIsNew);
#endif
			return;
		}

		const Texture& gbufferAlbedoAO = m_Registry.GetTexture(gbufferAlbedoAOHandle);
		const Texture& gbufferNormalRoughMetal = m_Registry.GetTexture(gbufferNormalRoughMetalHandle);
		const Texture& gbufferMotion = m_Registry.GetTexture(gbufferMotionHandle);
		const Texture& depthBuffer = m_Registry.GetTexture(depthBufferHandle);

		Viewport viewport;
		viewport.width = viewportWidth;
		viewport.height = viewportHeight;

		// Freshly (re)created G-buffer/depth images are undefined until this transition - unlike
		// the swap chain image (see RecordGUIPass), these aren't cycled every frame, so it must
		// only run once per (re)creation, not every frame.
		if (viewportTextureIsNew)
		{
			ImageBarrier barriers[4]{};

			barriers[0].image = gbufferAlbedoAO.image;
			barriers[0].srcAccess = BARRIER_ACCESS_NONE;
			barriers[0].dstAccess = BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barriers[0].srcLayout = IMAGE_LAYOUT_UNKNOWN;
			barriers[0].dstLayout = gbufferAlbedoAO.imageLayout;
			barriers[0].subresourceRange.aspect = SUBRESOURCE_ASPECT_COLOUR_BIT;
			barriers[0].subresourceRange.mipCount = 1;
			barriers[0].subresourceRange.arrayLayerCount = 1;
			barriers[0].srcStage = PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barriers[0].dstStage = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			barriers[1] = barriers[0];
			barriers[1].image = gbufferNormalRoughMetal.image;
			barriers[1].dstLayout = gbufferNormalRoughMetal.imageLayout;

			barriers[2] = barriers[0];
			barriers[2].image = gbufferMotion.image;
			barriers[2].dstLayout = gbufferMotion.imageLayout;

			barriers[3].image = depthBuffer.image;
			barriers[3].srcAccess = BARRIER_ACCESS_NONE;
			barriers[3].dstAccess = BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barriers[3].srcLayout = IMAGE_LAYOUT_UNKNOWN;
			barriers[3].dstLayout = depthBuffer.imageLayout;
			barriers[3].subresourceRange.aspect = SUBRESOURCE_ASPECT_DEPTH_BIT;
			barriers[3].subresourceRange.mipCount = 1;
			barriers[3].subresourceRange.arrayLayerCount = 1;
			barriers[3].srcStage = PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barriers[3].dstStage = PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

			cmdList.AddBarriers(nullptr, 0, barriers, 4);
		}

		// TODO: Render for all views and create render area for each view by calling GraphicsUtility::CreateRenderArea
		RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extents = { viewportWidth, viewportHeight };
		renderingInfo.viewMask = 0;
		renderingInfo.layerCount = 1;

		RenderingAttachmentInfo colourAttachments[3];
		colourAttachments[0].loadOp = AttachmentLoadOp::Clear;
		colourAttachments[0].storeOp = AttachmentStoreOp::Store;
		colourAttachments[0].resolveMode = RESOLVE_MODE_NONE;
		colourAttachments[0].imageLayout = gbufferAlbedoAO.imageLayout;
		colourAttachments[0].clearValue.colour = { 0.0f, 0.0f, 0.0f, 0.0f };
		colourAttachments[0].imageView = gbufferAlbedoAO.imageView;

		colourAttachments[1].loadOp = AttachmentLoadOp::Clear;
		colourAttachments[1].storeOp = AttachmentStoreOp::Store;
		colourAttachments[1].resolveMode = RESOLVE_MODE_NONE;
		colourAttachments[1].imageLayout = gbufferNormalRoughMetal.imageLayout;
		colourAttachments[1].clearValue.colour = { 0.0f, 0.0f, 0.0f, 0.0f };
		colourAttachments[1].imageView = gbufferNormalRoughMetal.imageView;

		colourAttachments[2].loadOp = AttachmentLoadOp::Clear;
		colourAttachments[2].storeOp = AttachmentStoreOp::Store;
		colourAttachments[2].resolveMode = RESOLVE_MODE_NONE;
		colourAttachments[2].imageLayout = gbufferMotion.imageLayout;
		colourAttachments[2].clearValue.colour = { 0.0f, 0.0f, 0.0f, 0.0f };
		colourAttachments[2].imageView = gbufferMotion.imageView;

		renderingInfo.colourAttachmentCount = 3;
		renderingInfo.colourAttachments = colourAttachments;

		renderingInfo.hasDepthAttachment = true;
		renderingInfo.depthAttachment.loadOp = AttachmentLoadOp::Clear;
		renderingInfo.depthAttachment.storeOp = AttachmentStoreOp::Store;
		renderingInfo.depthAttachment.resolveMode = RESOLVE_MODE_NONE;
		renderingInfo.depthAttachment.imageLayout = depthBuffer.imageLayout;
		// Reverse-Z - 0 represents "infinitely far" (see CreatePipelines' depthCompareOp).
		renderingInfo.depthAttachment.clearValue.depthStencil.depth = 0.0f;
		renderingInfo.depthAttachment.imageView = depthBuffer.imageView;

		cmdList.BeginRendering(renderingInfo);
		cmdList.SetViewport(&viewport, 1);
		cmdList.SetScissor(&renderingInfo.renderArea, 1);
		m_GeometryPass->Execute(cmdList);
		cmdList.EndRendering();
	}

	namespace
	{
		// Matches DeferredLightingCS.hlsl's push constant cbuffer byte-for-byte.
		struct LightingPushConstants
		{
			uint gbufferAlbedoAOIndex;
			uint gbufferNormalRoughMetalIndex;
			uint depthIndex;
			uint width;
			uint height;
			// Which entry of TYR_BINDING_LIGHTING_OUTPUT's outputImages[] array to write this
			// dispatch's result into - see RenderResources::ViewportTargets' own comment on why
			// there's one per buffered RenderFrame slot rather than a single shared image.
			uint renderFrameIndex;
		};
	}

	void Renderer::RecordLightingPass(CommandList& cmdList, uint renderFrameIndex)
	{
		// See RecordGeometryPass's identical read.
		RenderViewportTextureData* viewportData = GetActiveViewportTextureData(renderFrameIndex);
		if (!viewportData)
		{
#if TYR_RENDER_DEBUG
			// TEMP DEBUG - flicker investigation. Revert after.
			TYR_LOG_WARNING("[DBG] RecordLightingPass: SKIPPED (no viewport) renderFrameIndex=%u", renderFrameIndex);
#endif
			return;
		}

		const TextureHandle gbufferAlbedoAOHandle = viewportData->gbufferAlbedoAO;
		const TextureHandle gbufferNormalRoughMetalHandle = viewportData->gbufferNormalRoughMetal;
		const TextureHandle depthBufferHandle = viewportData->depthBuffer;
		const TextureHandle viewportColourTextureHandle = viewportData->colourTexture;
		const uint viewportWidth = viewportData->width;
		const uint viewportHeight = viewportData->height;

		if (viewportWidth == 0 || viewportHeight == 0)
		{
#if TYR_RENDER_DEBUG
			// TEMP DEBUG - flicker investigation. Revert after.
			TYR_LOG_WARNING("[DBG] RecordLightingPass: SKIPPED (zero size) renderFrameIndex=%u width=%u height=%u",
				renderFrameIndex, viewportWidth, viewportHeight);
#endif
			return;
		}

		const Texture& gbufferAlbedoAO = m_Registry.GetTexture(gbufferAlbedoAOHandle);
		const Texture& gbufferNormalRoughMetal = m_Registry.GetTexture(gbufferNormalRoughMetalHandle);
		const Texture& depthBuffer = m_Registry.GetTexture(depthBufferHandle);
		const Texture& outputTexture = m_Registry.GetTexture(viewportColourTextureHandle);

		// GeometryPass wrote these as attachments - make them visible to this pass's shader
		// reads. Layout stays GENERAL throughout (memory/execution barrier only, no transition).
		{
			ImageBarrier barriers[3]{};

			barriers[0].image = gbufferAlbedoAO.image;
			barriers[0].srcAccess = BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barriers[0].dstAccess = BARRIER_ACCESS_SHADER_READ_BIT;
			barriers[0].srcLayout = gbufferAlbedoAO.imageLayout;
			barriers[0].dstLayout = gbufferAlbedoAO.imageLayout;
			barriers[0].subresourceRange.aspect = SUBRESOURCE_ASPECT_COLOUR_BIT;
			barriers[0].subresourceRange.mipCount = 1;
			barriers[0].subresourceRange.arrayLayerCount = 1;
			barriers[0].srcStage = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barriers[0].dstStage = PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			barriers[1] = barriers[0];
			barriers[1].image = gbufferNormalRoughMetal.image;
			barriers[1].srcLayout = gbufferNormalRoughMetal.imageLayout;
			barriers[1].dstLayout = gbufferNormalRoughMetal.imageLayout;

			barriers[2].image = depthBuffer.image;
			barriers[2].srcAccess = BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barriers[2].dstAccess = BARRIER_ACCESS_SHADER_READ_BIT;
			barriers[2].srcLayout = depthBuffer.imageLayout;
			barriers[2].dstLayout = depthBuffer.imageLayout;
			barriers[2].subresourceRange.aspect = SUBRESOURCE_ASPECT_DEPTH_BIT;
			barriers[2].subresourceRange.mipCount = 1;
			barriers[2].subresourceRange.arrayLayerCount = 1;
			barriers[2].srcStage = PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			barriers[2].dstStage = PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			cmdList.AddBarriers(nullptr, 0, barriers, 3);
		}

		cmdList.BindComputePipeline(m_Resources.lightingPipeline);
		cmdList.BindDescriptorSet(m_Resources.descriptorSet, m_Resources.lightingPipeline);

		LightingPushConstants pushConstants;
		pushConstants.gbufferAlbedoAOIndex = gbufferAlbedoAOHandle.h.index;
		pushConstants.gbufferNormalRoughMetalIndex = gbufferNormalRoughMetalHandle.h.index;
		pushConstants.depthIndex = depthBufferHandle.h.index;
		pushConstants.width = viewportWidth;
		pushConstants.height = viewportHeight;
		pushConstants.renderFrameIndex = renderFrameIndex;
		cmdList.PushConstants(m_Resources.lightingPipeline, SHADER_STAGE_COMPUTE_BIT, 0, sizeof(LightingPushConstants), &pushConstants);

		const uint groupCountX = (viewportWidth + 7) / 8;
		const uint groupCountY = (viewportHeight + 7) / 8;
		cmdList.Dispatch(groupCountX, groupCountY, 1);

		// This pass's write needs to be visible to GUIPass's later sampled read of the same
		// texture (ImGui::Image() on the Viewport panel).
		{
			ImageBarrier barrier{};
			barrier.image = outputTexture.image;
			barrier.srcAccess = BARRIER_ACCESS_SHADER_WRITE_BIT;
			barrier.dstAccess = BARRIER_ACCESS_SHADER_READ_BIT;
			barrier.srcLayout = outputTexture.imageLayout;
			barrier.dstLayout = outputTexture.imageLayout;
			barrier.subresourceRange.aspect = SUBRESOURCE_ASPECT_COLOUR_BIT;
			barrier.subresourceRange.mipCount = 1;
			barrier.subresourceRange.arrayLayerCount = 1;
			barrier.srcStage = PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			barrier.dstStage = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			cmdList.AddBarriers(nullptr, 0, &barrier, 1);
		}
	}

	void Renderer::SetupRayTracingBuildPass(RenderGraphBuilder& builder)
	{
		const PipelineStage asBuildStage = PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT;

		// This frame's BLAS builds (see RenderFrame::blasBuildsToRecord, decided on the main
		// thread in Render()) read straight out of the global vertex/index buffers via GPU
		// address - the same buffers TransferPass just wrote this mesh's geometry into and
		// GeometryPass/CullInstancesCS.hlsl also read from. Per spec, build input (geometry and
		// instance data) is read as SHADER_READ, not ACCELERATION_STRUCTURE_READ - that access
		// type is for reading an already-built AS.
		builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.vertexBuffer), asBuildStage, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.indexBuffer), asBuildStage, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.tlasInstanceBuffer), asBuildStage, BARRIER_ACCESS_SHADER_READ_BIT);
	}

	void Renderer::RecordRayTracingBuildPass(CommandList& cmdList, uint renderFrameIndex)
	{
		const RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];

		// m_Resources.blasScratchBuffer is only ever created lazily, the first time a BLAS
		// actually needs building (see EnsureBLASScratchCapacity) - on any earlier frame it's
		// still PoolHandle's default "invalid" sentinel (index == UINT_MAX, not 0), and
		// RenderRegistry::GetBuffer on that crashes (LocalObjectPool::IsValid indexes
		// m_Generations[UINT_MAX]). Only touch it once we know there's at least one build this
		// frame - confirmed via a debugger-symbolized access violation in exactly this call,
		// reached whenever RecordRayTracingBuildPass ran before any mesh's BLAS had ever built.
		for (const BLASBuildRecord& record : renderFrame.blasBuildsToRecord)
		{
			const BufferHandle blasScratchBuffer = m_Registry.GetBuffer(m_Resources.blasScratchBuffer).buffer;

			AccelerationStructureBuildInfo buildInfo;
			buildInfo.accelerationStructure = record.blas;
			buildInfo.scratchBuffer = blasScratchBuffer;
			buildInfo.scratchOffset = record.scratchOffset;
			cmdList.BuildAccelerationStructures(&buildInfo, 1);

			// Every build in this frame's batch reuses this same slot's scratch range
			// sequentially, not concurrently (see RenderResources::blasScratchBuffer's own
			// comment), and the TLAS build below may reference any BLAS built here - Vulkan
			// requires a referenced bottom-level structure to have completed construction first.
			// One barrier serves both needs. Not tracked by the render graph (no AS-aware
			// resource type - see RenderGraphResourceType), so a manual global memory barrier,
			// same as RecordLightingPass's manual image barriers.
			PipelineBarrier barrier{};
			barrier.srcAccess = BARRIER_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT;
			barrier.dstAccess = static_cast<BarrierAccess>(BARRIER_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT | BARRIER_ACCESS_ACCELERATION_STRUCTURE_READ_BIT);
			barrier.srcStage = PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT;
			barrier.dstStage = PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT;
			cmdList.AddBarriers(nullptr, 0, nullptr, 0, &barrier, 1);
		}

		// Full rebuild every frame - refitting (only valid for topology-unchanged, transform-only
		// updates) is a possible later optimization, not needed for correctness now.
		AccelerationStructureBuildInfo tlasBuildInfo;
		tlasBuildInfo.accelerationStructure = m_Resources.tlas[renderFrameIndex];
		tlasBuildInfo.instanceBuffer = m_Registry.GetBuffer(m_Resources.tlasInstanceBuffer).buffer;
		tlasBuildInfo.instanceBufferOffset = (size_t)renderFrameIndex * RenderConstants::c_TLASInstanceBufferSize;
		tlasBuildInfo.instanceCount = m_TLASInstanceCount;
		tlasBuildInfo.scratchBuffer = m_Registry.GetBuffer(m_Resources.tlasScratchBuffer).buffer;
		tlasBuildInfo.scratchOffset = (size_t)renderFrameIndex * m_TLASScratchPerSlotSize;
		tlasBuildInfo.update = false;
		cmdList.BuildAccelerationStructures(&tlasBuildInfo, 1);
	}

	void Renderer::RecordGUIPass(CommandList& cmdList, uint renderFrameIndex)
	{
		Scene& scene = m_Data.scenePool[m_Data.activeScene.h];
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
		m_GUIPass->Execute(cmdList, m_RenderFrames[renderFrameIndex], renderFrameIndex);
		cmdList.EndRendering();

		// Presenting requires the image to be in the present-source layout, not whatever
		// rendering left it in - GUI is always the last thing drawn into this image each frame.
		{
			ImageBarrier barrier{};
			window.swapChain->CreatePresentingImageBarrier(barrier, swapChainImage);
			cmdList.AddBarriers(nullptr, 0, &barrier, 1);
		}
	}

	void Renderer::BuildAndExecuteRenderGraph(uint renderFrameIndex, uint64 frameNumber, bool hasActiveScene, bool hasValidSwapChainImage)
	{
		RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];
		FrameContext& frameCtx = m_Ctx.frameContexts[renderFrameIndex];

		TransferPassArgs transferArgs;
		transferArgs.device = m_Ctx.device;
		transferArgs.registry = &m_Registry;
		transferArgs.resources = &m_Resources;
		m_TransferPass->Recreate(transferArgs);

		GUIPassArgs guiArgs;
		guiArgs.registry = &m_Registry;
		guiArgs.pipeline = m_Resources.guiPipeline;
		guiArgs.descriptorSet = m_Resources.descriptorSet;
		guiArgs.vertexBuffer = m_Resources.guiVertexBuffer;
		guiArgs.indexBuffer = m_Resources.guiIndexBuffer;
		m_GUIPass->Recreate(guiArgs);

		// Only valid (non-null) when hasActiveScene - RenderAsync never dispatches this function
		// with hasActiveScene true unless m_Data.activeScene itself is (see its own guard), and
		// never with hasValidSwapChainImage true unless hasActiveScene is also true (can't
		// acquire an image without a window to acquire it from). Pointers rather than
		// references so there's a legitimate "unset" state for the no-active-scene case, where
		// there's no window to acquire/present to at all - not just "acquired but the image
		// turned out invalid", which is what hasValidSwapChainImage alone already covered.
		RenderWindow* window = nullptr;
		RenderWindowFrame* windowFrame = nullptr;
		RenderWindowHandle windowHandle{};

		if (hasActiveScene)
		{
			Scene& scene = m_Data.scenePool[m_Data.activeScene.h];

			GeometryPassArgs geometryArgs;
			geometryArgs.device = m_Ctx.device;
			geometryArgs.registry = &m_Registry;
			geometryArgs.resources = &m_Resources;
			geometryArgs.scene = &scene;
			geometryArgs.pipeline = m_Resources.geometryGraphicsPipeline;
			geometryArgs.descriptorSet = m_Resources.descriptorSet;
			m_GeometryPass->Recreate(geometryArgs);

			windowHandle = scene.windowHandle;
			window = &m_WindowPool[windowHandle.h];
			windowFrame = &window->frames[renderFrameIndex];
		}

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
			m_Resources.spotLightBuffer, m_Resources.sceneInfoBuffer,
			m_Resources.activeMeshInstanceIndexBuffer, m_Resources.visibleInstanceIndexBuffer,
			m_Resources.indirectDrawCommandBuffer, m_Resources.drawCountBuffer,
			m_Resources.tlasInstanceBuffer
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
			[this, renderFrameIndex](CommandList& cl) { RecordTransferPass(cl, renderFrameIndex); },
			RenderGraphPhase::Transfer, CommandQueueType::CQ_GRAPHICS);

		// Nothing to draw into or present without a valid acquired image (see RenderAsync -
		// this only happens when acquisition genuinely failed, VK_ERROR_OUT_OF_DATE_KHR). The
		// Transfer pass above still runs - it's not tied to the window - so this frame's uploads
		// aren't skipped, just its geometry/GUI/present.
		if (hasValidSwapChainImage)
		{
			// Same phase as "Geometry" below and added first - RenderGraph::AddPass appends each
			// pass' index to its phase's own bucket in registration order (see m_PassOrder's own
			// comment), so this always executes (and has its buffer usages recorded, which is
			// what the automatic barrier-building in RenderGraph::Compile relies on) before
			// GeometryPass's own indirect draw reads what this pass just wrote.
			graph.AddPass("Culling",
				[this](RenderGraphBuilder& builder) { SetupCullingPass(builder); },
				[this, renderFrameIndex](CommandList& cl) { RecordCullingPass(cl, renderFrameIndex); },
				RenderGraphPhase::Geometry, CommandQueueType::CQ_GRAPHICS);

			graph.AddPass("Geometry",
				[this](RenderGraphBuilder& builder) { m_GeometryPass->Setup(builder); },
				[this, renderFrameIndex](CommandList& cl) { RecordGeometryPass(cl, renderFrameIndex); },
				RenderGraphPhase::Geometry, CommandQueueType::CQ_GRAPHICS);

			// Builds at most one pending mesh's BLAS (see m_PendingBLASBuilds), then rebuilds the
			// TLAS from this frame's active instances - see RecordRayTracingBuildPass. Nothing
			// reads the TLAS yet (that's the upcoming shadow ray-query pass); this just keeps it
			// current every frame so that pass can be added later without touching this one.
			graph.AddPass("RayTracingBuild",
				[this](RenderGraphBuilder& builder) { SetupRayTracingBuildPass(builder); },
				[this, renderFrameIndex](CommandList& cl) { RecordRayTracingBuildPass(cl, renderFrameIndex); },
				RenderGraphPhase::RayTracing, CommandQueueType::CQ_GRAPHICS);

			// Reads the G-buffer/depth GeometryPass just wrote (via its own manual image
			// barriers - see RecordLightingPass) and writes the shaded result into the viewport
			// colour texture. Same scene-info/light buffers Geometry already reads - no new
			// buffer barrier needed for a read-after-read, so nothing to register here.
			graph.AddPass("Lighting",
				[](RenderGraphBuilder&) {},
				[this, renderFrameIndex](CommandList& cl) { RecordLightingPass(cl, renderFrameIndex); },
				RenderGraphPhase::Post, CommandQueueType::CQ_GRAPHICS);

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
			? window->executeCompleteSemaphores[window->swapChainImageIndex]
			: SemaphoreHandle{};

		RenderSubmissionRequest submissionRequest;
		submissionRequest.queueType = CommandQueueType::CQ_GRAPHICS;
		submissionRequest.commandLists.Add(cmdList);
		if (hasValidSwapChainImage)
		{
			// Wait on the semaphore used when acquiring the next swapchain image
			submissionRequest.waitSemaphores.Add(windowFrame->aquireSwapChainImageSemaphore);
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

		// Always enqueued, even with no active scene/window at all or a valid window but a
		// failed acquire - RenderSubmissionThread still needs to record this frameNumber as
		// "handled" (see RenderPresentRequest::present's comment) so Render()'s pacing wait
		// doesn't stall waiting for a present that was never going to happen. It just skips the
		// actual Present() call (and so never reads swapChain/imageIndex) when present is false,
		// so leaving them default/invalid below when there's no window at all is safe.
		RenderPresentRequest presentRequest;
		if (hasActiveScene)
		{
			presentRequest.swapChain = window->swapChain;
			presentRequest.imageIndex = window->swapChainImageIndex;
			presentRequest.window = windowHandle;
		}
		presentRequest.queue = m_Ctx.graphicsQueue;
		// Wait on the signal semaphore used in the submission above
		presentRequest.waitSemaphore = executeCompleteSemaphore;
		presentRequest.frameNumber = frameNumber;
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
			m_AllocManager.SignalFrameUpload(completion->frameNumber, completion->timelineValue);

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
			// A mesh whose one-time BLAS build (see RendererAPI::RequestBLASBuild) already ran
			// owns an acceleration structure that nothing else references, backed by a range of
			// the shared blasStorageBuffer (see Mesh::blasStorageAllocation's own comment) -
			// leaving the acceleration structure delete out is exactly what produced the
			// VMA_ASSERT_LEAK in VmaDeviceMemoryBlock::Destroy and the "Mesh BLAS" VkBuffer-not-
			// destroyed error from vkDestroyDevice at shutdown (back when each BLAS had its own
			// dedicated backing buffer); leaving the storage free out instead would leak that
			// range of blasStorageBuffer for the rest of the process' life.
			if (mesh.blas)
			{
				m_Ctx.device->DeleteAccelerationStructure(mesh.blas);
				m_AllocManager.FreeBLASStorageAllocation(mesh.blasStorageAllocation);
			}
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
			m_Data.scenePool.Delete(handle.h);
		}
		for (RenderViewportHandle handle : renderFrame.renderViewportsToDelete)
		{
			DeleteRenderViewportResources(handle);
		}
		for (RenderBufferHandle handle : renderFrame.buffersToDelete)
		{
			m_Registry.DeleteBuffer(handle);
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

	void Renderer::StallUntilGPUIdle()
	{
		// Stall the main thread until every already-created RenderAsync task (CPU-side
		// recording), everything RenderSubmissionThread has queued for them (GPU submit/
		// present), and the GPU itself have all finished - for whenever something is about to be
		// deleted/recreated that an in-flight frame could still be touching. Mirrors Shutdown()'s
		// quiesce, minus stopping the submission thread - rendering carries on normally once this
		// returns. Not frequent enough in practice (a window resize, or growing the BLAS scratch
		// buffer) for anything cleverer to be worth it.
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
	}

	void Renderer::ResizeWindow(RenderWindowHandle window)
	{
		StallUntilGPUIdle();

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

	RenderViewportHandle Renderer::CreateRenderViewport()
	{
		return RenderViewportHandle(m_RenderViewportPool.Create());
	}

	void Renderer::DeleteRenderViewport(RenderViewportHandle viewport)
	{
		// Deferred the same way RemoveWindow/RemoveScene are - a buffered slot's textures might
		// still be read by a worker thread (or still in-flight on the GPU) from up to
		// c_BufferedFrameCount frames ago. See DeleteRenderViewportResources, which does the actual
		// teardown once this slot cycles back around and that's confirmed safe.
		GetRenderFrame().renderViewportsToDelete.Add(viewport);
	}

	TextureHandle Renderer::CreateViewportTargetTexture(const char* debugName, PixelFormat format, ImageUsage usage, uint width, uint height)
	{
		TextureDesc desc;
		desc.debugName = debugName;
		desc.info.width = width;
		desc.info.height = height;
		desc.info.depth = 1;
		desc.info.arrayLayerCount = 1;
		desc.info.mipCount = 1;
		desc.info.format = format;
		desc.info.type = ImageType::Image2D;
		desc.sampleCount = SampleCount::OneBit;
		desc.usage = usage;
		desc.layout = ImageLayout::IMAGE_LAYOUT_GENERAL;

		const TextureHandle handle = m_Registry.CreateTexture(desc);
		GetRenderFrame().texturesToAdd.Add(handle);

		// Same bindless descriptor write RendererAPI::CreateTexture does - see its own comment.
		const Texture& texture = m_Registry.GetTexture(handle);
		ImageBindingInfo imageInfo;
		imageInfo.imageView = texture.imageView;
		imageInfo.hasSampler = false;
		imageInfo.layout = texture.imageLayout;
		QueueImageBindingUpdate(TYR_BINDING_TEXTURES, handle.h.index, imageInfo);

		return handle;
	}

	void Renderer::DeleteViewportTargetTexture(TextureHandle handle)
	{
		GetRenderFrame().texturesToDelete.Add(handle);
	}

	void Renderer::ResizeRenderViewportSlot(RenderViewport& viewport, uint slot, const char* debugName, uint width, uint height)
	{
		RenderViewportTextureData& targets = viewport.textureData[slot];

		if (targets.colourTexture)
		{
			DeleteViewportTargetTexture(targets.colourTexture);
			DeleteViewportTargetTexture(targets.gbufferAlbedoAO);
			DeleteViewportTargetTexture(targets.gbufferNormalRoughMetal);
			DeleteViewportTargetTexture(targets.gbufferMotion);
			DeleteViewportTargetTexture(targets.depthBuffer);
		}

		// Written by the deferred lighting pass (a storage image), read by GUIPass to display it
		// via ImGui::Image() - GeometryPass no longer writes into this directly, see
		// gbufferAlbedoAO/gbufferNormalRoughMetal below. UNORM, not SRGB - VK_FORMAT_R8G8B8A8_SRGB
		// doesn't support VK_IMAGE_USAGE_STORAGE_BIT on this hardware (confirmed via
		// VK_ERROR_FORMAT_NOT_SUPPORTED), so DeferredLightingCS.hlsl writes linear colour directly
		// instead of sRGB-encoding it - GUIPS.hlsl's later sample+swap-chain-write still does the
		// one real sRGB encode, same as any other UI element.
		targets.colourTexture = CreateViewportTargetTexture(debugName, PixelFormat::PF_R8G8B8A8_UNORM,
			static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_STORAGE_BIT), width, height);

		// Must match geometryGraphicsPipeline's declared colour attachment formats (see
		// CreatePipelines) - dynamic rendering requires the two to agree.
		targets.gbufferAlbedoAO = CreateViewportTargetTexture("GBuffer AlbedoAO", PixelFormat::PF_R8G8B8A8_SRGB,
			static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_COLOUR_ATTACHMENT_BIT), width, height);
		targets.gbufferNormalRoughMetal = CreateViewportTargetTexture("GBuffer NormalRoughMetal", PixelFormat::PF_R16G16B16A16_SFLOAT,
			static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_COLOUR_ATTACHMENT_BIT), width, height);
		targets.gbufferMotion = CreateViewportTargetTexture("GBuffer Motion", PixelFormat::PF_R16G16_SFLOAT,
			static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_COLOUR_ATTACHMENT_BIT), width, height);
		// Reverse-Z (see Renderer::Render's projection setup) - cleared to 0, compared Greater.
		targets.depthBuffer = CreateViewportTargetTexture("Depth Buffer", PixelFormat::PF_D32_SFLOAT,
			static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT), width, height);

		targets.width = width;
		targets.height = height;
		targets.isNew = true;
	}

	void Renderer::EnsureLightingOutputBound(uint renderFrameIndex, TextureHandle colourTexture)
	{
		if (!colourTexture || m_LightingOutputBoundTextures[renderFrameIndex] == colourTexture)
		{
			return;
		}

		// The lighting compute pass writes colourTexture directly (not through the bindless sampled
		// textures[] array CreateViewportTargetTexture already bound it into above) - needs its own
		// storage-image descriptor write, at this slot's own index into the
		// TYR_BINDING_LIGHTING_OUTPUT array (see DeferredLightingCS.hlsl's outputImages[]) - one
		// entry per buffered RenderFrame slot. Checked every tick (not just right after a resize)
		// since the active scene switching to a different RenderViewport also changes which colour
		// texture this slot should actually point at, even when neither viewport's size changed.
		const Texture& texture = m_Registry.GetTexture(colourTexture);
		ImageBindingInfo outputImageInfo;
		outputImageInfo.imageView = texture.imageView;
		outputImageInfo.hasSampler = false;
		outputImageInfo.layout = texture.imageLayout;
		QueueImageBindingUpdate(TYR_BINDING_LIGHTING_OUTPUT, renderFrameIndex, outputImageInfo);

		m_LightingOutputBoundTextures[renderFrameIndex] = colourTexture;
	}

	void Renderer::DeleteRenderViewportResources(RenderViewportHandle viewport)
	{
		RenderViewport& rv = m_RenderViewportPool[viewport.h];
		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			RenderViewportTextureData& targets = rv.textureData[i];
			if (!targets.colourTexture)
			{
				continue;
			}

			// Deleted immediately via the registry, not DeleteViewportTargetTexture/
			// texturesToDelete - this function only ever runs from ProcessFrameDeleteLists
			// (processing renderViewportsToDelete), by which point it's already established safe
			// to delete right away (same reasoning as DeleteWindowResources' direct teardown).
			// Queuing into texturesToDelete here would be too late: ProcessFrameDeleteLists has
			// already finished iterating that same list earlier in this same call (see its own
			// ordering), so a newly queued entry would just sit there and get silently discarded
			// by the renderFrame.Clear() that follows - confirmed as a real leak (reproduced and
			// fixed: LocalObjectPool<Texture> asserting on shutdown with exactly
			// c_BufferedFrameCount * 5 = 15 left over).
			m_Registry.DeleteTexture(targets.colourTexture);
			m_Registry.DeleteTexture(targets.gbufferAlbedoAO);
			m_Registry.DeleteTexture(targets.gbufferNormalRoughMetal);
			m_Registry.DeleteTexture(targets.gbufferMotion);
			m_Registry.DeleteTexture(targets.depthBuffer);
		}

		m_RenderViewportPool.Delete(viewport.h);
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
			desc.fileName = "GBufferPS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_FRAGMENT_BIT;
			m_Resources.geometryPixelShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "DeferredLightingCS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_COMPUTE_BIT;
			m_Resources.lightingComputeShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "CullInstancesCS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_COMPUTE_BIT;
			m_Resources.cullingComputeShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
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
		m_Ctx.device->DeleteShaderModule(m_Resources.lightingComputeShader);
		m_Ctx.device->DeleteShaderModule(m_Resources.cullingComputeShader);
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
				// 11 existing + activeMeshInstanceIndexBuffer/visibleInstanceIndexBuffer/
				// indirectDrawCommandBuffer/drawCountBuffer for GPU-driven instance culling.
				poolSize.descriptorCount = 15;
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
			{
				// TYR_BINDING_LIGHTING_OUTPUT - the deferred lighting pass's storage image output,
				// one per buffered RenderFrame slot (see RenderViewportTextureData).
				DescriptorPoolSize& poolSize = poolDesc.poolSizes.ExpandOne();
				poolSize.descriptorType = DescriptorType::StorageImage;
				poolSize.descriptorCount = RenderConstants::c_BufferedFrameCount;
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

			// Scene-info/lights/textures/samplers also need to be readable from the deferred
			// lighting compute pass (DeferredLightingCS.hlsl reads the G-buffer/depth via the
			// bindless textures[] array and the same light buffers GBufferPS.hlsl's forward
			// equivalent used to read directly), and mesh/meshLOD/mesh-instance from the
			// instance culling compute pass too (CullInstancesCS.hlsl) - meshlet/vertex/index/
			// material stay geometry-only, nothing else needs them.
			const ShaderStage meshAndComputeStages = static_cast<ShaderStage>(meshPipelineStages | SHADER_STAGE_COMPUTE_BIT);

			AddBinding(TYR_BINDING_SCENE_INFO, DescriptorType::UniformBuffer, 1, meshAndComputeStages);
			AddBinding(TYR_BINDING_MESH, DescriptorType::StorageBuffer, 1, meshAndComputeStages);
			AddBinding(TYR_BINDING_MESH_LOD, DescriptorType::StorageBuffer, 1, meshAndComputeStages);
			AddBinding(TYR_BINDING_MESHLET, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_VERTEX, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_INDEX, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_MESH_INSTANCE, DescriptorType::StorageBuffer, 1, meshAndComputeStages);
			AddBinding(TYR_BINDING_MATERIAL, DescriptorType::StorageBuffer, 1, meshPipelineStages);
			AddBinding(TYR_BINDING_DIR_LIGHT, DescriptorType::StorageBuffer, 1, meshAndComputeStages);
			AddBinding(TYR_BINDING_POINT_LIGHT, DescriptorType::StorageBuffer, 1, meshAndComputeStages);
			AddBinding(TYR_BINDING_SPOT_LIGHT, DescriptorType::StorageBuffer, 1, meshAndComputeStages);

			const DescriptorBindingFlags bindlessFlags = static_cast<DescriptorBindingFlags>(DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
			AddBinding(TYR_BINDING_TEXTURES, DescriptorType::SampledImage, RenderConstants::c_MaxTextures, meshAndComputeStages, bindlessFlags);
			AddBinding(TYR_BINDING_SAMPLERS, DescriptorType::Sampler, Device::c_MaxSamplers, meshAndComputeStages, bindlessFlags);
			// GUIVS.hlsl reads this directly (vertex-pulling via SV_VertexID) - a vertex stage,
			// not one of the mesh pipeline's task/mesh/fragment stages. Declared in ascending
			// binding-number order with no gaps after this, matching what
			// VulkanDescriptorSet.cpp's bindingDescriptorTypes lookup assumes.
			AddBinding(TYR_BINDING_GUI_VERTEX, DescriptorType::StorageBuffer, 1, SHADER_STAGE_VERTEX_BIT);
			// Deferred lighting pass's output (the viewport colour texture, written as a storage
			// image) - compute-only, not part of the bindless sampled textures[] array above.
			// One entry per buffered RenderFrame slot (see RenderViewportTextureData and
			// DeferredLightingCS.hlsl's outputImages[]) - each slot's compute dispatch only ever
			// writes its own array index, so up to c_BufferedFrameCount dispatches can genuinely
			// be in flight on the GPU at once without racing each other on the same image.
			//
			// Needs UPDATE_AFTER_BIND_BIT regardless: EditorViewport::Draw resizes a slot's texture
			// (and rewrites that slot's array entry via Renderer::EnsureLightingOutputBound)
			// whenever the panel size changes, and even though PrepareForNextFrame already guarantees that
			// specific slot's own previous dispatch has finished by the time its turn comes back
			// around, the descriptor SET as a whole is still normally "in use" by whichever OTHER
			// slots' dispatches are currently in flight - without this flag, updating any entry
			// while that's true is a real Vulkan spec violation (VUID-vkUpdateDescriptorSets-
			// None-03047), not just a validation warning.
			//
			// Also needs PARTIALLY_BOUND_BIT, same reason the bindless TYR_BINDING_TEXTURES array
			// does: g_PushConstants.renderFrameIndex is a dynamic (runtime, not shader-compile-time
			// constant) index into this array, and EditorViewport::Draw only creates a given slot's
			// texture - and so only writes that slot's array entry - on that slot's own first turn
			// through the 0/1/2 cycle. Without this flag, a dispatch for an already-created slot
			// (dynamically indexing this array) can require every entry to be validly bound, not
			// just the one it actually reads - including slots that haven't had their first turn
			// yet during the first couple of ticks.
			const DescriptorBindingFlags lightingOutputFlags = static_cast<DescriptorBindingFlags>(
				DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
			AddBinding(TYR_BINDING_LIGHTING_OUTPUT, DescriptorType::StorageImage, RenderConstants::c_BufferedFrameCount,
				SHADER_STAGE_COMPUTE_BIT, lightingOutputFlags);

			// GPU-driven instance culling (CullInstancesCS.hlsl) - compute-only except
			// TYR_BINDING_VISIBLE_INSTANCE_INDICES, which MeshAS.hlsl (task stage) also reads
			// per draw via SV_DrawIndex. See RecordCullingPass/RenderResources.h.
			AddBinding(TYR_BINDING_ACTIVE_INSTANCE_INDICES, DescriptorType::StorageBuffer, 1, SHADER_STAGE_COMPUTE_BIT);
			AddBinding(TYR_BINDING_VISIBLE_INSTANCE_INDICES, DescriptorType::StorageBuffer, 1,
				static_cast<ShaderStage>(SHADER_STAGE_COMPUTE_BIT | SHADER_STAGE_TASK_BIT));
			AddBinding(TYR_BINDING_INDIRECT_DRAW_COMMANDS, DescriptorType::StorageBuffer, 1, SHADER_STAGE_COMPUTE_BIT);
			AddBinding(TYR_BINDING_DRAW_COUNT, DescriptorType::StorageBuffer, 1, SHADER_STAGE_COMPUTE_BIT);

			m_Resources.descriptorSetLayout = m_Ctx.device->CreateDescriptorSetLayout(layoutDesc);

			DescriptorSetDesc setDesc;
			setDesc.layout = m_Resources.descriptorSetLayout;
			setDesc.pool = m_Resources.descriptorPool;
#if !TYR_FINAL
			setDesc.debugName = "DescriptorSet";
#endif
			m_Resources.descriptorSet = m_Ctx.device->CreateDescriptorSet(setDesc);

			desc.pipelineLayoutDesc.descriptorSetLayouts.Add(m_Resources.descriptorSetLayout);
			// No push constant range needed any more - MeshAS.hlsl resolves its mesh instance
			// index from CullInstancesCS.hlsl's compacted buffer via SV_DrawIndex instead (a
			// single indirect multi-draw call can't vary push constants per sub-draw anyway).
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

		// Depth stencil description - reverse-Z (see Renderer::Render's projection setup, which
		// swaps near/far), so a nearer fragment has a *larger* depth value than what's already
		// there: compare Greater, cleared to 0 (see RecordGeometryPass).
		desc.depthStencilStateDesc.depthCompareOp = CompareOp::Greater;
		desc.depthStencilStateDesc.minDepthBounds = 0.0f;
		desc.depthStencilStateDesc.maxDepthBounds = 1.0f;
		desc.depthStencilStateDesc.depthTestEnable = true;
		desc.depthStencilStateDesc.depthWriteEnable = true;
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

		// Must match Renderer::ResizeRenderViewportSlot's gbufferAlbedoAO/gbufferNormalRoughMetal/
		// gbufferMotion/depthBuffer formats - dynamic rendering requires the two to agree.
		desc.dynamicRendering.colorAttachmentFormats.Add(PF_R8G8B8A8_SRGB);
		desc.dynamicRendering.colorAttachmentFormats.Add(PF_R16G16B16A16_SFLOAT);
		desc.dynamicRendering.colorAttachmentFormats.Add(PF_R16G16_SFLOAT);
		desc.dynamicRendering.depthAttachmentFormat = PF_D32_SFLOAT;
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

		// Not desc.depthStencilStateDesc - that's real depth test/write now (for the G-buffer
		// pass), which would be invalid here since GUI declares no depth attachment at all below.
		guiDesc.depthStencilStateDesc.depthCompareOp = CompareOp::Never;
		guiDesc.depthStencilStateDesc.minDepthBounds = 0.0f;
		guiDesc.depthStencilStateDesc.maxDepthBounds = 1.0f;
		guiDesc.depthStencilStateDesc.depthTestEnable = false;
		guiDesc.depthStencilStateDesc.depthWriteEnable = false;
		guiDesc.depthStencilStateDesc.depthBoundsTestEnable = false;
		guiDesc.depthStencilStateDesc.stencilTestEnable = false;
		guiDesc.depthStencilStateDesc.front.failOp = StencilOp::Zero;
		guiDesc.depthStencilStateDesc.front.passOp = StencilOp::Zero;
		guiDesc.depthStencilStateDesc.front.depthFailOp = StencilOp::Zero;
		guiDesc.depthStencilStateDesc.front.compareOp = CompareOp::Never;
		guiDesc.depthStencilStateDesc.front.compareMask = 0;
		guiDesc.depthStencilStateDesc.front.writeMask = 0;
		guiDesc.depthStencilStateDesc.front.reference = 0;
		guiDesc.depthStencilStateDesc.back = guiDesc.depthStencilStateDesc.front;
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

		// Deferred lighting pass - full-screen compute, reads the G-buffer/depth via the same
		// bindless textures[] array and writes the shaded result into the viewport colour
		// texture via TYR_BINDING_LIGHTING_OUTPUT (see RecordLightingPass).
		ComputePipelineDesc lightingDesc;
		lightingDesc.pipelineLayoutDesc.descriptorSetLayouts.Add(m_Resources.descriptorSetLayout);

		PushConstantRange& lightingPushConstantRange = lightingDesc.pipelineLayoutDesc.pushConstantRanges.ExpandOne();
		lightingPushConstantRange.stageFlags = SHADER_STAGE_COMPUTE_BIT;
		lightingPushConstantRange.offset = 0;
		// sizeof(LightingPushConstants) directly, not a hand-counted field size, so this can't
		// silently drift out of sync with the struct (see DeferredLightingCS.hlsl's matching one)
		// again - this exact mismatch (a stale hardcoded size after adding renderFrameIndex) is
		// what produced a real VUID-VkComputePipelineCreateInfo-layout-10069 validation error.
		lightingPushConstantRange.size = sizeof(LightingPushConstants);

		lightingDesc.shader = m_Resources.lightingComputeShader;

		m_Resources.lightingPipeline = m_Ctx.device->CreateComputePipeline(lightingDesc);

		// GPU-driven instance frustum culling - runs just before GeometryPass, compacting
		// visible instances into the indirect draw buffers it then draws from directly. See
		// RecordCullingPass.
		ComputePipelineDesc cullingDesc;
		cullingDesc.pipelineLayoutDesc.descriptorSetLayouts.Add(m_Resources.descriptorSetLayout);

		PushConstantRange& cullingPushConstantRange = cullingDesc.pipelineLayoutDesc.pushConstantRanges.ExpandOne();
		cullingPushConstantRange.stageFlags = SHADER_STAGE_COMPUTE_BIT;
		cullingPushConstantRange.offset = 0;
		cullingPushConstantRange.size = sizeof(uint); // activeInstanceCount - see CullInstancesCS.hlsl

		cullingDesc.shader = m_Resources.cullingComputeShader;

		m_Resources.cullingPipeline = m_Ctx.device->CreateComputePipeline(cullingDesc);
	}

	void Renderer::DeletePipelines()
	{
		m_Ctx.device->DeleteGraphicsPipeline(m_Resources.geometryGraphicsPipeline);
		m_Ctx.device->DeleteGraphicsPipeline(m_Resources.guiPipeline);
		m_Ctx.device->DeleteComputePipeline(m_Resources.lightingPipeline);
		m_Ctx.device->DeleteComputePipeline(m_Resources.cullingPipeline);
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
			// RayTracing, not Storage - BLAS builds (Renderer::RecordRayTracingBuildPass) read
			// straight out of this buffer via its GPU address, on top of the existing mesh/task
			// shader StructuredBuffer reads.
			desc.usage = RenderBufferUsage::RayTracing;
			m_Resources.vertexBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Index Buffer";
			desc.size = RenderConstants::c_IndexBufferSize;
			desc.usage = RenderBufferUsage::RayTracing;
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
			// One c_BufferedFrameCount-th per buffered RenderFrame slot - see
			// RenderConstants::c_GUIVertexBufferSize's own comment on why.
			desc.size = RenderConstants::c_GUIVertexBufferSize * RenderConstants::c_BufferedFrameCount;
			// Storage, not Vertex - GUIVS.hlsl pulls its own vertex via a StructuredBuffer
			// binding (TYR_BINDING_GUI_VERTEX) instead of fixed-function vertex input.
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.guiVertexBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "GUI Index Buffer";
			// One c_BufferedFrameCount-th per buffered RenderFrame slot - see
			// RenderConstants::c_GUIIndexBufferSize's own comment on why.
			desc.size = RenderConstants::c_GUIIndexBufferSize * RenderConstants::c_BufferedFrameCount;
			desc.usage = RenderBufferUsage::Index;
			desc.stride = sizeof(uint16);
			m_Resources.guiIndexBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Active Mesh Instance Index Buffer";
			desc.size = sizeof(uint) * RenderConstants::c_MaxMeshInstances;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.activeMeshInstanceIndexBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Visible Instance Index Buffer";
			desc.size = sizeof(uint) * RenderConstants::c_MaxMeshInstances;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.visibleInstanceIndexBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Indirect Draw Command Buffer";
			desc.size = sizeof(uint) * 3 * RenderConstants::c_MaxMeshInstances; // VkDrawMeshTasksIndirectCommandEXT
			desc.usage = RenderBufferUsage::Indirect;
			m_Resources.indirectDrawCommandBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "Draw Count Buffer";
			desc.size = sizeof(uint);
			desc.usage = RenderBufferUsage::Indirect;
			m_Resources.drawCountBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "TLAS Instance Buffer";
			// One c_BufferedFrameCount-th per buffered RenderFrame slot - see
			// RenderResources::tlasInstanceBuffer's own comment on why.
			desc.size = RenderConstants::c_TLASInstanceBufferSize * RenderConstants::c_BufferedFrameCount;
			desc.usage = RenderBufferUsage::RayTracing;
			m_Resources.tlasInstanceBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			// See RenderResources::rtCullingStagingBuffer's own comment - RenderAsync writes
			// directly into this at a fixed offset for its own renderFrameIndex slot, no
			// allocator involved.
			RenderBufferDesc desc;
			desc.debugName = "RT/Culling Staging Buffer";
			desc.size = c_RTCullingStagingSlotSize * RenderConstants::c_BufferedFrameCount;
			desc.usage = RenderBufferUsage::Upload;
			m_Resources.rtCullingStagingBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			// See RenderResources::blasStorageBuffer's own comment.
			RenderBufferDesc desc;
			desc.debugName = "BLAS Storage Buffer";
			desc.size = RenderConstants::c_BLASStorageBufferSize;
			desc.usage = RenderBufferUsage::AccelerationStructureStorage;
			m_Resources.blasStorageBuffer = m_Registry.CreateBuffer(desc);
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
		m_Registry.DeleteBuffer(m_Resources.activeMeshInstanceIndexBuffer);
		m_Registry.DeleteBuffer(m_Resources.visibleInstanceIndexBuffer);
		m_Registry.DeleteBuffer(m_Resources.indirectDrawCommandBuffer);
		m_Registry.DeleteBuffer(m_Resources.drawCountBuffer);
		m_Registry.DeleteBuffer(m_Resources.tlasInstanceBuffer);
		m_Registry.DeleteBuffer(m_Resources.rtCullingStagingBuffer);
		m_Registry.DeleteBuffer(m_Resources.blasStorageBuffer);
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

	void Renderer::CreateAccelerationStructures()
	{
		// One TLAS per buffered RenderFrame slot - see RenderResources::tlas's own comment. Sized
		// once for the worst case (every active mesh instance visible); the TLAS itself is never
		// recreated, only rebuilt (see RecordRayTracingBuildPass). m_Resources.blasScratchBuffer
		// is the one that grows on demand, since a mesh's triangle count isn't known up front.
		AccelerationStructureDesc tlasDesc;
		tlasDesc.debugName = "TLAS";
		tlasDesc.type = AccelerationStructureType::TopLevel;
		tlasDesc.maxInstanceCount = RenderConstants::c_MaxMeshInstances;

		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			m_Resources.tlas[i] = m_Ctx.device->CreateAccelerationStructure(tlasDesc);
			// Every slot's TLAS shares the same desc, so the same scratch size - just keep
			// whichever came back, they're all identical.
			m_TLASScratchPerSlotSize = m_Ctx.device->GetAccelerationStructureBuildScratchSize(m_Resources.tlas[i]);
		}

		RenderBufferDesc scratchDesc;
		scratchDesc.debugName = "TLAS Scratch Buffer";
		// One c_BufferedFrameCount-th per buffered RenderFrame slot - see
		// RenderResources::tlasScratchBuffer's own comment.
		scratchDesc.size = m_TLASScratchPerSlotSize * RenderConstants::c_BufferedFrameCount;
		scratchDesc.usage = RenderBufferUsage::RayTracing;
		m_Resources.tlasScratchBuffer = m_Registry.CreateBuffer(scratchDesc);
	}

	void Renderer::DeleteAccelerationStructures()
	{
		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			m_Ctx.device->DeleteAccelerationStructure(m_Resources.tlas[i]);
		}
		m_Registry.DeleteBuffer(m_Resources.tlasScratchBuffer);
		if (m_Resources.blasScratchBuffer)
		{
			m_Registry.DeleteBuffer(m_Resources.blasScratchBuffer);
		}
	}

	void Renderer::EnsureBLASScratchCapacity(size_t requiredPerSlotSize)
	{
		// m_BLASScratchCapacity is a PER-SLOT size (see RenderResources::blasScratchBuffer's own
		// comment) - the buffer itself is c_BufferedFrameCount times this.
		if (requiredPerSlotSize <= m_BLASScratchCapacity)
		{
			return;
		}

		if (m_Resources.blasScratchBuffer)
		{
			GetRenderFrame().buffersToDelete.Add(m_Resources.blasScratchBuffer);
		}

		RenderBufferDesc desc;
		desc.debugName = "BLAS Scratch Buffer";
		desc.size = requiredPerSlotSize * RenderConstants::c_BufferedFrameCount;
		desc.usage = RenderBufferUsage::RayTracing;
		m_Resources.blasScratchBuffer = m_Registry.CreateBuffer(desc);
		m_BLASScratchCapacity = requiredPerSlotSize;
	}

	void Renderer::DeleteSamplers()
	{
		m_Ctx.device->DeleteSampler(m_Resources.materialSampler);
	}

	void Renderer::CreatePasses()
	{
		TransferPassArgs transferArgs;
		transferArgs.device = m_Ctx.device;
		transferArgs.registry = &m_Registry;
		transferArgs.resources = &m_Resources;
		m_TransferPass = new TransferPass(transferArgs);

		GeometryPassArgs geometryArgs;
		geometryArgs.device = m_Ctx.device;
		geometryArgs.registry = &m_Registry;
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