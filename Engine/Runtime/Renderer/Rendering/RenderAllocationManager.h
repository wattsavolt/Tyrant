#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "Rendering/RenderConstants.h"
#include "RenderTransfer/UploadRequest.h"
#include "Memory/PoolHandle.h"

namespace tyr
{
	struct UploadBufferAllocation;
	struct BufferAllocation;
	struct MeshLODAllocInfo;
	class Device;
	class RenderRegistry;
	class ResourceUploadAllocator;
	class FrameUploadAllocator;
	class LodAllocator;
	class GpuBufferAllocator;

	// Class that manages long-lived render resources
	class RenderAllocationManager final : INonCopyable
	{
	public:
		static constexpr size_t c_ResourceUploadBufferSize = 512 * 1024 * 1024; // 512 MB
		static constexpr size_t c_FrameUploadBufferSize = 4 * 1024 * 1024 * RenderConstants::c_BufferedFrameCount; // 4 MB per frame * buffered frame count

		RenderAllocationManager();
		~RenderAllocationManager();

		bool RequestResourceUploadAllocation(size_t size, UploadBufferAllocation& allocation);

		// Only ever called from the main thread, once a submission's timeline value is known.
		void SignalResourceUpload(Handle resourceId, uint64 timelineValue);

		void ReclaimResourceUploadMemory(uint64 timelineValue);

		// Only safe once the caller has confirmed the GPU is fully idle.
		void ReclaimAllResourceUploadMemory();

		bool RequestFrameUploadAllocation(size_t size, UploadBufferAllocation& allocation);

		// Call once per Render() tick, right after that tick's own RequestFrameUploadAllocation
		// calls are done (regardless of whether it actually made any).
		void RecordFrameUploadCheckpoint(uint64 frameNumber);

		// Only ever called from the main thread, once a completion (not just any submission)
		// reports frameNumber/timelineValue together.
		void SignalFrameUpload(uint64 frameNumber, uint64 timelineValue);

		void ReclaimFrameUploadMemory(uint64 timelineValue);

		uint AllocateMeshLODs();

		void FreeMeshLODs(uint offset, uint lodCount);

		const MeshLODAllocInfo& GetMeshLODAllocInfo(uint offset) const
		{
			return m_MeshLODAllocInfos[offset];
		}

		MeshLODAllocInfo& GetMeshLODAllocInfo(uint offset)
		{
			return m_MeshLODAllocInfos[offset];
		}

		bool RequestVertexBufferAllocation(size_t size, BufferAllocation& allocation);

		void FreeVertexBufferAllocation(const BufferAllocation& allocation);

		bool RequestIndexBufferAllocation(size_t size, BufferAllocation& allocation);

		void FreeIndexBufferAllocation(const BufferAllocation& allocation);

		bool RequestMeshletBufferAllocation(size_t size, BufferAllocation& allocation);

		void FreeMeshletBufferAllocation(const BufferAllocation& allocation);

		// Suballocates from one shared buffer instead of each BLAS getting its own dedicated
		// buffer.
		bool RequestBLASStorageAllocation(size_t size, BufferAllocation& allocation);

		void FreeBLASStorageAllocation(const BufferAllocation& allocation);

	private:
		RenderRegistry& m_Registry;

		// Textures and mesh geometry
		RenderBufferHandle m_ResourceUploadBuffer;
		// Materials, lights, instance data and constants
		RenderBufferHandle m_FrameUploadBuffer;
		ResourceUploadAllocator* m_ResourceUploadAllocator;
		FrameUploadAllocator* m_FrameUploadAllocator;
		LodAllocator* m_MeshLODBufferAllocator;
		Array<MeshLODAllocInfo> m_MeshLODAllocInfos;
		GpuBufferAllocator* m_VertexBufferAllocator;
		GpuBufferAllocator* m_IndexBufferAllocator;
		GpuBufferAllocator* m_MeshletBufferAllocator;
		GpuBufferAllocator* m_BLASStorageAllocator;
	};
	
}