#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "NavmeshTypes.h"

namespace tyr
{
	struct World;
	class AssetManager;
	class NavmeshManager;

	// Keeps each World's navmesh: built in the editor from the level's colliders, saved with the
	// level, and loaded from it for finding paths.
	class TYR_ENGINE_API NavmeshComponentsManager final : INonCopyable
	{
	public:
		NavmeshComponentsManager();

		// Loads the world's navmesh once it has data for one, such as after its level loads.
		void Sync(World& world);
		// Lets go of the world's navmesh, before the world is cleared or removed.
		void Release(World& world);

#if TYR_EDITOR
		// Builds the world's navmesh from everything that doesn't move, replacing any it had.
		// Moving things, like doors, are left out, so paths go through doorways.
		bool Build(World& world, const AssetManager& assetManager);
#endif

		bool HasNavmesh(const World& world) const;
		bool FindPath(const World& world, const Vector3& start, const Vector3& end, NavPath& outPath) const;
		// The navmesh's polygon edges within radius of the centre, as pairs of points, for drawing.
		void AddDebugLines(const World& world, const Vector3& centre, float radius, Array<Vector3>& outLines) const;

	private:
		NavmeshManager* m_NavmeshManager = nullptr;
#if TYR_EDITOR
		// Reused between builds, so building again doesn't allocate once it's grown to fit.
		Array<Vector3> m_Triangles;
		Array<NavmeshBounds> m_Bounds;
#endif
	};
}
