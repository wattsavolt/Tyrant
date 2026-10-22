#include "FirstPersonController.h"
#include "Gameplay/Sounds.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "World/Camera.h"
#include "Input/InputManager.h"
#include "Math/Math.h"
#include "Gameplay/Noise.h"
#include <algorithm>

namespace tyr
{
	namespace
	{
		// Win32 virtual-key codes, which InputManager's key codes are on PC.
		constexpr KeyCode c_KeyShift = 0x10;
		constexpr KeyCode c_KeyControl = 0x11;
		// Further than this in a frame is a teleport, not walking.
		constexpr float c_MaxStepPerFrame = 1.0f;

		Vector3 MoveTowards(const Vector3& current, const Vector3& target, float maxDistance)
		{
			const Vector3 toTarget = target - current;
			const float distance = toTarget.Length();
			return distance <= maxDistance || distance == 0.0f ? target : current + toTarget * (maxDistance / distance);
		}

		float MoveTowards(float current, float target, float maxDistance)
		{
			return Math::Abs(target - current) <= maxDistance ? target : current + (target > current ? maxDistance : -maxDistance);
		}

		float GetEyeHeight(const CharacterComponent& character, float eyeDepth)
		{
			return (character.crouched ? character.crouchHeight : character.height) - eyeDepth;
		}
	}

	void FirstPersonController::Possess(World& world, Entity character, float yaw)
	{
		m_Character = character;
		m_Yaw = yaw;
		m_Pitch = 0.0f;
		m_Velocity = Vector3::c_Zero;
		m_CrouchToggled = false;
		m_HasLastPosition = false;
		m_StrideProgress = 0.0f;
		m_Stamina = 1.0f;
		m_Exhausted = false;
		m_StaminaRecoverWait = 0.0f;
		if (world.entities.HasComponent<CharacterComponent>(character))
		{
			m_EyeHeight = GetEyeHeight(world.entities.GetComponent<CharacterComponent>(character), settings.eyeDepth);
		}
		world.cameraEntity = character;
		world.cameraOffset = Vector3(0.0f, m_EyeHeight, 0.0f);
	}

	void FirstPersonController::Release(World& world)
	{
		if (world.cameraEntity == m_Character)
		{
			world.cameraEntity = c_InvalidEntity;
		}
		if (world.entities.HasComponent<CharacterComponent>(m_Character))
		{
			CharacterComponent& character = world.entities.GetComponent<CharacterComponent>(m_Character);
			character.moveVelocity = Vector3::c_Zero;
			character.wantsCrouch = false;
		}
		m_Character = c_InvalidEntity;
	}

	void FirstPersonController::Update(World& world, const InputManager& input, float deltaTime, bool hasInput)
	{
		if (m_Character == c_InvalidEntity || !world.entities.HasComponent<CharacterComponent>(m_Character))
		{
			return;
		}
		CharacterComponent& character = world.entities.GetComponent<CharacterComponent>(m_Character);

		// Looking, or where a tool turned the view.
		if (world.viewRequested)
		{
			world.viewRequested = false;
			m_Yaw = world.requestedYaw;
			m_Pitch = Math::Clamp(world.requestedPitch, -settings.maxPitch * Math::c_DegToRad, settings.maxPitch * Math::c_DegToRad);
		}
		if (hasInput)
		{
			const Vector2& mouse = input.GetRawMouseDelta();
			// Squared, so a small push turns slowly enough to aim with and a full one still turns fast.
			const Vector2& rawStick = input.GetGamepadRightStick();
			const Vector2 stick(rawStick.x * Math::Abs(rawStick.x), rawStick.y * Math::Abs(rawStick.y));
			const float yawDegrees = mouse.x * settings.mouseSensitivity + stick.x * settings.gamepadLookSpeed * deltaTime;
			const float pitchDegrees = -mouse.y * settings.mouseSensitivity + stick.y * settings.gamepadLookSpeed * deltaTime;
			const float maxPitch = settings.maxPitch * Math::c_DegToRad;
			m_Yaw += yawDegrees * Math::c_DegToRad;
			m_Pitch = Math::Clamp(m_Pitch + pitchDegrees * Math::c_DegToRad, -maxPitch, maxPitch);
		}

		// Which way to move, as right and forward from -1 to 1.
		float right = 0.0f;
		float forward = 0.0f;
		bool sprint = false;
		if (hasInput)
		{
			forward = (input.IsKeyDown('W') ? 1.0f : 0.0f) - (input.IsKeyDown('S') ? 1.0f : 0.0f) + input.GetGamepadLeftStick().y;
			right = (input.IsKeyDown('D') ? 1.0f : 0.0f) - (input.IsKeyDown('A') ? 1.0f : 0.0f) + input.GetGamepadLeftStick().x;
			// Clicking the left stick runs until the stick is let go, rather than having to be held in.
			const Vector2& moveStick = input.GetGamepadLeftStick();
			if (input.IsGamepadButtonPressed(GamepadButton::LeftThumb))
			{
				m_PadSprint = !m_PadSprint;
			}
			m_PadSprint = m_PadSprint && moveStick.x * moveStick.x + moveStick.y * moveStick.y > 0.04f;
			sprint = input.IsKeyDown(c_KeyShift) || m_PadSprint;

			if (input.IsKeyPressed('C') || input.IsGamepadButtonPressed(GamepadButton::B))
			{
				m_CrouchToggled = !m_CrouchToggled;
			}
		}
		// Held still while hiding, only able to look about.
		if (character.frozen)
		{
			right = 0.0f;
			forward = 0.0f;
			sprint = false;
			m_Velocity = Vector3::c_Zero;
			m_HasLastPosition = false;
		}
		const float inputLength = Math::Sqrt(right * right + forward * forward);
		if (inputLength > 1.0f)
		{
			right /= inputLength;
			forward /= inputLength;
		}

		character.wantsCrouch = hasInput && !character.frozen && (m_CrouchToggled || input.IsKeyDown(c_KeyControl));

		// Running uses up stamina, which comes back after a moment's rest. Once spent, it's walking
		// until enough has come back.
		const bool running = sprint && !character.crouched && !m_Exhausted && inputLength > 0.1f;
		if (running)
		{
			m_Stamina = std::max(m_Stamina - deltaTime / std::max(settings.sprintStamina, 0.01f), 0.0f);
			m_StaminaRecoverWait = settings.staminaRecoverDelay;
			m_Exhausted = m_Stamina <= 0.0f;
			m_PadSprint = m_PadSprint && !m_Exhausted;
		}
		else
		{
			m_StaminaRecoverWait -= deltaTime;
			if (m_StaminaRecoverWait <= 0.0f)
			{
				m_Stamina = std::min(m_Stamina + deltaTime / std::max(settings.staminaRecoverTime, 0.01f), 1.0f);
			}
			m_Exhausted = m_Exhausted && m_Stamina < settings.exhaustedUntil;
		}
		sprint = running;
		// Standing up is refused while something is overhead, so speed follows the actual stance.
		const float speed = character.crouched ? settings.crouchSpeed : (sprint ? settings.sprintSpeed : settings.walkSpeed);

		const float sinYaw = Math::Sin(m_Yaw);
		const float cosYaw = Math::Cos(m_Yaw);
		const Vector3 forwardAxis(sinYaw, 0.0f, cosYaw);
		const Vector3 rightAxis(cosYaw, 0.0f, -sinYaw);
		const Vector3 targetVelocity = (forwardAxis * forward + rightAxis * right) * speed;
		const float acceleration = character.grounded ? settings.acceleration : settings.airAcceleration;
		m_Velocity = MoveTowards(m_Velocity, targetVelocity, acceleration * deltaTime);
		character.moveVelocity = m_Velocity;

		// A footstep every stride walked on the ground.
		if (world.entities.HasComponent<TransformComponent>(m_Character))
		{
			const Vector3 position = world.entities.GetComponent<TransformComponent>(m_Character).world.position;
			const Vector3 moved = position - m_LastPosition;
			const float distance = Math::Sqrt(moved.x * moved.x + moved.z * moved.z);
			if (m_HasLastPosition && character.grounded && distance < c_MaxStepPerFrame)
			{
				m_StrideProgress += distance;
			}
			m_LastPosition = position;
			m_HasLastPosition = true;

			const bool sprinting = !character.crouched && sprint;
			const float stride = character.crouched ? settings.crouchStride : (sprinting ? settings.sprintStride : settings.walkStride);
			if (m_StrideProgress >= stride)
			{
				m_StrideProgress = std::min(m_StrideProgress - stride, stride);
				m_LastStepRadius = character.crouched ? settings.crouchNoise : (sprinting ? settings.sprintNoise : settings.walkNoise);
				++m_StepCount;
				Noise::Emit(world, position, m_LastStepRadius, m_Character);
				Sounds::Play(world, settings.footstepSound, position, character.crouched ? settings.crouchStepVolume : (sprinting ? 1.0f : settings.walkStepVolume));
			}
		}

		// The world puts the camera at the character's eyes once physics has moved it this frame.
		m_EyeHeight = MoveTowards(m_EyeHeight, GetEyeHeight(character, settings.eyeDepth), settings.crouchCameraSpeed * deltaTime);
		world.cameraEntity = m_Character;
		world.cameraOffset = Vector3(0.0f, m_EyeHeight, 0.0f);

		const float sinPitch = Math::Sin(m_Pitch);
		const float cosPitch = Math::Cos(m_Pitch);
		const Vector3 look(sinYaw * cosPitch, sinPitch, cosYaw * cosPitch);
		const Vector3 up(-sinYaw * sinPitch, cosPitch, -cosYaw * sinPitch);
		world.camera->SetOrientation(look, up);
	}
}
