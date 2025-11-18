#pragma once

#include "Window/Window.h"

#include <windows.h>

namespace tyr
{
	class TYR_CORE_EXPORT PCWindow final : public Window
	{
	public:
		PCWindow(const WindowProperties& properties);
		void PollEvents() override;

	private:
		static ATOM RegisterWindowClass(const WindowProperties& properties);
		static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
		static INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

		static HINSTANCE m_HInstance;
		static LPCSTR m_WindowClass;
	};
}