#pragma once

#include "RenderAPI/AccelerationStructure.h"
#include "RenderResource.h"

namespace tyr
{
	// Persistent cross-tick barrier state for one acceleration structure, so the render graph
	// can track and barrier BLAS/TLAS builds the same way it does buffers.
	struct RenderAccelerationStructure : public RenderResource
	{
		AccelerationStructureHandle accelerationStructure{};
	};
}
