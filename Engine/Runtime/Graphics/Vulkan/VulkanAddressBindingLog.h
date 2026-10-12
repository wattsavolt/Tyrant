#pragma once

#include "Core.h"
#include "VulkanCommon.h"

#if !TYR_FINAL
namespace tyr
{
	// Remembers the GPU address ranges recently bound to and freed from memory, buffers and images,
	// as reported by VK_EXT_device_address_binding_report, so a GPU fault's address can be traced
	// back to its object. Recorded from whichever thread binds memory.
	class VulkanAddressBindingLog final
	{
	public:
		static void Record(const VkDeviceAddressBindingCallbackDataEXT& binding, const VkDebugUtilsMessengerCallbackDataEXT& message);
		// Logs the most recent bindings that cover the address, newest first.
		static void LogBindingsAt(uint64 address, uint64 precision);

	private:
		struct Entry
		{
			uint64 base;
			uint64 size;
			uint64 objectHandle;
			VkObjectType objectType;
			bool bound;
			char name[47];
		};

		static constexpr uint c_MaxEntries = 8192;
		static constexpr uint c_MaxLoggedEntries = 8;

		static Entry s_Entries[c_MaxEntries];
		// How many have ever been recorded. The oldest are overwritten once it passes c_MaxEntries.
		static uint s_RecordCount;
		static Mutex s_Mutex;
	};
}
#endif
