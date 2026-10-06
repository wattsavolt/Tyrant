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
#include "ShadowRTPass.h"
#include "ShadowDenoisePass.h"
#include "RenderGraph.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphAllocation.h"
namespace tyr
{
	namespace
	{
		// Low-discrepancy base-`base` value for index `index` - standard Halton sequence
		// construction. Used as a TAA sub-pixel jitter source: cycling through a handful of
		// indices gives a sequence of camera offsets that cover sub-pixel space evenly, rather
		// than clustering like plain random numbers would.
		float HaltonSequence(uint index, uint base)
		{
			float result = 0.0f;
			float f = 1.0f;
			while (index > 0)
			{
				f /= (float)base;
				result += f * (float)(index % base);
				index /= base;
			}
			return result;
		}

		// A held-constant 8-sample Halton(2,3) cycle - long enough to decorrelate well, short
		// enough to converge quickly once the camera stops moving. Index starts at 1, not 0, so
		// the first sample isn't the degenerate (0,0) offset every base-N Halton sequence starts
		// with.
		constexpr uint c_JitterSampleCount = 8;
		Vector2 ComputeJitterNDC(uint64 frameNumber, uint viewportWidth, uint viewportHeight)
		{
			const uint index = (uint)(frameNumber % c_JitterSampleCount) + 1;
			const float haltonX = HaltonSequence(index, 2) - 0.5f;
			const float haltonY = HaltonSequence(index, 3) - 0.5f;
			// A one-pixel screen-space offset is 2/resolution in NDC, since NDC spans [-1, 1]
			// across the full screen.
			return Vector2(haltonX * 2.0f / (float)viewportWidth, haltonY * 2.0f / (float)viewportHeight);
		}

		// Fixed, worst-case-sized byte regions within the ray-tracing culling staging buffer,
		// one contiguous block per buffered render frame slot, indexed directly by renderFrameIndex.
		constexpr size_t c_ActiveInstanceIndicesBytes = sizeof(uint) * RenderConstants::c_MaxMeshInstances;
		constexpr size_t c_DrawCountResetBytes = sizeof(uint);
		constexpr size_t c_TlasInstancesBytes = RenderConstants::c_TLASInstanceBufferSize;
		// Also built and uploaded from the worker thread (once ShadowRTPass's selection is known),
		// so it shares this same staging buffer rather than needing one of its own.
		constexpr size_t c_ShadowLightSlotMapBytes = sizeof(uint) * RenderConstants::c_ShadowLightSlotMapEntryCount;
		constexpr size_t c_RTCullingStagingSlotSize = c_ActiveInstanceIndicesBytes + c_DrawCountResetBytes + c_TlasInstancesBytes + c_ShadowLightSlotMapBytes;
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
			// A simple bump allocator over this tick's own batch - each build gets its own
			// distinct, non-overlapping sub-range instead of every build in the batch reusing the
			// same one, so no barrier is needed between consecutive builds' scratch writes. Starts
			// aligned (0) and stays aligned, since every advance below re-aligns it.
			size_t scratchCursor = 0;
			const size_t scratchAlignment = m_Ctx.device->GetAccelerationStructureScratchOffsetAlignment();

			for (uint i = 0; i < m_PendingBLASBuilds.Size(); ++i)
			{
				Mesh& mesh = m_Registry.GetMesh(m_PendingBLASBuilds[i]);
				if (mesh.blas.accelerationStructure)
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

				mesh.blas.accelerationStructure = m_Ctx.device->CreateAccelerationStructureAt(blasDesc, m_Registry.GetBuffer(m_Resources.blasStorageBuffer).buffer, mesh.blasStorageAllocation.offset);

				BLASBuildRecord& record = renderFrame.blasBuildsToRecord.ExpandOne();
				record.blas = mesh.blas.accelerationStructure;
				record.blasResource = &mesh.blas;
				// This tick's own within-batch offset, not yet the final byte offset - the
				// per-slot base below is only known once this frame's scratch capacity (and so
				// m_BLASScratchCapacity) is finalized, which depends on every build in the batch
				// having already been sized first.
				record.scratchOffset = scratchCursor;
				scratchCursor = MemoryUtil::Align(scratchCursor + buildScratchSize, scratchAlignment);

				accumulatedBytes += meshBytes;
				builtAny = true;
				processedCount++;
			}

			m_PendingBLASBuilds.EraseFromFront(processedCount);

			if (scratchCursor > 0)
			{
				// Sized for the sum of this tick's whole batch, not just the largest single build -
				// every build now needs its own space at once, rather than reusing one shared range
				// one build at a time.
				EnsureBLASScratchCapacity(scratchCursor);

				const size_t slotBaseOffset = (size_t)m_RenderFrameIndex * m_BLASScratchCapacity;
				for (BLASBuildRecord& record : renderFrame.blasBuildsToRecord)
				{
					record.scratchOffset += slotBaseOffset;
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

				// Resolve this scene's own RenderViewport and bring its current buffered slot up
				// to date before anything below reads its width/height or texture handles.
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

					// Apply this slot's own pending resize, if any. A no-op (just clears the flag) if
					// this slot already matches the requested size.
					if (viewport.pendingResize[m_RenderFrameIndex])
					{
						if (targets.width != viewport.requestedWidth || targets.height != viewport.requestedHeight)
						{
							ResizeRenderViewportSlot(viewport, m_RenderFrameIndex, "Viewport", viewport.requestedWidth, viewport.requestedHeight);
						}
						viewport.pendingResize[m_RenderFrameIndex] = false;
					}
					SyncViewportOverlay(viewport, m_RenderFrameIndex);

					// Keep this slot's lighting-output descriptor pointed at the current colour
					// texture.
					EnsureLightingOutputBound(m_RenderFrameIndex, targets.colourTexture);
					EnsureShadowMaskArraysBound(m_RenderFrameIndex, targets.shadowMasksRaw, targets.shadowMasks);
					EnsureTaaResolveOutputBound(m_RenderFrameIndex, targets.resolvedColourTexture);
					EnsureEditorGridOutputBound(m_RenderFrameIndex, targets.overlayColourTexture);

					// Only drawn once the slot has somewhere to draw it.
					if (targets.overlayColourTexture)
					{
						renderFrame.viewportGrid = viewport.grid;
					}

					if (targets.width != 0 && targets.height != 0)
					{
						viewportWidth = targets.width;
						viewportHeight = targets.height;
					}
				}

				// TODO: Extend to multiple views per scene - only the first view's data reaches
				// the shader for now.
				if (!sceneFrame.views.IsEmpty())
				{
					const SceneView& sv = sceneFrame.views[0];
					const float aspect = GraphicsUtility::CalculateAspectRatio(sv.viewArea, viewportWidth, viewportHeight);

					const Matrix4 view = Matrix4::CreateView(sv.camera.position, sv.camera.forward, sv.camera.up);
					// Do reverse-z for greater floating-point precision
					const Matrix4 projection = Matrix4::CreatePerspective(sv.camera.fov, aspect, sv.camera.farZ, sv.camera.nearZ);

					sceneInfo.viewProj = view * projection;
					if (renderFrame.viewportGrid.enabled)
					{
						renderFrame.gridInvViewProj = sceneInfo.viewProj.Inverse();
					}

					// TAA sub-pixel jitter - read via the main-thread-safe mirror, not
					// m_TaaEnabled directly, since that's only safe to touch from inside
					// RenderAsync (see its own comment). Zero whenever TAA is off, which also
					// makes jitterDelta below come out exactly zero - a full, clean bypass.
					const Vector2 jitterNDC = m_TaaEnabledMainThread
						? ComputeJitterNDC(m_FrameNumber, viewportWidth, viewportHeight)
						: Vector2::c_Zero;
					// clip.xy += jitterNDC * clip.w, expressed as columns since clip = v * M
					// (row-vector convention) means clip.x = dot(v, column0(M)) and
					// clip.w = dot(v, column3(M)).
					sceneInfo.viewProj.SetColumn(0, sceneInfo.viewProj.GetColumn4D(0) + sceneInfo.viewProj.GetColumn4D(3) * jitterNDC.x);
					sceneInfo.viewProj.SetColumn(1, sceneInfo.viewProj.GetColumn4D(1) + sceneInfo.viewProj.GetColumn4D(3) * jitterNDC.y);

					// Deferred lighting reconstructs world position from depth using this - must
					// be the jittered viewProj's own inverse, since depth was written using that
					// same jittered matrix.
					sceneInfo.invViewProj = sceneInfo.viewProj.Inverse();
					sceneInfo.camPos = sv.camera.position;

					// Gribb-Hartmann frustum plane extraction, adapted for this engine's row-vector
					// convention (v * M, not M * v) - planes come from viewProj's columns, not rows.
					// Each plane satisfies dot(worldPos,abc)+d >= 0 for "inside".
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

					// On the very first frame there is no real previous frame, so use this frame's
					// own viewProj - motion vectors come out exactly zero rather than reading
					// identity/garbage.
					sceneInfo.prevViewProj = m_FirstRender ? sceneInfo.viewProj : m_PrevViewProj;
					m_PrevViewProj = sceneInfo.viewProj;

					// Same first-frame reasoning as prevViewProj above - no real previous jitter
					// yet, so the delta comes out exactly zero instead of reading garbage.
					sceneInfo.jitterDelta = m_FirstRender ? Vector2::c_Zero : (jitterNDC - m_PrevJitterNDC);
					m_PrevJitterNDC = jitterNDC;
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
					// renders at a time, so there's no per-scene slot to index into here.
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
			constexpr uint bindingCount = 17;
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
			SetBinding(16, TYR_BINDING_SHADOW_LIGHT_SLOT_MAP, m_Resources.shadowLightSlotMapBuffer);

			m_Ctx.device->UpdateDescriptorSet(m_Resources.descriptorSet, bindingUpdates, bindingCount);

			// The one material sampler every texture is read with - bound once here, like the
			// buffers above, rather than per-texture, since the pixel shader always indexes
			// samplers[0] regardless of which texture it's sampling.
			ImageBindingInfo samplerBindingInfo{};
			samplerBindingInfo.sampler = m_Resources.materialSampler;
			samplerBindingInfo.hasSampler = true;

			ImageBindingUpdate samplerUpdate{};
			samplerUpdate.bindingIndex = TYR_BINDING_SAMPLERS;
			samplerUpdate.imageBindingInfos = &samplerBindingInfo;
			samplerUpdate.infoCount = 1;

			m_Ctx.device->UpdateDescriptorSet(m_Resources.descriptorSet, nullptr, 0, &samplerUpdate, 1);
		}

		// Everything queued this tick goes to the device in one batched call here, before
		// RenderAsync is dispatched below - a worker thread could start executing the moment the
		// task is created, so every queued write needs to have already landed before that point.
		FlushDescriptorUpdates();

		// A swap chain only guarantees one image held acquired-without-presenting at minimum, not
		// the full buffered-frame depth - wait for the previous frame's present to have been
		// issued first, keeping at most one frame's image in that state at a time.
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
		// allocation call this tick is going to make has already happened, and before the next
		// tick gets a chance to allocate anything more.
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

		// Whatever's still sitting in this slot is guaranteed already finished by the time
		// m_RenderFrameIndex reaches renderFrameIndex again - release it now so it isn't
		// overwritten below without ever being released.
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
		// FlushDescriptorUpdates, right before this array is actually read.
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

		// Point each update at its matching info entry now that the info array is done growing
		// for this tick - doing this earlier risks a dangling pointer if a later call in the
		// same tick triggers a reallocation.
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

		// Not const - the active-instance-index upload below is scheduled here since
		// scene.content.meshInstances isn't finalized until the merge loop just below has run.
		// Safe to mutate: nothing else touches this slot's data concurrently.
		RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];
		FrameContext& frameCtx = m_Ctx.frameContexts[renderFrameIndex];

		if (renderFrame.hasQualityLevelOverride)
		{
			m_QualitySettings = ResolveQualitySettings(renderFrame.qualityLevelOverride);
		}
		if (renderFrame.hasTaaEnabledOverride)
		{
			m_TaaEnabled = renderFrame.taaEnabledOverride;
		}

		m_Data.activeScene = renderFrame.activeScene;

		if (!m_Data.activeScene)
		{
			// Nothing to merge/cull/draw, and no window to acquire/present to - but this slot's
			// already-queued asset/frame buffer uploads still need to reach the GPU, so still run
			// the transfer-only path rather than stranding them unsubmitted.
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
		// other. Present is issued from the submission thread and this runs on any worker-pool
		// thread, so acquiring here directly would race the two on the same swap chain.
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

		// A failed acquire leaves no valid image - swapChainImageIndex must not be touched in
		// that case. Either way, a resize request was already turned into a notification
		// elsewhere, so nothing further is needed here.
		const bool hasValidSwapChainImage = acquireResult->valid;
		if (hasValidSwapChainImage)
		{
			window.swapChainImageIndex = acquireResult->imageIndex;
		}

#if TYR_RENDER_DEBUG
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

		// GPU-driven culling needs this frame's active mesh instance pool indices - the pool is
		// shared across scenes, so a naive "cull everything" dispatch would draw other scenes'
		// data too. Uploaded via this worker thread's own fixed staging slot.
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

			// The culling compute shader atomically increments this from 0 - without resetting it
			// every frame, fewer visible instances than last frame would still redraw stale
			// entries via the indirect draw.
			uint zero = 0;
			RenderResourceUtil::WriteUploadBuffer(stagingBuffer, *m_Ctx.device, drawCountOffset, &zero, sizeof(uint));

			BufferUploadRequest& countRequest = m_Data.workerUploadRequests.ExpandOne();
			countRequest.srcBuffer = m_Resources.rtCullingStagingBuffer;
			countRequest.srcOffset = drawCountOffset;
			countRequest.dstBuffer = m_Resources.drawCountBuffer;
			countRequest.dstOffset = 0;
			countRequest.size = sizeof(uint);
		}

		// Rebuild the TLAS's instance buffer from this same active list. An instance whose mesh
		// hasn't finished its one-time BLAS build yet is skipped - it simply doesn't
		// cast/receive shadows for its first few frames.
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
					if (!mesh.blas.accelerationStructure)
					{
						continue;
					}

					AccelerationStructureInstance& out = instances[tlasInstanceCount++];
					// VkAccelerationStructureInstanceKHR's transform is row-major 3x4 for a
					// column-vector transform, the transpose of this engine's row-vector Matrix4 -
					// GetColumn4D(r) of the engine matrix gives exactly VK row r's 4 components.
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
					out.accelerationStructureReference = m_Ctx.device->GetAccelerationStructureDeviceAddress(mesh.blas.accelerationStructure);
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
					// upload to its own slot.
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

		// No merge needed for the upload request lists - TransferPass reads them straight off
		// RenderFrame.
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
		// copy - safe from a worker thread since nothing else touches this slot's data
		// concurrently.
		RenderViewportTextureData* viewportData = GetActiveViewportTextureData(renderFrameIndex);
		if (!viewportData)
		{
#if TYR_RENDER_DEBUG
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

		// EditorViewport hasn't requested a render target size yet (e.g. the very first few
		// frames, before any ImGui layout has happened) - nothing to render into.
		if (viewportWidth == 0 || viewportHeight == 0)
		{
#if TYR_RENDER_DEBUG
			TYR_LOG_WARNING("[DBG] RecordGeometryPass: SKIPPED (zero size) renderFrameIndex=%u width=%u height=%u",
				renderFrameIndex, viewportWidth, viewportHeight);
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
		// Reverse-Z - 0 represents "infinitely far".
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
			// dispatch's result into - one per buffered RenderFrame slot.
			uint renderFrameIndex;
		};

		// Matches TAAResolveCS.hlsl's push constant cbuffer byte-for-byte.
		struct TAAResolvePushConstants
		{
			uint colourIndex;
			uint motionIndex;
			uint prevResolvedIndex;
			uint width;
			uint height;
			uint renderFrameIndex;
			uint hasHistory;
			float historyBlendWeight;
		};

		// Matches EditorGridCS.hlsl's push constant cbuffer byte-for-byte.
		struct EditorGridPushConstants
		{
			Matrix4 invViewProj;
			uint sourceIndex;
			uint depthIndex;
			uint width;
			uint height;
			uint renderFrameIndex;
			uint majorLineEvery;
			float cellSize;
			float _pad0;
		};
	}

	void Renderer::RecordLightingPass(CommandList& cmdList, uint renderFrameIndex)
	{
		RenderViewportTextureData* viewportData = GetActiveViewportTextureData(renderFrameIndex);
		if (!viewportData)
		{
#if TYR_RENDER_DEBUG
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
			TYR_LOG_WARNING("[DBG] RecordLightingPass: SKIPPED (zero size) renderFrameIndex=%u width=%u height=%u",
				renderFrameIndex, viewportWidth, viewportHeight);
#endif
			return;
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
	}

	void Renderer::RecordTAAResolvePass(CommandList& cmdList, uint renderFrameIndex)
	{
		RenderViewportTextureData* viewportData = GetActiveViewportTextureData(renderFrameIndex);
		if (!viewportData || viewportData->width == 0 || viewportData->height == 0)
		{
			return;
		}

		// Same cross-slot history lookup "TAAResolve"'s own setup lambda already did in
		// BuildAndExecuteRenderGraph - redone here too rather than threaded through as capture
		// state, to keep this Execute lambda's own capture list small (see this function's own
		// declaration comment).
		const Scene& scene = m_Data.scenePool[m_Data.activeScene.h];
		const RenderViewport& parentViewport = m_RenderViewportPool[scene.renderViewport.h];
		const uint prevRenderFrameIndex = (renderFrameIndex + RenderConstants::c_BufferedFrameCount - 1) % RenderConstants::c_BufferedFrameCount;
		const RenderViewportTextureData& prevViewportData = parentViewport.textureData[prevRenderFrameIndex];
		const bool hasHistory = prevViewportData.resolvedColourTexture &&
			prevViewportData.width == viewportData->width && prevViewportData.height == viewportData->height;

		cmdList.BindComputePipeline(m_Resources.taaResolvePipeline);
		cmdList.BindDescriptorSet(m_Resources.descriptorSet, m_Resources.taaResolvePipeline);

		TAAResolvePushConstants pushConstants{};
		pushConstants.colourIndex = viewportData->colourTexture.h.index;
		pushConstants.motionIndex = viewportData->gbufferMotion.h.index;
		pushConstants.prevResolvedIndex = hasHistory ? prevViewportData.resolvedColourTexture.h.index : viewportData->resolvedColourTexture.h.index;
		pushConstants.width = viewportData->width;
		pushConstants.height = viewportData->height;
		pushConstants.renderFrameIndex = renderFrameIndex;
		pushConstants.hasHistory = hasHistory ? 1u : 0u;
		pushConstants.historyBlendWeight = m_QualitySettings.taaHistoryBlendWeight;
		cmdList.PushConstants(m_Resources.taaResolvePipeline, SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TAAResolvePushConstants), &pushConstants);

		const uint groupCountX = (viewportData->width + 7) / 8;
		const uint groupCountY = (viewportData->height + 7) / 8;
		cmdList.Dispatch(groupCountX, groupCountY, 1);
	}

	void Renderer::RecordEditorGridPass(CommandList& cmdList, uint renderFrameIndex, uint sourceIndex)
	{
		const RenderViewportTextureData* viewportData = GetActiveViewportTextureData(renderFrameIndex);
		if (!viewportData || viewportData->width == 0 || viewportData->height == 0)
		{
			return;
		}

		const RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];

		cmdList.BindComputePipeline(m_Resources.editorGridPipeline);
		cmdList.BindDescriptorSet(m_Resources.descriptorSet, m_Resources.editorGridPipeline);

		EditorGridPushConstants pushConstants{};
		pushConstants.invViewProj = renderFrame.gridInvViewProj;
		pushConstants.sourceIndex = sourceIndex;
		pushConstants.depthIndex = viewportData->depthBuffer.h.index;
		pushConstants.width = viewportData->width;
		pushConstants.height = viewportData->height;
		pushConstants.renderFrameIndex = renderFrameIndex;
		pushConstants.majorLineEvery = renderFrame.viewportGrid.majorLineEvery;
		pushConstants.cellSize = renderFrame.viewportGrid.cellSize;
		cmdList.PushConstants(m_Resources.editorGridPipeline, SHADER_STAGE_COMPUTE_BIT, 0, sizeof(EditorGridPushConstants), &pushConstants);

		const uint groupCountX = (viewportData->width + 7) / 8;
		const uint groupCountY = (viewportData->height + 7) / 8;
		cmdList.Dispatch(groupCountX, groupCountY, 1);
	}

	void Renderer::SetupBLASBuildPass(RenderGraphBuilder& builder, uint renderFrameIndex)
	{
		const RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];
		const PipelineStage asBuildStage = PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT;

		// This frame's BLAS builds read straight out of the global vertex/index buffers via GPU
		// address. Per spec, build input is read as SHADER_READ, not
		// ACCELERATION_STRUCTURE_READ - that access type is for reading an already-built AS.
		builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.vertexBuffer), asBuildStage, BARRIER_ACCESS_SHADER_READ_BIT);
		builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.indexBuffer), asBuildStage, BARRIER_ACCESS_SHADER_READ_BIT);

		for (const BLASBuildRecord& record : renderFrame.blasBuildsToRecord)
		{
			builder.WriteAccelerationStructure(*record.blasResource, asBuildStage, BARRIER_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT);
		}
	}

	void Renderer::RecordBLASBuildPass(CommandList& cmdList, uint renderFrameIndex)
	{
		const RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];

		// m_Resources.blasScratchBuffer is only ever created lazily, the first time a BLAS
		// actually needs building - on any earlier frame it's still an invalid handle. Only
		// touch it once we know there's at least one build this frame.
		for (const BLASBuildRecord& record : renderFrame.blasBuildsToRecord)
		{
			const BufferHandle blasScratchBuffer = m_Registry.GetBuffer(m_Resources.blasScratchBuffer).buffer;

			AccelerationStructureBuildInfo buildInfo;
			buildInfo.accelerationStructure = record.blas;
			buildInfo.scratchBuffer = blasScratchBuffer;
			buildInfo.scratchOffset = record.scratchOffset;
			cmdList.BuildAccelerationStructures(&buildInfo, 1);
		}
	}

	void Renderer::SetupTLASBuildPass(RenderGraphBuilder& builder, uint renderFrameIndex)
	{
		const RenderFrame& renderFrame = m_RenderFrames[renderFrameIndex];
		const PipelineStage asBuildStage = PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT;

		builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.tlasInstanceBuffer), asBuildStage, BARRIER_ACCESS_SHADER_READ_BIT);

		// Every BLAS this tick's build batch touched must be visible before the TLAS build below
		// reads it - declared here (rather than a manual barrier) so a future pass (e.g. a
		// shadow/reflection pass reading the TLAS) just adds its own ReadAccelerationStructure
		// the same way, with no barrier bookkeeping of its own to get right.
		for (const BLASBuildRecord& record : renderFrame.blasBuildsToRecord)
		{
			builder.ReadAccelerationStructure(*record.blasResource, asBuildStage, BARRIER_ACCESS_ACCELERATION_STRUCTURE_READ_BIT);
		}
		builder.WriteAccelerationStructure(m_Resources.tlas[renderFrameIndex], asBuildStage, BARRIER_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT);
	}

	void Renderer::RecordTLASBuildPass(CommandList& cmdList, uint renderFrameIndex)
	{
		// Full rebuild every frame - refitting (only valid for topology-unchanged, transform-only
		// updates) is a possible later optimization, not needed for correctness now.
		AccelerationStructureBuildInfo tlasBuildInfo;
		tlasBuildInfo.accelerationStructure = m_Resources.tlas[renderFrameIndex].accelerationStructure;
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

		RenderingInfo renderingInfo{};
		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extents = { windowWidth, windowHeight };
		renderingInfo.viewMask = 0;
		renderingInfo.layerCount = 1;

		RenderingAttachmentInfo colourAttachment;
		// Clear, not Load - the 3D scene lives in its own offscreen texture, shown inside the
		// Viewport panel separately.
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

		// Only valid (non-null) when hasActiveScene - never true without an active scene, and
		// hasValidSwapChainImage is never true without hasActiveScene either. Pointers, not
		// references, so there's a legitimate unset state for the no-active-scene case.
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

		// Every buffer TransferPass might write and GeometryPass reads - both run on the
		// graphics queue for now, so this just needs a same-queue pipeline barrier between them,
		// no cross-queue semaphore wait.
		const RenderBufferHandle graphBuffers[] = {
			m_Resources.meshBuffer, m_Resources.meshLODBuffer, m_Resources.meshletBuffer,
			m_Resources.vertexBuffer, m_Resources.indexBuffer, m_Resources.meshInstanceBuffer,
			m_Resources.materialBuffer, m_Resources.directionalLightBuffer, m_Resources.pointLightBuffer,
			m_Resources.spotLightBuffer, m_Resources.sceneInfoBuffer,
			m_Resources.activeMeshInstanceIndexBuffer, m_Resources.visibleInstanceIndexBuffer,
			m_Resources.indirectDrawCommandBuffer, m_Resources.drawCountBuffer,
			m_Resources.tlasInstanceBuffer, m_Resources.shadowLightSlotMapBuffer
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

		// Every texture in this tick's upload requests is freshly created and going through
		// this path for the first time - force UNKNOWN so the graph's first-touch logic below
		// produces the real UNDEFINED->GENERAL transition instead of a same-layout no-op.
		for (const TextureUploadRequest& request : renderFrame.textureUploadRequests)
		{
			Texture& uploadTexture = m_Registry.GetTexture(request.dstTexture);
			uploadTexture.imageLayout = IMAGE_LAYOUT_UNKNOWN;
			graph.RegisterTexture(&uploadTexture);
		}

		graph.AddPass("Transfer",
			[this, &renderFrame](RenderGraphBuilder& builder) { m_TransferPass->Setup(builder, renderFrame); },
			[this, renderFrameIndex](CommandList& cl) { RecordTransferPass(cl, renderFrameIndex); },
			RenderGraphPhase::Transfer, CommandQueueType::CQ_GRAPHICS);

		// Nothing to draw into or present without a valid acquired image. The transfer pass
		// above still runs regardless, since it's not tied to the window.
		if (hasValidSwapChainImage)
		{
			// Resolved once here and reused for registration below and for the pass setup
			// lambdas further down - null or zero-sized exactly when RecordGeometryPass/
			// RecordLightingPass would themselves skip (no RenderViewport yet, or not sized).
			RenderViewportTextureData* viewportData = GetActiveViewportTextureData(renderFrameIndex);
			const bool hasViewportTextures = viewportData && viewportData->width != 0 && viewportData->height != 0;

			Texture* gbufferAlbedoAO = nullptr;
			Texture* gbufferNormalRoughMetal = nullptr;
			Texture* gbufferMotion = nullptr;
			Texture* depthBuffer = nullptr;
			Texture* colourTexture = nullptr;
			Texture* shadowMasksRaw = nullptr;
			Texture* shadowMasks = nullptr;
			Texture* resolvedColourTexture = nullptr;
			Texture* overlayColourTexture = nullptr;

			if (hasViewportTextures)
			{
				// Checked before isNew is cleared below, since the overlay can also be new on its own.
				if (renderFrame.viewportGrid.enabled && viewportData->overlayColourTexture)
				{
					overlayColourTexture = &m_Registry.GetTexture(viewportData->overlayColourTexture);
					if (viewportData->isNew || viewportData->overlayIsNew)
					{
						overlayColourTexture->imageLayout = IMAGE_LAYOUT_UNKNOWN;
						viewportData->overlayIsNew = false;
					}
					graph.RegisterTexture(overlayColourTexture);
				}

				gbufferAlbedoAO = &m_Registry.GetTexture(viewportData->gbufferAlbedoAO);
				gbufferNormalRoughMetal = &m_Registry.GetTexture(viewportData->gbufferNormalRoughMetal);
				gbufferMotion = &m_Registry.GetTexture(viewportData->gbufferMotion);
				depthBuffer = &m_Registry.GetTexture(viewportData->depthBuffer);
				colourTexture = &m_Registry.GetTexture(viewportData->colourTexture);
				shadowMasksRaw = &m_Registry.GetTexture(viewportData->shadowMasksRaw);
				shadowMasks = &m_Registry.GetTexture(viewportData->shadowMasks);
				resolvedColourTexture = &m_Registry.GetTexture(viewportData->resolvedColourTexture);

				// A freshly (re)created texture's imageLayout is pre-set to its steady-state
				// value, not UNKNOWN, even though the real image always starts life as
				// UNDEFINED - force it to UNKNOWN here, once, so the render graph's own
				// first-touch logic below produces the real transition instead of a same-layout
				// no-op. Every later tick is unaffected, since the graph's own persisted state
				// is correct from here on.
				if (viewportData->isNew)
				{
					gbufferAlbedoAO->imageLayout = IMAGE_LAYOUT_UNKNOWN;
					gbufferNormalRoughMetal->imageLayout = IMAGE_LAYOUT_UNKNOWN;
					gbufferMotion->imageLayout = IMAGE_LAYOUT_UNKNOWN;
					depthBuffer->imageLayout = IMAGE_LAYOUT_UNKNOWN;
					colourTexture->imageLayout = IMAGE_LAYOUT_UNKNOWN;
					shadowMasksRaw->imageLayout = IMAGE_LAYOUT_UNKNOWN;
					shadowMasks->imageLayout = IMAGE_LAYOUT_UNKNOWN;
					resolvedColourTexture->imageLayout = IMAGE_LAYOUT_UNKNOWN;
					viewportData->isNew = false;
				}

				graph.RegisterTexture(gbufferAlbedoAO);
				graph.RegisterTexture(gbufferNormalRoughMetal);
				graph.RegisterTexture(gbufferMotion);
				graph.RegisterTexture(depthBuffer);
				graph.RegisterTexture(colourTexture);
				graph.RegisterTexture(shadowMasksRaw);
				graph.RegisterTexture(shadowMasks);
				graph.RegisterTexture(resolvedColourTexture);
			}

			Texture& swapChainImageProxy = window->swapChainImageProxies[window->swapChainImageIndex];
			swapChainImageProxy.Reset();
			swapChainImageProxy.imageLayout = IMAGE_LAYOUT_UNKNOWN;
			swapChainImageProxy.image = window->swapChain->GetImages()[window->swapChainImageIndex];
			swapChainImageProxy.info.mipCount = 1;
			swapChainImageProxy.info.arrayLayerCount = 1;
			graph.RegisterTexture(&swapChainImageProxy);

			// Same phase as "Geometry" below and added first, so this always executes (and has
			// its buffer usages recorded) before GeometryPass's own indirect draw reads what this
			// pass wrote.
			graph.AddPass("Culling",
				[this](RenderGraphBuilder& builder) { SetupCullingPass(builder); },
				[this, renderFrameIndex](CommandList& cl) { RecordCullingPass(cl, renderFrameIndex); },
				RenderGraphPhase::Geometry, CommandQueueType::CQ_GRAPHICS);

			graph.AddPass("Geometry",
				[this, gbufferAlbedoAO, gbufferNormalRoughMetal, gbufferMotion, depthBuffer, &renderFrame](RenderGraphBuilder& builder)
				{
					m_GeometryPass->Setup(builder, renderFrame);
					if (!gbufferAlbedoAO)
						return;
					builder.WriteTexture(*gbufferAlbedoAO, PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, IMAGE_LAYOUT_GENERAL);
					builder.WriteTexture(*gbufferNormalRoughMetal, PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, IMAGE_LAYOUT_GENERAL);
					builder.WriteTexture(*gbufferMotion, PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, IMAGE_LAYOUT_GENERAL);
					builder.WriteTexture(*depthBuffer, PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, IMAGE_LAYOUT_GENERAL);
				},
				[this, renderFrameIndex](CommandList& cl) { RecordGeometryPass(cl, renderFrameIndex); },
				RenderGraphPhase::Geometry, CommandQueueType::CQ_GRAPHICS);

			// Every BLAS this tick's build batch will touch, plus the TLAS rebuilt every tick -
			// registered here so both passes' usage declarations below have a valid index.
			// Read by "ShadowRT" below, and a future reflection pass could add its own
			// ReadAccelerationStructure the same way.
			for (const BLASBuildRecord& record : renderFrame.blasBuildsToRecord)
			{
				graph.RegisterAccelerationStructure(record.blasResource);
			}
			graph.RegisterAccelerationStructure(&m_Resources.tlas[renderFrameIndex]);

			// Builds at most one pending mesh's BLAS.
			graph.AddPass("BLASBuild",
				[this, renderFrameIndex](RenderGraphBuilder& builder) { SetupBLASBuildPass(builder, renderFrameIndex); },
				[this, renderFrameIndex](CommandList& cl) { RecordBLASBuildPass(cl, renderFrameIndex); },
				RenderGraphPhase::RayTracing, CommandQueueType::CQ_GRAPHICS);

			// Rebuilds the TLAS from this frame's active instances - registered after "BLASBuild"
			// in the same phase, so the render graph's own barrier for any BLAS built above lands
			// before this pass runs.
			graph.AddPass("TLASBuild",
				[this, renderFrameIndex](RenderGraphBuilder& builder) { SetupTLASBuildPass(builder, renderFrameIndex); },
				[this, renderFrameIndex](CommandList& cl) { RecordTLASBuildPass(cl, renderFrameIndex); },
				RenderGraphPhase::RayTracing, CommandQueueType::CQ_GRAPHICS);

			// DeferredLightingCS's light-index -> shadow-slot lookup for this tick - defaults to
			// "nothing is shadowed" (c_ShadowSlotNone), filled in below once ShadowRTPass's
			// selection is known. Uploaded unconditionally after, including the all-default case
			// (e.g. no active viewport this tick), so a stale assignment from an earlier tick
			// never lingers.
			constexpr uint c_ShadowSlotNone = 0xFFFFFFFFu;
			uint shadowLightSlotMap[RenderConstants::c_ShadowLightSlotMapEntryCount];
			for (uint& slot : shadowLightSlotMap)
			{
				slot = c_ShadowSlotNone;
			}

			// Ray-traced shadow visibility for this tick's shadow-casting lights - registered
			// after "TLASBuild" in the same phase, so the render graph's barrier for this tick's
			// TLAS rebuild lands before this pass's own ReadAccelerationStructure runs.
			if (hasViewportTextures && !renderFrame.sceneFrame.views.IsEmpty())
			{
				Scene& shadowScene = m_Data.scenePool[m_Data.activeScene.h];
				const Vector3 cameraPosition = renderFrame.sceneFrame.views[0].camera.position;
				const TextureHandle shadowDepthBuffer = viewportData->depthBuffer;
				const TextureHandle shadowGBufferNormalRoughMetal = viewportData->gbufferNormalRoughMetal;
				const TextureHandle shadowMasksRawHandle = viewportData->shadowMasksRaw;
				const uint shadowViewportWidth = viewportData->width;
				const uint shadowViewportHeight = viewportData->height;

				graph.AddPass("ShadowRT",
					[this, &shadowScene, cameraPosition, shadowDepthBuffer, shadowGBufferNormalRoughMetal, shadowMasksRawHandle, renderFrameIndex](RenderGraphBuilder& builder)
					{
						m_ShadowRTPass->Setup(builder, shadowScene, cameraPosition, m_QualitySettings, renderFrameIndex,
							shadowDepthBuffer, shadowGBufferNormalRoughMetal, shadowMasksRawHandle);
					},
					[this, renderFrameIndex, shadowViewportWidth, shadowViewportHeight](CommandList& cl)
					{
						m_ShadowRTPass->Execute(cl, renderFrameIndex, shadowViewportWidth, shadowViewportHeight);
					},
					RenderGraphPhase::RayTracing, CommandQueueType::CQ_GRAPHICS);

				// Denoise of this tick's raw shadow trace - registered right after "ShadowRT" in
				// the same phase, so the render graph's barrier for its writes lands before this
				// pass's reads. The previous buffered viewport slot's own shadowMasks texture (a
				// distinct resource from this tick's) is this tick's temporal history, the same
				// cross-slot-reuse pattern planned for TAA's own colour history.
				const RenderViewport& parentViewport = m_RenderViewportPool[shadowScene.renderViewport.h];
				const uint prevRenderFrameIndex = (renderFrameIndex + RenderConstants::c_BufferedFrameCount - 1) % RenderConstants::c_BufferedFrameCount;
				const RenderViewportTextureData& prevViewportData = parentViewport.textureData[prevRenderFrameIndex];
				const bool shadowHasHistory = prevViewportData.shadowMasks &&
					prevViewportData.width == shadowViewportWidth && prevViewportData.height == shadowViewportHeight;
				const TextureHandle shadowGBufferMotion = viewportData->gbufferMotion;
				const TextureHandle shadowMasksHandle = viewportData->shadowMasks;
				const TextureHandle prevShadowMasksHandle = prevViewportData.shadowMasks;

				// A different buffered slot's own texture - never registered with this tick's
				// graph otherwise, since nothing else here reads or writes it.
				if (shadowHasHistory)
				{
					graph.RegisterTexture(&m_Registry.GetTexture(prevShadowMasksHandle));
				}

				graph.AddPass("ShadowDenoise",
					[this, shadowDepthBuffer, shadowGBufferMotion, shadowMasksRawHandle, shadowMasksHandle, prevShadowMasksHandle, shadowHasHistory](RenderGraphBuilder& builder)
					{
						m_ShadowDenoisePass->Setup(builder, shadowDepthBuffer, shadowGBufferMotion,
							shadowMasksRawHandle, shadowMasksHandle, prevShadowMasksHandle, shadowHasHistory);
					},
					[this, renderFrameIndex, prevRenderFrameIndex, shadowHasHistory, shadowViewportWidth, shadowViewportHeight](CommandList& cl)
					{
						m_ShadowDenoisePass->Execute(cl, m_ShadowRTPass->GetActiveSlots(), renderFrameIndex, prevRenderFrameIndex,
							shadowHasHistory, shadowViewportWidth, shadowViewportHeight, m_QualitySettings.denoiserSpatialRadius);
					},
					RenderGraphPhase::RayTracing, CommandQueueType::CQ_GRAPHICS);

				// Directional lights need no entry here - a directional light's own pool index
				// already is its shadow slot (see ShadowRTPass), so only point/spot need a real
				// lookup.
				for (const ShadowRTPass::SelectedLocalLight& selected : m_ShadowRTPass->GetSelectedLocalLights())
				{
					const uint mapIndex = selected.isSpot ? (RenderConstants::c_MaxPointLights + selected.lightIndex) : selected.lightIndex;
					shadowLightSlotMap[mapIndex] = selected.slot;
				}
			}

			{
				// Worker-thread-built, same staging mechanism the GPU-driven-culling data above
				// uses (RendererAPI's shared upload allocator is main-thread-only).
				const size_t stagingSlotBase = (size_t)renderFrameIndex * c_RTCullingStagingSlotSize;
				const size_t shadowSlotMapOffset = stagingSlotBase + c_ActiveInstanceIndicesBytes + c_DrawCountResetBytes + c_TlasInstancesBytes;
				RenderBuffer& stagingBuffer = m_Registry.GetBuffer(m_Resources.rtCullingStagingBuffer);
				RenderResourceUtil::WriteUploadBuffer(stagingBuffer, *m_Ctx.device, shadowSlotMapOffset, shadowLightSlotMap, sizeof(shadowLightSlotMap));

				BufferUploadRequest& request = m_Data.workerUploadRequests.ExpandOne();
				request.srcBuffer = m_Resources.rtCullingStagingBuffer;
				request.srcOffset = shadowSlotMapOffset;
				request.dstBuffer = m_Resources.shadowLightSlotMapBuffer;
				request.dstOffset = 0;
				request.size = sizeof(shadowLightSlotMap);
			}

			// Reads the G-buffer/depth GeometryPass just wrote and writes the shaded result into
			// the viewport colour texture. No new buffer barrier needed for the read-after-read
			// on scene-info/lights.
			graph.AddPass("Lighting",
				[this, gbufferAlbedoAO, gbufferNormalRoughMetal, depthBuffer, colourTexture, shadowMasks](RenderGraphBuilder& builder)
				{
					if (!gbufferAlbedoAO)
						return;
					builder.ReadTexture(*gbufferAlbedoAO, PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT, IMAGE_LAYOUT_GENERAL);
					builder.ReadTexture(*gbufferNormalRoughMetal, PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT, IMAGE_LAYOUT_GENERAL);
					builder.ReadTexture(*depthBuffer, PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT, IMAGE_LAYOUT_GENERAL);
					builder.WriteTexture(*colourTexture, PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_WRITE_BIT, IMAGE_LAYOUT_GENERAL);
					if (shadowMasks)
					{
						// Written by "ShadowDenoise" above - this is what makes the shadow result
						// actually visible to this pass's reads, not just computed.
						builder.ReadTexture(*shadowMasks, PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT, IMAGE_LAYOUT_GENERAL);
					}
					builder.ReadBuffer(m_Registry.GetBuffer(m_Resources.shadowLightSlotMapBuffer), PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT);
				},
				[this, renderFrameIndex](CommandList& cl) { RecordLightingPass(cl, renderFrameIndex); },
				RenderGraphPhase::Post, CommandQueueType::CQ_GRAPHICS);

			// TAA resolve - optional (m_TaaEnabled, independent of quality level so it can be
			// isolated on its own). Whichever texture it leaves as the "real" result this tick is
			// what "GUI" below actually reads and what the editor displays.
			Texture* taaDisplayTexture = colourTexture;
			if (m_TaaEnabled && colourTexture)
			{
				const Scene& taaScene = m_Data.scenePool[m_Data.activeScene.h];
				const RenderViewport& parentViewport = m_RenderViewportPool[taaScene.renderViewport.h];
				const uint prevRenderFrameIndex = (renderFrameIndex + RenderConstants::c_BufferedFrameCount - 1) % RenderConstants::c_BufferedFrameCount;
				const RenderViewportTextureData& prevViewportData = parentViewport.textureData[prevRenderFrameIndex];
				const bool taaHasHistory = prevViewportData.resolvedColourTexture &&
					prevViewportData.width == viewportData->width && prevViewportData.height == viewportData->height;
				const TextureHandle taaPrevResolvedColourTextureHandle = prevViewportData.resolvedColourTexture;

				// A different buffered slot's own texture - never registered with this tick's
				// graph otherwise, the same reasoning as the shadow denoiser's own history read.
				if (taaHasHistory)
				{
					graph.RegisterTexture(&m_Registry.GetTexture(taaPrevResolvedColourTextureHandle));
				}

				graph.AddPass("TAAResolve",
					[this, colourTexture, gbufferMotion, resolvedColourTexture, taaPrevResolvedColourTextureHandle, taaHasHistory](RenderGraphBuilder& builder)
					{
						builder.ReadTexture(*colourTexture, PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT, IMAGE_LAYOUT_GENERAL);
						builder.ReadTexture(*gbufferMotion, PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT, IMAGE_LAYOUT_GENERAL);
						builder.WriteTexture(*resolvedColourTexture, PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_WRITE_BIT, IMAGE_LAYOUT_GENERAL);
						if (taaHasHistory)
						{
							builder.ReadTexture(m_Registry.GetTexture(taaPrevResolvedColourTextureHandle), PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT, IMAGE_LAYOUT_GENERAL);
						}
					},
					[this, renderFrameIndex](CommandList& cl) { RecordTAAResolvePass(cl, renderFrameIndex); },
					RenderGraphPhase::Post, CommandQueueType::CQ_GRAPHICS);

				taaDisplayTexture = resolvedColourTexture;
			}

			// Drawn after TAA into its own texture, so the grid never ends up in TAA's history.
			Texture* displayTexture = taaDisplayTexture;
			if (overlayColourTexture && taaDisplayTexture)
			{
				const uint gridSourceIndex = taaDisplayTexture == resolvedColourTexture
					? viewportData->resolvedColourTexture.h.index
					: viewportData->colourTexture.h.index;

				graph.AddPass("EditorGrid",
					[taaDisplayTexture, depthBuffer, overlayColourTexture](RenderGraphBuilder& builder)
					{
						builder.ReadTexture(*taaDisplayTexture, PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT, IMAGE_LAYOUT_GENERAL);
						builder.ReadTexture(*depthBuffer, PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT, IMAGE_LAYOUT_GENERAL);
						builder.WriteTexture(*overlayColourTexture, PIPELINE_STAGE_COMPUTE_SHADER_BIT, BARRIER_ACCESS_SHADER_WRITE_BIT, IMAGE_LAYOUT_GENERAL);
					},
					[this, renderFrameIndex, gridSourceIndex](CommandList& cl) { RecordEditorGridPass(cl, renderFrameIndex, gridSourceIndex); },
					RenderGraphPhase::Post, CommandQueueType::CQ_GRAPHICS);

				displayTexture = overlayColourTexture;
			}

			// Always added, even on a frame with nothing to draw - this is the only pass that
			// touches the swap chain image.
			graph.AddPass("GUI",
				[this, displayTexture, &swapChainImageProxy, window, &renderFrame](RenderGraphBuilder& builder)
				{
					m_GUIPass->Setup(builder, renderFrame);
					if (displayTexture)
						builder.ReadTexture(*displayTexture, PIPELINE_STAGE_FRAGMENT_SHADER_BIT, BARRIER_ACCESS_SHADER_READ_BIT, IMAGE_LAYOUT_GENERAL);
					builder.WriteTexture(swapChainImageProxy, PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, window->swapChain->GetRenderingWriteAccess(), window->swapChain->GetRenderingLayout());
				},
				[this, renderFrameIndex](CommandList& cl) { RecordGUIPass(cl, renderFrameIndex); },
				RenderGraphPhase::Output, CommandQueueType::CQ_GRAPHICS);

			// Otherwise-empty - its only purpose is declaring the swap chain image's final
			// per-tick layout, so the render graph transitions it to present-source the same way
			// it would any other resource. Registered right after "GUI" in the same phase, which
			// executes in registration order, so this always runs last.
			graph.AddPass("Present",
				[&swapChainImageProxy](RenderGraphBuilder& builder)
				{
					builder.WriteTexture(swapChainImageProxy, PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, BARRIER_ACCESS_NONE, IMAGE_LAYOUT_PRESENT_SRC);
				},
				[](CommandList&) {},
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

		// Always enqueued, even with no active scene/window at all or a failed acquire - the
		// completion thread still needs to record this frameNumber as "handled" so the pacing
		// wait doesn't stall. Present itself is skipped when present is false.
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

			// Safe to read - this slot can't be reused until this completion drains first.
			// Resource-upload-backed requests get signalled so that memory can be reclaimed once
			// the GPU catches up.
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

		// RenderAsync reads this slot's RenderFrame by reference on a worker thread - wait for
		// its task here before this slot's data gets touched/cleared below. A GPU semaphore wait
		// can't substitute for this, since its value stays 0 until the task actually submits.
		TaskID& slotTask = m_RenderAsyncTasks[m_RenderFrameIndex];
		// Captured before the release logic below can clear slotTask back to c_InvalidTaskID -
		// this is the one true way to know whether this slot was ever actually used, since a
		// completionTimelineValue of 0 is ambiguous (see the wait further down).
		const bool slotWasUsed = (slotTask != c_InvalidTaskID);

		if (slotWasUsed)
		{
			TaskScheduler::Instance().WaitOnTask(slotTask);

			// A tick can be skipped without creating a task, so a run of skipped ticks can bring
			// us back to this slot while its task is still the one the next real tick needs to
			// depend on - only release it once a newer task has taken over that role.
			if (slotTask != m_PrevRenderAsyncTask)
			{
				TaskScheduler::Instance().ReleaseTask(slotTask);
				slotTask = c_InvalidTaskID;
			}
		}

		const SemaphoreHandle graphicsTimelineSemaphore = m_Ctx.graphicsQueue->GetTimelineSemaphore();

		// This slot was last used 3 frames ago - wait for that frame's GPU work to finish first.
		// Completion reports arrive asynchronously, so check against the exact frame number this
		// use was submitted under, not just a nonzero value, to avoid matching a stale report.
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

		// Resource uploads share the graphics queue for now too, in the absence of a transfer
		// queue. Every upload allocation still live at this point was already signalled above,
		// as soon as its timeline value was known.
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
			// A mesh whose one-time BLAS build already ran owns an acceleration structure backed
			// by a range of the shared storage buffer - both must be freed here, or the
			// acceleration structure leaks and its storage range is never reclaimed.
			if (mesh.blas.accelerationStructure)
			{
				m_Ctx.device->DeleteAccelerationStructure(mesh.blas.accelerationStructure);
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
		// The last RenderAsync task might still be running - wait for it before tearing anything
		// down. Every earlier task depended on the one before it, so this alone guarantees all
		// of them are done too.
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

		// No WaitIdle() here: the last task finishing only means its work was enqueued to the
		// submission thread, not that it's actually been issued yet - calling it from this
		// thread while that one might still be submitting is a Vulkan threading violation.
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
		// The swap chain must have as many images as the renderer keeps frames in flight, or it
		// runs out of images to acquire before earlier ones are done with.
		swapChainDesc.minImageCount = RenderConstants::c_BufferedFrameCount;

		// Always a fresh swap chain - never reuses one from a just-removed window at the same
		// pool slot, since a deferred delete of the old one could still be pending.
		SwapChain* swapChain = m_Ctx.device->CreateSwapChain(osHandle, swapChainDesc);
		renderWindow.swapChain = swapChain;

		// Binary semaphores used to sync swapchain image acquisition/presentation with command
		// submission - always need recreating here regardless of whether the swap chain itself
		// was fresh or reused.
		SemaphoreDesc semaphoreDesc;

		// Indexed by frame-in-flight - needed before AcquireNextImage returns an image index.
		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
#if !TYR_FINAL
			semaphoreDesc.debugName = "AcquireSwapChainImageSemaphore";
#endif
			renderWindow.frames[i].aquireSwapChainImageSemaphore = m_Ctx.device->CreateSemaphoreResource(semaphoreDesc);
		}

		// Indexed by swap chain image index instead - frame-in-flight indexing isn't safe for
		// this one.
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
		// Deferred the same way other resource removals are - a buffered slot's textures might
		// still be read by a worker thread, or still in-flight on the GPU, from frames ago.
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

		// Same bindless descriptor write pattern other texture creation uses.
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

	TextureHandle Renderer::CreateShadowMaskArrayTexture(const char* debugName, uint width, uint height)
	{
		TextureDesc desc;
		desc.debugName = debugName;
		desc.info.width = width;
		desc.info.height = height;
		desc.info.depth = 1;
		desc.info.arrayLayerCount = RenderConstants::c_MaxShadowSlots;
		desc.info.mipCount = 1;
		// No single-channel storage format exists in PixelFormat yet - reusing this already-
		// proven two-channel format (also used for gbufferMotion) and only ever touching the R
		// channel, rather than adding a new format for one feature.
		desc.info.format = PixelFormat::PF_R16G16_SFLOAT;
		desc.info.type = ImageType::Image2DArray;
		desc.sampleCount = SampleCount::OneBit;
		desc.usage = static_cast<ImageUsage>(IMAGE_USAGE_STORAGE_BIT);
		desc.layout = ImageLayout::IMAGE_LAYOUT_GENERAL;

		const TextureHandle handle = m_Registry.CreateTexture(desc);
		GetRenderFrame().texturesToAdd.Add(handle);
		return handle;
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
			DeleteViewportTargetTexture(targets.shadowMasksRaw);
			DeleteViewportTargetTexture(targets.shadowMasks);
			DeleteViewportTargetTexture(targets.resolvedColourTexture);
		}
		if (targets.overlayColourTexture)
		{
			DeleteViewportTargetTexture(targets.overlayColourTexture);
			targets.overlayColourTexture = {};
		}

		// Written by the deferred lighting pass (a storage image), read by the GUI pass to
		// display it. UNORM, not SRGB - the storage-image format doesn't support SRGB on this
		// hardware, so the shader writes linear colour and the final sample does the sRGB encode.
		targets.colourTexture = CreateViewportTargetTexture(debugName, PixelFormat::PF_R8G8B8A8_UNORM,
			static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_STORAGE_BIT), width, height);

		// Must match the geometry pipeline's declared colour attachment formats - dynamic
		// rendering requires the two to agree.
		targets.gbufferAlbedoAO = CreateViewportTargetTexture("GBuffer AlbedoAO", PixelFormat::PF_R8G8B8A8_SRGB,
			static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_COLOUR_ATTACHMENT_BIT), width, height);
		targets.gbufferNormalRoughMetal = CreateViewportTargetTexture("GBuffer NormalRoughMetal", PixelFormat::PF_R16G16B16A16_SFLOAT,
			static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_COLOUR_ATTACHMENT_BIT), width, height);
		targets.gbufferMotion = CreateViewportTargetTexture("GBuffer Motion", PixelFormat::PF_R16G16_SFLOAT,
			static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_COLOUR_ATTACHMENT_BIT), width, height);
		// Reverse-Z - cleared to 0, compared Greater.
		targets.depthBuffer = CreateViewportTargetTexture("Depth Buffer", PixelFormat::PF_D32_SFLOAT,
			static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT), width, height);

		targets.shadowMasksRaw = CreateShadowMaskArrayTexture("Shadow Masks Raw", width, height);
		targets.shadowMasks = CreateShadowMaskArrayTexture("Shadow Masks", width, height);

		// Same usage as colourTexture above - written by a compute pass, sampled by the editor
		// for display.
		targets.resolvedColourTexture = CreateViewportTargetTexture("Resolved Colour", PixelFormat::PF_R8G8B8A8_UNORM,
			static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_STORAGE_BIT), width, height);

		targets.width = width;
		targets.height = height;
		targets.isNew = true;

		SyncViewportOverlay(viewport, slot);
	}

	void Renderer::SyncViewportOverlay(RenderViewport& viewport, uint slot)
	{
		RenderViewportTextureData& targets = viewport.textureData[slot];
		const bool wanted = viewport.grid.enabled && targets.colourTexture;
		if (wanted == static_cast<bool>(targets.overlayColourTexture))
		{
			return;
		}

		if (wanted)
		{
			// Same usage as resolvedColourTexture - written by a compute pass, sampled for display.
			targets.overlayColourTexture = CreateViewportTargetTexture("Overlay Colour", PixelFormat::PF_R8G8B8A8_UNORM,
				static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_STORAGE_BIT), targets.width, targets.height);
			targets.overlayIsNew = true;
		}
		else
		{
			DeleteViewportTargetTexture(targets.overlayColourTexture);
			targets.overlayColourTexture = {};
		}
	}

	void Renderer::EnsureLightingOutputBound(uint renderFrameIndex, TextureHandle colourTexture)
	{
		if (!colourTexture || m_LightingOutputBoundTextures[renderFrameIndex] == colourTexture)
		{
			return;
		}

		// The lighting compute pass writes colourTexture directly, needing its own storage-image
		// descriptor write at this slot's own index. Checked every tick since switching scenes
		// can change which texture this slot should point at.
		const Texture& texture = m_Registry.GetTexture(colourTexture);
		ImageBindingInfo outputImageInfo;
		outputImageInfo.imageView = texture.imageView;
		outputImageInfo.hasSampler = false;
		outputImageInfo.layout = texture.imageLayout;
		QueueImageBindingUpdate(TYR_BINDING_LIGHTING_OUTPUT, renderFrameIndex, outputImageInfo);

		m_LightingOutputBoundTextures[renderFrameIndex] = colourTexture;
	}

	void Renderer::EnsureShadowMaskArraysBound(uint renderFrameIndex, TextureHandle shadowMasksRaw, TextureHandle shadowMasks)
	{
		if (shadowMasksRaw && m_ShadowMasksRawBoundTextures[renderFrameIndex] != shadowMasksRaw)
		{
			const Texture& texture = m_Registry.GetTexture(shadowMasksRaw);
			ImageBindingInfo bindingInfo;
			bindingInfo.imageView = texture.imageView;
			bindingInfo.hasSampler = false;
			bindingInfo.layout = texture.imageLayout;
			QueueImageBindingUpdate(TYR_BINDING_SHADOW_MASKS_RAW, renderFrameIndex, bindingInfo);
			m_ShadowMasksRawBoundTextures[renderFrameIndex] = shadowMasksRaw;
		}

		if (shadowMasks && m_ShadowMasksBoundTextures[renderFrameIndex] != shadowMasks)
		{
			const Texture& texture = m_Registry.GetTexture(shadowMasks);
			ImageBindingInfo bindingInfo;
			bindingInfo.imageView = texture.imageView;
			bindingInfo.hasSampler = false;
			bindingInfo.layout = texture.imageLayout;
			QueueImageBindingUpdate(TYR_BINDING_SHADOW_MASKS, renderFrameIndex, bindingInfo);
			m_ShadowMasksBoundTextures[renderFrameIndex] = shadowMasks;
		}
	}

	void Renderer::EnsureTaaResolveOutputBound(uint renderFrameIndex, TextureHandle resolvedColourTexture)
	{
		if (!resolvedColourTexture || m_TaaResolveOutputBoundTextures[renderFrameIndex] == resolvedColourTexture)
		{
			return;
		}

		const Texture& texture = m_Registry.GetTexture(resolvedColourTexture);
		ImageBindingInfo bindingInfo;
		bindingInfo.imageView = texture.imageView;
		bindingInfo.hasSampler = false;
		bindingInfo.layout = texture.imageLayout;
		QueueImageBindingUpdate(TYR_BINDING_TAA_RESOLVE_OUTPUT, renderFrameIndex, bindingInfo);
		m_TaaResolveOutputBoundTextures[renderFrameIndex] = resolvedColourTexture;
	}

	void Renderer::EnsureEditorGridOutputBound(uint renderFrameIndex, TextureHandle overlayColourTexture)
	{
		if (!overlayColourTexture || m_EditorGridOutputBoundTextures[renderFrameIndex] == overlayColourTexture)
		{
			return;
		}

		const Texture& texture = m_Registry.GetTexture(overlayColourTexture);
		ImageBindingInfo bindingInfo;
		bindingInfo.imageView = texture.imageView;
		bindingInfo.hasSampler = false;
		bindingInfo.layout = texture.imageLayout;
		QueueImageBindingUpdate(TYR_BINDING_EDITOR_GRID_OUTPUT, renderFrameIndex, bindingInfo);
		m_EditorGridOutputBoundTextures[renderFrameIndex] = overlayColourTexture;
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

			// Deleted immediately via the registry rather than queued for deferred deletion - this
			// only ever runs once it's already established safe to delete right away, and queuing
			// here would be too late for this same tick's delete-list processing to pick it up.
			m_Registry.DeleteTexture(targets.colourTexture);
			m_Registry.DeleteTexture(targets.gbufferAlbedoAO);
			m_Registry.DeleteTexture(targets.gbufferNormalRoughMetal);
			m_Registry.DeleteTexture(targets.gbufferMotion);
			m_Registry.DeleteTexture(targets.depthBuffer);
			m_Registry.DeleteTexture(targets.shadowMasksRaw);
			m_Registry.DeleteTexture(targets.shadowMasks);
			m_Registry.DeleteTexture(targets.resolvedColourTexture);
			if (targets.overlayColourTexture)
			{
				m_Registry.DeleteTexture(targets.overlayColourTexture);
			}
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
			desc.fileName = "ShadowRTCS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_COMPUTE_BIT;
			m_Resources.shadowRTComputeShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "ShadowDenoiseCS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_COMPUTE_BIT;
			m_Resources.shadowDenoiseComputeShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "TAAResolveCS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_COMPUTE_BIT;
			m_Resources.taaResolveComputeShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
		}
		{
			ShaderDesc desc;
			desc.entryPoint = "main";
			desc.fileName = "EditorGridCS";
			desc.dirPath = "";
			desc.stage = SHADER_STAGE_COMPUTE_BIT;
			m_Resources.editorGridComputeShader = m_ShaderCreator.CompileAndCreateShader(shaderCompileConfig, desc);
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
		m_Ctx.device->DeleteShaderModule(m_Resources.shadowRTComputeShader);
		m_Ctx.device->DeleteShaderModule(m_Resources.shadowDenoiseComputeShader);
		m_Ctx.device->DeleteShaderModule(m_Resources.taaResolveComputeShader);
		m_Ctx.device->DeleteShaderModule(m_Resources.editorGridComputeShader);
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
		// Freed here rather than immediately in RemoveWindow, once deferred deletion confirms
		// it's safe.
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
				// 11 original + activeMeshInstanceIndexBuffer/visibleInstanceIndexBuffer/
				// indirectDrawCommandBuffer/drawCountBuffer for GPU-driven instance culling +
				// shadowLightSlotMapBuffer for ray-traced shadows.
				poolSize.descriptorCount = 16;
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
				// TYR_BINDING_LIGHTING_OUTPUT, TYR_BINDING_SHADOW_MASKS_RAW/SHADOW_MASKS,
				// TYR_BINDING_TAA_RESOLVE_OUTPUT and TYR_BINDING_EDITOR_GRID_OUTPUT - five
				// storage-image arrays, each one entry per buffered RenderFrame slot.
				DescriptorPoolSize& poolSize = poolDesc.poolSizes.ExpandOne();
				poolSize.descriptorType = DescriptorType::StorageImage;
				poolSize.descriptorCount = RenderConstants::c_BufferedFrameCount * 5;
			}
			{
				// TYR_BINDING_TLAS - one top-level acceleration structure per buffered
				// RenderFrame slot.
				DescriptorPoolSize& poolSize = poolDesc.poolSizes.ExpandOne();
				poolSize.descriptorType = DescriptorType::AccelerationStructure;
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
			// lighting compute pass, and mesh/meshLOD/mesh-instance from the instance culling
			// compute pass too - meshlet/vertex/index/material stay geometry-only.
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
			// Read directly by the vertex shader (vertex-pulling via SV_VertexID) - a vertex
			// stage, not one of the mesh pipeline's task/mesh/fragment stages. Declared in
			// ascending binding-number order with no gaps after this.
			AddBinding(TYR_BINDING_GUI_VERTEX, DescriptorType::StorageBuffer, 1, SHADER_STAGE_VERTEX_BIT);
			// Compute-only storage image output, one entry per buffered RenderFrame slot. Needs
			// UPDATE_AFTER_BIND_BIT (an entry can be rewritten while others are in flight) and
			// PARTIALLY_BOUND_BIT (not every slot is bound yet).
			const DescriptorBindingFlags lightingOutputFlags = static_cast<DescriptorBindingFlags>(
				DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
			AddBinding(TYR_BINDING_LIGHTING_OUTPUT, DescriptorType::StorageImage, RenderConstants::c_BufferedFrameCount,
				SHADER_STAGE_COMPUTE_BIT, lightingOutputFlags);

			// GPU-driven instance culling - compute-only except TYR_BINDING_VISIBLE_INSTANCE_INDICES,
			// which the task stage also reads per draw via SV_DrawIndex.
			AddBinding(TYR_BINDING_ACTIVE_INSTANCE_INDICES, DescriptorType::StorageBuffer, 1, SHADER_STAGE_COMPUTE_BIT);
			AddBinding(TYR_BINDING_VISIBLE_INSTANCE_INDICES, DescriptorType::StorageBuffer, 1,
				static_cast<ShaderStage>(SHADER_STAGE_COMPUTE_BIT | SHADER_STAGE_TASK_BIT));
			AddBinding(TYR_BINDING_INDIRECT_DRAW_COMMANDS, DescriptorType::StorageBuffer, 1, SHADER_STAGE_COMPUTE_BIT);
			AddBinding(TYR_BINDING_DRAW_COUNT, DescriptorType::StorageBuffer, 1, SHADER_STAGE_COMPUTE_BIT);

			// Ray-traced shadows - one TLAS per buffered RenderFrame slot, read by the shadow
			// compute pass via an inline ray query.
			AddBinding(TYR_BINDING_TLAS, DescriptorType::AccelerationStructure, RenderConstants::c_BufferedFrameCount, SHADER_STAGE_COMPUTE_BIT);

			// Shadow mask storage - one Texture2DArray per buffered RenderFrame slot, same
			// re-bindable-on-resize shape as TYR_BINDING_LIGHTING_OUTPUT above.
			AddBinding(TYR_BINDING_SHADOW_MASKS_RAW, DescriptorType::StorageImage, RenderConstants::c_BufferedFrameCount,
				SHADER_STAGE_COMPUTE_BIT, lightingOutputFlags);
			AddBinding(TYR_BINDING_SHADOW_MASKS, DescriptorType::StorageImage, RenderConstants::c_BufferedFrameCount,
				SHADER_STAGE_COMPUTE_BIT, lightingOutputFlags);
			AddBinding(TYR_BINDING_SHADOW_LIGHT_SLOT_MAP, DescriptorType::StorageBuffer, 1, SHADER_STAGE_COMPUTE_BIT);
			AddBinding(TYR_BINDING_TAA_RESOLVE_OUTPUT, DescriptorType::StorageImage, RenderConstants::c_BufferedFrameCount,
				SHADER_STAGE_COMPUTE_BIT, lightingOutputFlags);
			AddBinding(TYR_BINDING_EDITOR_GRID_OUTPUT, DescriptorType::StorageImage, RenderConstants::c_BufferedFrameCount,
				SHADER_STAGE_COMPUTE_BIT, lightingOutputFlags);

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

		// Depth stencil description - reverse-Z, so a nearer fragment has a larger depth value
		// than what's already there: compare Greater, cleared to 0.
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

		// Must match the G-buffer render target formats - dynamic rendering requires the two to
		// agree.
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

		// GUI pipeline - reuses the same bindless descriptor set layout, but is otherwise a
		// completely separate pipeline: vertex-pulling, alpha blending on, no depth testing,
		// drawn over whatever the geometry pass already rendered.
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
		guiPushConstantRange.size = sizeof(float) * 4 + sizeof(uint); // scale, translate, textureIndex

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

		// GPU-driven instance frustum culling - runs just before the geometry pass, compacting
		// visible instances into the indirect draw buffers it then draws from directly.
		ComputePipelineDesc cullingDesc;
		cullingDesc.pipelineLayoutDesc.descriptorSetLayouts.Add(m_Resources.descriptorSetLayout);

		PushConstantRange& cullingPushConstantRange = cullingDesc.pipelineLayoutDesc.pushConstantRanges.ExpandOne();
		cullingPushConstantRange.stageFlags = SHADER_STAGE_COMPUTE_BIT;
		cullingPushConstantRange.offset = 0;
		cullingPushConstantRange.size = sizeof(uint); // activeInstanceCount

		cullingDesc.shader = m_Resources.cullingComputeShader;

		m_Resources.cullingPipeline = m_Ctx.device->CreateComputePipeline(cullingDesc);

		// Ray-traced shadow visibility - one dispatch per shadow-casting light.
		ComputePipelineDesc shadowRTDesc;
		shadowRTDesc.pipelineLayoutDesc.descriptorSetLayouts.Add(m_Resources.descriptorSetLayout);

		PushConstantRange& shadowRTPushConstantRange = shadowRTDesc.pipelineLayoutDesc.pushConstantRanges.ExpandOne();
		shadowRTPushConstantRange.stageFlags = SHADER_STAGE_COMPUTE_BIT;
		shadowRTPushConstantRange.offset = 0;
		shadowRTPushConstantRange.size = sizeof(ShadowRTPushConstants);

		shadowRTDesc.shader = m_Resources.shadowRTComputeShader;

		m_Resources.shadowRTPipeline = m_Ctx.device->CreateComputePipeline(shadowRTDesc);

		// Denoise of shadowRTPipeline's raw output.
		ComputePipelineDesc shadowDenoiseDesc;
		shadowDenoiseDesc.pipelineLayoutDesc.descriptorSetLayouts.Add(m_Resources.descriptorSetLayout);

		PushConstantRange& shadowDenoisePushConstantRange = shadowDenoiseDesc.pipelineLayoutDesc.pushConstantRanges.ExpandOne();
		shadowDenoisePushConstantRange.stageFlags = SHADER_STAGE_COMPUTE_BIT;
		shadowDenoisePushConstantRange.offset = 0;
		shadowDenoisePushConstantRange.size = sizeof(ShadowDenoisePushConstants);

		shadowDenoiseDesc.shader = m_Resources.shadowDenoiseComputeShader;

		m_Resources.shadowDenoisePipeline = m_Ctx.device->CreateComputePipeline(shadowDenoiseDesc);

		// TAA resolve.
		ComputePipelineDesc taaResolveDesc;
		taaResolveDesc.pipelineLayoutDesc.descriptorSetLayouts.Add(m_Resources.descriptorSetLayout);

		PushConstantRange& taaResolvePushConstantRange = taaResolveDesc.pipelineLayoutDesc.pushConstantRanges.ExpandOne();
		taaResolvePushConstantRange.stageFlags = SHADER_STAGE_COMPUTE_BIT;
		taaResolvePushConstantRange.offset = 0;
		taaResolvePushConstantRange.size = sizeof(TAAResolvePushConstants);

		taaResolveDesc.shader = m_Resources.taaResolveComputeShader;

		m_Resources.taaResolvePipeline = m_Ctx.device->CreateComputePipeline(taaResolveDesc);

		ComputePipelineDesc editorGridDesc;
		editorGridDesc.pipelineLayoutDesc.descriptorSetLayouts.Add(m_Resources.descriptorSetLayout);

		PushConstantRange& editorGridPushConstantRange = editorGridDesc.pipelineLayoutDesc.pushConstantRanges.ExpandOne();
		editorGridPushConstantRange.stageFlags = SHADER_STAGE_COMPUTE_BIT;
		editorGridPushConstantRange.offset = 0;
		editorGridPushConstantRange.size = sizeof(EditorGridPushConstants);

		editorGridDesc.shader = m_Resources.editorGridComputeShader;

		m_Resources.editorGridPipeline = m_Ctx.device->CreateComputePipeline(editorGridDesc);
	}

	void Renderer::DeletePipelines()
	{
		m_Ctx.device->DeleteGraphicsPipeline(m_Resources.geometryGraphicsPipeline);
		m_Ctx.device->DeleteGraphicsPipeline(m_Resources.guiPipeline);
		m_Ctx.device->DeleteComputePipeline(m_Resources.lightingPipeline);
		m_Ctx.device->DeleteComputePipeline(m_Resources.cullingPipeline);
		m_Ctx.device->DeleteComputePipeline(m_Resources.shadowRTPipeline);
		m_Ctx.device->DeleteComputePipeline(m_Resources.shadowDenoisePipeline);
		m_Ctx.device->DeleteComputePipeline(m_Resources.taaResolvePipeline);
		m_Ctx.device->DeleteComputePipeline(m_Resources.editorGridPipeline);
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
			// One c_BufferedFrameCount-th per buffered RenderFrame slot.
			desc.size = RenderConstants::c_GUIVertexBufferSize * RenderConstants::c_BufferedFrameCount;
			// Storage, not Vertex - GUIVS.hlsl pulls its own vertex via a StructuredBuffer
			// binding (TYR_BINDING_GUI_VERTEX) instead of fixed-function vertex input.
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.guiVertexBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "GUI Index Buffer";
			// One c_BufferedFrameCount-th per buffered RenderFrame slot.
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
			desc.debugName = "Shadow Light Slot Map Buffer";
			desc.size = sizeof(uint) * RenderConstants::c_ShadowLightSlotMapEntryCount;
			desc.usage = RenderBufferUsage::Storage;
			m_Resources.shadowLightSlotMapBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			RenderBufferDesc desc;
			desc.debugName = "TLAS Instance Buffer";
			// One c_BufferedFrameCount-th per buffered RenderFrame slot.
			desc.size = RenderConstants::c_TLASInstanceBufferSize * RenderConstants::c_BufferedFrameCount;
			desc.usage = RenderBufferUsage::RayTracing;
			m_Resources.tlasInstanceBuffer = m_Registry.CreateBuffer(desc);
		}
		{
			// Written directly at a fixed offset for its own renderFrameIndex slot, no allocator
			// involved.
			RenderBufferDesc desc;
			desc.debugName = "RT/Culling Staging Buffer";
			desc.size = c_RTCullingStagingSlotSize * RenderConstants::c_BufferedFrameCount;
			desc.usage = RenderBufferUsage::Upload;
			m_Resources.rtCullingStagingBuffer = m_Registry.CreateBuffer(desc);
		}
		{
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
		m_Registry.DeleteBuffer(m_Resources.shadowLightSlotMapBuffer);
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
		// One TLAS per buffered RenderFrame slot, sized once for the worst case (every active
		// mesh instance visible) - never recreated, only rebuilt. The scratch buffer is the one
		// that grows on demand, since a mesh's triangle count isn't known up front.
		AccelerationStructureDesc tlasDesc;
		tlasDesc.debugName = "TLAS";
		tlasDesc.type = AccelerationStructureType::TopLevel;
		tlasDesc.maxInstanceCount = RenderConstants::c_MaxMeshInstances;

		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			m_Resources.tlas[i].accelerationStructure = m_Ctx.device->CreateAccelerationStructure(tlasDesc);
			// Every slot's TLAS shares the same desc, so the same scratch size - just keep
			// whichever came back, they're all identical.
			m_TLASScratchPerSlotSize = m_Ctx.device->GetAccelerationStructureBuildScratchSize(m_Resources.tlas[i].accelerationStructure);

			// Queued, not applied immediately - picked up by the first FlushDescriptorUpdates()
			// call like any other startup binding. The handle itself never changes again, only
			// what it points to gets rebuilt each frame, so this one-time bind is all it needs.
			AccelerationStructureBindingInfo tlasBindingInfo;
			tlasBindingInfo.accelerationStructure = m_Resources.tlas[i].accelerationStructure;
			QueueAccelerationStructureBindingUpdate(TYR_BINDING_TLAS, i, tlasBindingInfo);
		}

		RenderBufferDesc scratchDesc;
		scratchDesc.debugName = "TLAS Scratch Buffer";
		// One c_BufferedFrameCount-th per buffered RenderFrame slot.
		scratchDesc.size = m_TLASScratchPerSlotSize * RenderConstants::c_BufferedFrameCount;
		scratchDesc.usage = RenderBufferUsage::RayTracing;
		m_Resources.tlasScratchBuffer = m_Registry.CreateBuffer(scratchDesc);
	}

	void Renderer::DeleteAccelerationStructures()
	{
		for (uint i = 0; i < RenderConstants::c_BufferedFrameCount; ++i)
		{
			m_Ctx.device->DeleteAccelerationStructure(m_Resources.tlas[i].accelerationStructure);
		}
		m_Registry.DeleteBuffer(m_Resources.tlasScratchBuffer);
		if (m_Resources.blasScratchBuffer)
		{
			m_Registry.DeleteBuffer(m_Resources.blasScratchBuffer);
		}
	}

	void Renderer::EnsureBLASScratchCapacity(size_t requiredPerSlotSize)
	{
		// m_BLASScratchCapacity is a per-slot size - the buffer itself is c_BufferedFrameCount
		// times this.
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

		ShadowRTPassArgs shadowRTArgs;
		shadowRTArgs.registry = &m_Registry;
		shadowRTArgs.resources = &m_Resources;
		shadowRTArgs.pipeline = m_Resources.shadowRTPipeline;
		m_ShadowRTPass = new ShadowRTPass(shadowRTArgs);

		ShadowDenoisePassArgs shadowDenoiseArgs;
		shadowDenoiseArgs.registry = &m_Registry;
		shadowDenoiseArgs.resources = &m_Resources;
		shadowDenoiseArgs.pipeline = m_Resources.shadowDenoisePipeline;
		m_ShadowDenoisePass = new ShadowDenoisePass(shadowDenoiseArgs);
	}

	void Renderer::DeletePasses()
	{
		delete m_TransferPass;
		m_TransferPass = nullptr;
		delete m_GeometryPass;
		m_GeometryPass = nullptr;
		delete m_GUIPass;
		m_GUIPass = nullptr;
		delete m_ShadowRTPass;
		m_ShadowRTPass = nullptr;
		delete m_ShadowDenoisePass;
		m_ShadowDenoisePass = nullptr;
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