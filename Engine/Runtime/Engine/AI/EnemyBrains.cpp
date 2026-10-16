#include "EnemyBrains.h"
#include "NavAgents.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "Navigation/NavmeshComponentsManager.h"
#include "Physics/PhysicsComponentsManager.h"
#include "Gameplay/LightLevel.h"
#include "Gameplay/Health.h"
#include "Gameplay/Sounds.h"
#include "Math/Math.h"
#include <algorithm>

namespace tyr
{
	namespace
	{
		// How far around a lost target, or a noise, it looks.
		constexpr float c_SearchRadius = 5.0f;
		constexpr float c_InvestigateRadius = 3.0f;
		// It keeps attacking until its target is this much further than its reach.
		constexpr float c_AttackReachSlack = 1.25f;
		// The pause before its first hit after closing in, in seconds.
		constexpr float c_AttackWindup = 0.4f;
		// Where on it the flashlight's beam is checked, above its feet.
		constexpr float c_BeamCheckHeight = 1.0f;

		uint g_RandomState = 0x9E3779B9u;

		float Random01()
		{
			g_RandomState = g_RandomState * 1664525u + 1013904223u;
			return static_cast<float>(g_RandomState >> 8) * (1.0f / 16777216.0f);
		}

		void Enter(EnemyComponent& enemy, EnemyState state, float timer)
		{
			enemy.state = static_cast<uint8>(state);
			enemy.stateTimer = timer;
		}

		// Keeps an agent that's already walking on its path, which it finds again as the target moves.
		void WalkTo(World& world, Entity entity, NavAgentComponent& agent, const Vector3& target)
		{
			if (agent.hasTarget)
			{
				agent.target = target;
			}
			else
			{
				NavAgents::SetTarget(world, entity, target);
			}
		}

		// Walks to a random spot near the centre, if there's one to walk to.
		void WalkNear(World& world, const NavmeshComponentsManager& navmesh, Entity entity, const Vector3& centre, float radius)
		{
			Vector3 point;
			if (navmesh.FindRandomPoint(world, centre, radius, point))
			{
				NavAgents::SetTarget(world, entity, point);
			}
		}
	}

	namespace EnemyBrains
	{
		const char* GetStateName(uint8 state)
		{
			switch (static_cast<EnemyState>(state))
			{
			case EnemyState::Wander: return "Wander";
			case EnemyState::Investigate: return "Investigate";
			case EnemyState::Chase: return "Chase";
			case EnemyState::Attack: return "Attack";
			case EnemyState::Search: return "Search";
			case EnemyState::Dazed: return "Dazed";
			default: return "?";
			}
		}

		void Update(World& world, const NavmeshComponentsManager& navmesh, const PhysicsComponentsManager& physics, float deltaTime)
		{
			world.entities.ForEach<EnemyComponent>([&](Entity entity, EnemyComponent& enemy)
			{
				if (!world.entities.HasComponent<NavAgentComponent>(entity) || !world.entities.HasComponent<PerceptionComponent>(entity)
					|| !world.entities.HasComponent<ComponentTransform>(entity))
				{
					return;
				}
				NavAgentComponent& agent = world.entities.GetComponent<NavAgentComponent>(entity);
				PerceptionComponent& perception = world.entities.GetComponent<PerceptionComponent>(entity);
				const Vector3 position = world.entities.GetComponent<ComponentTransform>(entity).world.position;
				if (!enemy.homeKnown)
				{
					enemy.home = position;
					enemy.homeKnown = true;
				}
				enemy.stateTimer -= deltaTime;
				enemy.attackTimer -= deltaTime;

				// Now and then it makes itself heard while it isn't after anything.
				enemy.idleSoundTimer -= deltaTime;
				const EnemyState current = static_cast<EnemyState>(enemy.state);
				if (enemy.idleSoundTimer <= 0.0f)
				{
					enemy.idleSoundTimer = enemy.idleSoundInterval * (0.6f + 0.8f * Random01());
					if (current != EnemyState::Chase && current != EnemyState::Attack && current != EnemyState::Dazed)
					{
						Sounds::Play(world, enemy.idleSound, position);
					}
				}

				// Light holds it still for as long as it's in the beam, and a moment after.
				if (enemy.dazedByFlashlight && LightLevel::IsInFlashlightBeam(world, physics, position + Vector3(0.0f, c_BeamCheckHeight, 0.0f)))
				{
					NavAgents::Stop(world, entity);
					Enter(enemy, EnemyState::Dazed, enemy.dazedRecovery);
					return;
				}

				const bool aware = perception.targetKnown && perception.targetAge <= enemy.memory;
				const bool targetValid = perception.target != c_InvalidEntity && world.entities.HasComponent<ComponentTransform>(perception.target);
				const Vector3 targetPosition = targetValid ? world.entities.GetComponent<ComponentTransform>(perception.target).world.position : position;
				const float targetDistance = targetValid ? (targetPosition - position).Length() : 1e30f;

				EnemyState state = static_cast<EnemyState>(enemy.state);
				if (state == EnemyState::Dazed)
				{
					if (enemy.stateTimer > 0.0f)
					{
						return;
					}
					// Coming round, it goes back after its target, or to where it last knew it was.
					state = perception.targetKnown ? EnemyState::Chase : EnemyState::Wander;
					Enter(enemy, state, 0.0f);
				}

				if (state == EnemyState::Attack)
				{
					if (!aware || !targetValid || targetDistance > enemy.attackRange * c_AttackReachSlack)
					{
						Enter(enemy, EnemyState::Chase, 0.0f);
						state = EnemyState::Chase;
					}
					else
					{
						NavAgents::Stop(world, entity);
						agent.yaw = Math::Atan2(targetPosition.x - position.x, targetPosition.z - position.z);
						if (enemy.attackTimer <= 0.0f)
						{
							Sounds::Play(world, enemy.attackSound, position);
							Health::ApplyDamage(world, perception.target, enemy.attackDamage);
							enemy.attackTimer = enemy.attackInterval;
						}
						return;
					}
				}

				if (aware)
				{
					if (targetValid && targetDistance <= enemy.attackRange)
					{
						NavAgents::Stop(world, entity);
						Enter(enemy, EnemyState::Attack, 0.0f);
						enemy.attackTimer = std::max(enemy.attackTimer, c_AttackWindup);
						return;
					}
					if (state != EnemyState::Chase)
					{
						// Only a fresh chase is announced, not picking it back up after attacking or being dazed.
						if (state != EnemyState::Attack)
						{
							Sounds::Play(world, enemy.alertSound, position);
						}
						Enter(enemy, EnemyState::Chase, 0.0f);
					}
					agent.speed = enemy.chaseSpeed;
					WalkTo(world, entity, agent, perception.targetPosition);
					perception.heardOther = false;
					return;
				}

				// A noise that wasn't its target's gets looked into, unless it's already after something.
				if (perception.heardOther && state != EnemyState::Chase)
				{
					perception.heardOther = false;
					enemy.searchCentre = perception.otherPosition;
					Enter(enemy, EnemyState::Investigate, enemy.investigateTime);
					agent.speed = enemy.walkSpeed;
					NavAgents::SetTarget(world, entity, perception.otherPosition);
					return;
				}

				switch (state)
				{
				case EnemyState::Chase:
					// Lost it: it carries on to where it last knew it was, then searches there.
					if (!agent.hasTarget)
					{
						enemy.searchCentre = perception.targetKnown ? perception.targetPosition : position;
						Enter(enemy, EnemyState::Search, enemy.searchTime);
					}
					break;
				case EnemyState::Search:
				case EnemyState::Investigate:
					agent.speed = enemy.walkSpeed;
					if (enemy.stateTimer <= 0.0f)
					{
						perception.targetKnown = false;
						Enter(enemy, EnemyState::Wander, Random01() * enemy.wanderPause);
					}
					else if (!agent.hasTarget)
					{
						WalkNear(world, navmesh, entity, enemy.searchCentre, state == EnemyState::Search ? c_SearchRadius : c_InvestigateRadius);
					}
					break;
				default:
					agent.speed = enemy.walkSpeed;
					if (!agent.hasTarget && enemy.stateTimer <= 0.0f)
					{
						WalkNear(world, navmesh, entity, enemy.home, enemy.wanderRadius);
						enemy.stateTimer = Random01() * enemy.wanderPause;
					}
					break;
				}
			});
		}
	}
}
