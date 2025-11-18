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
		// The scene passed can be a nullptr for a pass that is not scene-specific ie. shared by a tw or more scenes
		Pass(Scene* scene = nullptr);
		virtual ~Pass();

		Scene* GetScene() const { return m_Scene; }

		// Called by the render graph builder every time the render graph is built.
		virtual void CreateRenderGraphDependencies(RGArray<RenderGraphDependencyInput>& inputs, RGArray<RenderGraphDependencyOutput>& outputs) {};

	protected:
		// Used by the rendergraph to organize nodes based on the scene id.
		Scene* m_Scene;
	};
}