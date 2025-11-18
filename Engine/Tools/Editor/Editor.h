#pragma once

#include "App/AppBase.h"
#include "EditorMacros.h"

namespace tyr
{
	class Camera;
	class WorldManager;
	class World;
	class TYR_EDITOR_EXPORT Editor final : public AppBase
	{
	public:
		Editor();
		~Editor();

		void Initialize() override;
		void Update(float deltaTime) override;
		void Shutdown() override;

	private:
		URef<Camera> m_Camera;
		WorldManager* m_WorldManager;
		World* m_LevelEditorWorld;
	};
	
}