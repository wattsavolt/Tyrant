#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Gameplay/GameplayTypes.h"

namespace tyr
{
	struct World;

	namespace Sounds
	{
		// Asks for a sound event to play once at the position, such as a door opening. The world
		// plays the sounds asked for each frame while it's being played, and drops them otherwise.
		// Volume goes from 0 silent to 1 as authored. Does nothing for an empty name.
		TYR_ENGINE_API void Play(World& world, const SoundName& sound, const Vector3& position, float volume = 1.0f);
	}
}
