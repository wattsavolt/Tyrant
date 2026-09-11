#pragma once

#include "Core.h"
#include "RendererMacros.h"
#include "RenderAPI/Pipeline.h"

namespace tyr
{
	class Device;
	struct Scene;

	struct GeometryPassArgs
	{
		Device* device;
		Scene* scene;
		GraphicsPipelineHandle pipeline;
	};

	class GeometryPass final : public INonCopyable
	{
	public:
		GeometryPass(const GeometryPassArgs& args);
		~GeometryPass();

		void Recreate(const GeometryPassArgs& args);

	private:
		Device* m_Device;
		Scene* m_Scene;
		GraphicsPipelineHandle m_Pipeline;
	};
	
}