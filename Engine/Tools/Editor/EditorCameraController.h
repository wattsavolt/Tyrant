#pragma once

#include "EditorMacros.h"

namespace tyr
{
	class InputManager;
	class Camera;

	// Unreal-style editor viewport fly camera: hold Right Mouse Button while the viewport panel
	// is hovered to enter fly mode (cursor hidden and captured, mouse-look plus WASD/QE
	// movement), release Right Mouse Button to exit. Owns only its own fly-mode state - the
	// InputManager and Camera it drives are passed in each call, same as EditorViewport itself.
	class TYR_EDITOR_EXPORT EditorCameraController final
	{
	public:
		// isViewportHovered only gates entering fly mode on right-mouse-down - once active,
		// movement keeps going even if the now-hidden cursor would have left the viewport panel,
		// matching Unreal's own behaviour, until the button is released.
		void Update(InputManager& inputManager, Camera& camera, float deltaTime, bool isViewportHovered);

	private:
		bool m_Flying = false;
		float m_FlySpeed = 2.0f;
	};
}
