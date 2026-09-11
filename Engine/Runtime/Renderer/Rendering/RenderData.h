#pragma once

#include "RenderFrame.h"
#include "Scene.h"

namespace tyr
{
	struct RenderData
	{
		Array<BufferUploadRequest> assetBufferUploadRequests;
		Array<TextureUploadRequest> textureUploadRequests;
		uint activeSceneIndex = RenderFrame::c_InvalidSceneIndex;
		uint sceneCount = 0;
		Scene scenes[RenderConstants::c_MaxScenes];

		RenderData()
		{
			assetBufferUploadRequests.Reserve(128);
			textureUploadRequests.Reserve(128);
		}

		void BeginFrame()
		{
			assetBufferUploadRequests.Clear();
			textureUploadRequests.Clear();
			if (activeSceneIndex != RenderFrame::c_InvalidSceneIndex)
			{
				Scene& scene = scenes[activeSceneIndex];
				scene.frameUploadRequests.Clear();
				scene.views.Clear();
			}
		}
	};

}