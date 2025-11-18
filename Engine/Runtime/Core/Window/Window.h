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

	static constexpr float c_DefaultAspectRatio = 16.0f / 9.0f;

	class TYR_CORE_EXPORT Window : public INonCopyable
	{
	public:
		virtual void PollEvents() = 0;

		bool IsActive() const;
		void* GetHandle() const;
		const WindowProperties& GetProperties() const;

		static void GetMaxResolution(int& width, int& height, float aspectRatio = c_DefaultAspectRatio);

	protected:
		Window(const WindowProperties& properties);

		void* m_Handle;
		WindowProperties m_Properties;
	};
}