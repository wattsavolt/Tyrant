#include "WindowModule.h"

#include "WindowDesc.h"
#include "Window.h"

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#include "Win32/PCWindow.h"
#endif

namespace tyr
{
	struct WindowModulePrivate
	{
		LocalObjectPool<Window, WindowConstants::c_MaxWindows> windowPool;
		Array<WindowHandle> windows;

		WindowModulePrivate()
			: windows(WindowConstants::c_MaxWindows)
		{

		}
	};

	WindowModule::WindowModule()
	{
		
	}

	WindowModule::~WindowModule()
	{
		
	}

	void WindowModule::Initialize()
	{
		m_Private = new WindowModulePrivate();
		
		//TODO: Other platforms
	}

	void WindowModule::Shutdown()
	{
		delete m_Private;
	}

	void WindowModule::Update(float deltaTime)
	{
		for (const WindowHandle window : m_Private->windows)
		{
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
			PCWindow::PollEvents(m_Private->windowPool[window.h.index]);
#endif
		}
	}

	WindowHandle WindowModule::MakeWindow(const WindowDesc& desc)
	{
		const WindowHandle handle = WindowHandle(m_Private->windowPool.Create());
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
		PCWindow::InitializeWindow(desc, m_Private->windowPool[handle.h.index]);
#endif
		m_Private->windows.Add(handle);
		return handle;
	}

	void WindowModule::DestroyWindow(WindowHandle handle)
	{
		for (uint i = 0; i < m_Private->windows.Size(); ++i)
		{
			if (handle == m_Private->windows[i])
			{
				m_Private->windows.Erase(i);
				break;
			}
		}
		m_Private->windowPool.Delete(handle.h);
	}

	const Window& WindowModule::GetWindow(WindowHandle handle) const
	{
		return m_Private->windowPool[handle.h];
	}

	const uint WindowModule::GetWindowWidth(WindowHandle handle) const
	{
		return m_Private->windowPool[handle.h].width;
	}

	const uint WindowModule::GetWindowHeight(WindowHandle handle) const
	{
		return m_Private->windowPool[handle.h].height;
	}

	const bool WindowModule::IsWindowActive(WindowHandle handle) const
	{
		return m_Private->windowPool[handle.h].handle != nullptr;
	}
}