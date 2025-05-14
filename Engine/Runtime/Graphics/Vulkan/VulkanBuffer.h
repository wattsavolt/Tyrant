#pragma once

#include "RenderAPI/Buffer.h"
#include "VulkanDevice.h"

namespace tyr
{
	class VulkanBuffer final : public Buffer
	{
	public:
		VulkanBuffer(VulkanDevice& device, const BufferDesc& desc);
		~VulkanBuffer();
		uint8* Map() const override;
		void Unmap() override;
		void Write(const void* data, size_t offset, size_t size) override;
		void Read(void* data, size_t offset, size_t size) override;
		VkBuffer GetBuffer() const { return m_Buffer; }
		Handle GetNativePtr() const override { return static_cast<void*>(m_Buffer); }

	private:
		VulkanDevice& m_Device;
		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;
	};

	class VulkanBufferView : public BufferView
	{
	public:
		VulkanBufferView(VulkanDevice& device, const BufferViewDesc& desc);
		~VulkanBufferView();

		VulkanBuffer* GetBuffer() { return m_Buffer; };

		const VulkanBuffer* GetBuffer() const { return m_Buffer; };

	private:
		VulkanDevice& m_Device;
		VkBufferView m_BufferView;
		// The base class has a shared ptr to the buffer so this can be a raw pointer
		VulkanBuffer* m_Buffer;
	};
}
