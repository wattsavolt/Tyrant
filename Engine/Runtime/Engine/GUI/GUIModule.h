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
#if !TYR_FINAL
		ImGuiContext* GetImGuiContext() const { return m_ImGuiContext; }
#endif

		// Lets widgets fill the real window instead of the hardcoded fallback size - called
		// once by whoever owns the window, after creating it.
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
		// module, each in reverse registration order - so a module can't rely on Update()'s
		// deltaTime for something it needs in its own, earlier-running BeginFrame().
		Timer m_Timer;
		double m_LastFrameTimeMs = 0.0;

		nk_context* m_NuklearContext = nullptr;
		// Fixed, pre-allocated memory nk_init_fixed/nk_buffer_init_fixed use instead of
		// Nuklear's own malloc-based default allocator - reused every frame, never reallocated.
		Array<uint8> m_NuklearContextMemory;
		Array<uint8> m_NuklearCmdMemory;
		Array<uint8> m_NuklearVertexMemory;
		Array<uint8> m_NuklearIndexMemory;
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
