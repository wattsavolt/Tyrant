#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"

namespace tyr
{
	TYR_CREATE_HANDLE_TYPE(BufferHandle);
	TYR_CREATE_HANDLE_TYPE(BufferViewHandle);

	struct BufferDesc
	{
		TYR_DECLARE_GDEBUGNAME(debugName);
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
		TYR_DECLARE_GDEBUGNAME(debugName);
		BufferHandle buffer;
		size_t offset;
		size_t size;
	};
}
