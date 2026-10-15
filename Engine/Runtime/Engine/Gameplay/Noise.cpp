#include "Noise.h"
#include "World/World.h"

namespace tyr
{
	namespace Noise
	{
		void Emit(World& world, const Vector3& position, float radius, Entity source)
		{
			if (radius <= 0.0f)
			{
				return;
			}
			NoiseEvent noise;
			noise.position = position;
			noise.radius = radius;
			noise.source = source;
			if (world.noises.Size() < c_MaxNoiseEvents)
			{
				world.noises.Add(noise);
				return;
			}

			uint oldest = 0;
			for (uint i = 1; i < world.noises.Size(); ++i)
			{
				if (world.noises[i].age > world.noises[oldest].age)
				{
					oldest = i;
				}
			}
			world.noises[oldest] = noise;
		}

		void Update(World& world, float deltaTime)
		{
			for (uint i = world.noises.Size(); i-- > 0;)
			{
				world.noises[i].age += deltaTime;
				if (world.noises[i].age > c_Lifetime)
				{
					world.noises.SwapAndPopBack(i);
				}
			}
		}
	}
}
