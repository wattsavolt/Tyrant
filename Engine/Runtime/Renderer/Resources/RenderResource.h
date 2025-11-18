#pragma once

#include "Base/Base.h"
#include "RendererMacros.h"
#include "RenderAPI/RenderAPITypes.h"

namespace tyr
{
	using ResourceID = uint64;

	/// Base class for a graphics resource.
	struct RenderResource
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
	};
}
