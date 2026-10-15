#pragma once

#include "NavmeshMacros.h"
#include "NavmeshTypes.h"

namespace tyr
{
	// Builds navmeshes, in the editor only.
	namespace NavmeshBuilder
	{
		// Builds a navmesh from world-space triangles, three points each, in the form
		// NavmeshManager::CreateNavmesh reads. Triangles can wind either way: any flat enough is
		// walkable. Only floors inside one of the bounds are kept, though everything still blocks;
		// without bounds the whole of it is. False when there's nothing to walk on.
		TYR_NAVMESH_API bool Build(const Vector3* triangles, uint triangleCount, const NavmeshBounds* bounds, uint boundsCount,
			const NavmeshBuildSettings& settings, Array<uint8>& outData);
	}
}
