#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "ECS/EntitySystem.h"

namespace tyr
{
	struct World;
	class InputManager;

	struct FirstPersonSettings
	{
		// Metres per second.
		float walkSpeed = 2.2f;
		float sprintSpeed = 4.5f;
		float crouchSpeed = 1.2f;
		// How quickly it gets up to speed or stops, in metres per second per second.
		float acceleration = 14.0f;
		float airAcceleration = 3.0f;
		// Upward speed a jump starts with, in metres per second.
		float jumpSpeed = 4.0f;
		// How far below the top of the character the eyes are.
		float eyeDepth = 0.12f;
		// How fast the eyes move between standing and crouching heights, in metres per second.
		float crouchCameraSpeed = 3.5f;
		// Degrees turned per pixel of mouse movement.
		float mouseSensitivity = 0.12f;
		// Degrees turned per second with a gamepad stick pushed all the way.
		float gamepadLookSpeed = 160.0f;
		// How far up or down it can look, in degrees.
		float maxPitch = 85.0f;
	};

	// Moves an entity with a CharacterComponent from mouse and keyboard or gamepad input, and puts
	// the world's camera at its eyes. WASD or the left stick moves, Shift or the left stick click
	// sprints, Ctrl held or C toggles crouching, and Space or A jumps.
	class TYR_ENGINE_API FirstPersonController final
	{
	public:
		// Takes control of the character, facing yaw radians around Y from +Z.
		void Possess(World& world, Entity character, float yaw);
		// Stops moving the character and leaves the camera where it is.
		void Release(World& world);

		// Without input the character stops and the view stays still.
		void Update(World& world, const InputManager& input, float deltaTime, bool hasInput);

		Entity GetCharacter() const { return m_Character; }

		FirstPersonSettings settings;

	private:
		Entity m_Character = c_InvalidEntity;
		// Radians. Yaw turns around Y from +Z, and pitch looks up.
		float m_Yaw = 0.0f;
		float m_Pitch = 0.0f;
		Vector3 m_Velocity;
		float m_EyeHeight = 0.0f;
		bool m_CrouchToggled = false;
	};
}
