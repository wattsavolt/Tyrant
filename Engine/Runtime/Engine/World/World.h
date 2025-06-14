#pragma once

#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Matrix4.h"
#include "EngineMacros.h"
#include "RenderUpdate/RenderUpdate.h"

namespace tyr
{
	using WorldID = SceneID;

	class Camera;

	struct WorldParams
	{
		// TODO : Add window index later if required
		SceneViewArea viewArea;
		// The world is provided the camera but the engine will update the camera dimensions when the window resizes
		Camera* camera = nullptr;
	};

	/// A class that represents a world / scene in an app.
	class TYR_ENGINE_EXPORT World final
	{
	public:
		World();
		~World();
		void Initialize(WorldID worldID, const WorldParams& params);
		void Shutdown();
		bool IsFirstFrame() const { return m_IsFirstFrame; }
		void SetIsFirstFrame(bool value) { m_IsFirstFrame = value; }
		bool IsInitialized() const { return m_Initialized; }
		Camera* GetCamera() const; 
		WorldID GetWorldID() const { return m_WorldID; }
		void SetCamera(Camera* camera);
		SceneViewArea& GetViewArea() { return m_ViewArea; }

	private:
		WorldID m_WorldID;
		Camera* m_Camera;
		SceneViewArea m_ViewArea;
		bool m_IsFirstFrame;
		bool m_Initialized;
	};
}