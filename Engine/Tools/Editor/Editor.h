#pragma once

#include "App/AppBase.h"
#include "EditorMacros.h"

namespace tyr
{
	class Camera;
	class TYR_EDITOR_EXPORT Editor final : public AppBase
	{
	public:
		Editor();
		~Editor();

		void Start() override;
		void Update(double deltaTime) override;
		void Stop() override;

	private:
		URef<Camera> m_Camera;
	};
	
}