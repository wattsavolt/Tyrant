#pragma once

#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Matrix4.h"
#include "EngineMacros.h"
#include "Rendering/Scene.h"
#include "AssetSystem/AssetID.h"

namespace tyr
{
	class Camera;

	struct WorldConfig
	{
		Name name;
		// TODO : Add window index later if required
		ViewArea viewArea;
		// The world is provided the camera (will be a component later) but its dimensions will be updated by the world manager when the window resizes
		Camera* camera = nullptr;
	};

	class Device;
	class RendererAPI;
	class RenderRegistry;
	/// A class that represents a world / scene in an app.
	class TYR_ENGINE_API World final
	{
	public:
		World();
		~World();

		void Initialize(const WorldConfig& config);

		void Shutdown();

		void Update(float deltaTime);

		const char* GetName() const { return m_Name.CStr(); }

		Camera* GetCamera() const; 

		void SetCamera(Camera* camera);

		const ViewArea& GetViewArea() const { return m_ViewArea; }

		ViewArea& GetViewArea() { return m_ViewArea; }

		uint8 GetSceneIndex() const { return m_SceneIndex; }

		void SetActive(bool active);

		bool IsActive() const { return m_Active; }

		void SetVisible(bool visible);

		bool IsVisible() const { return m_Visible; }

	private:
		friend class WorldManager;

		Name m_Name;
		Camera* m_Camera;
		Device* m_Device;
		RendererAPI* m_RendererAPI;
		ViewArea m_ViewArea;
		uint m_SceneIndex;
		bool m_Active;
		bool m_Visible;
		bool m_Initialized;
	};
}