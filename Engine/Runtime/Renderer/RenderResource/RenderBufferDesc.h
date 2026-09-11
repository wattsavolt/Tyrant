#pragma once

#include "RenderAPI/RenderAPITypes.h"

namespace tyr
{
	enum class RenderBufferUsage
	{
		Uknown = 0,
		Upload,
		UniformTexel,
		StorageTexel,
		Uniform,
		Storage,
		Index,
		Vertex,
		IndexAndVertex,
		RayTracing
	};

	struct RenderBufferDesc
	{
		TYR_DECLARE_GDEBUGNAME(debugName);
		RenderBufferUsage usage = RenderBufferUsage::Uniform;
		size_t size = 0;
		// Stride only needed for index buffers
		uint stride = 0;
	};
}
