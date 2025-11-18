#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "World.h"
#include "RenderUpdate/RenderFrame.h"

namespace tyr
{
	class Renderer;
	class TYR_ENGINE_EXPORT WorldManager final
	{
	public:
		static constexpr uint8 c_MaxWorlds = Scene::c_MaxScenes;

		WorldManager();
		~WorldManager();

		void Update(float deltaTime);

		World* AddWorld(const WorldParams& params);

		void RemoveWorld(World* world);

		void RemoveWorlds();

	private:
		LocalObjectPool<World, c_MaxWorlds, false> m_WorldPool;
		LocalArray<World*, c_MaxWorlds> m_Worlds;
		RenderFrame m_RenderFrames[RenderFrame::c_MaxRenderFrames];
		Renderer* m_Renderer;
		uint8 m_RenderFrameIndex;
	};
	
}