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
			if (!world.entities.HasComponent<ComponentTransform>(entity))
			{
				return;
			}
			if (world.entities.HasComponent<RigidBodyComponent>(entity)
				&& static_cast<MotionType>(world.entities.GetComponent<RigidBodyComponent>(entity).motionType) != MotionType::Static)
			{
				return;
			}

			const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
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
			if (!world.entities.HasComponent<ComponentTransform>(entity))
			{
				return;
			}
			const Transform& transform = world.entities.GetComponent<ComponentTransform>(entity).world;
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

		Release(world);
		const NavmeshBuildSettings settings;
		if (!NavmeshBuilder::Build(m_Triangles.Data(), m_Triangles.Size() / 3, m_Bounds.Data(), m_Bounds.Size(), settings, world.navmeshData))
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

	bool NavmeshComponentsManager::FindPath(const World& world, const Vector3& start, const Vector3& end, NavPath& outPath) const
	{
		return world.navmesh && m_NavmeshManager->FindPath(world.navmesh, start, end, outPath);
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
