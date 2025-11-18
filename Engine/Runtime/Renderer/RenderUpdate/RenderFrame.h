#pragma once

#include "Rendering/Scene.h"

namespace tyr
{
	struct RenderFrame
	{
		static constexpr uint8 c_MaxRenderFrames = 3;
		// True if the primary window resized since the last frame
		bool windowResize = false;
		Array<TextureDesc> m_NewTextures;
		Array<uint> m_DeletedTextures;
		Array<MaterialDesc> m_NewMaterials;
		Array<uint> m_DeletedMaterials;
		SceneFrame sceneFrames[Scene::c_MaxScenes];

		void Clear()
		{
			m_NewTextures.Clear();
			m_DeletedTextures.Clear();
			m_NewMaterials.Clear();
			m_DeletedMaterials.Clear();
		}
	};
}