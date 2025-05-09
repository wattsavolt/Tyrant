

#include "PCInputManager.h"

#include "resource.h"

namespace tyr
{
	PCInputManager::PCInputManager()
	{
		hInstance = GetModuleHandle(nullptr);
	}

	PCInputManager::~PCInputManager()
	{
		if (mInitialized)
		{
			Shutdown();
		}
	}

	void PCInputManager::Initialize()
	{
		mInitialized = true;
	}

	bool PCInputManager::HandleNextMessage(bool& quitMsgReceived)
	{
		//static HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ENGINE));
		MSG msg;

		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				quitMsgReceived = true;
			}
			else
			{
				quitMsgReceived = false;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			
			return true;
		}
		return false;
	}

	void PCInputManager::Shutdown() 
	{
		mInitialized = false;
	}
}