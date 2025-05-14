
#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"

namespace tyr
{
	struct BufferDesc
	{
#if TYR_DEBUG
		String debugName;
#endif
		BufferUsage usage = BUFFER_USAGE_UNKNOWN;
		MemoryProperty memoryProperty;
		SharingMode sharingMode = SharingMode::Exclusive;
		size_t size = 0;
		// Stride only needed for index buffers
		uint stride = 0;
		// Following three members are only needed for D3D12 image buffers
		uint imageWidth = 0;
		uint imageHeight = 0;
		uint pixelSize = 0;
	};
	
	struct BufferCopyInfo
	{
		size_t srcOffset;
		size_t dstOffset;
		size_t size;
	};

	/// Class repesenting a buffer in vulkan and a heap in D3D12
	class TYR_GRAPHICS_EXPORT Buffer : public AtomicRefCountedObject
	{
	public:
		Buffer(const BufferDesc& desc);
		virtual ~Buffer() = default;

		///  Functions for CPU accessible buffer only - start ///
		virtual uint8* Map() const = 0;
		virtual void Unmap() = 0;
		virtual void Write(const void* data, size_t offset, size_t size) = 0;
		virtual void Read(void* data, size_t offset, size_t size) = 0;

		const BufferDesc& GetDesc() const { return m_Desc; }
		virtual Handle GetNativePtr() const = 0;

		// Returns the size allocated for the buffer in bytes.
		size_t GetSizeAllocated() const { return m_SizeAllocated; };

	protected:
		BufferDesc m_Desc;
		size_t m_SizeAllocated;
	};

	struct BufferViewDesc
	{
		SRef<Buffer> buffer;
		size_t offset;
		size_t size;
	};

	/// Class repesenting a CPU or GPU buffer view
	class TYR_GRAPHICS_EXPORT BufferView : public AtomicRefCountedObject
	{
	public:
		BufferView(const BufferViewDesc& desc);
		virtual ~BufferView() = default;

		const BufferViewDesc& GetDesc() const { return m_Desc; }
	protected:
		BufferViewDesc m_Desc;
	};
}
