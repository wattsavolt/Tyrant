#include "Triggers.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Math/Math.h"

namespace tyr
{
	namespace
	{
		// The player is tested at about their middle, so a low box on the floor still catches them.
		constexpr float c_TestHeight = 0.9f;
		// Most players in a world at once.
		constexpr uint c_MaxPlayers = 4;

		bool Contains(const Transform& transform, const Vector3& halfExtents, const Vector3& point)
		{
			const Vector3 local = transform.rotation.Inverse().Rotate(point - transform.position);
			return Math::Abs(local.x) <= halfExtents.x * transform.scale.x && Math::Abs(local.y) <= halfExtents.y * transform.scale.y
				&& Math::Abs(local.z) <= halfExtents.z * transform.scale.z;
		}
	}

	namespace Triggers
	{
		void Update(World& world)
		{
			// The player is the character enemies hunt.
			LocalArray<Vector3, c_MaxPlayers> players;
			world.entities.ForEach<PerceivableComponent>([&world, &players](Entity entity, const PerceivableComponent&)
			{
				if (players.Size() < c_MaxPlayers && world.entities.HasComponent<CharacterComponent>(entity) && world.entities.HasComponent<TransformComponent>(entity))
				{
					players.Add(world.entities.GetComponent<TransformComponent>(entity).world.position + Vector3(0.0f, c_TestHeight, 0.0f));
				}
			});

			world.entities.ForEach<TriggerVolumeComponent>([&world, &players](Entity entity, TriggerVolumeComponent& trigger)
			{
				if ((trigger.triggered && trigger.once) || !world.entities.HasComponent<TransformComponent>(entity))
				{
					return;
				}
				const Transform& transform = world.entities.GetComponent<TransformComponent>(entity).world;
				bool inside = false;
				for (const Vector3& player : players)
				{
					inside = inside || Contains(transform, trigger.halfExtents, player);
				}
				trigger.triggered = inside;
			});
		}

		bool IsTriggered(const World& world, const ActorName& name)
		{
			for (const ActorInstance& actor : world.actorInstances)
			{
				if (actor.name == name && world.entities.HasComponent<TriggerVolumeComponent>(actor.RootEntity()))
				{
					return world.entities.GetComponent<TriggerVolumeComponent>(actor.RootEntity()).triggered;
				}
			}
			return false;
		}
	}
}
