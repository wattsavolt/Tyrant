#include "ParticleSystem.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "World/Camera.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetUtil.h"
#include "Rendering/RendererAPI.h"
#include "Math/Math.h"
#include <algorithm>
#include <cmath>

namespace tyr
{
	namespace
	{
		// Each particle fades in over this many seconds, so a burst doesn't pop on.
		constexpr float c_FadeInTime = 0.06f;

		uint PackColour(const Vector4& colour)
		{
			const uint r = static_cast<uint>(Math::Clamp01(colour.x) * 255.0f + 0.5f);
			const uint g = static_cast<uint>(Math::Clamp01(colour.y) * 255.0f + 0.5f);
			const uint b = static_cast<uint>(Math::Clamp01(colour.z) * 255.0f + 0.5f);
			const uint a = static_cast<uint>(Math::Clamp01(colour.w) * 255.0f + 0.5f);
			return r | (g << 8) | (b << 16) | (a << 24);
		}

		// Blends two packed colours a share t of the way, byte by byte.
		uint LerpColour(uint a, uint b, float t)
		{
			uint result = 0;
			for (uint shift = 0; shift < 32; shift += 8)
			{
				const float from = static_cast<float>((a >> shift) & 0xFF);
				const float to = static_cast<float>((b >> shift) & 0xFF);
				result |= static_cast<uint>(from + (to - from) * t + 0.5f) << shift;
			}
			return result;
		}
	}

	void ParticleSystem::Clear()
	{
		m_Count = 0;
	}

	float ParticleSystem::Random01()
	{
		// Xorshift: cheap, and plenty for scattering particles.
		m_RandomState ^= m_RandomState << 13;
		m_RandomState ^= m_RandomState >> 17;
		m_RandomState ^= m_RandomState << 5;
		return static_cast<float>(m_RandomState >> 8) * (1.0f / 16777216.0f);
	}

	Vector3 ParticleSystem::RandomInBall()
	{
		for (uint tries = 0; tries < 8; ++tries)
		{
			const Vector3 point(Random01() * 2.0f - 1.0f, Random01() * 2.0f - 1.0f, Random01() * 2.0f - 1.0f);
			if (point.SqrLength() <= 1.0f)
			{
				return point;
			}
		}
		return Vector3::c_Zero;
	}

	void ParticleSystem::Emit(const EmitterComponent& emitter, const Vector3& origin, const Vector3& up, uint textureIndex, uint count)
	{
		// Directions round the emitter's up axis, inside the spread.
		const Vector3 side = Math::Abs(up.y) > 0.99f ? Vector3::c_Right : Vector3::Normalize(Vector3::c_Up.Cross(up));
		const Vector3 other = up.Cross(side);
		const float minCos = Math::Cos(Math::Clamp(emitter.spreadAngle, 0.0f, 180.0f) * Math::c_DegToRad);
		const uint frameCount = std::max(static_cast<uint>(emitter.columns) * static_cast<uint>(emitter.rows), 1u);
		const uint startColour = PackColour(emitter.startColour);
		const uint endColour = PackColour(emitter.endColour);
		for (uint i = 0; i < count && m_Count < c_MaxParticles; ++i)
		{
			const float cosTheta = 1.0f + (minCos - 1.0f) * Random01();
			const float sinTheta = Math::Sqrt(std::max(1.0f - cosTheta * cosTheta, 0.0f));
			const float phi = Random01() * Math::c_TwoPi;
			const Vector3 direction = up * cosTheta + (side * Math::Cos(phi) + other * Math::Sin(phi)) * sinTheta;
			const float sizeScale = 1.0f + (Random01() * 2.0f - 1.0f) * emitter.sizeVariation;

			Particle& particle = m_Particles[m_Count++];
			particle.position = origin + RandomInBall() * emitter.spawnRadius;
			particle.velocity = direction * (emitter.speedMin + (emitter.speedMax - emitter.speedMin) * Random01());
			particle.age = 0.0f;
			particle.life = std::max(emitter.lifeMin + (emitter.lifeMax - emitter.lifeMin) * Random01(), 0.01f);
			particle.rotation = emitter.randomRotation ? Random01() * Math::c_TwoPi : 0.0f;
			particle.spin = (emitter.spinMin + (emitter.spinMax - emitter.spinMin) * Random01()) * Math::c_DegToRad;
			particle.startSize = emitter.startSize * sizeScale;
			particle.endSize = emitter.endSize * sizeScale;
			particle.gravity = emitter.gravity;
			particle.drag = emitter.drag;
			particle.floorY = emitter.floorDistance > 0.0f ? origin.y - emitter.floorDistance : -1e30f;
			particle.startFrame = emitter.randomStartFrame ? static_cast<float>(static_cast<uint>(Random01() * frameCount) % frameCount) : 0.0f;
			particle.frameRate = emitter.frameRate;
			particle.startColour = startColour;
			particle.endColour = endColour;
			particle.textureIndex = textureIndex;
			particle.flipbook = static_cast<uint>(emitter.columns) | (static_cast<uint>(emitter.rows) << 8);
			particle.brightness = emitter.brightness;
			particle.additive = Math::Clamp01(emitter.additive);
			particle.softness = emitter.softness;
			particle.stretch = emitter.stretch;
		}
	}

	void ParticleSystem::Update(World& world, AssetManager& assets, float deltaTime, Entity (&outFinished)[c_MaxFinished], uint& outFinishedCount)
	{
		outFinishedCount = 0;
		world.entities.ForEach<EmitterComponent>([&](Entity entity, EmitterComponent& emitter)
		{
			if (!world.entities.HasComponent<TransformComponent>(entity))
			{
				return;
			}
			if (!emitter.textureRequested && AssetUtil::IsValidAssetID(emitter.texture))
			{
				assets.LoadTexture(emitter.texture);
				emitter.textureRequested = true;
			}
			const TextureHandle texture = emitter.textureRequested ? assets.GetTexture(emitter.texture) : TextureHandle{};
			const uint textureIndex = texture ? texture.h.index : c_NoParticleTexture;

			const bool emitting = emitter.duration <= 0.0f || emitter.elapsed < emitter.duration;
			uint count = 0;
			if (!emitter.burstDone)
			{
				count += emitter.burstAtStart ? emitter.burst : 0;
				emitter.burstDone = true;
			}
			if (emitter.burstRequested)
			{
				count += emitter.burst;
				emitter.burstRequested = false;
			}
			if (emitting)
			{
				emitter.rateCarry += emitter.rate * deltaTime;
				const uint fromRate = static_cast<uint>(emitter.rateCarry);
				emitter.rateCarry -= static_cast<float>(fromRate);
				count += fromRate;
			}
			if (count > 0)
			{
				const Transform& transform = world.entities.GetComponent<TransformComponent>(entity).world;
				Emit(emitter, transform.position, Vector3::Normalize(transform.rotation.Rotate(Vector3::c_Up)), textureIndex, count);
			}
			emitter.elapsed += deltaTime;

			// Its last particles have gone once it's stopped for as long as any of them could live.
			if (emitter.removeWhenDone && emitter.duration > 0.0f && emitter.elapsed > emitter.duration + emitter.lifeMax
				&& outFinishedCount < c_MaxFinished)
			{
				outFinished[outFinishedCount++] = entity;
			}
		});

		for (uint i = 0; i < m_Count;)
		{
			Particle& particle = m_Particles[i];
			particle.age += deltaTime;
			if (particle.age >= particle.life || particle.position.y < particle.floorY)
			{
				// Order doesn't matter, since they're sorted for drawing.
				m_Particles[i] = m_Particles[--m_Count];
				continue;
			}
			particle.velocity.y += particle.gravity * deltaTime;
			particle.velocity *= std::max(1.0f - particle.drag * deltaTime, 0.0f);
			particle.position += particle.velocity * deltaTime;
			particle.rotation += particle.spin * deltaTime;
			++i;
		}
	}

	void ParticleSystem::Submit(const World& world, RendererAPI& renderer)
	{
		if (m_Count == 0 || !world.camera)
		{
			return;
		}
		const Vector3 eye = world.camera->GetPosition();
		const Vector3 forward = world.camera->GetForward();
		uint visible = 0;
		for (uint i = 0; i < m_Count; ++i)
		{
			const Particle& particle = m_Particles[i];
			const float t = particle.age / particle.life;
			const float size = particle.startSize + (particle.endSize - particle.startSize) * t;
			const Vector3 toParticle = particle.position - eye;
			// Wholly behind the camera, it can't be seen.
			if (toParticle.Dot(forward) < -size)
			{
				continue;
			}
			uint colour = LerpColour(particle.startColour, particle.endColour, t);
			const float fadeIn = Math::Clamp01(particle.age / c_FadeInTime);
			const uint alpha = static_cast<uint>(static_cast<float>(colour >> 24) * fadeIn + 0.5f);
			colour = (colour & 0x00FFFFFFu) | (alpha << 24);

			const uint frameCount = std::max((particle.flipbook & 0xFF) * ((particle.flipbook >> 8) & 0xFF), 1u);
			float frame;
			if (particle.frameRate > 0.0f)
			{
				frame = std::fmod(particle.startFrame + particle.age * particle.frameRate, static_cast<float>(frameCount));
			}
			else
			{
				// Played once over its life, ending on the last frame.
				frame = particle.startFrame + t * static_cast<float>(frameCount - 1);
			}

			ParticleInstance& instance = m_Instances[visible];
			instance.position = particle.position;
			instance.size = size;
			instance.velocity = particle.velocity;
			instance.stretch = particle.stretch;
			instance.colour = colour;
			instance.rotation = particle.rotation;
			instance.frame = frame;
			instance.textureIndex = particle.textureIndex;
			instance.flipbook = particle.flipbook;
			instance.brightness = particle.brightness;
			instance.additive = particle.additive;
			instance.softness = particle.softness;
			m_Distances[visible] = toParticle.SqrLength();
			m_Order[visible] = static_cast<uint16>(visible);
			++visible;
		}

		// Furthest first, so nearer ones blend over them.
		std::sort(m_Order, m_Order + visible, [this](uint16 a, uint16 b) { return m_Distances[a] > m_Distances[b]; });
		for (uint i = 0; i < visible; ++i)
		{
			m_Sorted[i] = m_Instances[m_Order[i]];
		}
		renderer.SubmitParticles(m_Sorted, visible);
	}
}
