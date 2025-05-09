#pragma once

#include "AppBase.h"
#include "Engine.h"

namespace tyr
{
	AppBase::AppBase()
		: m_Engine(nullptr) {}

	void AppBase::Start()
	{
		m_Engine = Engine::Get();
	}

	void AppBase::Stop()
	{
		m_Engine = nullptr;
	}
}