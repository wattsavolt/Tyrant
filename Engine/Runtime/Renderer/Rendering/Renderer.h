#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "Containers/SPSCRingBuffer.h"
#include "Threading/TaskScheduler.h"
#include "Shaders/ShaderTypes.h"
#include "RendererConfig.h"
#include "RenderAPI/CommandList.h"
#include "Shader/ShaderCreator.h"
#include "RenderResource/RenderBuffer.h"
#include "RenderFrame.h"
#include "RenderWindow.h"
#include "RenderViewport.h"
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
	class GUIPass;
	class RenderSubmissionThread;
	class RenderGraphBuilder;
	struct BufferBindingUpdate;

	struct RenderSyncData
	{
		// The graphics queue timeline value that this frame's submission was signalled with.
		uint64 completionTimelineValue = 0;
		// Which frame (see Renderer::m_FrameNumber) completionTimelineValue belongs to - lets
		// PrepareForNextFrame tell a fresh report apart from a stale one left over from an
		// earlier use of the same slot (see its own comment for why that distinction matters).
		// Starts at an otherwise-unreachable sentinel, not 0, since 0 is itself a legitimate
		// frame number (the very first frame) that just hasn't been reported yet.
		uint64 frameNumber = ~uint64(0);
	};

	class Renderer final : INonCopyable
	{
	public:
		Renderer(const RendererConfig& rendererConfig, RenderAPI* renderAPI);
		~Renderer();

		// Needs to be called each frame (RendererModule::Update) - see its own comment in the .cpp
		// for why it's safe to dispatch RenderAsync directly from here.
		void Render(float deltaTime);

		// Needs to be called at end of each frame (RendererModule::EndFrame).
		void PrepareForNextFrame();

		RenderWindowHandle AddWindow(void* osHandle);

		void RemoveWindow(RenderWindowHandle window);

		// Width/height aren't needed - the swap chain reads the current extent straight from the
		// surface itself. Stalls the main thread until it's safe to recreate (see its own comment) -
		// acceptable since resizes aren't frequent.
		void ResizeWindow(RenderWindowHandle window);

		// Just allocates a pool slot - see RenderViewport's own comment on why creation doesn't
		// also create any textures yet (that only happens once something actually requests a size).
		RenderViewportHandle CreateRenderViewport();

		// Deferred the same way RemoveWindow/RemoveScene are - see RenderFrame::renderViewportsToDelete.
		void DeleteRenderViewport(RenderViewportHandle viewport);

		LocalObjectPool<RenderViewport, RenderConstants::c_MaxScenes>& GetRenderViewportPool() { return m_RenderViewportPool; }

		// Resizes one buffered slot of a RenderViewport in place (deleting the old four textures,
		// if any, then creating four fresh ones at the new size) and marks it isNew - shared by the
		// immediate current-slot resize and the deferred application of a pending resize (see
		// RenderViewport's own comment on resize propagation, and Render()). Main thread only, like
		// every texture create/delete.
		void ResizeRenderViewportSlot(RenderViewport& viewport, uint slot, const char* debugName, uint width, uint height);

		ShaderCreator& GetShaderCreator() { return m_ShaderCreator; }

		RenderFrame& GetRenderFrame()
		{
			return m_RenderFrames[m_RenderFrameIndex];
		}

		// Which buffered RenderFrame slot (and so which RenderViewportTextureData entry) this
		// tick's Update()/EndFrame() calls are all operating on - stable for the whole tick,
		// only advancing in PrepareForNextFrame. Main thread only, like GetRenderFrame().
		uint GetRenderFrameIndex() const { return m_RenderFrameIndex; }

		RenderFrame& GetPrevRenderFrame()
		{
			const uint index = Utility::GetPrevCircularIndex(m_RenderFrameIndex, RenderConstants::c_BufferedFrameCount);
			return m_RenderFrames[index];
		}

		RenderResources& GetRenderResources() { return m_Resources; }

		RenderData& GetRenderData() { return m_Data; }

		// See ImmediateSceneData's own comment - main thread only, both reads and writes.
		ImmediateSceneData& GetImmediateSceneData(SceneHandle handle) { return m_ImmediateSceneData[handle.h.index]; }

		RenderAllocationManager& GetAllocationManager() { return m_AllocManager; }

		// Queues a descriptor write instead of calling Device::UpdateDescriptorSet immediately -
		// see PendingDescriptorUpdates' own comment on why. Main thread only, like every
		// RendererAPI Create*/Update* call these back - the same single-writer assumption
		// RenderRegistry's pools already rely on.
		void QueueBufferBindingUpdate(uint bindingIndex, uint descriptorArrayIndex, const BufferBindingInfo& info);
		void QueueImageBindingUpdate(uint bindingIndex, uint descriptorArrayIndex, const ImageBindingInfo& info);
		void QueueAccelerationStructureBindingUpdate(uint bindingIndex, uint descriptorArrayIndex, const AccelerationStructureBindingInfo& info);

	private:
		// Waits for the last RenderAsync task to finish (CPU-side) and releases every buffered
		// slot's task. Only used internally now, by Shutdown() and StallUntilGPUIdle() - both
		// need "nothing is still recording" as part of a bigger wait before they touch shared
		// state.
		void WaitForCompletion();

		// Builds on WaitForCompletion() with a wait for RenderSubmissionThread to finish
		// presenting the last frame and a device-wide WaitIdle - see its own comment. Used
		// wherever something is about to be deleted/recreated that an in-flight frame could
		// still be touching (ResizeWindow, EnsureBLASScratchCapacity's growth path).
		void StallUntilGPUIdle();

		// Stops all async rendering activity (the last RenderAsync task, the render submission
		// thread) and waits for the GPU to go idle. Only called from the destructor now, as its
		// natural, safety-net position in module shutdown order is enough on its own: everything
		// that could unsafely race a worker thread (RemoveWindow, RemoveScene) defers its actual
		// work instead of touching live data immediately, so nothing before RendererModule's own
		// Shutdown() runs needs this to have already happened.
		void Shutdown();

		void RenderAsync(uint renderFrameIndex, uint64 frameNumber);
		// hasActiveScene is false when renderFrame.activeScene itself was invalid this tick
		// (nothing to merge/cull/draw, and no window to acquire/present to) - hasValidSwapChainImage
		// is only ever true when hasActiveScene also is (see RenderAsync's own guard). The
		// Transfer pass always runs regardless of either, so already-queued asset/frame buffer
		// uploads are never stranded just because no scene happened to be active this tick.
		void BuildAndExecuteRenderGraph(uint renderFrameIndex, uint64 frameNumber, bool hasActiveScene, bool hasValidSwapChainImage);
		void RecordTransferPass(CommandList& cmdList, uint renderFrameIndex);
		void SetupCullingPass(RenderGraphBuilder& builder);
		void RecordCullingPass(CommandList& cmdList, uint renderFrameIndex);
		void RecordGeometryPass(CommandList& cmdList, uint renderFrameIndex);
		void RecordLightingPass(CommandList& cmdList, uint renderFrameIndex);
		// Resolves m_Data.activeScene's own RenderViewport (see Scene::renderViewport) down to this
		// renderFrameIndex's own buffered slot - returns null if the active scene has no
		// RenderViewport yet (merge hasn't caught up, or World hasn't assigned one). Called only
		// from RecordGeometryPass/RecordLightingPass (a RenderAsync worker thread) - safe to read/
		// write this slot in place without a lock, same reasoning as every other per-slot resource
		// here (see RenderViewport's own comment).
		RenderViewportTextureData* GetActiveViewportTextureData(uint renderFrameIndex);
		void RecordGUIPass(CommandList& cmdList, uint renderFrameIndex);
		void SetupRayTracingBuildPass(RenderGraphBuilder& builder);
		// Issues this frame's BLAS builds (see RenderFrame::blasBuildsToRecord - decided and
		// created on the main thread in Render(), not here), then rebuilds this slot's TLAS from
		// this frame's active scene instances.
		void RecordRayTracingBuildPass(CommandList& cmdList, uint renderFrameIndex);
		// Grows m_Resources.blasScratchBuffer in place if requiredPerSlotSize exceeds its current
		// (also per-slot) capacity - see the buffer's own comment on why it's shared/reused across
		// builds, and why growing it needs StallUntilGPUIdle. Main thread only, called from
		// Render()'s BLAS batch.
		void EnsureBLASScratchCapacity(size_t requiredPerSlotSize);
		void CreateAccelerationStructures();
		void DeleteAccelerationStructures();

		// Creates one viewport G-buffer/colour/depth texture at the given size, mirroring
		// RendererAPI::CreateTexture's bookkeeping (bindless descriptor write, texturesToAdd) - a
		// separate, minimal copy rather than a call into RendererAPI, since Renderer has no
		// reference back to it (RendererAPI wraps Renderer, not the other way around - see
		// RendererModule, which owns both as siblings). Used only by ResizeRenderViewportSlot.
		TextureHandle CreateViewportTargetTexture(const char* debugName, PixelFormat format, ImageUsage usage, uint width, uint height);
		// Mirrors RendererAPI::DeleteTexture - see CreateViewportTargetTexture's own comment.
		void DeleteViewportTargetTexture(TextureHandle handle);
		// Keeps TYR_BINDING_LIGHTING_OUTPUT[renderFrameIndex] pointed at whatever colour texture is
		// actually in the active scene's viewport at this slot right now - needed not just after a
		// resize but also whenever the active scene itself changes to one whose own viewport has a
		// different (even if identically-sized) colour texture at this slot. A no-op once already
		// correct (see m_LightingOutputBoundTextures).
		void EnsureLightingOutputBound(uint renderFrameIndex, TextureHandle colourTexture);
		// Actually deletes every buffered slot's textures (if any were ever created) and frees the
		// pool slot - shared by ProcessFrameDeleteLists' handling of renderViewportsToDelete,
		// wherever it's called from. Mirrors DeleteWindowResources.
		void DeleteRenderViewportResources(RenderViewportHandle viewport);

		// Reads whatever RenderSubmissionThread has reported finishing since the last call,
		// updates m_SyncDatas with it, and signals the frame upload allocator with it.
		void DrainSubmissionCompletions();

		void CreateQueues();
		void DeleteQueues();
		void CreateShaders();
		void DeleteShaders();
		// Processes every *ToDelete/*ToRemove list on one RenderFrame slot - shared by
		// PrepareForNextFrame's normal per-tick processing of the current slot and
		// DeleteRemainingFrameResources' shutdown-time sweep of every slot.
		void ProcessFrameDeleteLists(RenderFrame& renderFrame);
		// Runs ProcessFrameDeleteLists over every buffered RenderFrame slot. Only safe to call
		// once nothing can possibly still be using any queued resource - after Shutdown() has
		// stopped RenderSubmissionThread and waited for the GPU to go idle, and every module has
		// finished calling RemoveWindow/RemoveScene/DeleteMesh/etc (see those calls' own
		// comments) - so this runs from the destructor, not Shutdown() itself. Without this,
		// anything queued during module shutdown (which happens after the main loop - and so
		// PrepareForNextFrame - has already stopped running) would sit in its RenderFrame slot
		// forever, left un-deleted when RenderRegistry's pools assert they're empty.
		void DeleteRemainingFrameResources();
		// Actually deletes one window's swap chain, semaphores, and pool slot - shared by
		// ProcessFrameDeleteLists' handling of windowsToDelete, wherever it's called from.
		void DeleteWindowResources(const PendingWindowDelete& pending);
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

		// Sends every descriptor write queued this tick (see QueueBufferBindingUpdate etc.) to the
		// device in one batched Device::UpdateDescriptorSet call instead of one vkUpdateDescriptorSets
		// per write, then clears the three arrays for the next tick. Called once per Render(), before
		// RenderAsync is dispatched - every RendererAPI Create*/Update* call that queues one of these
		// runs earlier in the same tick (other modules' Update(), which all run before
		// RendererModule::Update() - see EngineLoop.cpp's own comment on module ordering), so by the
		// time this flush runs, every write queued this tick is already present, and by the time
		// RenderAsync's recorded command buffers actually read the descriptor set, this flush has
		// already landed every one of them. A no-op if nothing was queued.
		void FlushDescriptorUpdates();

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
		LocalObjectPool<RenderViewport, RenderConstants::c_MaxScenes> m_RenderViewportPool;
		// See ImmediateSceneData's own comment - indexed the same way RenderData::scenePool is,
		// reset in RendererAPI::AddScene (covers pool-slot reuse, same as every other pool here).
		ImmediateSceneData m_ImmediateSceneData[RenderConstants::c_MaxScenes];
		// The colour texture handle TYR_BINDING_LIGHTING_OUTPUT[slot] was last bound to - compared
		// against the active scene's current one each tick so a scene switch with no actual resize
		// still rebinds correctly (see EnsureLightingOutputBound).
		TextureHandle m_LightingOutputBoundTextures[RenderConstants::c_BufferedFrameCount];
		HashMap<uint, uint> m_ViewIdIndexMap;
		RenderContext m_Ctx{};
		RenderResources m_Resources{};
		RenderData m_Data{};
		TransferPass* m_TransferPass = nullptr;
		GeometryPass* m_GeometryPass = nullptr;
		GUIPass* m_GUIPass = nullptr;
		uint m_RenderFrameIndex = 0;
		// Starts true so the very first Render() call runs the one-time descriptor binding block.
		bool m_FirstRender = true;

		// Descriptor writes queued this tick by QueueBufferBindingUpdate/QueueImageBindingUpdate/
		// QueueAccelerationStructureBindingUpdate (RendererAPI::CreateTexture's bindless write,
		// EnsureLightingOutputBound's per-slot write, etc.) instead of each call site hitting
		// Device::UpdateDescriptorSet - and so vkUpdateDescriptorSets - immediately on its own.
		// FlushDescriptorUpdates sends all of them in a single batched call once per Render(),
		// then clears these arrays back to empty.
		//
		// Each Info array holds its update's actual data; the matching Update array holds the
		// plain BufferBindingUpdate/ImageBindingUpdate/AccelerationStructureBindingUpdate structs
		// Device::UpdateDescriptorSet actually takes, index-for-index with its Info array. They're
		// kept as two parallel arrays rather than one combined struct so the Update arrays stay
		// exactly BufferBindingUpdate/etc-sized: UpdateDescriptorSet indexes into the array it's
		// given using sizeof the type in its own signature, so passing an array of some derived
		// "combined" struct as that base pointer would compute wrong addresses for every index
		// past the first. Instead, each Update entry's own *BindingInfos pointer is left unset by
		// Queue*BindingUpdate and only pointed at its matching Info entry inside
		// FlushDescriptorUpdates, right before the arrays are read - Add()'s own reallocate-on-grow
		// would otherwise leave an earlier-computed pointer dangling once a later Queue* call grows
		// the Info array again.
		struct PendingDescriptorUpdates
		{
			Array<BufferBindingInfo> bufferInfos;
			Array<BufferBindingUpdate> bufferUpdates;
			Array<ImageBindingInfo> imageInfos;
			Array<ImageBindingUpdate> imageUpdates;
			Array<AccelerationStructureBindingInfo> accelerationStructureInfos;
			Array<AccelerationStructureBindingUpdate> accelerationStructureUpdates;
		};
		PendingDescriptorUpdates m_PendingDescriptorUpdates;
		// Last frame's viewProj, fed into SceneInfo::prevViewProj for GBufferPS.hlsl's motion
		// vectors - camera motion only for now (no per-instance previous transform is tracked,
		// so a moving/rotating object won't get a motion vector of its own yet).
		Matrix4 m_PrevViewProj = Matrix4::c_Identity;

		// Meshes still waiting on their one-time BLAS build (see RendererAPI::RequestBLASBuild),
		// persistent across frames (unlike RenderFrame's own per-slot lists) - Render() (main
		// thread) processes this strictly front-to-back every frame, never reordering it (earlier
		// requests build first, for streaming priority), stopping once the next mesh's geometry
		// size would exceed RenderConstants::c_MaxBLASBuildBytesPerFrame (always building at least
		// one, so a single oversized mesh can't stall everything behind it).
		Array<MeshHandle> m_PendingBLASBuilds;
		// Current PER-SLOT capacity of m_Resources.blasScratchBuffer - see its own comment.
		size_t m_BLASScratchCapacity = 0;
		// Per-slot size of m_Resources.tlasScratchBuffer, set once in CreateAccelerationStructures
		// (every slot's TLAS is identically sized, so this never changes afterward) - see that
		// buffer's own comment.
		size_t m_TLASScratchPerSlotSize = 0;
		// How many entries RenderAsync actually wrote into tlasInstanceBuffer this frame (can be
		// less than the active instance count - instances whose mesh has no BLAS yet are
		// skipped) - RecordRayTracingBuildPass needs this for the TLAS build's instanceCount.
		uint m_TLASInstanceCount = 0;

		// True, non-wrapping frame counter (unlike m_RenderFrameIndex, which wraps every
		// c_BufferedFrameCount) - incremented once per frame Render() actually submits. Used to
		// pace swap chain image acquisition - see its use in Render() for why.
		uint64 m_FrameNumber = 0;

		// The previous frame's RenderAsync task - each new one depends on this, so they
		// never run at the same time. Not released once the dependency is added (unlike
		// before) - see m_RenderAsyncTasks for why.
		TaskID m_PrevRenderAsyncTask = c_InvalidTaskID;

		// The RenderAsync task last enqueued against each buffered RenderFrame slot. RenderAsync
		// reads its slot's RenderFrame by reference on a worker thread, so PrepareForNextFrame
		// must wait for a slot's task to actually finish before it cycles back around and starts
		// clearing/rewriting that same RenderFrame - the GPU-timeline-semaphore wait it already
		// does isn't enough on its own, since the semaphore value is still 0 (indistinguishable
		// from "nothing submitted yet") until the task has actually run and submitted anything.
		// Each entry is released (once, by PrepareForNextFrame) the next time its slot cycles
		// back around and is confirmed finished.
		TaskID m_RenderAsyncTasks[RenderConstants::c_BufferedFrameCount];

		// The frame number (see m_FrameNumber) each buffered slot's task was submitted under -
		// what PrepareForNextFrame compares m_SyncDatas[slot].frameNumber against to know
		// whether that slot's GPU-completion report has actually arrived yet, rather than
		// still being left over from an earlier use of the same slot.
		uint64 m_RenderFrameNumbers[RenderConstants::c_BufferedFrameCount];
	};
	
}