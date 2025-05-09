/** Copyright (c) 2019-2021 Aidan Clear */
#pragma once

#include "GraphicsBase.h"
#include "RenderAPITypes.h"

namespace tyr
{
	struct CommandAllocatorDesc
	{
		const char* debugName = "";
		CommandAllocatorCreateFlags flags = CommandAllocatorCreateFlags::COMMAND_ALLOC_CREATE_RESET_COMMAND_BUFFER_BIT;
		CommandQueueType queueType = CommandQueueType::CQ_GRAPHICS;
	};

	/// Class repesenting a command queue 
	class TYR_GRAPHICS_EXPORT CommandAllocator
	{
	public:
		CommandAllocator(const CommandAllocatorDesc& desc);
		virtual ~CommandAllocator() = default;
		virtual void Reset() = 0;

		const CommandAllocatorDesc& GetDesc() const { return m_Desc; }
	protected:
		CommandAllocatorDesc m_Desc;
	};
}
