#pragma once

#include "Base/Base.h"
#include <windows.h>

namespace tyr
{
	struct WindowDesc;
	struct Window;

	class PCWindow final 
	{
	public:
		static void InitializeWindow(const WindowDesc& desc, Window& window);
		static void PollEvents(Window& window);
		static bool IsWindowActive(const Window& window);

	private:
		static ATOM RegisterWindowClass(uint16 iconResourceId);
		static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
		static INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

		static LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM, Window&);

		static HINSTANCE m_HInstance;
		static LPCSTR m_WindowClass;
	};
}