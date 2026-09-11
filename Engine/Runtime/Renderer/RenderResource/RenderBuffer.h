#pragma once

#include "RenderAPI/RenderAPITypes.h"
#include "RenderAPI/Buffer.h"
#include "RenderResource.h"
#include "RenderBufferDesc.h"

namespace tyr
{
	struct RenderBuffer : public RenderResource
	{
		BufferHandle buffer;
		BufferViewHandle bufferView;
		// Only non-null for upload buffers
		void* mappedMemory = nullptr;
	};
}
