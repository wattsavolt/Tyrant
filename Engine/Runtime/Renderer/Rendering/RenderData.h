#pragma once

#include "RenderFrame.h"
#include "Scene.h"

namespace tyr
{
	struct RenderData
	{
		Array<BufferUploadRequest> assetBufferUploadRequests;
		Array<TextureUploadRequest> textureUploadRequests;
		SceneHandle activeScene;
		// A pool rather than a plain array so removal can be deferred safely (RenderAsync might
		// still be using a scene from up to c_BufferedFrameCount frames ago) the same way window
		// removal is - see RendererAPI::RemoveScene and RenderFrame::scenesToDelete.
		LocalObjectPool<Scene, RenderConstants::c_MaxScenes, ResetObjectPolicy> scenes;

		RenderData()
		{
			assetBufferUploadRequests.Reserve(128);
			textureUploadRequests.Reserve(128);
		}

		void BeginFrame()
		{
			assetBufferUploadRequests.Clear();
			textureUploadRequests.Clear();
			if (activeScene)
			{
				Scene& scene = scenes[activeScene.h];
				scene.frameUploadRequests.Clear();
				scene.views.Clear();
			}
		}
	};

}