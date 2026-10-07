#include "EditorViewport.h"
#include "AssetBrowserPanel.h"
#include "Rendering/RendererAPI.h"
#include "RenderAPI/GraphicsUtility.h"
#include "Input/InputManager.h"
#include "World/Camera.h"
#include "World/World.h"
#include "imgui.h"

namespace tyr
{
	namespace
	{
		constexpr float c_GizmoButtonIconSize = 16.0f;
		// Space between the buttons and the image's edges, and between the two button groups.
		constexpr float c_GizmoButtonMargin = 8.0f;
		constexpr float c_GizmoButtonGroupGap = 8.0f;
	}

	EditorViewport::EditorViewport(RendererAPI& rendererAPI, const EditorIcons& icons)
		: m_RendererAPI(rendererAPI)
		, m_Icons(icons)
	{
	}

	void EditorViewport::Draw(const PanelRect& rect, World& world, InputManager& inputManager, float deltaTime, const EditState& state, Events& events)
	{
		ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y));
		ImGui::SetNextWindowSize(ImVec2(rect.width, rect.height));
		constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings;

		ImGui::Begin("##Viewport", nullptr, windowFlags);

		const ImVec2 available = ImGui::GetContentRegionAvail();
		const uint width = (uint)(available.x > 1.0f ? available.x : 1.0f);
		const uint height = (uint)(available.y > 1.0f ? available.y : 1.0f);

		// Fetched every frame since each buffered frame has its own texture.
		const TextureHandle texture = m_RendererAPI.GetOrCreateRenderViewportTexture(world.renderViewportHandle, "Viewport", width, height);

		// +1 because ImGui treats a texture ID of 0 as unset.
		const ImTextureID texID = (ImTextureID)(intptr_t)(texture.h.index + 1);
		const ImVec2 imageMin = ImGui::GetCursorScreenPos();
		ImGui::Image(texID, ImVec2((float)width, (float)height));
		const bool imageHovered = ImGui::IsItemHovered();

		if (state.editing)
		{
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetBrowserPanel::c_MeshPayload))
				{
					const ImVec2 mouse = ImGui::GetMousePos();
					events.meshDropped = true;
					events.drop.mesh = *static_cast<const AssetID*>(payload->Data);
					events.drop.ray = CalculateMouseRay(world, mouse.x - imageMin.x, mouse.y - imageMin.y, width, height);
				}
				ImGui::EndDragDropTarget();
			}

			const bool flying = ImGui::IsMouseDown(ImGuiMouseButton_Right);
			if (imageHovered && !flying)
			{
				HandleGizmoShortcuts();
			}

			// The camera takes the mouse while flying.
			const bool overGizmo = DrawGizmo(world, imageMin.x, imageMin.y, width, height, imageHovered && !flying, state, events);
			const bool overButtons = DrawGizmoButtons(imageMin.x + (float)width, imageMin.y);

			// Like Unreal, a click picks whatever is under the mouse, or clears the selection.
			if (imageHovered && !overGizmo && !overButtons && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				const ImVec2 mouse = ImGui::GetMousePos();
				events.clicked = true;
				events.clickRay = CalculateMouseRay(world, mouse.x - imageMin.x, mouse.y - imageMin.y, width, height);
			}

			m_CameraController.Update(inputManager, *world.camera, deltaTime, ImGui::IsWindowHovered());
		}

		ImGui::End();
	}

	void EditorViewport::CalculateViewProjection(const World& world, uint width, uint height, Matrix4& view, Matrix4& projection)
	{
		const Camera& camera = *world.camera;
		const float aspect = GraphicsUtility::CalculateAspectRatio(world.viewArea, width, height);
		view = Matrix4::CreateView(camera.GetPosition(), camera.GetForward(), camera.GetUp());
		// Reverse-Z, the same as the renderer.
		projection = Matrix4::CreatePerspective(camera.GetFOV(), aspect, camera.GetFarZ(), camera.GetNearZ());
	}

	ViewportRay EditorViewport::CalculateMouseRay(const World& world, float mouseX, float mouseY, uint width, uint height)
	{
		Matrix4 view;
		Matrix4 projection;
		CalculateViewProjection(world, width, height, view, projection);
		const Matrix4 inverseViewProj = (view * projection).Inverse();

		// Normalized device coordinates within the view area, with +Y up.
		const ViewArea& viewArea = world.viewArea;
		const float u = (mouseX / static_cast<float>(width) - viewArea.x) / viewArea.width;
		const float v = (mouseY / static_cast<float>(height) - viewArea.y) / viewArea.height;
		const Vector4 clip(u * 2.0f - 1.0f, 1.0f - v * 2.0f, 0.5f, 1.0f);
		const Vector4 worldPoint = inverseViewProj.Multiply(clip);

		ViewportRay ray;
		ray.origin = world.camera->GetPosition();
		ray.direction = Vector3::Normalize(Vector3(worldPoint.x, worldPoint.y, worldPoint.z) / worldPoint.w - ray.origin);
		return ray;
	}

	bool EditorViewport::DrawGizmo(World& world, float imageX, float imageY, uint width, uint height, bool canInteract, const EditState& state, Events& events)
	{
		if (state.selectedActor == c_InvalidEntity || !world.entities.HasComponent<ComponentTransform>(state.selectedActor))
		{
			return false;
		}

		Matrix4 view;
		Matrix4 projection;
		CalculateViewProjection(world, width, height, view, projection);

		const Camera& camera = *world.camera;
		EditorGizmo::View gizmoView;
		gizmoView.viewProj = view * projection;
		gizmoView.inverseViewProj = gizmoView.viewProj.Inverse();
		gizmoView.cameraPosition = camera.GetPosition();
		gizmoView.cameraRight = camera.GetUp().Cross(camera.GetForward());
		gizmoView.x = imageX;
		gizmoView.y = imageY;
		gizmoView.width = (float)width;
		gizmoView.height = (float)height;

		// Grid snapping applies to moving, the same as placing.
		const float translationSnap = state.snapToGrid ? state.gridCellSize : 0.0f;

		Transform transform = world.entities.GetComponent<ComponentTransform>(state.selectedActor).world;
		if (m_Gizmo.Manipulate(gizmoView, m_GizmoOperation, m_GizmoWorldSpace, canInteract, translationSnap, transform))
		{
			events.transformChanged = true;
			events.transform = transform;
		}
		return m_Gizmo.IsActive();
	}

	bool EditorViewport::DrawGizmoButtons(float imageRight, float imageTop)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float buttonWidth = c_GizmoButtonIconSize + style.FramePadding.x * 2.0f;
		const float groupWidth = buttonWidth * 4.0f + style.ItemSpacing.x * 2.0f + c_GizmoButtonGroupGap;
		ImGui::SetCursorScreenPos(ImVec2(imageRight - groupWidth - c_GizmoButtonMargin, imageTop + c_GizmoButtonMargin));

		bool hovered = false;
		if (DrawGizmoButton(EditorIcons::Translate, "Translate (W)", m_GizmoOperation == EditorGizmo::Operation::Translate))
		{
			m_GizmoOperation = EditorGizmo::Operation::Translate;
		}
		hovered |= ImGui::IsItemHovered();

		ImGui::SameLine();
		if (DrawGizmoButton(EditorIcons::Rotate, "Rotate (E)", m_GizmoOperation == EditorGizmo::Operation::Rotate))
		{
			m_GizmoOperation = EditorGizmo::Operation::Rotate;
		}
		hovered |= ImGui::IsItemHovered();

		ImGui::SameLine();
		if (DrawGizmoButton(EditorIcons::Scale, "Scale (R)", m_GizmoOperation == EditorGizmo::Operation::Scale))
		{
			m_GizmoOperation = EditorGizmo::Operation::Scale;
		}
		hovered |= ImGui::IsItemHovered();

		ImGui::SameLine(0.0f, c_GizmoButtonGroupGap);
		const EditorIcons::Icon spaceIcon = m_GizmoWorldSpace ? EditorIcons::World : EditorIcons::Local;
		if (DrawGizmoButton(spaceIcon, m_GizmoWorldSpace ? "World space, click for local" : "Local space, click for world", false))
		{
			m_GizmoWorldSpace = !m_GizmoWorldSpace;
		}
		hovered |= ImGui::IsItemHovered();

		return hovered;
	}

	bool EditorViewport::DrawGizmoButton(EditorIcons::Icon icon, const char* tooltip, bool active)
	{
		if (active)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		}

		ImGui::PushID(static_cast<int>(icon));
		bool clicked;
		if (const ImTextureID textureID = m_Icons.GetImTextureID(icon))
		{
			clicked = ImGui::ImageButton("##Gizmo", textureID, ImVec2(c_GizmoButtonIconSize, c_GizmoButtonIconSize));
		}
		else
		{
			// Same size as the icon button, shown until the icon has loaded.
			const ImGuiStyle& style = ImGui::GetStyle();
			clicked = ImGui::Button("##Gizmo", ImVec2(c_GizmoButtonIconSize + style.FramePadding.x * 2.0f, c_GizmoButtonIconSize + style.FramePadding.y * 2.0f));
		}
		ImGui::PopID();
		ImGui::SetItemTooltip("%s", tooltip);

		if (active)
		{
			ImGui::PopStyleColor();
		}
		return clicked;
	}

	void EditorViewport::HandleGizmoShortcuts()
	{
		if (ImGui::GetIO().WantTextInput)
		{
			return;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_W, false))
		{
			m_GizmoOperation = EditorGizmo::Operation::Translate;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_E, false))
		{
			m_GizmoOperation = EditorGizmo::Operation::Rotate;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_R, false))
		{
			m_GizmoOperation = EditorGizmo::Operation::Scale;
		}
	}
}
