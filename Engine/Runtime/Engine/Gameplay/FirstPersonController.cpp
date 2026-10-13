#include "FirstPersonController.h"
#include "ECS/Components.h"
#include "World/World.h"
#include "World/Camera.h"
#include "Input/InputManager.h"
#include "Math/Math.h"

namespace tyr
{
	namespace
	{
		// Win32 virtual-key codes, which InputManager's key codes are on PC.
		constexpr KeyCode c_KeyShift = 0x10;
		constexpr KeyCode c_KeyControl = 0x11;
		constexpr KeyCode c_KeySpace = 0x20;

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

		// Looking.
		if (hasInput)
		{
			const Vector2& mouse = input.GetRawMouseDelta();
			const Vector2& stick = input.GetGamepadRightStick();
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
			sprint = input.IsKeyDown(c_KeyShift) || input.IsGamepadButtonDown(GamepadButton::LeftThumb);

			if (input.IsKeyPressed('C') || input.IsGamepadButtonPressed(GamepadButton::B))
			{
				m_CrouchToggled = !m_CrouchToggled;
			}
			if ((input.IsKeyPressed(c_KeySpace) || input.IsGamepadButtonPressed(GamepadButton::A)) && character.grounded && !character.crouched)
			{
				character.jumpSpeed = settings.jumpSpeed;
			}
		}
		const float inputLength = Math::Sqrt(right * right + forward * forward);
		if (inputLength > 1.0f)
		{
			right /= inputLength;
			forward /= inputLength;
		}

		character.wantsCrouch = hasInput && (m_CrouchToggled || input.IsKeyDown(c_KeyControl));
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
