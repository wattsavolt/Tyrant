/// Copyright (c) 2023 Aidan Clear 

#include "App.h"
#include "Engine.h"
#include "World/World.h"

using namespace tyr;

TYR_EXPORT AppBase* TYR_STDCALL CreateApp()
{
	// #pragam comment line below prevents having to use extern "C"
#pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
	return new App();
}


App::App()
{
		
}

App::~App()
{
		
}

void App::Initialize()
{
	m_Engine = Engine::Instance();
}

void App::Update(float deltaTime)
{
		
}

void App::Shutdown()
{
	m_Engine = nullptr;
}
