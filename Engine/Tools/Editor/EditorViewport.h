#pragma once

#include "EditorMacros.h"
#include "RenderBase/RenderHandles.h"
#include "AssetSystem/AssetID.h"
#include "ECS/Components.h"
#include "EditorCameraController.h"
#include "EditorLayout.h"
#include "EditorIcons.h"
#include "EditorGizmo.h"

namespace tyr
{
	class RendererAPI;
	class InputManager;
	struct World;

	// A ray from the camera through a point in the viewport.
	struct ViewportRay
	{
		Vector3 origin;
		Vector3 direction;
	};

	// Shows a world's render target in a fixed panel, with the editor's fly camera controls and
	// Unreal-style translate, rotate and scale gizmos.
	class TYR_EDITOR_EXPORT EditorViewport final
	{
	public:
		// A mesh asset dropped into the viewport, and the ray through where it was dropped.
		struct MeshDrop
		{
			AssetID mesh;
			ViewportRay ray;
		};

		// What the viewport may do this frame.
		struct EditState
		{
			// The fly controls, drops, picking and gizmos are only active while editing.
			bool editing = false;
			// The selected actor's root entity, or c_InvalidEntity.
			Entity selectedActor = c_InvalidEntity;
			bool snapToGrid = false;
			float gridCellSize = 1.0f;
		};

		// What happened in the viewport this frame.
		struct Events
		{
			bool meshDropped = false;
			MeshDrop drop;
			// A left click away from the gizmo, for picking an actor.
			bool clicked = false;
			ViewportRay clickRay;
			// The gizmo moved the selected actor to this transform.
			bool transformChanged = false;
			Transform transform;
		};

		EditorViewport(RendererAPI& rendererAPI, const EditorIcons& icons);

		void Draw(const PanelRect& rect, World& world, InputManager& inputManager, float deltaTime, const EditState& state, Events& events);

	private:
		// The same view and projection the renderer builds for the world.
		static void CalculateViewProjection(const World& world, uint width, uint height, Matrix4& view, Matrix4& projection);
		// Draws the debug labels over the viewport image at their projected positions.
		static void DrawDebugTexts(const World& world, float imageX, float imageY, uint width, uint height);
		static ViewportRay CalculateMouseRay(const World& world, float mouseX, float mouseY, uint width, uint height);

		// Draws the selected actor's gizmo over the image. Returns true while it's hovered or dragged.
		bool DrawGizmo(World& world, float imageX, float imageY, uint width, uint height, bool canInteract, const EditState& state, Events& events);
		// Draws the buttons for picking the gizmo in the image's top right corner. Returns true
		// when one is hovered.
		bool DrawGizmoButtons(float imageRight, float imageTop);
		// Returns true when clicked.
		bool DrawGizmoButton(EditorIcons::Icon icon, const char* tooltip, bool active);
		// W, E and R switch the gizmo like in Unreal, unless the camera is flying.
		void HandleGizmoShortcuts();

		RendererAPI& m_RendererAPI;
		const EditorIcons& m_Icons;
		EditorCameraController m_CameraController;
		EditorGizmo m_Gizmo;
		EditorGizmo::Operation m_GizmoOperation = EditorGizmo::Operation::Translate;
		// Translate and rotate along the world's axes rather than the actor's own. Scaling is
		// always along the actor's own axes.
		bool m_GizmoWorldSpace = true;
	};
}
