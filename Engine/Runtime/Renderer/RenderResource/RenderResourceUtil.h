#pragma once

#include "RenderAPI/GraphicsBase.h"
#include "RenderAPI/RenderAPITypes.h"
#include "RenderBase/RenderHandles.h"
#include "RenderAPI/Sync.h"

namespace tyr
{
	class Device;
	struct BufferBarrier;
	struct RenderBufferDesc;
	struct TextureDesc;
	struct RenderBuffer;
	struct Texture;

	class RenderResourceUtil final
	{
	public:
		static void InitializeRenderBuffer(RenderBuffer& buffer, Device& device, const RenderBufferDesc& desc);

		static void DeinitializeRenderBuffer(RenderBuffer& buffer, Device& device);

		static void WriteUploadBuffer(RenderBuffer& buffer, Device& device, size_t offset, void* data, size_t dataSize);

		static void CreateTransferWriteBarrier(BufferBarrier& barrier, BufferHandle buffer, size_t size = TYR_BUFFER_BARRIER_WHOLE_SIZE, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		static void CreateTransferReadBarrier(BufferBarrier& barrier, BufferHandle buffer, size_t size = TYR_BUFFER_BARRIER_WHOLE_SIZE, uint srcQueueFamilyIndex = QUEUE_FAMILY_IGNORED);

		static void InitializeTexture(Texture& texture, Device& device, const TextureDesc& desc);

		static void DeinitializeTexture(Texture& texture, Device& device);
	};
}
