#pragma once

#include <Base/Base.h>

namespace tyr
{
	struct WindowProperties
	{
		int showFlag = 1;
		int width = 1920;
		int height = 1080;
		uint16 iconResourceId = 0;
		const char* name;
	};

	static constexpr float DefaultAspectRatio = 16.0f / 9.0f;

	class Window
	{
	public:
		static Window* Create();

		Window& operator=(const Window&) = delete;
		Window(const Window&) = delete;
		virtual void Initialize(const WindowProperties& properties);
		bool IsActive() const;
		void* GetHandle() const;
		const WindowProperties& GetProperties() const;

		static void GetMaxResolution(int& width, int& height, float aspectRatio = DefaultAspectRatio);

	protected:
		Window();
		void* m_Handle;
		WindowProperties m_Properties;
	};
}