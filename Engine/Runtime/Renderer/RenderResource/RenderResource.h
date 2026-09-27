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
		uint renderGraphIndex;
		RenderResourceType type;
	};
}
