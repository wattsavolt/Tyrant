#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"
#include "Math/Matrix4.h"
#include "RenderAPI/RenderAPITypes.h"
#include "RenderDataTypes/RenderDataTypes.h"

namespace tyr
{
	typedef int8 SceneID;

	enum class SceneType : uint
	{
		App = 0,
		WorldEditor,
		GUI,
		Viewer
	};

	enum class SceneActionType
	{
		None = 0,
		Add,
		Update,
		Remove
	};

	struct SceneAction
	{
		SceneActionType action;
		Matrix4 viewProj;
		SceneViewArea viewArea;
		Vector3 cameraPosition;
	};

	constexpr uint c_MaxDirectionalLights = 3;
	constexpr uint c_MaxPointLights = 20;
	constexpr uint c_MaxSpotLights = 10;

	struct SceneData
	{
		Array<RigidMeshInstance> rigidMeshInstances;
		Array<SkeletalMeshInstance> skeletalModelInstances;
		DirectionalLight dirLights[c_MaxDirectionalLights];
		uint dirLightCount;
		PointLight pointLights[c_MaxPointLights];
		uint pointLightCount;
		SpotLight spotLights[c_MaxSpotLights];
		uint spotLightCount;
	};

	class Scene final
	{
	public:
		Scene(SceneType type, SceneID sceneId, bool active = true);
		~Scene();

		SceneData const& GetSceneData() const { return m_SceneData; }

		SceneType GetType() const { return m_Type; }

		SceneID GetId() const { return m_Id; }

		void SetVisible(bool visible) { m_Visible = visible; }

		bool IsVisible() const { return m_Visible; };

		static constexpr uint c_MaxScenes = 4;

		static constexpr SceneID c_InvalidID = -1;

	private:
		friend class SceneRenderer;

		SceneType m_Type;
		SceneID m_Id;
		SceneData m_SceneData;
		bool m_Visible;
	};
	
}