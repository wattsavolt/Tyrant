#pragma once

#include "EditorMacros.h"
#include "RenderBase/RenderHandles.h"
#include "AssetSystem/AssetID.h"
#include "Math/Vector3.h"
#include "EditorCameraController.h"
#include "EditorLayout.h"

namespace tyr
{
	class RendererAPI;
	class InputManager;
	struct World;

	// Shows a world's render target in a fixed panel and owns the editor's fly camera controls.
	class TYR_EDITOR_EXPORT EditorViewport final
	{
	public:
		// A mesh asset dropped into the viewport, and the camera ray through where it was dropped.
		struct MeshDrop
		{
			AssetID mesh;
			Vector3 rayOrigin;
			Vector3 rayDirection;
		};

		EditorViewport(RendererAPI& rendererAPI);

		// The fly controls and drops are only active while editing, since the game owns its own
		// camera. Returns true when a mesh was dropped this frame, filling in drop.
		bool Draw(const PanelRect& rect, const World& world, InputManager& inputManager, bool editing, float deltaTime, MeshDrop& drop);

	private:
		// Fills in drop's ray from the camera through the mouse position.
		static void CalculateMouseRay(const World& world, float mouseX, float mouseY, uint width, uint height, MeshDrop& drop);

		RendererAPI& m_RendererAPI;
		EditorCameraController m_CameraController;
	};
}
