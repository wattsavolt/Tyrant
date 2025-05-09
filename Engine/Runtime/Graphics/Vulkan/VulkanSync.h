
#pragma once

#include "RenderAPI/Sync.h"
#include "VulkanDevice.h"

namespace tyr
{
	class VulkanFence final : public Fence
	{
	public:
		VulkanFence(VulkanDevice& device, const FenceDesc& desc);
		~VulkanFence();

		void Reset() override;
		bool GetStatus() const override;
		bool Wait() override;

		VkFence GetFence() const { return m_Fence; }

	private:
		VkFence m_Fence;
		VulkanDevice& m_Device;
	};

	class VulkanSemaphore final : public Semaphore
	{
	public:
		VulkanSemaphore(VulkanDevice& device, const SemaphoreDesc& desc);
		~VulkanSemaphore();

		void Signal(uint64 value) override;
		uint64 GetValue() const override;
		bool Wait(uint64 value, uint64 timeout) override;

		VkSemaphore GetSemaphore() const { return m_Semaphore; }

	private:
		VkSemaphore m_Semaphore;
		VulkanDevice& m_Device;
	};

	class VulkanEvent final : public Event
	{
	public:
		VulkanEvent(VulkanDevice& device, const EventDesc& desc);
		~VulkanEvent();

		bool Set() override;
		void Reset() override;
		bool IsSet() const override;

		VkEvent GetEvent() const { return m_Event; }

	private:
		VkEvent m_Event;
		VulkanDevice& m_Device;
	};
}
