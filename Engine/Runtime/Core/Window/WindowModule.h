#pragma once

#include "Core.h"
#include "Module/IModule.h"
#include "Containers/LocalArray.h"
#include "Memory/LocalObjectPool.h"
#include "WindowConstants.h"
#include "WindowHandle.h"

namespace tyr
{
	class Window;
	struct WindowDesc;
	struct WindowModulePrivate;
	struct WindowInputState;

	class TYR_CORE_API WindowModule final : public IModule
	{
	public:
		WindowModule();

		~WindowModule();

		void Initialize() override;

		void Shutdown() override;

		// Pumps OS messages here rather than in Update() - BeginFrame/Update/EndFrame each run
		// as a full pass over every module before the next phase starts (see ModuleManager), so
		// this guarantees every module's Update() this frame sees messages the OS delivered this
		// same frame, regardless of module registration order. See GUIModule::BeginFrame's own
		// comment for the same reasoning applied to ImGui::NewFrame().
		void BeginFrame() override;

		void Update(float deltaTime) override;

		// Not calling it CreateWindow to avoid conflicts with winapi CreateWindow function
		WindowHandle MakeWindow(const WindowDesc& desc);

		void DestroyWindow(WindowHandle handle);

		const Window& GetWindow(WindowHandle handle) const;

		const uint GetWindowWidth(WindowHandle handle) const;

		const uint GetWindowHeight(WindowHandle handle) const;

		const bool IsWindowActive(WindowHandle handle) const;

		// True if the OS has reported a size change for this window since the last call - clears
		// the flag, so this is a one-shot "did it change" check, not a live query.
		bool ConsumeResizePending(WindowHandle handle);

		// Live level state (keys/mouse buttons/position) - always safe to read repeatedly,
		// no draining needed.
		const WindowInputState& GetInputState(WindowHandle handle) const;

		// Copies this window's typed characters since the last call into outBuffer (up to
		// bufferCapacity) and clears them - a one-shot drain, same pattern as
		// ConsumeResizePending above.
		void ConsumeTypedChars(WindowHandle handle, char* outBuffer, uint bufferCapacity, uint& outCount);

		// Returns the accumulated scroll wheel delta since the last call and clears it.
		float ConsumeScrollDelta(WindowHandle handle);

		// Returns this window's accumulated raw (relative, OS-level) mouse motion since the last
		// call and clears it - unlike the mouse position, this isn't affected by cursor clipping
		// or screen edges, so it's what a continuous look control should read.
		void ConsumeRawMouseDelta(WindowHandle handle, int& outDeltaX, int& outDeltaY);

		// Hides and confines the cursor to this window's client area for continuous look controls,
		// or restores normal cursor behaviour.
		void SetCursorCaptured(WindowHandle handle, bool captured);

	private:
		WindowModulePrivate* m_Private{};
	};
}