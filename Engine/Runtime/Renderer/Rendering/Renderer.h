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
#include "RenderQualitySettings.h"
#include "RenderContext.h"
#include "RenderResources.h"
#include "RenderData.h"
#include "RenderRegistry.h"
#include "RenderTargetPool.h"
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
	class ShadowRTPass;
	class ShadowDenoisePass;
	class RenderSubmissionThread;
	class RenderGraphBuilder;
	struct BufferBindingUpdate;

	struct RenderSyncData
	{
		// The graphics queue timeline value that this frame's submission was signalled with.
		uint64 completionTimelineValue = 0;
		// Which frame completionTimelineValue belongs to, so a fresh report can be told apart
		// from a stale one left over from an earlier use of the same slot. Starts at an
		// otherwise-unreachable sentinel, not 0, since 0 is itself a legitimate frame number.
		uint64 frameNumber = ~uint64(0);
	};

	class Renderer final : INonCopyable
	{
	public:
		Renderer(const RendererConfig& rendererConfig, RenderAPI* renderAPI);
		~Renderer();

		// Needs to be called each frame (RendererModule::Update).
		void Render(float deltaTime);

		// Needs to be called at end of each frame (RendererModule::EndFrame).
		void PrepareForNextFrame();

		RenderWindowHandle AddWindow(void* osHandle);

		void RemoveWindow(RenderWindowHandle window);

		// Width/height aren't needed - the swap chain reads the current extent straight from the
		// surface itself. Stalls the main thread until it's safe to recreate; acceptable since
		// resizes aren't frequent.
		void ResizeWindow(RenderWindowHandle window);

		// Just allocates a pool slot - creation doesn't also create any textures yet, only once
		// something actually requests a size.
		RenderViewportHandle CreateRenderViewport();

		// Deferred the same way other resource removals are.
		void DeleteRenderViewport(RenderViewportHandle viewport);

		LocalObjectPool<RenderViewport, RenderConstants::c_MaxScenes>& GetRenderViewportPool() { return m_RenderViewportPool; }

		// Resizes one buffered slot of a RenderViewport in place (deleting the old four textures,
		// if any, then creating four fresh ones at the new size) and marks it isNew. Main thread
		// only, like every texture create/delete.
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

		// Main-thread-only mirror of the worker-owned m_TaaEnabled - see its own comment. Read by
		// Render() (jitter decision) and RendererAPI::GetOrCreateRenderViewportTexture (which
		// texture to display); written only by RendererAPI::SetTaaEnabled.
		bool IsTaaEnabledMainThread() const { return m_TaaEnabledMainThread; }
		void SetTaaEnabledMainThread(bool enabled) { m_TaaEnabledMainThread = enabled; }

		RenderFrame& GetPrevRenderFrame()
		{
			const uint index = Utility::GetPrevCircularIndex(m_RenderFrameIndex, RenderConstants::c_BufferedFrameCount);
			return m_RenderFrames[index];
		}

		RenderResources& GetRenderResources() { return m_Resources; }

		RenderData& GetRenderData() { return m_Data; }

		// Main thread only, both reads and writes.
		ImmediateSceneData& GetImmediateSceneData(SceneHandle handle) { return m_ImmediateSceneData[handle.h.index]; }

		RenderAllocationManager& GetAllocationManager() { return m_AllocManager; }

		// Queues a descriptor write instead of calling Device::UpdateDescriptorSet immediately.
		// Main thread only, like every RendererAPI Create*/Update* call these back.
		void QueueBufferBindingUpdate(uint bindingIndex, uint descriptorArrayIndex, const BufferBindingInfo& info);
		void QueueImageBindingUpdate(uint bindingIndex, uint descriptorArrayIndex, const ImageBindingInfo& info);
		void QueueAccelerationStructureBindingUpdate(uint bindingIndex, uint descriptorArrayIndex, const AccelerationStructureBindingInfo& info);

	private:
		// Waits for the last RenderAsync task to finish (CPU-side) and releases every buffered
		// slot's task. Used internally by Shutdown() and StallUntilGPUIdle(), both of which need
		// "nothing is still recording" before touching shared state.
		void WaitForCompletion();

		// Builds on WaitForCompletion() with a wait for the submission thread to finish
		// presenting the last frame and a device-wide WaitIdle. Used wherever something is about
		// to be deleted/recreated that an in-flight frame could still be touching.
		void StallUntilGPUIdle();

		// Stops all async rendering activity and waits for the GPU to go idle. Only called from
		// the destructor - everything that could unsafely race a worker thread defers its actual
		// work instead of touching live data immediately.
		void Shutdown();

		void RenderAsync(uint renderFrameIndex, uint64 frameNumber);
		// hasActiveScene is false when renderFrame.activeScene itself was invalid this tick, and
		// hasValidSwapChainImage is only ever true when hasActiveScene also is. The transfer
		// pass always runs regardless of either.
		void BuildAndExecuteRenderGraph(uint renderFrameIndex, uint64 frameNumber, bool hasActiveScene, bool hasValidSwapChainImage);
		void RecordTransferPass(CommandList& cmdList, uint renderFrameIndex);
		void SetupCullingPass(RenderGraphBuilder& builder);
		void RecordCullingPass(CommandList& cmdList, uint renderFrameIndex);
		void RecordGeometryPass(CommandList& cmdList, uint renderFrameIndex);
		void RecordLightingPass(CommandList& cmdList, uint renderFrameIndex);
		// No separate Setup - the render graph usage declarations this needs are simple enough to
		// stay inline in BuildAndExecuteRenderGraph's own "TAAResolve" lambda, the same way
		// RecordLightingPass's sibling AddPass call already does. Re-derives the previous slot's
		// own data itself (same cross-slot lookup Setup's lambda already did) rather than taking
		// it all as parameters - keeps this Execute lambda's capture list small enough for
		// Function<>'s fixed inline buffer (see Function.h).
		void RecordTAAResolvePass(CommandList& cmdList, uint renderFrameIndex);
		// Draws the viewport grid over sourceIndex's image into the slot's overlay texture.
		void RecordEditorGridPass(CommandList& cmdList, uint renderFrameIndex, uint sourceIndex);
		// Draws this frame's debug lines into the slot's overlay texture.
		void RecordDebugLinePass(CommandList& cmdList, uint renderFrameIndex);
		// Resolves the active scene's own RenderViewport down to this renderFrameIndex's own
		// buffered slot - returns null if the active scene has no RenderViewport yet. Called
		// from a RenderAsync worker thread - safe to read/write this slot without a lock.
		RenderViewportTextureData* GetActiveViewportTextureData(uint renderFrameIndex);
		void RecordGUIPass(CommandList& cmdList, uint renderFrameIndex);
		// Split into two passes (rather than one), since the TLAS build's dependency on the BLAS
		// builds completing needs to be a barrier the render graph inserts between passes, not
		// something issued by hand in the middle of one pass's own execute callback.
		void SetupBLASBuildPass(RenderGraphBuilder& builder, uint renderFrameIndex);
		void RecordBLASBuildPass(CommandList& cmdList, uint renderFrameIndex);
		void SetupTLASBuildPass(RenderGraphBuilder& builder, uint renderFrameIndex);
		// Rebuilds this slot's TLAS from this frame's active scene instances.
		void RecordTLASBuildPass(CommandList& cmdList, uint renderFrameIndex);
		// Grows m_Resources.blasScratchBuffer in place if requiredPerSlotSize exceeds its current
		// (also per-slot) capacity. Main thread only, called from Render()'s BLAS batch.
		void EnsureBLASScratchCapacity(size_t requiredPerSlotSize);
		void CreateAccelerationStructures();
		void DeleteAccelerationStructures();

		// Creates one viewport G-buffer/colour/depth texture at the given size, mirroring
		// RendererAPI::CreateTexture's bookkeeping - a separate, minimal copy since Renderer has
		// no reference back to it. Used only by ResizeRenderViewportSlot.
		TextureHandle CreateViewportTargetTexture(const char* debugName, PixelFormat format, ImageUsage usage, uint width, uint height);
		// Shadow mask storage - a Texture2DArray (RenderConstants::c_MaxShadowSlots layers),
		// storage-only (read/written via Load(), never sampled), so unlike
		// CreateViewportTargetTexture this never registers into the bindless TYR_BINDING_TEXTURES
		// array.
		TextureHandle CreateShadowMaskArrayTexture(const char* debugName, uint width, uint height);
		// Mirrors RendererAPI::DeleteTexture.
		void DeleteViewportTargetTexture(TextureHandle handle);
		// Keeps TYR_BINDING_LIGHTING_OUTPUT[renderFrameIndex] pointed at whatever colour texture
		// is actually in the active scene's viewport at this slot right now - needed after a
		// resize or whenever the active scene itself changes. A no-op once already correct.
		void EnsureLightingOutputBound(uint renderFrameIndex, TextureHandle colourTexture);
		// Same idea as EnsureLightingOutputBound, for the two shadow mask arrays.
		void EnsureShadowMaskArraysBound(uint renderFrameIndex, TextureHandle shadowMasksRaw, TextureHandle shadowMasks);
		// Same idea as EnsureLightingOutputBound, for TAA's own resolve output.
		void EnsureTaaResolveOutputBound(uint renderFrameIndex, TextureHandle resolvedColourTexture);
		// Same idea as EnsureLightingOutputBound, for the grid's overlay output.
		void EnsureEditorGridOutputBound(uint renderFrameIndex, TextureHandle overlayColourTexture);
		// Creates or deletes the slot's overlay texture to match whether anything draws into it.
		void SyncViewportOverlay(RenderViewport& viewport, uint slot);
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
		// stopped the submission thread and the GPU is idle. Runs from the destructor, not
		// Shutdown() itself.
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

		// Sends every descriptor write queued this tick to the device in one batched call instead
		// of one per write, then clears the arrays for the next tick. Called once per Render(),
		// before RenderAsync is dispatched. A no-op if nothing was queued.
		void FlushDescriptorUpdates();

		static bool s_Instantiated;

		RenderAPI* m_RenderAPI;
		RenderSubmissionThread* m_RenderSubmissionThread;
		ShaderCreator m_ShaderCreator;	
		ShaderMaterial m_ShaderMaterial;
		RendererConfig m_Config;
		RenderRegistry m_Registry;
		// After the registry it creates render targets in.
		RenderTargetPool m_RenderTargetPool;
		RenderAllocationManager m_AllocManager{};
		RenderFrame m_RenderFrames[RenderConstants::c_BufferedFrameCount];
		RenderSyncData m_SyncDatas[RenderConstants::c_BufferedFrameCount];
		LocalObjectPool<RenderWindow, WindowConstants::c_MaxWindows> m_WindowPool;
		LocalObjectPool<RenderViewport, RenderConstants::c_MaxScenes> m_RenderViewportPool;
		// Indexed the same way RenderData::scenePool is, reset on scene add (covers pool-slot
		// reuse, same as every other pool here).
		ImmediateSceneData m_ImmediateSceneData[RenderConstants::c_MaxScenes];
		// The colour texture handle TYR_BINDING_LIGHTING_OUTPUT[slot] was last bound to - compared
		// each tick so a scene switch with no actual resize still rebinds correctly.
		TextureHandle m_LightingOutputBoundTextures[RenderConstants::c_BufferedFrameCount];
		// Same idea as m_LightingOutputBoundTextures, for the two shadow mask arrays.
		TextureHandle m_ShadowMasksRawBoundTextures[RenderConstants::c_BufferedFrameCount];
		TextureHandle m_ShadowMasksBoundTextures[RenderConstants::c_BufferedFrameCount];
		TextureHandle m_TaaResolveOutputBoundTextures[RenderConstants::c_BufferedFrameCount];
		TextureHandle m_EditorGridOutputBoundTextures[RenderConstants::c_BufferedFrameCount];
		HashMap<uint, uint> m_ViewIdIndexMap;
		RenderContext m_Ctx{};
		RenderResources m_Resources{};
		RenderData m_Data{};
		TransferPass* m_TransferPass = nullptr;
		GeometryPass* m_GeometryPass = nullptr;
		GUIPass* m_GUIPass = nullptr;
		ShadowRTPass* m_ShadowRTPass = nullptr;
		ShadowDenoisePass* m_ShadowDenoisePass = nullptr;
		uint m_RenderFrameIndex = 0;
		// Starts true so the very first Render() call runs the one-time descriptor binding block.
		bool m_FirstRender = true;

		// Descriptor writes queued this tick, sent in one batched call by FlushDescriptorUpdates
		// instead of each call site hitting the device immediately. Each Info array holds an
		// update's data; the matching Update array holds the plain structs the device call needs.
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
		// Last frame's TAA jitter offset (NDC units) - fed into SceneInfo::jitterDelta the same
		// way m_PrevViewProj feeds prevViewProj, so GBufferPS.hlsl's motion vectors can subtract
		// out the jitter itself rather than mistaking it for real scene motion.
		Vector2 m_PrevJitterNDC = Vector2::c_Zero;

		// Meshes still waiting on their one-time BLAS build, persistent across frames - processed
		// strictly front-to-back every frame (earlier requests build first), stopping once the
		// next mesh would exceed the per-frame byte budget.
		Array<MeshHandle> m_PendingBLASBuilds;
		// Current per-slot capacity of m_Resources.blasScratchBuffer.
		size_t m_BLASScratchCapacity = 0;
		// Per-slot size of m_Resources.tlasScratchBuffer, set once in CreateAccelerationStructures -
		// every slot's TLAS is identically sized, so this never changes afterward.
		size_t m_TLASScratchPerSlotSize = 0;
		// How many entries RenderAsync actually wrote into tlasInstanceBuffer this frame (can be
		// less than the active instance count - instances whose mesh has no BLAS yet are
		// skipped) - RecordRayTracingBuildPass needs this for the TLAS build's instanceCount.
		uint m_TLASInstanceCount = 0;

		// True, non-wrapping frame counter (unlike m_RenderFrameIndex, which wraps every
		// c_BufferedFrameCount) - incremented once per frame Render() actually submits, used to
		// pace swap chain image acquisition.
		uint64 m_FrameNumber = 0;

		// Only ever read/written from inside RenderAsync, never the main thread directly - safe
		// as plain shared state since consecutive RenderAsync invocations are never concurrent
		// (see m_PrevRenderAsyncTask below).
		RenderQualitySettings m_QualitySettings = ResolveQualitySettings(QualityLevel::Ultra);
		// TAA's own independent on/off switch - not part of RenderQualitySettings, same
		// RenderAsync-only-access safety as m_QualitySettings above.
		bool m_TaaEnabled = true;
		// Main-thread-only mirror of m_TaaEnabled, updated directly by RendererAPI::SetTaaEnabled
		// (never derived from m_TaaEnabled itself, which is worker-owned) - lets Render() decide
		// whether to apply this tick's jitter, and lets RendererAPI::GetOrCreateRenderViewportTexture
		// decide which texture the editor should display, without racing the worker thread's own
		// read/write of m_TaaEnabled. The same ImmediateSceneData-style pattern used for other
		// worker-owned state the main thread also needs a safe view of.
		bool m_TaaEnabledMainThread = true;

		// The previous frame's RenderAsync task - each new one depends on this, so they never
		// run at the same time.
		TaskID m_PrevRenderAsyncTask = c_InvalidTaskID;

		// The RenderAsync task last enqueued against each buffered RenderFrame slot -
		// PrepareForNextFrame must wait for a slot's task to finish before cycling back around
		// and rewriting that RenderFrame, since a GPU semaphore alone can't distinguish "not
		// submitted yet" from "done".
		TaskID m_RenderAsyncTasks[RenderConstants::c_BufferedFrameCount];

		// The frame number each buffered slot's task was submitted under - what
		// PrepareForNextFrame compares against to know whether that slot's completion report has
		// actually arrived yet, rather than being left over from an earlier use.
		uint64 m_RenderFrameNumbers[RenderConstants::c_BufferedFrameCount];
	};
	
}