#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"
#include "ECS/EntitySystem.h"
#include "Actor/ActorTypes.h"

namespace tyr
{
	struct World;
	struct DoorComponent;

	namespace Doors
	{
		// Where the door is for its open amount, eased so it speeds up and slows down.
		TYR_ENGINE_API void GetPose(const DoorComponent& door, Vector3& position, Quaternion& rotation);

		// Remembers where the door is as its closed pose, if it doesn't know it yet.
		TYR_ENGINE_API void CaptureClosedPose(World& world, Entity entity);

		// Starts the door, and the door linked to it, opening or closing. A swinging door that
		// opens away turns away from userPosition. Enemies hear it, except the user if it's given.
		TYR_ENGINE_API void SetOpen(World& world, Entity entity, bool open, const Vector3& userPosition, Entity user = c_InvalidEntity);

		// Opens the door, and the door linked to it, flying away from userPosition in its burst open time.
		TYR_ENGINE_API void BurstOpen(World& world, Entity entity, const Vector3& userPosition, Entity user = c_InvalidEntity);

		// Knocks a shut door, and the door linked to it, a little open away from userPosition, to fall
		// straight back, with its bash sound.
		TYR_ENGINE_API void Bash(World& world, Entity entity, const Vector3& userPosition, Entity user = c_InvalidEntity);

		// Breaks the door off, to fall flat towards fromPosition and lie there for good.
		TYR_ENGINE_API void Break(World& world, Entity entity, const Vector3& fromPosition);

		// Moves a broken door on as it falls. False once it's lying flat, or if it isn't broken.
		TYR_ENGINE_API bool UpdateFall(DoorComponent& door, float deltaTime);

		// Opens a closed door or closes an open one.
		TYR_ENGINE_API void Toggle(World& world, Entity entity, const Vector3& userPosition, Entity user = c_InvalidEntity);

		// Unlocks the door and the door linked to it for good.
		TYR_ENGINE_API void Unlock(World& world, Entity entity);

		// True when the door is open or opening.
		TYR_ENGINE_API bool IsOpening(const World& world, Entity entity);

		// The root entity of the door actor with this name, if it has a door.
		TYR_ENGINE_API Entity Find(const World& world, const ActorName& name);

		// True when the position is on the door's front, the side its Z axis points to.
		TYR_ENGINE_API bool IsInFront(const World& world, Entity entity, const Vector3& position);
	}
}
