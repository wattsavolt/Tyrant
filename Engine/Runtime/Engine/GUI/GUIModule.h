#pragma once

#include "Core.h"
#include "EngineMacros.h"
#include "Module/IModule.h"
#include "Containers/Array.h"
#include "Time/Timer.h"
#include "Window/WindowHandle.h"
#include "Rendering/GUIDrawData.h"

struct nk_context;

#if !TYR_FINAL
struct ImGuiContext;
#endif

namespace tyr
{
	class RendererAPI;
	class WindowModule;
	class InputModule;

	// Owns the immediate-mode UI contexts and turns whatever gets drawn into them each frame
	// into a GUIDrawData submission via RendererAPI::SubmitGUIDrawData. Doesn't own any widget
	// content itself - Editor.cpp (via GetImGuiContext) and game/App code (via
	// GetNuklearContext) are expected to draw between this module's per-frame begin/end.
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
#if !TYR_FINAL
		ImGuiContext* GetImGuiContext() const { return m_ImGuiContext; }
#endif

		// Lets widgets fill the real window instead of the hardcoded fallback size - called once
		// by whoever owns the window (Editor::Initialize) after creating it.
		void SetPrimaryWindow(WindowModule* windowModule, WindowHandle window) { m_WindowModule = windowModule; m_Window = window; }

	private:
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
		// module, each pass in reverse registration order - so a module can't rely on the
		// deltaTime Update() gets for something it needs to do in its own BeginFrame(), which
		// runs in an earlier pass. This tracks time independently instead.
		Timer m_Timer;
		double m_LastFrameTimeMs = 0.0;

		nk_context* m_NuklearContext = nullptr;
		// Fixed, pre-allocated memory nk_init_fixed/nk_buffer_init_fixed use instead of
		// Nuklear's own malloc-based default allocator - reused every frame, never reallocated.
		Array<uint8> m_NuklearContextMemory;
		Array<uint8> m_NuklearCmdMemory;
		Array<uint8> m_NuklearVertexMemory;
		Array<uint8> m_NuklearIndexMemory;
		// Persistent, reused-every-frame scratch for the GUIDrawData SubmitNuklearDrawData builds
		// and hands to RendererAPI::SubmitGUIDrawData - same reasoning as the fixed Nuklear memory
		// above (its own comment), just for the converted output instead of Nuklear's own working
		// memory: a fresh GUIDrawData (and so fresh heap allocations for its three Arrays) every
		// single call would otherwise be built and torn down every frame for no reason, since
		// SubmitGUIDrawData only ever reads from it and never keeps a reference past that call.
		GUIDrawData m_NuklearDrawData;

#if !TYR_FINAL
		ImGuiContext* m_ImGuiContext = nullptr;
		// See m_NuklearDrawData's own comment - same reasoning, for SubmitImGuiDrawData.
		GUIDrawData m_ImGuiDrawData;
#endif
	};
}
