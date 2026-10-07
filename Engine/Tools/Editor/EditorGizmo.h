#pragma once

#include "EditorMacros.h"
#include "ECS/Components.h"
#include "Math/Matrix4.h"

namespace tyr
{
	// Unreal-style translate, rotate and scale handles for one transform, drawn with ImGui over
	// the viewport's image and dragged with the left mouse button.
	class TYR_EDITOR_EXPORT EditorGizmo final
	{
	public:
		enum class Operation : uint8
		{
			Translate,
			Rotate,
			Scale
		};

		// The camera, and the screen rectangle the scene is drawn into.
		struct View
		{
			Matrix4 viewProj;
			Matrix4 inverseViewProj;
			Vector3 cameraPosition;
			Vector3 cameraRight;
			float x = 0.0f;
			float y = 0.0f;
			float width = 1.0f;
			float height = 1.0f;
		};

		// Draws the handles into the current window and drags them. A drag can only start while
		// canInteract is set. Translate and rotate use the world's axes when worldSpace is set,
		// otherwise the transform's own. Scaling always uses its own. A translationSnap of 0
		// turns snapping off. Returns true when transform was changed.
		bool Manipulate(const View& view, Operation operation, bool worldSpace, bool canInteract, float translationSnap, Transform& transform);

		// True while the mouse is over a handle or one is being dragged.
		bool IsActive() const { return m_HoveredHandle != Handle::None || m_DragHandle != Handle::None; }

	private:
		enum class Handle : uint8
		{
			None,
			AxisX,
			AxisY,
			AxisZ,
			// Named by the axis each plane faces along.
			PlaneX,
			PlaneY,
			PlaneZ,
			Uniform
		};

		// Everything about the gizmo's placement on screen for this frame.
		struct Frame
		{
			const View* view;
			Vector3 origin;
			Vector3 axes[3];
			// World-space length of an axis handle, so it stays the same size on screen.
			float length;
			float screenLength;
			bool axisVisible[3];
			bool planeVisible[3];
			Vector3 rayOrigin;
			Vector3 rayDirection;
		};

		Handle FindHoveredHandle(const Frame& frame, Operation operation) const;
		bool BeginDrag(const Frame& frame, Operation operation, const Transform& transform);
		bool UpdateDrag(const Frame& frame, Operation operation, bool worldSpace, float translationSnap, Transform& transform);
		void Draw(const Frame& frame, Operation operation) const;
		void DrawTranslate(const Frame& frame) const;
		void DrawRotate(const Frame& frame) const;
		void DrawScale(const Frame& frame) const;
		uint GetHandleColour(Handle handle, uint baseColour) const;

		Handle m_HoveredHandle = Handle::None;
		Handle m_DragHandle = Handle::None;

		// Captured when a drag starts.
		Transform m_StartTransform;
		Matrix4 m_StartModel;
		Vector3 m_StartAxes[3];
		Vector3 m_StartOrigin;
		float m_StartLength = 1.0f;
		// The plane the mouse ray is followed on, and where the ray first hit it.
		Vector3 m_DragPlaneNormal;
		Vector3 m_DragStartHit;
		float m_DragStartMouseX = 0.0f;
		// The rotation dragged so far, for drawing.
		float m_DragAngle = 0.0f;
	};
}
