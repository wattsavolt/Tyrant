#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"

namespace tyr
{
	struct BufferHandle : public ResourceHandle {};
	struct BufferViewHandle : public ResourceHandle {};

	struct BufferDesc
	{
#if !TYR_FINAL
		GDebugString debugName;
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

	struct BufferViewDesc
	{
#if !TYR_FINAL
		GDebugString debugName;
#endif
		BufferHandle buffer;
		size_t offset;
		size_t size;
	};
}
