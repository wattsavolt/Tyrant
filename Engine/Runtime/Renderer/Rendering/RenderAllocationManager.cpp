#include "RenderAllocationManager.h"
#include "RenderAPI/RenderAPI.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Image.h"
#include "Shaders/ShaderTypes.h"
#include "RenderResource/Mesh.h"
#include "RenderTransfer/RenderTransferTypes.h"
#include "RenderTransfer/BufferAllocation.h"
#include "RenderTransfer/ResourceUploadAllocator.h"
#include "RenderTransfer/FrameUploadAllocator.h"
#include "RenderTransfer/LodAllocator.h"
#include "RenderTransfer/GpuBufferAllocator.h"
#include "RenderRegistry.h"

namespace tyr
{
	RenderAllocationManager::RenderAllocationManager()
		: m_Registry(*RenderRegistry::Instance())
		, m_MeshLODAllocInfos(1024)
	{
		{
			RenderBufferDesc desc{};
			desc.usage = RenderBufferUsage::Upload;
			desc.debugName = "Resource Upload Buffer";
			desc.size = c_ResourceUploadBufferSize;
			m_ResourceUploadBuffer = m_Registry.CreateBuffer(desc);
		}

		{
			RenderBufferDesc desc{};
			desc.usage = RenderBufferUsage::Upload;
			desc.debugName = "Frame Upload Buffer";
			desc.size = c_FrameUploadBufferSize;
			m_FrameUploadBuffer = m_Registry.CreateBuffer(desc);
		}

		m_ResourceUploadAllocator = new ResourceUploadAllocator({ c_ResourceUploadBufferSize, m_Registry.GetBuffer(m_ResourceUploadBuffer).mappedMemory });
		m_FrameUploadAllocator = new FrameUploadAllocator({ c_FrameUploadBufferSize, m_Registry.GetBuffer(m_FrameUploadBuffer).mappedMemory });
		m_MeshLODBufferAllocator = new LodAllocator();
		m_VertexBufferAllocator = new GpuBufferAllocator({ RenderConstants::c_VertexBufferSize });
		m_IndexBufferAllocator = new GpuBufferAllocator({ RenderConstants::c_IndexBufferSize });
		m_MeshletBufferAllocator = new GpuBufferAllocator({ RenderConstants::c_MeshletBufferSize });
	}

	RenderAllocationManager::~RenderAllocationManager()
	{
		m_Registry.DeleteBuffer(m_ResourceUploadBuffer);
		m_Registry.DeleteBuffer(m_FrameUploadBuffer);

		delete m_ResourceUploadAllocator;
		delete m_FrameUploadAllocator;
		delete m_MeshLODBufferAllocator;
		delete m_VertexBufferAllocator;
		delete m_IndexBufferAllocator;
		delete m_MeshletBufferAllocator;
	}

	bool RenderAllocationManager::RequestResourceUploadAllocation(size_t size, UploadBufferAllocation& allocation)
	{
		RenderBuffer& buffer = m_Registry.GetBuffer(m_ResourceUploadBuffer);

		ResourceUploadAllocator::Allocation alloc;
		const bool success = m_ResourceUploadAllocator->Allocate(size, RenderConstants::c_UploadAlignment, alloc);

		if (success)
		{
			allocation.buffer = m_ResourceUploadBuffer;
			allocation.cpuPtr = buffer.mappedMemory;
			allocation.offset = alloc.offset;
			allocation.size = alloc.size;
		}
	
		return success;
	}

	void RenderAllocationManager::SignalResourceUpload(uint64 timelineValue)
	{
		m_ResourceUploadAllocator->Signal(timelineValue);
	}

	void RenderAllocationManager::ReclaimResourceUploadMemory(uint64 timelineValue)
	{
		m_ResourceUploadAllocator->Reclaim(timelineValue);
	}

	bool RenderAllocationManager::RequestFrameUploadAllocation(size_t size, UploadBufferAllocation& allocation)
	{
		RenderBuffer& buffer = m_Registry.GetBuffer(m_FrameUploadBuffer);

		FrameUploadAllocator::Allocation alloc;
		const bool success = m_FrameUploadAllocator->Allocate(size, RenderConstants::c_UploadAlignment, alloc);

		if (success)
		{
			allocation.buffer = m_FrameUploadBuffer;
			allocation.cpuPtr = alloc.cpuPtr;
			allocation.offset = alloc.offset;
			allocation.size = alloc.size;
		}

		return success;
	}

	void RenderAllocationManager::SignalFrameUpload(uint64 timelineValue)
	{
		m_FrameUploadAllocator->Signal(timelineValue);
	}

	void RenderAllocationManager::ReclaimFrameUploadMemory(uint64 timelineValue)
	{
		m_FrameUploadAllocator->Reclaim(timelineValue);
	}

	uint RenderAllocationManager::AllocateMeshLODs()
	{
		const uint offset = m_MeshLODBufferAllocator->Allocate();
		// Max lods must be used here instead of the mesh's lod count as the lod allocator uses chunks of size max lods to calculate the offsets
		const uint newSize = offset + MeshConstants::c_MaxLods;
		if (newSize > m_MeshLODAllocInfos.Size())
		{
			m_MeshLODAllocInfos.Resize(newSize);
		}
		return offset;
	}

	void RenderAllocationManager::FreeMeshLODs(uint offset, uint lodCount)
	{
		for (uint i = offset; i < lodCount; ++i)
		{
			m_MeshLODAllocInfos[i] = {};
		}
		m_MeshLODBufferAllocator->Free(offset);
	}

	bool RenderAllocationManager::RequestVertexBufferAllocation(size_t size, BufferAllocation& allocation)
	{
		return m_VertexBufferAllocator->Allocate(size, alignof(ShaderVertex), allocation);
	}

	void RenderAllocationManager::FreeVertexBufferAllocation(const BufferAllocation& allocation)
	{
		m_VertexBufferAllocator->Free(allocation);
	}

	bool RenderAllocationManager::RequestIndexBufferAllocation(size_t size, BufferAllocation& allocation)
	{
		return m_IndexBufferAllocator->Allocate(size, alignof(uint), allocation);
	}

	void RenderAllocationManager::FreeIndexBufferAllocation(const BufferAllocation& allocation)
	{
		m_IndexBufferAllocator->Free(allocation);
	}

	bool RenderAllocationManager::RequestMeshletBufferAllocation(size_t size, BufferAllocation& allocation)
	{
		return m_MeshletBufferAllocator->Allocate(size, alignof(ShaderMeshlet), allocation);
	}

	void RenderAllocationManager::FreeMeshletBufferAllocation(const BufferAllocation& allocation)
	{
		m_MeshletBufferAllocator->Free(allocation);
	}
}