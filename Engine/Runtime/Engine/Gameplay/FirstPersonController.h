#pragma once

#include "Gameplay/GameplayTypes.h"
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
		// Seconds of running from rested, and seconds to get all of it back, which starts once it's
		// stopped running for staminaRecoverDelay. Run out, it can't run again until it has this share back.
		float sprintStamina = 10.0f;
		float staminaRecoverTime = 7.0f;
		float staminaRecoverDelay = 1.0f;
		float exhaustedUntil = 0.4f;
		// How quickly it gets up to speed or stops, in metres per second per second.
		float acceleration = 14.0f;
		float airAcceleration = 3.0f;
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
		// Metres walked between footsteps, how far enemies hear each, and how loud each sounds against a
		// running step, by gait.
		float walkStride = 0.75f;
		float sprintStride = 1.0f;
		float crouchStride = 0.55f;
		float walkNoise = 2.0f;
		float sprintNoise = 6.0f;
		float crouchNoise = 0.0f;
		float walkStepVolume = 0.45f;
		float crouchStepVolume = 0.25f;
		// Played at the feet with each footstep, quieter for quieter gaits.
		SoundName footstepSound = "event:/Player/Footstep";
	};

	// Moves an entity with a CharacterComponent from mouse and keyboard or gamepad input, and puts
	// the world's camera at its eyes. WASD or the left stick moves, Shift or the left stick click
	// sprints while there's stamina left, and Ctrl held or C toggles crouching. There's no jumping.
	// Each footstep is a noise enemies can hear, louder the faster the gait.
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
		// Goes up with each footstep, which was heard out to the radius, for footstep sounds.
		uint GetStepCount() const { return m_StepCount; }
		float GetLastStepRadius() const { return m_LastStepRadius; }
		// From 0, run out, to 1, rested.
		float GetStamina() const { return m_Stamina; }
		bool IsExhausted() const { return m_Exhausted; }

		FirstPersonSettings settings;

	private:
		Entity m_Character = c_InvalidEntity;
		// Radians. Yaw turns around Y from +Z, and pitch looks up.
		float m_Yaw = 0.0f;
		float m_Pitch = 0.0f;
		Vector3 m_Velocity;
		float m_EyeHeight = 0.0f;
		bool m_CrouchToggled = false;
		// Running from a click of the controller's left stick, until the stick is let go.
		bool m_PadSprint = false;
		float m_Stamina = 1.0f;
		bool m_Exhausted = false;
		float m_StaminaRecoverWait = 0.0f;
		// Where the character was last frame, and how far it's walked since its last footstep.
		Vector3 m_LastPosition;
		bool m_HasLastPosition = false;
		float m_StrideProgress = 0.0f;
		uint m_StepCount = 0;
		float m_LastStepRadius = 0.0f;
	};
}
