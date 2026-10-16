#include "Sounds.h"
#include "World/World.h"

namespace tyr
{
	namespace Sounds
	{
		void Play(World& world, const SoundName& sound, const Vector3& position, float volume)
		{
			if (sound.Size() == 0 || !world.simulate || world.sounds.Size() >= c_MaxSoundRequests)
			{
				return;
			}
			SoundRequest request;
			request.sound = sound;
			request.position = position;
			request.volume = volume;
			world.sounds.Add(request);
		}
	}
}
