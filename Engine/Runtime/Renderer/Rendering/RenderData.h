#pragma once

#include "RenderFrame.h"
#include "Scene.h"

namespace tyr
{
	struct RenderData
	{
		SceneHandle activeScene;
		// A pool rather than a plain array so removal can be deferred safely - RenderAsync
		// might still be using a scene from up to c_BufferedFrameCount frames ago.
		LocalObjectPool<Scene, RenderConstants::c_MaxScenes, ResetObjectPolicy> scenePool;

		// Uploads RenderAsync computes itself from a worker thread can't go through
		// RenderFrame (only the main thread may write that) - this worker-owned array needs
		// no extra synchronization beyond a plain Clear() each frame.
		Array<BufferUploadRequest> workerUploadRequests;

		void BeginFrame()
		{
			if (activeScene)
			{
				Scene& scene = scenePool[activeScene.h];
				scene.views.Clear();
			}
			workerUploadRequests.Clear();
		}
	};

}