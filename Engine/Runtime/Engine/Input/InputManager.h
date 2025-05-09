#pragma once

#include <Base/Base.h>

namespace tyr
{
	class InputManager
	{
	public:
		virtual ~InputManager();
		InputManager& operator=(const InputManager&) = delete;
		InputManager(const InputManager&) = delete;

		/// Initializes any input devices.
		virtual void Initialize() = 0;
		/// Dispatches next message and returns false if no message.
		virtual bool HandleNextMessage(bool& quitMsgReceived) = 0;
		/// Deletes any input devices. */
		virtual void Shutdown() = 0;

		static InputManager* Create();

	protected:
		InputManager();

		bool mInitialized;

	private:
		static bool mInstantiated;
	};
}