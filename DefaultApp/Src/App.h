#pragma once

#include "App/AppBase.h"


class App final : public tyr::AppBase
{
public:
	App();
	~App();

	void Start() override;
	void Update(double deltaTime) override;
	void Stop() override;
};
	
