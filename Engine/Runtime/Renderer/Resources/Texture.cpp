
#include "Texture.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/Device.h"
#include "TransferBuffer.h"

namespace tyr
{
	Texture::Texture(Device& device, const TextureDesc& desc)
		: RenderResource(device)
	{
		Recreate(desc);
	}

	void Texture::Recreate(const TextureDesc& desc)
	{
		m_IsCubemap = desc.info.type == ImageType::Cubemap || desc.info.type == ImageType::CubemapArray;
		TYR_ASSERT(!m_IsCubemap || desc.arrayLayerCount % 6 == 0);

		m_IsDepthTexture = desc.info.format == PF_D16_UNORM || desc.info.format == PF_D24_UNORM_S8_UINT
			|| desc.info.format == PF_D32_SFLOAT || desc.info.format == PF_D32_FLOAT_S8_UINT;

		ImageDesc imageDesc;
#if TYR_DEBUG
		imageDesc.debugName = String(desc.debugName) + "_Image";;
#endif
		imageDesc.type = desc.info.type;
		imageDesc.format = desc.info.format;
		imageDesc.width = static_cast<uint>(desc.info.width);
		imageDesc.height = static_cast<uint>(desc.info.height);
		imageDesc.depth = static_cast<uint>(desc.info.depth);
		imageDesc.mipLevelCount = static_cast<uint>(desc.info.mipLevelCount);
		imageDesc.arrayLayerCount = desc.arrayLayerCount;
		imageDesc.sampleCount = desc.sampleCount;
		imageDesc.usage = desc.usage;
		imageDesc.layout = desc.layout;
		imageDesc.memoryProperty = MemoryProperty::MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		m_Image = m_Device.CreateImage(imageDesc);

		m_ImageSize = m_Image->GetSizeAllocated();

		m_ImageLayout = desc.layout;

		ImageViewDesc viewDesc;
#if TYR_DEBUG
		viewDesc.debugName = String(desc.debugName) + "_ImageView";
#endif
		viewDesc.image = m_Image;
		viewDesc.isSwapChainView = false;
		viewDesc.viewType = desc.info.type;
		viewDesc.subresourceRange.aspect = m_IsDepthTexture ? SUBRESOURCE_ASPECT_DEPTH_BIT : SUBRESOURCE_ASPECT_COLOUR_BIT;
		viewDesc.subresourceRange.baseMipLevel = 0;
		viewDesc.subresourceRange.mipLevelCount = desc.info.mipLevelCount;
		viewDesc.subresourceRange.baseArrayLayer = 0;
		viewDesc.subresourceRange.arrayLayerCount = desc.arrayLayerCount;

		m_ImageView = m_Device.CreateImageView(viewDesc);

		m_AccessState = BARRIER_ACCESS_NONE;
	}

	Ref<Texture> Texture::Create(Device& device, const TextureDesc& desc)
	{
		return MakeRef<Texture>(device, desc);
	}

	/*void Texture::CreateTransferBuffer(const TextureDesc& desc)
	{
		TransferBufferDesc bufferDesc;
#if TYR_DEBUG 
		bufferDesc.debugName = String(desc.imageDesc.debugName) + "_TransferBuffer";
#endif
		bufferDesc.size = desc.imageDesc.size;
		bufferDesc.data = desc.imageDesc.data;
		bufferDesc.imageWidth = desc.imageDesc.width;
		bufferDesc.imageHeight = desc.imageDesc.height;
		bufferDesc.pixelSize = GetTexelFormatSize(desc.imageDesc.format);

		m_TransferBuffer = TransferBuffer::Create(m_Device, bufferDesc);
	}*/

	uint Texture::GetTexelFormatSize(PixelFormat format)
	{
		switch (format)
		{
		case PF_D16_UNORM:
			return 2;

		case PF_R8G8B8A8_UNORM:
		case PF_R8G8B8A8_SNORM:
		case PF_R8G8B8A8_SSCALED:
		case PF_R8G8B8A8_USCALED:
		case PF_R8G8B8A8_SINT:
		case PF_R8G8B8A8_UINT:
		case PF_R8G8B8A8_SRGB:

		case PF_B8G8R8A8_UNORM:
		case PF_B8G8R8A8_SNORM:
		case PF_B8G8R8A8_SSCALED:
		case PF_B8G8R8A8_USCALED:
		case PF_B8G8R8A8_SINT:
		case PF_B8G8R8A8_UINT:
		case PF_B8G8R8A8_SRGB:

		case PF_D24_UNORM_S8_UINT:
		case PF_D32_SFLOAT:
			return 4;

		case PF_D32_FLOAT_S8_UINT:
			return 5;

		case PF_R16G16B16A16_UNORM:
		case PF_R16G16B16A16_SNORM:
		case PF_R16G16B16A16_SSCALED:
		case PF_R16G16B16A16_USCALED:
		case PF_R16G16B16A16_SINT:
		case PF_R16G16B16A16_UINT:
		case PF_R16G16B16A16_SFLOAT:

		case PF_B16G16R16A16_UNORM:
		case PF_B16G16R16A16_SNORM:
		case PF_B16G16R16A16_SSCALED:
		case PF_B16G16R16A16_USCALED:
		case PF_B16G16R16A16_SINT:
		case PF_B16G16R16A16_UINT:
		case PF_B16G16R16A16_SFLOAT:
			return 8;

		case PF_R32G32B32_UINT:
		case PF_R32G32B32_SINT:
		case PF_R32G32B32_SFLOAT:
			return 12;

		case PF_R32G32B32A32_SINT:
		case PF_R32G32B32A32_UINT:
		case PF_R32G32B32A32_SFLOAT:
			return 16;
		}

		TYR_ASSERT(false);
		return 0;
	}
}