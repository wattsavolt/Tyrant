#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "Rendering/RenderConstants.h"
#include "RenderTransfer/UploadRequest.h"

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

		void SignalResourceUpload(uint64 timelineValue);

		void ReclaimResourceUploadMemory(uint64 timelineValue);

		bool RequestFrameUploadAllocation(size_t size, UploadBufferAllocation& allocation);

		void SignalFrameUpload(uint64 timelineValue);

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
	};
	
}