#include "Window.h"

#include <Core.h>

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#include "Win32/PCWindow.h"
#endif

namespace tyr
{
	Window* Window::Create()
	{
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
		return new PCWindow();
#endif
		//TODO: Other platforms
	}

	Window::Window()
		: m_Handle(nullptr)
	{}

	void Window::Initialize(const WindowProperties& properties)
	{
		m_Properties = properties;
	}

	bool Window::IsActive() const
	{
		return m_Handle;
	}

	void* Window::GetHandle() const
	{
		return m_Handle;
	}

	const WindowProperties& Window::GetProperties() const
	{
		return m_Properties;
	}

	void Window::GetMaxResolution(int& width, int& height, float aspectRatio)
	{
		// Get the screen size
		const float screenWidth = static_cast<float>(Platform::GetScreenWidth());
		const float screenHeight = static_cast<float>(Platform::GetScreenHeight());

		// Calculate recommended window width and height based on the desired aspect ratio
		width = 1920;// static_cast<int>(std::round(std::min(screenWidth, screenHeight * aspectRatio)));
		height = 1080;// static_cast<int>(std::round(width / aspectRatio));
	}
}

	