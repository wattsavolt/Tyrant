#include "EditorCameraController.h"
#include "Input/InputManager.h"
#include "World/Camera.h"
#include "Math/Math.h"

namespace tyr
{
	namespace
	{
		constexpr float c_MinFlySpeed = 0.25f;
		constexpr float c_MaxFlySpeed = 20.0f;
		// Multiplier applied per scroll-wheel notch while flying, to speed up or slow down.
		constexpr float c_FlySpeedScrollStep = 1.1f;
		// Degrees of rotation per pixel of raw mouse motion.
		constexpr float c_LookSensitivityDegrees = 0.15f;
	}

	void EditorCameraController::Update(InputManager& inputManager, Camera& camera, float deltaTime, bool isViewportHovered)
	{
		const bool rightMouseDown = inputManager.IsMouseButtonDown(MouseButton::Right);

		if (!m_Flying)
		{
			if (!rightMouseDown || !isViewportHovered)
			{
				return;
			}

			m_Flying = true;
			inputManager.SetMouseCaptured(true);
		}
		else if (!rightMouseDown)
		{
			m_Flying = false;
			inputManager.SetMouseCaptured(false);
			return;
		}

		// Mouse-look. If this feels inverted once tried in the editor, flip these two signs.
		const Vector2& lookDelta = inputManager.GetRawMouseDelta();
		if (lookDelta.x != 0.0f)
		{
			camera.SetYaw(lookDelta.x * c_LookSensitivityDegrees * Math::c_DegToRad);
		}
		if (lookDelta.y != 0.0f)
		{
			camera.SetPitch(-lookDelta.y * c_LookSensitivityDegrees * Math::c_DegToRad);
		}

		const float scrollDelta = inputManager.GetScrollDelta();
		if (scrollDelta != 0.0f)
		{
			m_FlySpeed = Math::Clamp(m_FlySpeed * Math::Pow(c_FlySpeedScrollStep, scrollDelta), c_MinFlySpeed, c_MaxFlySpeed);
		}

		const float move = m_FlySpeed * deltaTime;
		if (inputManager.IsKeyDown('W')) { camera.MoveForwardBack(move); }
		if (inputManager.IsKeyDown('S')) { camera.MoveForwardBack(-move); }
		if (inputManager.IsKeyDown('D')) { camera.MoveRightLeft(-move); }
		if (inputManager.IsKeyDown('A')) { camera.MoveRightLeft(move); }
		if (inputManager.IsKeyDown('E')) { camera.MoveUpDown(move); }
		if (inputManager.IsKeyDown('Q')) { camera.MoveUpDown(-move); }
	}
}
