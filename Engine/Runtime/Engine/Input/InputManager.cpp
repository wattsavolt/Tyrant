#include "InputManager.h"

#include "Core.h"

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#include "Win32/PCInputManager.h"
#endif

namespace tyr
{
	bool InputManager::mInstantiated = false;

	InputManager* InputManager::Create()
	{
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
		return new PCInputManager();
#endif
		//TODO: Other platforms
	}

	InputManager::InputManager()
		: mInitialized(false)
	{
		if (mInstantiated)
		{
			TYR_LOG_FATAL("Can't create more than one input manager!");
		}
		mInstantiated = true;
	}

	InputManager::~InputManager()
	{
		mInstantiated = false;
	}
}

	