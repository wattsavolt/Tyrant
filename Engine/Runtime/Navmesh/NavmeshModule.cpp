#include "NavmeshModule.h"
#include "NavmeshManager.h"

namespace tyr
{
	NavmeshModule::NavmeshModule()
	{
	}

	NavmeshModule::~NavmeshModule()
	{
	}

	void NavmeshModule::Initialize()
	{
		m_NavmeshManager = new NavmeshManager();
	}

	void NavmeshModule::Update(float /*deltaTime*/)
	{
	}

	void NavmeshModule::Shutdown()
	{
		TYR_SAFE_DELETE(m_NavmeshManager);
	}
}
