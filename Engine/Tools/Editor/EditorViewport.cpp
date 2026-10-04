#include "EditorViewport.h"
#include "Rendering/RendererAPI.h"
#include "Input/InputManager.h"
#include "World/Camera.h"
#include "imgui.h"

namespace tyr
{
	EditorViewport::EditorViewport(RendererAPI& rendererAPI)
		: m_RendererAPI(rendererAPI)
	{
	}

	void EditorViewport::Draw(RenderViewportHandle viewport, InputManager& inputManager, Camera& camera, float deltaTime)
	{
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

		// Only takes effect the first time this window is ever seen (i.e. no size saved in
		// imgui.ini yet) - after that, whatever size the user last dragged it to wins.
		const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowSize(ImVec2(mainViewport->WorkSize.x * 0.7f, mainViewport->WorkSize.y * 0.7f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(mainViewport->WorkPos.x + 20.0f, mainViewport->WorkPos.y + 20.0f), ImGuiCond_FirstUseEver);

		if (m_Maximized)
		{
			ImGui::SetNextWindowPos(mainViewport->WorkPos);
			ImGui::SetNextWindowSize(mainViewport->WorkSize);
			windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		}

		ImGui::Begin("Viewport", nullptr, windowFlags);

		// Ctrl+Shift+M toggles maximize - matches the "short cuts instead of clutter" direction
		// for editor panels rather than a permanent toolbar button eating into viewport space.
		ImGuiIO& io = ImGui::GetIO();
		if (ImGui::IsWindowFocused() && io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_M))
		{
			m_Maximized = !m_Maximized;
		}

		const ImVec2 available = ImGui::GetContentRegionAvail();
		const uint width = (uint)(available.x > 1.0f ? available.x : 1.0f);
		const uint height = (uint)(available.y > 1.0f ? available.y : 1.0f);

		if (width != m_Width || height != m_Height || !m_Texture)
		{
			m_Texture = m_RendererAPI.GetOrCreateRenderViewportTexture(viewport, "Viewport", width, height);
			m_Width = width;
			m_Height = height;
		}

		// +1: ImGui treats ImTextureID(0) as "not yet set" and a texture's own pool index can
		// legitimately be 0 - see GUIModule's PackTextureHandle for the same offset.
		const ImTextureID texID = (ImTextureID)(intptr_t)(m_Texture.h.index + 1);
		ImGui::Image(texID, ImVec2((float)m_Width, (float)m_Height));

		const bool isHovered = ImGui::IsWindowHovered();
		m_CameraController.Update(inputManager, camera, deltaTime, isHovered);

		ImGui::End();
	}
}
