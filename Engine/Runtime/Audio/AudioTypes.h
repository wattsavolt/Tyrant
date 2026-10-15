#pragma once

#include "Core.h"
#include "Math/Vector3.h"

namespace tyr
{
	// A loaded bank of sound events. Invalid while 0.
	struct AudioBankHandle
	{
		uint id = 0;

		bool IsValid() const { return id != 0; }
	};

	// A sound event that keeps playing until it's stopped, such as a loop or ambience. The index
	// and generation make an old handle invalid once its slot is reused. Invalid while 0.
	struct AudioEventHandle
	{
		uint index = 0;
		uint generation = 0;

		bool IsValid() const { return generation != 0; }
	};

	// Where sounds are heard from, usually the camera.
	struct AudioListener
	{
		Vector3 position;
		Vector3 forward = Vector3(0.0f, 0.0f, 1.0f);
		Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
		// Metres per second, for doppler.
		Vector3 velocity;
	};

	struct AudioConstants
	{
		static constexpr uint c_MaxBanks = 64;
		// Sound events playing at once with handles. One-shots don't count towards this.
		static constexpr uint c_MaxEvents = 256;
	};
}
