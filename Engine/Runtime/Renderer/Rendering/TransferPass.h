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

		void Setup(RenderGraphBuilder& builder);
		// renderFrame/data are taken here as parameters, not stored on this object - see
		// GUIPass::Execute's own comment on why a single long-lived instance reused every tick
		// must not cache per-tick state like this across a Recreate()/Execute() pair: nothing
		// guarantees a later tick's Recreate() can't run before this tick's own Execute() does,
		// and unlike GUIPass's symptom (a wrong vertex/index base), the equivalent bug here would
		// have this pass upload a completely different (and possibly not-yet-populated) tick's
		// data into this tick's destination buffers, or skip this tick's own uploads entirely.
		void Execute(CommandList& cmdList, RenderFrame& renderFrame, RenderData& data);

	private:
		Device* m_Device;
		RenderRegistry* m_Registry;
		RenderResources* m_Resources;
		bool m_Initialized;

	};

}