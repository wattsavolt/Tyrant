#include "EditorGizmo.h"
#include "Math/Math.h"
#include "imgui.h"
#include <algorithm>

namespace tyr
{
	namespace
	{
		// An axis handle's length on screen, as a fraction of the view's height.
		constexpr float c_ScreenSizeFactor = 0.15f;
		// How close in pixels the mouse has to be to grab a handle.
		constexpr float c_HoverDistance = 10.0f;
		constexpr float c_LineThickness = 3.0f;
		// Where along an axis its handle starts, leaving the centre clear.
		constexpr float c_AxisStart = 0.15f;
		// Where the plane handles sit between their two axes.
		constexpr float c_PlaneStart = 0.3f;
		constexpr float c_PlaneEnd = 0.55f;
		constexpr uint c_RingSegments = 64;
		constexpr uint c_RotationFanSegments = 24;
		constexpr float c_ArrowLength = 12.0f;
		constexpr float c_ArrowHalfWidth = 6.0f;
		constexpr float c_ScaleBoxHalfSize = 5.0f;
		constexpr float c_UniformBoxHalfSize = 7.0f;
		// How much the uniform scale changes per pixel the mouse moves sideways.
		constexpr float c_UniformScalePerPixel = 0.01f;
		constexpr float c_MinScale = 0.001f;
		// Below these, an axis or plane faces the camera too directly to be grabbed.
		constexpr float c_MinAxisScreenFraction = 0.15f;
		constexpr float c_MinPlaneScreenArea = 0.04f;

		const ImU32 c_AxisColours[3] = { IM_COL32(220, 60, 60, 255), IM_COL32(90, 200, 70, 255), IM_COL32(60, 110, 235, 255) };
		const ImU32 c_HighlightColour = IM_COL32(255, 215, 40, 255);
		const ImU32 c_UniformColour = IM_COL32(230, 230, 230, 255);

		ImU32 WithAlpha(ImU32 colour, uint alpha)
		{
			return (colour & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
		}

		bool WorldToScreen(const EditorGizmo::View& view, const Vector3& point, ImVec2& screen)
		{
			const Vector4 clip = view.viewProj.Multiply(Vector4(point.x, point.y, point.z, 1.0f));
			if (clip.w <= 1e-5f)
			{
				return false;
			}
			// The viewport is flipped, so NDC y is +1 at the top.
			const float ndcX = clip.x / clip.w;
			const float ndcY = clip.y / clip.w;
			screen = ImVec2(view.x + (ndcX * 0.5f + 0.5f) * view.width, view.y + (0.5f - ndcY * 0.5f) * view.height);
			return true;
		}

		void ScreenToRay(const EditorGizmo::View& view, const ImVec2& screen, Vector3& origin, Vector3& direction)
		{
			const float ndcX = (screen.x - view.x) / view.width * 2.0f - 1.0f;
			const float ndcY = 1.0f - (screen.y - view.y) / view.height * 2.0f;
			const Vector4 point = view.inverseViewProj.Multiply(Vector4(ndcX, ndcY, 0.5f, 1.0f));
			origin = view.cameraPosition;
			direction = Vector3::Normalize(Vector3(point.x, point.y, point.z) / point.w - origin);
		}

		bool IntersectPlane(const Vector3& rayOrigin, const Vector3& rayDirection, const Vector3& planePoint, const Vector3& planeNormal, Vector3& hit)
		{
			const float denominator = rayDirection.Dot(planeNormal);
			if (Math::Abs(denominator) < 1e-6f)
			{
				return false;
			}
			const float distance = (planePoint - rayOrigin).Dot(planeNormal) / denominator;
			if (distance < 0.0f)
			{
				return false;
			}
			hit = rayOrigin + rayDirection * distance;
			return true;
		}

		float DistanceToSegment(const ImVec2& point, const ImVec2& a, const ImVec2& b)
		{
			const ImVec2 ab(b.x - a.x, b.y - a.y);
			const ImVec2 ap(point.x - a.x, point.y - a.y);
			const float lengthSquared = ab.x * ab.x + ab.y * ab.y;
			const float t = lengthSquared > 0.0f ? std::clamp((ap.x * ab.x + ap.y * ab.y) / lengthSquared, 0.0f, 1.0f) : 0.0f;
			const float dx = ap.x - ab.x * t;
			const float dy = ap.y - ab.y * t;
			return Math::Sqrt(dx * dx + dy * dy);
		}

		float ScreenDistance(const ImVec2& a, const ImVec2& b)
		{
			const float dx = a.x - b.x;
			const float dy = a.y - b.y;
			return Math::Sqrt(dx * dx + dy * dy);
		}

		// Rotates v around a unit axis by angle radians.
		Vector3 RotateAroundAxis(const Vector3& v, const Vector3& axis, float angle)
		{
			const float cosAngle = Math::Cos(angle);
			const float sinAngle = Math::Sin(angle);
			return v * cosAngle + axis.Cross(v) * sinAngle + axis * (axis.Dot(v) * (1.0f - cosAngle));
		}

		// Any unit vector at right angles to the unit axis.
		Vector3 GetPerpendicular(const Vector3& axis)
		{
			const Vector3 other = Math::Abs(axis.x) < 0.9f ? Vector3(1.0f, 0.0f, 0.0f) : Vector3(0.0f, 1.0f, 0.0f);
			return Vector3::Normalize(axis.Cross(other));
		}

		float Snap(float value, float step)
		{
			return Math::Round(value / step) * step;
		}
	}

	bool EditorGizmo::Manipulate(const View& view, Operation operation, bool worldSpace, bool canInteract, float translationSnap, Transform& transform)
	{
		Frame frame;
		frame.view = &view;
		frame.origin = transform.position;

		// Scaling always follows the transform's own axes. A drag keeps the axes it started with.
		const bool localAxes = !worldSpace || operation == Operation::Scale;
		if (m_DragHandle != Handle::None)
		{
			for (uint i = 0; i < 3; ++i)
			{
				frame.axes[i] = m_StartAxes[i];
			}
		}
		else if (localAxes)
		{
			const Matrix4 rotation = Matrix4::CreateTRS(Vector3::c_Zero, transform.rotation, Vector3::c_One);
			for (uint i = 0; i < 3; ++i)
			{
				const Vector4& row = rotation[i];
				frame.axes[i] = Vector3::Normalize(Vector3(row.x, row.y, row.z));
			}
		}
		else
		{
			frame.axes[0] = Vector3(1.0f, 0.0f, 0.0f);
			frame.axes[1] = Vector3(0.0f, 1.0f, 0.0f);
			frame.axes[2] = Vector3(0.0f, 0.0f, 1.0f);
		}

		// Sized so a world unit along the camera's right covers the same pixels at any distance.
		ImVec2 originScreen;
		ImVec2 rightScreen;
		if (!WorldToScreen(view, frame.origin, originScreen) || !WorldToScreen(view, frame.origin + view.cameraRight, rightScreen))
		{
			m_HoveredHandle = Handle::None;
			m_DragHandle = Handle::None;
			return false;
		}
		const float pixelsPerUnit = std::max(ScreenDistance(originScreen, rightScreen), 1e-4f);
		frame.screenLength = view.height * c_ScreenSizeFactor;
		frame.length = frame.screenLength / pixelsPerUnit;

		for (uint i = 0; i < 3; ++i)
		{
			ImVec2 tip;
			frame.axisVisible[i] = WorldToScreen(view, frame.origin + frame.axes[i] * frame.length, tip)
				&& ScreenDistance(originScreen, tip) > frame.screenLength * c_MinAxisScreenFraction;
		}
		for (uint i = 0; i < 3; ++i)
		{
			// The area the plane's two axes span on screen, compared with a full handle's square.
			ImVec2 a;
			ImVec2 b;
			const bool onScreen = WorldToScreen(view, frame.origin + frame.axes[(i + 1) % 3] * frame.length, a)
				&& WorldToScreen(view, frame.origin + frame.axes[(i + 2) % 3] * frame.length, b);
			const float area = Math::Abs((a.x - originScreen.x) * (b.y - originScreen.y) - (a.y - originScreen.y) * (b.x - originScreen.x));
			frame.planeVisible[i] = onScreen && area > frame.screenLength * frame.screenLength * c_MinPlaneScreenArea;
		}

		ScreenToRay(view, ImGui::GetMousePos(), frame.rayOrigin, frame.rayDirection);

		bool changed = false;
		if (m_DragHandle != Handle::None)
		{
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				changed = UpdateDrag(frame, operation, worldSpace, translationSnap, transform);
				// Drawn where the drag just moved it, so it doesn't trail the mouse by a frame.
				frame.origin = transform.position;
			}
			else
			{
				m_DragHandle = Handle::None;
			}
		}

		m_HoveredHandle = m_DragHandle == Handle::None && canInteract ? FindHoveredHandle(frame, operation) : Handle::None;
		if (m_HoveredHandle != Handle::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && BeginDrag(frame, operation, transform))
		{
			m_DragHandle = m_HoveredHandle;
		}

		Draw(frame, operation);
		return changed;
	}

	EditorGizmo::Handle EditorGizmo::FindHoveredHandle(const Frame& frame, Operation operation) const
	{
		const View& view = *frame.view;
		const ImVec2 mouse = ImGui::GetMousePos();
		Handle hovered = Handle::None;
		float nearest = c_HoverDistance;

		if (operation == Operation::Rotate)
		{
			// The nearest point on any ring's half facing the camera.
			const Vector3 toCamera = view.cameraPosition - frame.origin;
			for (uint i = 0; i < 3; ++i)
			{
				const Vector3 u = GetPerpendicular(frame.axes[i]);
				const Vector3 w = frame.axes[i].Cross(u);
				Vector3 previous = frame.origin + u * frame.length;
				for (uint segment = 1; segment <= c_RingSegments; ++segment)
				{
					const float angle = Math::c_TwoPi * static_cast<float>(segment) / static_cast<float>(c_RingSegments);
					const Vector3 current = frame.origin + (u * Math::Cos(angle) + w * Math::Sin(angle)) * frame.length;
					ImVec2 a;
					ImVec2 b;
					const bool facesCamera = ((previous + current) * 0.5f - frame.origin).Dot(toCamera) >= 0.0f;
					if (facesCamera && WorldToScreen(view, previous, a) && WorldToScreen(view, current, b))
					{
						const float distance = DistanceToSegment(mouse, a, b);
						if (distance < nearest)
						{
							nearest = distance;
							hovered = static_cast<Handle>(static_cast<uint>(Handle::AxisX) + i);
						}
					}
					previous = current;
				}
			}
			return hovered;
		}

		if (operation == Operation::Scale)
		{
			ImVec2 originScreen;
			if (WorldToScreen(view, frame.origin, originScreen) && ScreenDistance(mouse, originScreen) < c_UniformBoxHalfSize + 3.0f)
			{
				return Handle::Uniform;
			}
		}

		// The axis handles take priority over the planes between them.
		for (uint i = 0; i < 3; ++i)
		{
			ImVec2 a;
			ImVec2 b;
			if (frame.axisVisible[i] && WorldToScreen(view, frame.origin + frame.axes[i] * (frame.length * c_AxisStart), a)
				&& WorldToScreen(view, frame.origin + frame.axes[i] * frame.length, b))
			{
				const float distance = DistanceToSegment(mouse, a, b);
				if (distance < nearest)
				{
					nearest = distance;
					hovered = static_cast<Handle>(static_cast<uint>(Handle::AxisX) + i);
				}
			}
		}
		if (hovered != Handle::None || operation != Operation::Translate)
		{
			return hovered;
		}

		for (uint i = 0; i < 3; ++i)
		{
			Vector3 hit;
			if (!frame.planeVisible[i] || !IntersectPlane(frame.rayOrigin, frame.rayDirection, frame.origin, frame.axes[i], hit))
			{
				continue;
			}
			const Vector3 offset = (hit - frame.origin) / frame.length;
			const float u = offset.Dot(frame.axes[(i + 1) % 3]);
			const float v = offset.Dot(frame.axes[(i + 2) % 3]);
			if (u >= c_PlaneStart && u <= c_PlaneEnd && v >= c_PlaneStart && v <= c_PlaneEnd)
			{
				return static_cast<Handle>(static_cast<uint>(Handle::PlaneX) + i);
			}
		}
		return Handle::None;
	}

	bool EditorGizmo::BeginDrag(const Frame& frame, Operation operation, const Transform& transform)
	{
		const uint handle = static_cast<uint>(m_HoveredHandle);
		const bool isAxis = m_HoveredHandle >= Handle::AxisX && m_HoveredHandle <= Handle::AxisZ;
		const uint axisIndex = isAxis ? handle - static_cast<uint>(Handle::AxisX) : handle - static_cast<uint>(Handle::PlaneX);

		if (m_HoveredHandle == Handle::Uniform)
		{
			m_DragPlaneNormal = Vector3::c_Zero;
		}
		else if (operation == Operation::Rotate || !isAxis)
		{
			// Rings and planes are followed on their own plane.
			m_DragPlaneNormal = frame.axes[axisIndex];
		}
		else
		{
			// An axis is followed on the plane holding it that faces the camera most.
			const Vector3& axis = frame.axes[axisIndex];
			const Vector3 toCamera = Vector3::Normalize(frame.view->cameraPosition - frame.origin);
			m_DragPlaneNormal = axis.Cross(axis.Cross(toCamera));
			if (m_DragPlaneNormal.Length() < 1e-4f)
			{
				return false;
			}
			m_DragPlaneNormal.Normalize();
		}

		if (m_HoveredHandle != Handle::Uniform
			&& !IntersectPlane(frame.rayOrigin, frame.rayDirection, frame.origin, m_DragPlaneNormal, m_DragStartHit))
		{
			return false;
		}

		m_StartTransform = transform;
		m_StartModel = Matrix4::CreateTRS(transform.position, transform.rotation, transform.scale);
		for (uint i = 0; i < 3; ++i)
		{
			m_StartAxes[i] = frame.axes[i];
		}
		m_StartOrigin = frame.origin;
		m_StartLength = frame.length;
		m_DragStartMouseX = ImGui::GetMousePos().x;
		m_DragAngle = 0.0f;
		return true;
	}

	bool EditorGizmo::UpdateDrag(const Frame& frame, Operation operation, bool worldSpace, float translationSnap, Transform& transform)
	{
		const Transform previous = transform;

		if (m_DragHandle == Handle::Uniform)
		{
			const float factor = std::max(1.0f + (ImGui::GetMousePos().x - m_DragStartMouseX) * c_UniformScalePerPixel, c_MinScale);
			transform.scale = m_StartTransform.scale * factor;
		}
		else
		{
			Vector3 hit;
			if (!IntersectPlane(frame.rayOrigin, frame.rayDirection, m_StartOrigin, m_DragPlaneNormal, hit))
			{
				return false;
			}

			const uint handle = static_cast<uint>(m_DragHandle);
			const bool isAxis = m_DragHandle <= Handle::AxisZ;
			const uint axisIndex = isAxis ? handle - static_cast<uint>(Handle::AxisX) : handle - static_cast<uint>(Handle::PlaneX);
			const Vector3& axis = m_StartAxes[axisIndex];

			if (operation == Operation::Translate)
			{
				// Along the axis, or anywhere on the plane.
				Vector3 delta = hit - m_DragStartHit;
				uint movedAxes[2] = { axisIndex, axisIndex };
				uint movedAxisCount = 1;
				if (isAxis)
				{
					delta = axis * delta.Dot(axis);
				}
				else
				{
					delta = delta - axis * delta.Dot(axis);
					movedAxes[0] = (axisIndex + 1) % 3;
					movedAxes[1] = (axisIndex + 2) % 3;
					movedAxisCount = 2;
				}

				Vector3 position = m_StartTransform.position + delta;
				if (translationSnap > 0.0f)
				{
					if (worldSpace)
					{
						// Like Unreal, onto the grid.
						for (uint i = 0; i < movedAxisCount; ++i)
						{
							position[movedAxes[i]] = Snap(position[movedAxes[i]], translationSnap);
						}
					}
					else
					{
						// In whole steps along the transform's own axes.
						Vector3 snappedDelta = Vector3::c_Zero;
						for (uint i = 0; i < movedAxisCount; ++i)
						{
							const Vector3& movedAxis = m_StartAxes[movedAxes[i]];
							snappedDelta += movedAxis * Snap(delta.Dot(movedAxis), translationSnap);
						}
						position = m_StartTransform.position + snappedDelta;
					}
				}
				transform.position = position;
			}
			else if (operation == Operation::Rotate)
			{
				const Vector3 startDirection = Vector3::Normalize(m_DragStartHit - m_StartOrigin);
				const Vector3 currentDirection = Vector3::Normalize(hit - m_StartOrigin);
				m_DragAngle = Math::Atan2(startDirection.Cross(currentDirection).Dot(axis), startDirection.Dot(currentDirection));

				// Turns each of the transform's scaled axes around the ring's axis.
				Matrix4 model = m_StartModel;
				for (uint row = 0; row < 3; ++row)
				{
					const Vector4& startRow = m_StartModel[row];
					const Vector3 rotated = RotateAroundAxis(Vector3(startRow.x, startRow.y, startRow.z), axis, m_DragAngle);
					model.SetRow(row, Vector4(rotated.x, rotated.y, rotated.z, 0.0f));
				}
				model.DecomposeTRS(transform.position, transform.rotation, transform.scale);
				transform.position = m_StartTransform.position;
			}
			else
			{
				// How far the mouse is along the axis compared with where the drag started.
				const float startDistance = (m_DragStartHit - m_StartOrigin).Dot(axis);
				if (Math::Abs(startDistance) < 1e-5f)
				{
					return false;
				}
				const float ratio = std::max((hit - m_StartOrigin).Dot(axis) / startDistance, c_MinScale);
				transform.scale = m_StartTransform.scale;
				transform.scale[axisIndex] = std::max(m_StartTransform.scale[axisIndex] * ratio, c_MinScale);
			}
		}

		return !(transform.position == previous.position) || !(transform.rotation == previous.rotation) || !(transform.scale == previous.scale);
	}

	ImU32 EditorGizmo::GetHandleColour(Handle handle, ImU32 baseColour) const
	{
		const Handle active = m_DragHandle != Handle::None ? m_DragHandle : m_HoveredHandle;
		return handle == active ? c_HighlightColour : baseColour;
	}

	void EditorGizmo::Draw(const Frame& frame, Operation operation) const
	{
		const View& view = *frame.view;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->PushClipRect(ImVec2(view.x, view.y), ImVec2(view.x + view.width, view.y + view.height), true);

		switch (operation)
		{
		case Operation::Translate:
			DrawTranslate(frame);
			break;
		case Operation::Rotate:
			DrawRotate(frame);
			break;
		case Operation::Scale:
			DrawScale(frame);
			break;
		}

		drawList->PopClipRect();
	}

	void EditorGizmo::DrawTranslate(const Frame& frame) const
	{
		const View& view = *frame.view;
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		for (uint i = 0; i < 3; ++i)
		{
			if (!frame.planeVisible[i])
			{
				continue;
			}
			const Vector3& a = frame.axes[(i + 1) % 3];
			const Vector3& b = frame.axes[(i + 2) % 3];
			const float corners[4][2] = { { c_PlaneStart, c_PlaneStart }, { c_PlaneEnd, c_PlaneStart }, { c_PlaneEnd, c_PlaneEnd }, { c_PlaneStart, c_PlaneEnd } };
			ImVec2 points[4];
			bool onScreen = true;
			for (uint corner = 0; corner < 4; ++corner)
			{
				const Vector3 point = frame.origin + (a * corners[corner][0] + b * corners[corner][1]) * frame.length;
				onScreen &= WorldToScreen(view, point, points[corner]);
			}
			if (onScreen)
			{
				const Handle handle = static_cast<Handle>(static_cast<uint>(Handle::PlaneX) + i);
				const ImU32 colour = GetHandleColour(handle, c_AxisColours[i]);
				drawList->AddConvexPolyFilled(points, 4, WithAlpha(colour, colour == c_HighlightColour ? 160 : 80));
				drawList->AddPolyline(points, 4, colour, ImDrawFlags_Closed, 1.0f);
			}
		}

		for (uint i = 0; i < 3; ++i)
		{
			ImVec2 start;
			ImVec2 end;
			if (!frame.axisVisible[i] || !WorldToScreen(view, frame.origin + frame.axes[i] * (frame.length * c_AxisStart), start)
				|| !WorldToScreen(view, frame.origin + frame.axes[i] * frame.length, end))
			{
				continue;
			}

			const ImU32 colour = GetHandleColour(static_cast<Handle>(static_cast<uint>(Handle::AxisX) + i), c_AxisColours[i]);
			drawList->AddLine(start, end, colour, c_LineThickness);

			// An arrowhead pointing on along the axis.
			const float length = std::max(ScreenDistance(start, end), 1e-4f);
			const ImVec2 direction((end.x - start.x) / length, (end.y - start.y) / length);
			const ImVec2 side(-direction.y * c_ArrowHalfWidth, direction.x * c_ArrowHalfWidth);
			const ImVec2 tip(end.x + direction.x * c_ArrowLength, end.y + direction.y * c_ArrowLength);
			drawList->AddTriangleFilled(tip, ImVec2(end.x + side.x, end.y + side.y), ImVec2(end.x - side.x, end.y - side.y), colour);
		}
	}

	void EditorGizmo::DrawRotate(const Frame& frame) const
	{
		const View& view = *frame.view;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const Vector3 toCamera = view.cameraPosition - frame.origin;

		for (uint i = 0; i < 3; ++i)
		{
			const Handle handle = static_cast<Handle>(static_cast<uint>(Handle::AxisX) + i);
			const ImU32 colour = GetHandleColour(handle, c_AxisColours[i]);
			const Vector3 u = GetPerpendicular(frame.axes[i]);
			const Vector3 w = frame.axes[i].Cross(u);

			// The half facing away from the camera is drawn faint.
			Vector3 previous = frame.origin + u * frame.length;
			for (uint segment = 1; segment <= c_RingSegments; ++segment)
			{
				const float angle = Math::c_TwoPi * static_cast<float>(segment) / static_cast<float>(c_RingSegments);
				const Vector3 current = frame.origin + (u * Math::Cos(angle) + w * Math::Sin(angle)) * frame.length;
				ImVec2 a;
				ImVec2 b;
				if (WorldToScreen(view, previous, a) && WorldToScreen(view, current, b))
				{
					const bool facesCamera = ((previous + current) * 0.5f - frame.origin).Dot(toCamera) >= 0.0f;
					drawList->AddLine(a, b, facesCamera ? colour : WithAlpha(colour, 70), facesCamera ? c_LineThickness : 1.5f);
				}
				previous = current;
			}
		}

		// The angle turned so far, as a filled slice of the ring.
		if (m_DragHandle != Handle::None && Math::Abs(m_DragAngle) > 1e-4f)
		{
			const Vector3& axis = m_StartAxes[static_cast<uint>(m_DragHandle) - static_cast<uint>(Handle::AxisX)];
			const Vector3 startDirection = Vector3::Normalize(m_DragStartHit - m_StartOrigin);
			ImVec2 points[c_RotationFanSegments + 2];
			bool onScreen = WorldToScreen(view, frame.origin, points[0]);
			for (uint segment = 0; segment <= c_RotationFanSegments; ++segment)
			{
				const float angle = m_DragAngle * static_cast<float>(segment) / static_cast<float>(c_RotationFanSegments);
				onScreen &= WorldToScreen(view, frame.origin + RotateAroundAxis(startDirection, axis, angle) * frame.length, points[segment + 1]);
			}
			if (onScreen)
			{
				drawList->AddConvexPolyFilled(points, c_RotationFanSegments + 2, WithAlpha(c_HighlightColour, 70));
				drawList->AddLine(points[0], points[1], c_HighlightColour, 1.5f);
				drawList->AddLine(points[0], points[c_RotationFanSegments + 1], c_HighlightColour, 1.5f);
			}
		}
	}

	void EditorGizmo::DrawScale(const Frame& frame) const
	{
		const View& view = *frame.view;
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		for (uint i = 0; i < 3; ++i)
		{
			ImVec2 start;
			ImVec2 end;
			if (!frame.axisVisible[i] || !WorldToScreen(view, frame.origin + frame.axes[i] * (frame.length * c_AxisStart), start)
				|| !WorldToScreen(view, frame.origin + frame.axes[i] * frame.length, end))
			{
				continue;
			}

			const ImU32 colour = GetHandleColour(static_cast<Handle>(static_cast<uint>(Handle::AxisX) + i), c_AxisColours[i]);
			drawList->AddLine(start, end, colour, c_LineThickness);
			drawList->AddRectFilled(ImVec2(end.x - c_ScaleBoxHalfSize, end.y - c_ScaleBoxHalfSize), ImVec2(end.x + c_ScaleBoxHalfSize, end.y + c_ScaleBoxHalfSize), colour);
		}

		ImVec2 originScreen;
		if (WorldToScreen(view, frame.origin, originScreen))
		{
			const ImU32 colour = GetHandleColour(Handle::Uniform, c_UniformColour);
			drawList->AddRectFilled(ImVec2(originScreen.x - c_UniformBoxHalfSize, originScreen.y - c_UniformBoxHalfSize),
				ImVec2(originScreen.x + c_UniformBoxHalfSize, originScreen.y + c_UniformBoxHalfSize), colour);
		}
	}
}
