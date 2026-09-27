#include "PCInputManager.h"
#include "Window/WindowModule.h"
#include "Window/Window.h"
#include <cstring>

namespace tyr
{
	PCInputManager::PCInputManager()
	{
		hInstance = GetModuleHandle(nullptr);
	}

	PCInputManager::~PCInputManager()
	{

	}

	void PCInputManager::Initialize()
	{

	}

	void PCInputManager::Shutdown()
	{

	}

	void PCInputManager::Update()
	{
		if (!m_WindowModule)
		{
			return;
		}

		memcpy(m_KeysDownPrev, m_KeysDown, sizeof(m_KeysDown));
		memcpy(m_MouseButtonsDownPrev, m_MouseButtonsDown, sizeof(m_MouseButtonsDown));
		m_MousePosPrev = m_MousePos;

		const WindowInputState& input = m_WindowModule->GetInputState(m_Window);
		memcpy(m_KeysDown, input.keysDown, sizeof(m_KeysDown));
		memcpy(m_MouseButtonsDown, input.mouseButtonsDown, sizeof(m_MouseButtonsDown));
		m_MousePos = Vector2((float)input.mouseX, (float)input.mouseY);

		m_ScrollDelta = m_WindowModule->ConsumeScrollDelta(m_Window);
		m_WindowModule->ConsumeTypedChars(m_Window, m_TypedChars, c_MaxTypedCharsPerFrame, m_TypedCharCount);
	}
}
