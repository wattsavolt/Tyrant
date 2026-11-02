#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "ECS/EntitySystem.h"
#include "Rendering/RenderConstants.h"

namespace tyr
{
	struct World;
	struct EmitterComponent;
	class AssetManager;
	class RendererAPI;

	// The active world's particles: given off by its EmitterComponents, moved on each frame
	// and handed to the renderer furthest first. They live in one fixed pool, and once it's full
	// new ones wait for room.
	class TYR_ENGINE_API ParticleSystem final
	{
	public:
		static constexpr uint c_MaxParticles = RenderConstants::c_MaxParticles;
		// The most emitters made while playing that can finish in one frame.
		static constexpr uint c_MaxFinished = 32;

		// Lets every particle go, such as when another world becomes the active one.
		void Clear();
		// Gives off particles from every emitter and moves every particle on. Emitters made while
		// playing that are done are listed in outFinished, for the world to take out.
		void Update(World& world, AssetManager& assets, float deltaTime, Entity (&outFinished)[c_MaxFinished], uint& outFinishedCount);
		// Hands the particles in front of the camera to the renderer, furthest first.
		void Submit(const World& world, RendererAPI& renderer);

		uint GetCount() const { return m_Count; }

	private:
		// Each particle keeps how it should look from its emitter, so it carries on if that goes.
		struct Particle
		{
			Vector3 position;
			Vector3 velocity;
			float age;
			float life;
			float rotation;
			float spin;
			float startSize;
			float endSize;
			float gravity;
			float drag;
			// It goes once it's below this height.
			float floorY;
			float startFrame;
			float frameRate;
			uint startColour;
			uint endColour;
			uint textureIndex;
			uint flipbook;
			float brightness;
			float additive;
			float softness;
			float stretch;
		};

		void Emit(const EmitterComponent& emitter, const Vector3& origin, const Vector3& up, uint textureIndex, uint count);
		float Random01();
		Vector3 RandomInBall();

		Particle m_Particles[c_MaxParticles];
		uint m_Count = 0;
		uint m_RandomState = 0x9E3779B9u;
		// Filled each frame for the renderer.
		ParticleInstance m_Instances[c_MaxParticles];
		ParticleInstance m_Sorted[c_MaxParticles];
		uint16 m_Order[c_MaxParticles];
		float m_Distances[c_MaxParticles];
	};
}
