#include "Health.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Gameplay/Sounds.h"
#include <algorithm>

namespace tyr
{
	namespace
	{
		// Losing this share of full health shows as being as hurt as it gets.
		constexpr float c_FullHurtShare = 0.5f;
	}

	namespace Health
	{
		bool ApplyDamage(World& world, Entity entity, float amount)
		{
			if (!world.entities.HasComponent<HealthComponent>(entity) || amount <= 0.0f)
			{
				return false;
			}
			HealthComponent& health = world.entities.GetComponent<HealthComponent>(entity);
			if (health.health <= 0.0f)
			{
				return false;
			}
			health.health = std::max(health.health - amount, 0.0f);
			health.hurt = std::min(health.hurt + amount / (std::max(health.maxHealth, 1.0f) * c_FullHurtShare), 1.0f);
			const bool died = health.health <= 0.0f;
			if (world.entities.HasComponent<ComponentTransform>(entity))
			{
				Sounds::Play(world, died ? health.deathSound : health.hurtSound, world.entities.GetComponent<ComponentTransform>(entity).world.position);
			}
			return died;
		}

		void Restore(World& world, Entity entity)
		{
			if (world.entities.HasComponent<HealthComponent>(entity))
			{
				HealthComponent& health = world.entities.GetComponent<HealthComponent>(entity);
				health.health = health.maxHealth;
				health.hurt = 0.0f;
			}
		}

		bool IsDead(const World& world, Entity entity)
		{
			return world.entities.HasComponent<HealthComponent>(entity) && world.entities.GetComponent<HealthComponent>(entity).health <= 0.0f;
		}
	}
}
