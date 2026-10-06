#pragma once

#include "AppBase.h"
#include "Window/WindowModule.h"
#include "Window/WindowDesc.h"
#include "Window/Window.h"
#include "World/WorldManager.h"
#include "World/World.h"
#include "RendererModule.h"
#include "Rendering/RendererAPI.h"

namespace tyr
{
	AppBase::AppBase()
	{

	}

	Handle AppBase::CreatePrimaryWorld(WindowModule& windowModule, WorldManager& worldManager,
		const WindowDesc& windowDesc, WorldConfig worldConfig, WindowHandle& outWindowHandle)
	{
		outWindowHandle = windowModule.MakeWindow(windowDesc);
		const Window& window = windowModule.GetWindow(outWindowHandle);

		RendererModule* rendererModule;
		TYR_GET_MODULE(RendererModule, rendererModule);
		RendererAPI* rendererAPI = rendererModule->GetRendererAPI();

		worldConfig.windowHandle = rendererAPI->AddWindow(window.handle);
		worldConfig.osWindowHandle = outWindowHandle;
		const Handle worldHandle = worldManager.AddWorld(worldConfig);
		worldManager.SetActiveWorld(worldHandle);
		return worldHandle;
	}

	void AppBase::DestroyPrimaryWorld(WindowModule& windowModule, WorldManager& worldManager,
		Handle worldHandle, WindowHandle windowHandle)
	{
		const RenderWindowHandle renderWindow = worldManager.GetWorld(worldHandle).windowHandle;
		worldManager.RemoveWorld(worldHandle);

		RendererModule* rendererModule;
		TYR_GET_MODULE(RendererModule, rendererModule);
		rendererModule->GetRendererAPI()->RemoveWindow(renderWindow);

		windowModule.DestroyWindow(windowHandle);
	}
}