#include "Scents.h"
#include "ECS/Components.h"
#include "World/World.h"

namespace tyr
{
	namespace Scents
	{
		void Update(World& world, float deltaTime)
		{
			for (uint i = world.scents.Size(); i-- > 0;)
			{
				ScentMarker& marker = world.scents[i];
				marker.age += deltaTime;
				if (marker.age > marker.lifetime)
				{
					world.scents.SwapAndPopBack(i);
				}
			}

			world.entities.ForEach<ScentComponent>([&world](Entity entity, ScentComponent& scent)
			{
				if (!world.entities.HasComponent<ComponentTransform>(entity))
				{
					return;
				}
				const Vector3& position = world.entities.GetComponent<ComponentTransform>(entity).world.position;
				const Vector3 moved = position - scent.lastMarker;
				if (scent.hasMarker && moved.Dot(moved) < scent.spacing * scent.spacing)
				{
					return;
				}
				scent.lastMarker = position;
				scent.hasMarker = true;

				ScentMarker marker;
				marker.position = position;
				marker.lifetime = scent.lifetime;
				marker.source = entity;
				if (world.scents.Size() < c_MaxScentMarkers)
				{
					world.scents.Add(marker);
					return;
				}
				// Full, so the oldest marker makes way.
				uint oldest = 0;
				for (uint i = 1; i < world.scents.Size(); ++i)
				{
					if (world.scents[i].age > world.scents[oldest].age)
					{
						oldest = i;
					}
				}
				world.scents[oldest] = marker;
			});
		}
	}
}
