#include "GeometryPass.h"
#include "RenderAPI/Device.h"
#include "Rendering/Scene.h"

namespace tyr
{
	GeometryPass::GeometryPass(const GeometryPassArgs& args)
	{
		Recreate(args);
	}

	GeometryPass::~GeometryPass()
	{
	
	}

	void GeometryPass::Recreate(const GeometryPassArgs& args)
	{
		m_Device = args.device;
		m_Scene = args.scene;
		m_Pipeline = args.pipeline;
	}
	
}