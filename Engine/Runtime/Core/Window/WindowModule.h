#pragma once

#include "Base/Base.h"
#include "Module/IModule.h"
#include "Containers/LocalArray.h"

namespace tyr
{
	class Window;
	class TYR_CORE_EXPORT WindowModule final : public IModule
	{
	public:
		static constexpr uint8 c_MaxSecondaryWindows = 2;

		WindowModule();
		~WindowModule();

		void InitializeModule() override;

		void ShutdownModule() override;

		void UpdateModule(float deltaTime) override;

		const Window* GetPrimaryWindow() const { return m_PrimaryWindow; }

		Window* GetPrimaryWindow() { return m_PrimaryWindow; }

#if TYR_EDITOR
		const Window* GetSecondaryWindow(uint8 index) const
		{
			return m_SecondaryWindows[index];
		}

		Window* GetSecondaryWindow(uint8 index)
		{
			return m_SecondaryWindows[index];
		}
#endif

	private:
		Window* m_PrimaryWindow;
#if TYR_EDITOR
		LocalArray<Window*, c_MaxSecondaryWindows> m_SecondaryWindows;
#endif
	};
}