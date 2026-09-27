#pragma once

#include "Core.h"
#include "RendererMacros.h"

namespace tyr
{
	class Device;
	class RenderData;
	class CommandList;
	class RenderRegistry;
	class RenderGraphBuilder;
	struct RenderResources;
	struct RenderFrame;

	struct TransferPassArgs
	{
		Device* device;
		RenderData* data;
		RenderRegistry* registry;
		RenderResources* resources;
		RenderFrame* renderFrame;
	};

	// A pass that executes the transfer of data from the cpu to gpu
	class TransferPass final : public INonCopyable
	{
	public:
		// The scene passed can be nullptr if this pass instance is needed by more than one scene
		TransferPass(const TransferPassArgs& args);
		~TransferPass();

		void Recreate(const TransferPassArgs& args);

		void Setup(RenderGraphBuilder& builder);
		void Execute(CommandList& cmdList);

	private:
		Device* m_Device;
		RenderData* m_Data;
		RenderRegistry* m_Registry;
		RenderResources* m_Resources;
		RenderFrame* m_RenderFrame;
		bool m_Initialized;

	};

}