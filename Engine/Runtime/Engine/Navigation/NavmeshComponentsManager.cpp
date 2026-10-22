#include "NavmeshComponentsManager.h"
#include "NavmeshModule.h"
#include "NavmeshManager.h"
#include "ECS/Components.h"
#include "World/World.h"
#include <algorithm>
#if TYR_EDITOR
#include "Build/NavmeshBuilder.h"
#include "PhysicsShapes.h"
#include "Physics/PhysicsComponentsManager.h"
#endif

namespace tyr
{
#if TYR_EDITOR
	namespace
	{
		// A door zone's size about the hinge, in metres: behind it along the wall, into the doorway,
		// and out from the wall on each side.
		constexpr float c_DoorZoneBehind = 1.0f;
		constexpr float c_DoorZoneAhead = 0.3f;
		constexpr float c_DoorZoneDepth = 1.0f;
	}
#endif

	NavmeshComponentsManager::NavmeshComponentsManager()
	{
		NavmeshModule* navmeshModule;
		TYR_GET_MODULE(NavmeshModule, navmeshModule);
		m_NavmeshManager = navmeshModule->GetNavmeshManager();
	}

	void NavmeshComponentsManager::Sync(World& world)
	{
		if (world.navmeshChanged)
		{
			Release(world);
			world.navmeshChanged = false;
		}
		if (world.navmesh || world.navmeshData.IsEmpty())
		{
			return;
		}
		world.navmesh = m_NavmeshManager->CreateNavmesh(world.navmeshData.Data(), world.navmeshData.Size());
		// Data that can't be read is dropped, so it isn't tried again every frame.
		if (!world.navmesh)
		{
			world.navmeshData.Clear();
		}
	}

	void NavmeshComponentsManager::Release(World& world)
	{
		if (world.navmesh)
		{
			m_NavmeshManager->DestroyNavmesh(world.navmesh);
			world.navmesh = {};
		}
	}

#if TYR_EDITOR
	bool NavmeshComponentsManager::Build(World& world, const AssetManager& assetManager)
	{
		m_Triangles.Clear();
		uint skipped = 0;
		world.entities.ForEach<ColliderComponent>([&](Entity entity, const ColliderComponent& collider)
		{
			if (!world.entities.HasComponent<TransformComponent>(entity))
			{
				return;
			}
			if (world.entities.HasComponent<RigidBodyComponent>(entity)
				&& static_cast<MotionType>(world.entities.GetComponent<RigidBodyComponent>(entity).motionType) != MotionType::Static)
			{
				return;
			}

			const Transform& transform = world.entities.GetComponent<TransformComponent>(entity).world;
			const PhysicsShape* shape = PhysicsComponentsManager::CreateColliderShape(collider, transform.scale, MotionType::Static, assetManager);
			if (!shape)
			{
				++skipped;
				return;
			}
			PhysicsShapes::AddTriangles(shape, transform.position, transform.rotation, m_Triangles);
			PhysicsShapes::Release(shape);
		});

		if (skipped > 0)
		{
			TYR_LOG_WARNING("%u colliders were left out of the navmesh, as they have no size or their collision mesh hasn't loaded.", skipped);
		}

		// Each bounds box, as the world-space box around its turned corners.
		m_Bounds.Clear();
		world.entities.ForEach<NavmeshBoundsComponent>([&](Entity entity, const NavmeshBoundsComponent& boundsComponent)
		{
			if (!world.entities.HasComponent<TransformComponent>(entity))
			{
				return;
			}
			const Transform& transform = world.entities.GetComponent<TransformComponent>(entity).world;
			NavmeshBounds& bounds = m_Bounds.ExpandOne();
			for (uint corner = 0; corner < 8; ++corner)
			{
				const Vector3 extent = boundsComponent.halfExtents * transform.scale;
				const Vector3 offset((corner & 1) ? extent.x : -extent.x, (corner & 2) ? extent.y : -extent.y, (corner & 4) ? extent.z : -extent.z);
				const Vector3 point = transform.position + transform.rotation.Rotate(offset);
				bounds.min = corner == 0 ? point : Vector3(std::min(bounds.min.x, point.x), std::min(bounds.min.y, point.y), std::min(bounds.min.z, point.z));
				bounds.max = corner == 0 ? point : Vector3(std::max(bounds.max.x, point.x), std::max(bounds.max.y, point.y), std::max(bounds.max.z, point.z));
			}
		});

		// Beside each swinging door's hinge, either side of its wall, where its leaf stands once open: enemies
		// don't stop there or brush past it, so their arms don't go through the leaf. The rest of the
		// doorway is left for walking through.
		m_Zones.Clear();
		world.entities.ForEach<DoorComponent>([&](Entity entity, const DoorComponent& door)
		{
			if (static_cast<DoorMotion>(door.motion) != DoorMotion::Swing || !world.entities.HasComponent<TransformComponent>(entity))
			{
				return;
			}
			const Transform& transform = world.entities.GetComponent<TransformComponent>(entity).world;
			// The leaf runs along its local x, the way its box is offset, from the hinge at its origin.
			const float leafSide = world.entities.HasComponent<BoxComponent>(entity) && world.entities.GetComponent<BoxComponent>(entity).center.x < 0.0f ? -1.0f : 1.0f;
			const float along[4] = { -c_DoorZoneBehind, c_DoorZoneAhead, c_DoorZoneAhead, -c_DoorZoneBehind };
			const float across[4] = { -c_DoorZoneDepth, -c_DoorZoneDepth, c_DoorZoneDepth, c_DoorZoneDepth };
			NavmeshZone& zone = m_Zones.ExpandOne();
			for (uint c = 0; c < 4; ++c)
			{
				zone.corners[c] = transform.position + transform.rotation.Rotate(Vector3(along[c] * leafSide, 0.0f, across[c]));
			}
			zone.minY = transform.position.y - 0.5f;
			zone.maxY = transform.position.y + 2.0f;
		});

		// Area boxes, flattened to their four lower corners, between their lowest and highest points.
		world.entities.ForEach<NavmeshAreaComponent>([&](Entity entity, const NavmeshAreaComponent& areaComponent)
		{
			if (!world.entities.HasComponent<TransformComponent>(entity))
			{
				return;
			}
			const Transform& transform = world.entities.GetComponent<TransformComponent>(entity).world;
			const Vector3 extent = areaComponent.halfExtents * transform.scale;
			const float x[4] = { -extent.x, extent.x, extent.x, -extent.x };
			const float z[4] = { -extent.z, -extent.z, extent.z, extent.z };
			NavmeshZone& zone = m_Zones.ExpandOne();
			zone.area = areaComponent.area;
			for (uint c = 0; c < 4; ++c)
			{
				zone.corners[c] = transform.position + transform.rotation.Rotate(Vector3(x[c], 0.0f, z[c]));
			}
			zone.minY = transform.position.y - extent.y;
			zone.maxY = transform.position.y + extent.y;
		});

		Release(world);
		const NavmeshBuildSettings settings;
		if (!NavmeshBuilder::Build(m_Triangles.Data(), m_Triangles.Size() / 3, m_Bounds.Data(), m_Bounds.Size(), m_Zones.Data(), m_Zones.Size(),
			settings, world.navmeshData))
		{
			TYR_LOG_WARNING("The navmesh has nothing to walk on, so the level has none.");
		}
		++world.changeCount;
		Sync(world);
		return world.navmesh ? true : false;
	}
#endif

	bool NavmeshComponentsManager::HasNavmesh(const World& world) const
	{
		return world.navmesh ? true : false;
	}

	bool NavmeshComponentsManager::FindPath(const World& world, const Vector3& start, const Vector3& end, bool useStairs, NavPath& outPath) const
	{
		return world.navmesh && m_NavmeshManager->FindPath(world.navmesh, start, end, useStairs, outPath);
	}

	bool NavmeshComponentsManager::FindRandomPoint(const World& world, const Vector3& centre, float radius, Vector3& outPoint) const
	{
		return world.navmesh && m_NavmeshManager->FindRandomPoint(world.navmesh, centre, radius, outPoint);
	}

	void NavmeshComponentsManager::AddDebugLines(const World& world, const Vector3& centre, float radius, Array<Vector3>& outLines) const
	{
		if (world.navmesh)
		{
			m_NavmeshManager->AddDebugLines(world.navmesh, centre, radius, outLines);
		}
	}
}
