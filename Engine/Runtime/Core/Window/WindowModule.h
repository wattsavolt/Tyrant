#pragma once

#include "Core.h"
#include "Module/IModule.h"
#include "Containers/LocalArray.h"
#include "Memory/LocalObjectPool.h"
#include "WindowConstants.h"
#include "WindowHandle.h"

namespace tyr
{
	class Window;
	struct WindowDesc;
	struct WindowModulePrivate;

	class TYR_CORE_API WindowModule final : public IModule
	{
	public:
		WindowModule();

		~WindowModule();

		void Initialize() override;

		void Shutdown() override;

		void Update(float deltaTime) override;

		// Not calling it CreateWindow to avoid conflicts with winapi CreateWindow function
		WindowHandle MakeWindow(const WindowDesc& desc);

		void DestroyWindow(WindowHandle handle);

		const Window& GetWindow(WindowHandle handle) const;

		const uint GetWindowWidth(WindowHandle handle) const;

		const uint GetWindowHeight(WindowHandle handle) const;

		const bool IsWindowActive(WindowHandle handle) const;

	private:
		WindowModulePrivate* m_Private{};
	};
}