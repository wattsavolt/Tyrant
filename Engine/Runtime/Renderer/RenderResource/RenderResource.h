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
		}

		RenderResource()
		{
			Reset();
		}

		ResourceID id;
		BarrierAccess accessState;
		uint renderGraphIndex;
		RenderResourceType type;
	};
}
