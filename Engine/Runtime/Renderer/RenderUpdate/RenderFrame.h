#pragma once

#include "Rendering/Scene.h"

namespace tyr
{
	struct RenderFrame
	{
		static constexpr uint8 c_MaxRenderFrames = 3;
		// True if the primary window resized since the last frame
		bool windowResize = false;
		Array<TextureDesc> newTextures;
		Array<uint> deletedTextures;
		Array<MaterialDesc> newMaterials;
		Array<uint> deletedMaterials;
		SceneFrame sceneFrames[Scene::c_MaxScenes];

		void Clear()
		{
			newTextures.Clear();
			deletedTextures.Clear();
			newMaterials.Clear();
			deletedMaterials.Clear();

			for (uint8 i = 0; i < Scene::c_MaxScenes; ++i)
			{
				sceneFrames[i].Clear();
			}
		}
	};
}