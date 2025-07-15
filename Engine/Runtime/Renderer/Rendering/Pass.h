#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderAPI/RenderAPI.h"
#include "Scene.h"
#include "RenderGraphResource.h"
#include "RenderGraphAllocation.h"

namespace tyr
{
	
	// A render pass
	class Pass : INonCopyable
	{
	public:
		// The scene id passed can be invalid if this pass instance is needed by more than one scene
		Pass(SceneID sceneId = Scene::c_InvalidID);
		virtual ~Pass();

		SceneID GetSceneId() const { return m_SceneId; }

		// Called by the render graph builder every time the render graph is built.
		virtual void CreateRenderGraphDependencies(RGArray<RenderGraphDependencyInput>& inputs, RGArray<RenderGraphDependencyOutput>& outputs) {};

	protected:
		// Used by the rendergraph to organize nodes based on the scene.
		SceneID m_SceneId;
	};

	// A pass that uses a graphics or compute shader
	class ShaderPass : public Pass
	{
	public:
		ShaderPass(Scene* scene);
		virtual ~ShaderPass();

	protected:
		Scene* m_Scene;
	};
}