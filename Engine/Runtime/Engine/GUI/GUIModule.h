#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "Module/IModule.h"
#include "Containers/Array.h"
#include "Time/Timer.h"
#include "Window/WindowHandle.h"
#include "Rendering/GUIDrawData.h"
#include "RenderBase/RenderHandles.h"

struct nk_context;
struct nk_font_atlas;
struct nk_font;
struct nk_user_font;

#if !TYR_FINAL
struct ImGuiContext;
#endif

namespace tyr
{
	class RendererAPI;
	class WindowModule;
	class InputModule;
	struct FontFile;

	// The game UI's kinds of text.
	enum class GameFont : uint8
	{
		Body,
		Title,
		// Notes and papers the player reads.
		Document,
		Count
	};

	// Owns the immediate-mode UI contexts and turns whatever gets drawn into them each frame
	// into a GUIDrawData submission. Doesn't own any widget content itself - other code is
	// expected to draw between this module's per-frame begin/end.
	class TYR_ENGINE_API GUIModule final : public IModule
	{
	public:
		GUIModule();
		~GUIModule();

		void Initialize() override;
		void Shutdown() override;
		void BeginFrame() override;
		void Update(float deltaTime) override;

		nk_context* GetNuklearContext() const { return m_NuklearContext; }
		const nk_user_font* GetGameFont(GameFont font) const;
		// Bakes the game UI's fonts again from these files, one for each GameFont. A null one is
		// Nuklear's own font. Call it before anything's drawn in the frame, as the old fonts go.
		void SetGameFonts(const FontFile* const* files);

		// Where the game's view is in the window, such as the editor's viewport while it plays. The
		// game UI is laid out in a space this size and moved there. The whole window until it's set.
		void SetGameViewRect(float x, float y, float width, float height);
		void ClearGameViewRect() { m_HasGameViewRect = false; }
		Vector2 GetGameViewSize() const;
#if !TYR_FINAL
		ImGuiContext* GetImGuiContext() const { return m_ImGuiContext; }
#endif

		// Lets widgets fill the real window instead of the hardcoded fallback size - called
		// once by whoever owns the window, after creating it.
		void SetPrimaryWindow(WindowModule* windowModule, WindowHandle window) { m_WindowModule = windowModule; m_Window = window; }

	private:
		// Bakes the game UI's fonts into a texture, with Nuklear's own font for any file that's null.
		void CreateGameFonts(const FontFile* const* files);
		Vector2 GetWindowSize() const;
		void SubmitNuklearDrawData();
#if !TYR_FINAL
		void ProcessImGuiTextures();
		void SubmitImGuiDrawData();
		void ApplyInputToImGui();
#endif

		RendererAPI* m_RendererAPI = nullptr;
		WindowModule* m_WindowModule = nullptr;
		WindowHandle m_Window{};
		InputModule* m_InputModule = nullptr;

		// ModuleManager runs BeginFrame/Update/EndFrame as three separate passes over every
		// module, so a module can't rely on Update()'s deltaTime for something it needs in its
		// own, earlier-running BeginFrame().
		Timer m_Timer;
		double m_LastFrameTimeMs = 0.0;

		nk_context* m_NuklearContext = nullptr;
		// Fixed, pre-allocated memory nk_init_fixed/nk_buffer_init_fixed use instead of
		// Nuklear's own malloc-based default allocator - reused every frame, never reallocated.
		Array<uint8> m_NuklearContextMemory;
		Array<uint8> m_NuklearCmdMemory;
		Array<uint8> m_NuklearVertexMemory;
		Array<uint8> m_NuklearIndexMemory;
		// Fixed memory the fonts are baked into and kept in.
		Array<uint8> m_NuklearFontMemory;
		nk_font_atlas* m_FontAtlas = nullptr;
		nk_font* m_Fonts[static_cast<uint>(GameFont::Count)] = {};
		TextureHandle m_FontTexture;
		// Where in the font texture a solid white texel is, for shapes that aren't text.
		Vector2 m_WhiteTexelUV;
		bool m_HasGameViewRect = false;
		float m_GameViewX = 0.0f;
		float m_GameViewY = 0.0f;
		float m_GameViewWidth = 0.0f;
		float m_GameViewHeight = 0.0f;
		// Persistent, reused-every-frame scratch for the converted draw data, to avoid a fresh
		// heap allocation every frame for something only ever read once and never kept.
		GUIDrawData m_NuklearDrawData;

#if !TYR_FINAL
		ImGuiContext* m_ImGuiContext = nullptr;
		// Persistent, reused-every-frame scratch for ImGui's converted draw data, to avoid a
		// fresh heap allocation every frame for something read once and never kept.
		GUIDrawData m_ImGuiDrawData;
#endif
	};
}
