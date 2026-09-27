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

		// Needs to be called each frame
		void Render(float deltaTime);

		// Needs to be called at end of each frame
		void PrepareForNextFrame();

		RenderWindowHandle AddWindow(void* osHandle);

		void RemoveWindow(RenderWindowHandle window);

		// Width/height aren't needed - the swap chain reads the current extent straight from the
		// surface itself. Stalls the main thread until it's safe to recreate (see its own comment) -
		// acceptable since resizes aren't frequent.
		void ResizeWindow(RenderWindowHandle window);

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
		// Waits for the last RenderAsync task to finish (CPU-side) and releases every buffered
		// slot's task. Only used internally now, by Shutdown() and ResizeWindow() - both need
		// "nothing is still recording" as part of a bigger wait before they touch shared state.
		void WaitForCompletion();

		// Stops all async rendering activity (the last RenderAsync task, the render submission
		// thread) and waits for the GPU to go idle. Only called from the destructor now, as its
		// natural, safety-net position in module shutdown order is enough on its own: everything
		// that could unsafely race a worker thread (RemoveWindow, RemoveScene) defers its actual
		// work instead of touching live data immediately, so nothing before RendererModule's own
		// Shutdown() runs needs this to have already happened.
		void Shutdown();

		void RenderAsync(uint renderFrameIndex, uint64 frameNumber);
		void BuildAndExecuteRenderGraph(uint renderFrameIndex, uint64 frameNumber, bool hasValidSwapChainImage);
		void RecordTransferPass(CommandList& cmdList);
		void RecordGeometryPass(CommandList& cmdList, uint renderFrameIndex);
		void RecordGUIPass(CommandList& cmdList, uint renderFrameIndex);

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