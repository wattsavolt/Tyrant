#include "EditorViewport.h"
#include "AssetBrowserPanel.h"
#include "Rendering/RendererAPI.h"
#include "RenderAPI/GraphicsUtility.h"
#include "Input/InputManager.h"
#include "World/Camera.h"
#include "World/World.h"
#include "Math/Matrix4.h"
#include "imgui.h"

namespace tyr
{
	EditorViewport::EditorViewport(RendererAPI& rendererAPI)
		: m_RendererAPI(rendererAPI)
	{
	}

	bool EditorViewport::Draw(const PanelRect& rect, const World& world, InputManager& inputManager, bool editing, float deltaTime, MeshDrop& drop)
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

		bool dropped = false;
		if (editing)
		{
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(AssetBrowserPanel::c_MeshPayload))
				{
					const ImVec2 mouse = ImGui::GetMousePos();
					drop.mesh = *static_cast<const AssetID*>(payload->Data);
					CalculateMouseRay(world, mouse.x - imageMin.x, mouse.y - imageMin.y, width, height, drop);
					dropped = true;
				}
				ImGui::EndDragDropTarget();
			}

			m_CameraController.Update(inputManager, *world.camera, deltaTime, ImGui::IsWindowHovered());
		}

		ImGui::End();
		return dropped;
	}

	void EditorViewport::CalculateMouseRay(const World& world, float mouseX, float mouseY, uint width, uint height, MeshDrop& drop)
	{
		const Camera& camera = *world.camera;
		const ViewArea& viewArea = world.viewArea;

		// Matches the view and projection the renderer builds for this world.
		const float aspect = GraphicsUtility::CalculateAspectRatio(viewArea, width, height);
		const Matrix4 view = Matrix4::CreateView(camera.GetPosition(), camera.GetForward(), camera.GetUp());
		const Matrix4 projection = Matrix4::CreatePerspective(camera.GetFOV(), aspect, camera.GetFarZ(), camera.GetNearZ());
		const Matrix4 inverseViewProj = (view * projection).Inverse();

		// Normalized device coordinates within the view area, with +Y up.
		const float u = (mouseX / static_cast<float>(width) - viewArea.x) / viewArea.width;
		const float v = (mouseY / static_cast<float>(height) - viewArea.y) / viewArea.height;
		const Vector4 clip(u * 2.0f - 1.0f, 1.0f - v * 2.0f, 0.5f, 1.0f);
		const Vector4 worldPoint = inverseViewProj.Multiply(clip);

		drop.rayOrigin = camera.GetPosition();
		drop.rayDirection = Vector3::Normalize(Vector3(worldPoint.x, worldPoint.y, worldPoint.z) / worldPoint.w - drop.rayOrigin);
	}
}
