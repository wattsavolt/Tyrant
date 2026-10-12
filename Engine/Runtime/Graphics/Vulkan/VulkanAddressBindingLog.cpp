#include "VulkanAddressBindingLog.h"
#include <cstdio>
#include <cstring>

#if !TYR_FINAL
namespace tyr
{
	VulkanAddressBindingLog::Entry VulkanAddressBindingLog::s_Entries[VulkanAddressBindingLog::c_MaxEntries];
	uint VulkanAddressBindingLog::s_RecordCount = 0;
	Mutex VulkanAddressBindingLog::s_Mutex;

	void VulkanAddressBindingLog::Record(const VkDeviceAddressBindingCallbackDataEXT& binding, const VkDebugUtilsMessengerCallbackDataEXT& message)
	{
		LockGuard lock(s_Mutex);
		Entry& entry = s_Entries[s_RecordCount % c_MaxEntries];
		++s_RecordCount;
		entry.base = binding.baseAddress;
		entry.size = binding.size;
		entry.bound = binding.bindingType == VK_DEVICE_ADDRESS_BINDING_TYPE_BIND_EXT;
		entry.objectHandle = message.objectCount > 0 ? message.pObjects[0].objectHandle : 0;
		entry.objectType = message.objectCount > 0 ? message.pObjects[0].objectType : VK_OBJECT_TYPE_UNKNOWN;
		const char* name = message.objectCount > 0 && message.pObjects[0].pObjectName ? message.pObjects[0].pObjectName : "";
		snprintf(entry.name, sizeof(entry.name), "%s", name);
	}

	void VulkanAddressBindingLog::LogBindingsAt(uint64 address, uint64 precision)
	{
		LockGuard lock(s_Mutex);
		const uint count = s_RecordCount < c_MaxEntries ? s_RecordCount : c_MaxEntries;
		uint logged = 0;
		for (uint i = 0; i < count && logged < c_MaxLoggedEntries; ++i)
		{
			const Entry& entry = s_Entries[(s_RecordCount - 1 - i) % c_MaxEntries];
			// The fault is only known to within its precision, so any overlap counts.
			if (address + precision <= entry.base || address >= entry.base + entry.size)
			{
				continue;
			}
			TYR_LOG_ERROR("GPU fault address is in '%s' (Vulkan object type %d, handle 0x%llx) at 0x%llx to 0x%llx, which was %s %u bindings ago.",
				entry.name, static_cast<int>(entry.objectType), static_cast<unsigned long long>(entry.objectHandle), static_cast<unsigned long long>(entry.base),
				static_cast<unsigned long long>(entry.base + entry.size), entry.bound ? "bound" : "freed", i);
			++logged;
		}
		if (logged > 0)
		{
			return;
		}

		// A read just past the end of an object usually means an index out of its range.
		const Entry* below = nullptr;
		for (uint i = 0; i < count; ++i)
		{
			const Entry& entry = s_Entries[i];
			const bool endsBelow = entry.base + entry.size <= address;
			below = endsBelow && (!below || entry.base + entry.size > below->base + below->size) ? &entry : below;
		}
		if (!below)
		{
			TYR_LOG_ERROR("None of the last %u bindings covers or comes before the GPU fault address.", count);
			return;
		}
		TYR_LOG_ERROR("None of the last %u bindings covers the GPU fault address. The nearest below is '%s' (Vulkan object type %d, %s) at 0x%llx to 0x%llx, 0x%llx bytes before it.",
			count, below->name, static_cast<int>(below->objectType), below->bound ? "bound" : "freed", static_cast<unsigned long long>(below->base),
			static_cast<unsigned long long>(below->base + below->size), static_cast<unsigned long long>(address - (below->base + below->size)));
	}
}
#endif
