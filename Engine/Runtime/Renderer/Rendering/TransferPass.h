#pragma once

#include "Core.h"
#include "RendererMacros.h"

namespace tyr
{
	class Device;
	class RenderData;
	class CommandList;

	struct TransferPassArgs
	{
		Device* device;
		RenderData* data;
	};

	// A pass that executes the transfer of data from the cpu to gpu
	class TransferPass final : public INonCopyable
	{
	public:
		// The scene passed can be nullptr if this pass instance is needed by more than one scene
		TransferPass(const TransferPassArgs& args);
		~TransferPass();

	private:
		Device* m_Device;
		RenderData* m_Data;
		bool m_Initialized;
		
	};
	
}