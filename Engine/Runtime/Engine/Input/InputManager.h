#pragma once

#include <Base/Base.h>

namespace tyr
{
	class InputManager : public INonCopyable
	{
	public:
		virtual ~InputManager();

		/// Initializes any input devices.
		virtual void Initialize() = 0;
		/// Deletes any input devices. */
		virtual void Shutdown() = 0;

	protected:
		InputManager();
	};
}