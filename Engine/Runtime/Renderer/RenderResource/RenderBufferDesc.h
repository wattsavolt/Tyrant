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
		RayTracing,
		// GPU-written and GPU-read only: indirect draw/dispatch commands, an atomic draw counter,
		// or anything else an indirect draw/dispatch call consumes directly - needs Storage,
		// Indirect, and TransferDst (so vkCmdFillBuffer can reset a counter each frame) together.
		Indirect,
		// Backing storage an acceleration structure object is bound to at creation, never written
		// via a copy/upload - just ACCELERATION_STRUCTURE_STORAGE + device address (a build
		// addresses it directly, not through a descriptor).
		AccelerationStructureStorage
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
