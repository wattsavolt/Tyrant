#pragma once

#include "App/AppBase.h"


class App final : public tyr::AppBase
{
public:
	App();
	~App();

	void Initialize() override;
	void Update(float deltaTime) override;
	void Shutdown() override;
};
	
