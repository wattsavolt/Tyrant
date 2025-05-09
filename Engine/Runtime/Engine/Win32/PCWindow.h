

#pragma once

#include "Window/Window.h"

#include <windows.h>

namespace tyr
{
	class PCWindow final : public Window
	{
	public:
		PCWindow() = default;
		void Initialize(const WindowProperties& properties) override;

	private:
		static ATOM RegisterWindowClass(const WindowProperties& properties);
		static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
		static INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

		static HINSTANCE m_HInstance;
		static LPCSTR m_WindowClass;
	};
}