#pragma once

#include "RendererMacros.h"
#include "Core.h"

namespace tyr
{
	class Vertex;
	class TYR_RENDERER_EXPORT MeshUtil
	{
	public:
		static void CreateNormalsAndTangents(Vertex* vertices, uint numVertices, const uint* indices, uint numIndices);
	};
}