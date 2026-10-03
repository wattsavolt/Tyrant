#pragma once

#include "Core.h"
#include "RendererMacros.h"

namespace tyr
{
	class Device;
	class CommandList;
	class RenderRegistry;
	class RenderGraphBuilder;
	struct RenderResources;
	struct RenderFrame;
	struct RenderData;

	struct TransferPassArgs
	{
		Device* device;
		RenderRegistry* registry;
		RenderResources* resources;
	};

	// A pass that executes the transfer of data from the cpu to gpu
	class TransferPass final : public INonCopyable
	{
	public:
		// The scene passed can be nullptr if this pass instance is needed by more than one scene
		TransferPass(const TransferPassArgs& args);
		~TransferPass();

		void Recreate(const TransferPassArgs& args);

		// renderFrame is taken as a parameter rather than stored, for the same reason Execute's
		// own parameters are - declares a WriteTexture for each of this tick's texture upload
		// requests, so the render graph inserts their UNDEFINED->GENERAL transition.
		void Setup(RenderGraphBuilder& builder, const RenderFrame& renderFrame);
		// renderFrame/data are taken here as parameters, not stored on this object - a reused
		// long-lived instance must not cache per-tick state, since a later tick's Recreate()
		// could run before this tick's own Execute() does.
		void Execute(CommandList& cmdList, RenderFrame& renderFrame, RenderData& data);

	private:
		Device* m_Device;
		RenderRegistry* m_Registry;
		RenderResources* m_Resources;
		bool m_Initialized;

	};

}