#pragma once

#include "Base/Base.h"
#include "RendererMacros.h"
#include "RenderAPI/RenderAPITypes.h"
#include "Rendering/RenderGraphTypes.h"

namespace tyr
{
	using ResourceID = uint64;

	enum class RenderResourceType : uint8
	{
		Buffer = 0,
		Texture
	};

	/// Base struct for a graphics resource.
	struct TYR_RENDERER_API RenderResource
	{
		static ResourceID s_NextID;

		static TYR_FORCEINLINE ResourceID GenerateID()
		{
			return s_NextID++;
		}

		void Reset()
		{
			id = GenerateID();
			accessState = BARRIER_ACCESS_NONE;
			queueTypeState = CommandQueueType::CQ_GRAPHICS;
			// Write, not Read: a fresh resource has never been touched, so its first-ever usage
			// (even a read) should still get a real barrier rather than being skipped as if it
			// followed a prior read.
			lastAccessType = RenderGraphAccessType::Write;
			lastStageMask = PIPELINE_STAGE_NONE;
		}

		RenderResource()
		{
			Reset();
		}

		ResourceID id;
		BarrierAccess accessState;
		// Which queue last accessed this resource - a same-queue pipeline barrier can't
		// synchronize against a different queue's access, so this is what lets the render
		// graph know to skip the barrier and rely on the cross-queue semaphore wait instead.
		CommandQueueType queueTypeState;
		// Whether the last usage processed for this resource was a read or a write - any usage
		// after a write always needs a barrier even when access flags match. A read after a read
		// only needs one if the new read's pipeline stage isn't already covered by lastStageMask.
		RenderGraphAccessType lastAccessType;
		// Union of every stage a barrier has already made the current access visible to, since
		// the last write - a same-access read at a stage not yet in this mask still needs its
		// own barrier. Reset to the new stage on a write; extended (OR'd) on a read that needed one.
		PipelineStage lastStageMask;
		uint renderGraphIndex;
		RenderResourceType type;
	};
}
