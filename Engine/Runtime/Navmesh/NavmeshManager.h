#pragma once

#include "NavmeshMacros.h"
#include "NavmeshTypes.h"

namespace tyr
{
	struct NavmeshData;

	// Holds the navmeshes worlds walk on and finds paths across them.
	class TYR_NAVMESH_API NavmeshManager final : INonCopyable
	{
	public:
		NavmeshManager();
		~NavmeshManager();

		// Makes a navmesh from data NavmeshBuilder made. Invalid when the data can't be read.
		Handle CreateNavmesh(const uint8* data, size_t size);
		void DestroyNavmesh(Handle navmesh);

		// The nearest point on the navmesh within searchExtents of the point, along each axis.
		bool FindNearestPoint(Handle navmesh, const Vector3& point, const Vector3& searchExtents, Vector3& outPoint) const;

		// The straightest path along the navmesh from near the start to near the end. When the end
		// can't be reached, the path goes as close to it as it can and is marked partial.
		bool FindPath(Handle navmesh, const Vector3& start, const Vector3& end, NavPath& outPath) const;

		// Each polygon's edges within radius of the centre, as pairs of points, for drawing.
		void AddDebugLines(Handle navmesh, const Vector3& centre, float radius, Array<Vector3>& outLines) const;

	private:
		LocalObjectPool<NavmeshData*, NavmeshConstants::c_MaxNavmeshes> m_Navmeshes;
	};
}
