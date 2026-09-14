#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "World.h"
#include "RenderInstance/RenderInstances.h"

namespace tyr
{
	class RendererAPI;
	class TYR_ENGINE_API WorldManager final
	{
	public:
		static constexpr uint8 c_MaxWorlds = RenderConstants::c_MaxScenes;

		WorldManager();
		~WorldManager();

		void Update(float deltaTime);

		void UpdateWorld(World& world, float deltaTime);

		Handle AddWorld(const WorldConfig& params);

		const World& GetWorld(Handle worldHandle) const;

		World& GetWorld(Handle worldHandle);

		void RemoveWorld(Handle worldHandle);

		void RemoveWorlds();

	private:
		// Both are lifecycle transitions that have to stay coordinated with the pool
		// (handle creation/deletion, m_Worlds membership), unlike UpdateWorld which is
		// safe to call on a live world at any time - so these stay private, only ever
		// called from AddWorld/RemoveWorld(s).
		void InitWorld(World& world, const WorldConfig& config);
		void ShutdownWorld(World& world);

		RendererAPI* m_RendererAPI;
		LocalObjectPool<World, c_MaxWorlds, ResetObjectPolicy> m_WorldPool;
		LocalArray<Handle, c_MaxWorlds> m_Worlds;
	};
	
}