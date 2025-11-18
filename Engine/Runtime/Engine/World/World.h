#pragma once

#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Matrix4.h"
#include "EngineMacros.h"
#include "Rendering/Scene.h"

namespace tyr
{
	class Camera;

	struct WorldParams
	{
		Name name;
		// TODO : Add window index later if required
		SceneViewArea viewArea;
		// The world is provided the camera but its dimensions will be updated by the world manager when the window resizes
		Camera* camera = nullptr;
	};

	/// A class that represents a world / scene in an app.
	class TYR_ENGINE_EXPORT World final
	{
	public:
		World();
		~World();

		void Initialize(const WorldParams& params);

		void Shutdown();

		void Update(float deltaTime, SceneFrame& sceneFrame);

		const char* GetName() const { return m_Name.CStr(); }

		Camera* GetCamera() const; 

		void SetCamera(Camera* camera);

		const SceneViewArea& GetViewArea() const { return m_ViewArea; }

		SceneViewArea& GetViewArea() { return m_ViewArea; }

		uint8 GetSceneIndex() const { return m_SceneIndex; }

		void SetActive(bool active);

		bool IsActive() const { return m_Active; }

		void SetVisible(bool visible);

		bool IsVisible() const { return m_Visible; }

	private:
		friend class WorldManager;

		static uint8 s_NextSceneIndex;

		Name m_Name;
		Camera* m_Camera;
		SceneViewArea m_ViewArea;
		uint8 m_SceneIndex;
		bool m_Active;
		bool m_Visible;
		bool m_Initialized;
	};
}