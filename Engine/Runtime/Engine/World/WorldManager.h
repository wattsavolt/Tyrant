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

		Handle AddWorld(const WorldConfig& params);

		const World& GetWorld(Handle worldHandle) const;

		World& GetWorld(Handle worldHandle);

		void RemoveWorld(Handle worldHandle);

		void RemoveWorlds();

	private:
		RendererAPI* m_RendererAPI;
		LocalObjectPool<World, c_MaxWorlds, false> m_WorldPool;
		LocalArray<Handle, c_MaxWorlds> m_Worlds;
	};
	
}