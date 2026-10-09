#include "WindowModule.h"

#include "WindowDesc.h"
#include "Window.h"
#include <algorithm>
#include <cstring>

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
		{
			// Array(uint size) sets the initial SIZE, not just capacity - it would populate
			// this with c_MaxWindows default (invalid-index) WindowHandle entries otherwise.
			windows.Reserve(WindowConstants::c_MaxWindows);
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

	void WindowModule::BeginFrame()
	{
		for (const WindowHandle window : m_Private->windows)
		{
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
			PCWindow::PollEvents(m_Private->windowPool[window.h.index]);
#endif
		}
	}

	void WindowModule::Update(float deltaTime)
	{
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

	bool WindowModule::ConsumeResizePending(WindowHandle handle)
	{
		Window& window = m_Private->windowPool[handle.h];
		const bool pending = window.resizePending;
		window.resizePending = false;
		return pending;
	}

	void WindowModule::SetCloseIntercepted(WindowHandle handle, bool intercepted)
	{
		m_Private->windowPool[handle.h].interceptClose = intercepted;
	}

	bool WindowModule::ConsumeCloseRequested(WindowHandle handle)
	{
		Window& window = m_Private->windowPool[handle.h];
		const bool requested = window.closeRequested;
		window.closeRequested = false;
		return requested;
	}

	const WindowInputState& WindowModule::GetInputState(WindowHandle handle) const
	{
		return m_Private->windowPool[handle.h].input;
	}

	void WindowModule::ConsumeTypedChars(WindowHandle handle, char* outBuffer, uint bufferCapacity, uint& outCount)
	{
		WindowInputState& input = m_Private->windowPool[handle.h].input;
		outCount = std::min(input.typedCharCount, bufferCapacity);
		memcpy(outBuffer, input.typedChars, outCount);
		input.typedCharCount = 0;
	}

	float WindowModule::ConsumeScrollDelta(WindowHandle handle)
	{
		WindowInputState& input = m_Private->windowPool[handle.h].input;
		const float delta = input.scrollDelta;
		input.scrollDelta = 0.0f;
		return delta;
	}

	void WindowModule::ConsumeRawMouseDelta(WindowHandle handle, int& outDeltaX, int& outDeltaY)
	{
		WindowInputState& input = m_Private->windowPool[handle.h].input;
		outDeltaX = input.rawMouseDeltaX;
		outDeltaY = input.rawMouseDeltaY;
		input.rawMouseDeltaX = 0;
		input.rawMouseDeltaY = 0;
	}

	void WindowModule::SetCursorCaptured(WindowHandle handle, bool captured)
	{
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
		PCWindow::SetCursorCaptured(m_Private->windowPool[handle.h], captured);
#endif
	}
}