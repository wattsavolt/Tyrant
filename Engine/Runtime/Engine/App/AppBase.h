#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "Window/WindowHandle.h"

namespace tyr
{
	class WindowModule;
	class WorldManager;
	struct WindowDesc;
	struct WorldConfig;

	class TYR_ENGINE_API AppBase
	{
	public:
		virtual ~AppBase() = default;

		virtual void Initialize() = 0;

		// Called before engine shutdown
		virtual void Shutdown() = 0;

		// Called every frame by the engine. deltaTime is in seconds.
		virtual void Update(float deltaTime) = 0;

		virtual bool WantsExit() const = 0;

	protected:
		AppBase();

		// Shared by Editor and App: creates an OS window, registers it with the renderer to get a
		// RenderWindowHandle, then creates a world bound to that window. worldConfig.windowHandle is
		// overwritten internally - callers don't need to (and shouldn't) set it themselves.
		static Handle CreatePrimaryWorld(WindowModule& windowModule, WorldManager& worldManager,
			const WindowDesc& windowDesc, WorldConfig worldConfig, WindowHandle& outWindowHandle);

		// Undoes CreatePrimaryWorld: removes the world, its render window and the OS window.
		static void DestroyPrimaryWorld(WindowModule& windowModule, WorldManager& worldManager,
			Handle worldHandle, WindowHandle windowHandle);
	};

}