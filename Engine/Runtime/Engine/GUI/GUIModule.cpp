#include "GUIModule.h"
#include "RendererModule.h"
#include "Rendering/RendererAPI.h"
#include "Rendering/GUIDrawData.h"
#include "RenderTransfer/RenderTransferTypes.h"
#include "RenderTransfer/UploadRequest.h"
#include "RenderResource/TextureDesc.h"
#include "Rendering/RenderConstants.h"
#include "Memory/MemoryUtil.h"
#include "Module/ModuleManager.h"
#include "Window/WindowModule.h"
#include "Input/InputModule.h"
#include "Input/InputManager.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include "nuklear.h"

#if !TYR_FINAL
#include "imgui.h"
// KeyCode is documented (InputManager.h) as a raw Win32 VK_* code for now - only one platform
// exists, so the mapping table below just uses VK_ constants directly rather than inventing a
// cross-platform key enum this codebase doesn't need yet.
#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#include <windows.h>
#endif
#endif

namespace tyr
{
	namespace
	{
		constexpr uint c_NuklearContextMemorySize = 256 * 1024;
		constexpr uint c_NuklearCmdMemorySize = 64 * 1024;
		constexpr uint c_NuklearVertexMemorySize = 512 * 1024;
		constexpr uint c_NuklearIndexMemorySize = 128 * 1024;

		// Matches GUIVertex (Rendering/GUIDrawData.h) field for field, so nk_convert can write
		// straight into that layout instead of Nuklear's own default vertex format.
		const nk_draw_vertex_layout_element g_NuklearVertexLayout[] =
		{
			{ NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(GUIVertex, pos) },
			{ NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(GUIVertex, uv) },
			{ NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, offsetof(GUIVertex, colour) },
			{ NK_VERTEX_LAYOUT_END }
		};

#if !TYR_FINAL
		// ImTextureData::BackendUserData is our one slot for backend bookkeeping - stash the
		// full TextureHandle (index + generation) there. +1 on the index avoids a first-ever
		// handle (index 0) packing to literal 0, indistinguishable from "no handle stored yet".
		void* PackTextureHandle(TextureHandle handle)
		{
			const uint64 packed = (uint64)(handle.h.index + 1) | ((uint64)handle.h.generation << 32);
			return (void*)(uintptr_t)packed;
		}

		TextureHandle UnpackTextureHandle(void* packed)
		{
			const uint64 value = (uint64)(uintptr_t)packed;
			TextureHandle handle;
			handle.h.index = (uint)(value & 0xFFFFFFFFu) - 1;
			handle.h.generation = (uint)(value >> 32);
			return handle;
		}

		// Creates a GPU texture for an ImGui texture and queues its pixels for upload.
		TextureHandle CreateImGuiTexture(RendererAPI& rendererAPI, ImTextureData& tex)
		{
			TextureDesc desc;
			desc.debugName = "ImGui Texture";
			desc.info.width = (uint)tex.Width;
			desc.info.height = (uint)tex.Height;
			desc.info.depth = 1;
			desc.info.arrayLayerCount = 1;
			desc.info.mipCount = 1;
			desc.info.format = PixelFormat::PF_R8G8B8A8_UNORM;
			desc.info.type = ImageType::Image2D;
			desc.sampleCount = SampleCount::OneBit;
			desc.usage = static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT);
			desc.layout = ImageLayout::IMAGE_LAYOUT_GENERAL;

			const TextureHandle handle = rendererAPI.CreateTexture(desc);

			// Uploaded texture rows have to be padded to the row pitch alignment.
			const uint rowSize = (uint)tex.GetPitch();
			const uint alignedRowSize = MemoryUtil::Align(rowSize, RenderConstants::c_RowPitchAlignment);
			UploadBufferAllocation alloc;
			if (rendererAPI.RequestResourceUploadAllocation((size_t)alignedRowSize * tex.Height, alloc))
			{
				const uint8* src = static_cast<const uint8*>(tex.GetPixels());
				uint8* dst = static_cast<uint8*>(alloc.cpuPtr);
				for (int row = 0; row < tex.Height; ++row)
				{
					memcpy(dst + (size_t)row * alignedRowSize, src + (size_t)row * rowSize, rowSize);
				}
				rendererAPI.FlushBufferUploadAllocation(alloc);

				TextureUploadRequest request;
				request.srcBuffer = alloc.buffer;
				request.srcOffset = alloc.offset;
				request.dstTexture = handle;
				request.highestMip = 0;
				request.mipCount = 1;
				request.type = TextureUploadRequestType::GUI;
				request.resourceId = alloc.resourceId;
				rendererAPI.AddTextureUploadRequest(request);
			}

			return handle;
		}

		// (Win32 virtual-key code, ImGuiKey) pairs - not exhaustive, just enough for menus,
		// text fields and dialog navigation.
		struct KeyMapping
		{
			KeyCode vk;
			ImGuiKey imguiKey;
		};
		const KeyMapping g_KeyMappings[] =
		{
			{ VK_TAB, ImGuiKey_Tab },
			{ VK_LEFT, ImGuiKey_LeftArrow },
			{ VK_RIGHT, ImGuiKey_RightArrow },
			{ VK_UP, ImGuiKey_UpArrow },
			{ VK_DOWN, ImGuiKey_DownArrow },
			{ VK_PRIOR, ImGuiKey_PageUp },
			{ VK_NEXT, ImGuiKey_PageDown },
			{ VK_HOME, ImGuiKey_Home },
			{ VK_END, ImGuiKey_End },
			{ VK_INSERT, ImGuiKey_Insert },
			{ VK_DELETE, ImGuiKey_Delete },
			{ VK_BACK, ImGuiKey_Backspace },
			{ VK_SPACE, ImGuiKey_Space },
			{ VK_RETURN, ImGuiKey_Enter },
			{ VK_ESCAPE, ImGuiKey_Escape },
			{ VK_CONTROL, ImGuiKey_LeftCtrl },
			{ VK_SHIFT, ImGuiKey_LeftShift },
			{ VK_MENU, ImGuiKey_LeftAlt },
		};

		ImGuiKey VkDigitOrLetterToImGuiKey(KeyCode vk)
		{
			if (vk >= '0' && vk <= '9')
			{
				return (ImGuiKey)(ImGuiKey_0 + (vk - '0'));
			}
			if (vk >= 'A' && vk <= 'Z')
			{
				return (ImGuiKey)(ImGuiKey_A + (vk - 'A'));
			}
			if (vk >= VK_F1 && vk <= VK_F12)
			{
				return (ImGuiKey)(ImGuiKey_F1 + (vk - VK_F1));
			}
			return ImGuiKey_None;
		}
#endif
	}

	GUIModule::GUIModule()
	{
	}

	GUIModule::~GUIModule()
	{
	}

	void GUIModule::Initialize()
	{
		RendererModule* rendererModule;
		TYR_GET_MODULE(RendererModule, rendererModule);
		m_RendererAPI = rendererModule->GetRendererAPI();

		TYR_GET_MODULE(InputModule, m_InputModule);

		m_NuklearContextMemory.Resize(c_NuklearContextMemorySize);
		m_NuklearCmdMemory.Resize(c_NuklearCmdMemorySize);
		m_NuklearVertexMemory.Resize(c_NuklearVertexMemorySize);
		m_NuklearIndexMemory.Resize(c_NuklearIndexMemorySize);

		m_NuklearContext = new nk_context();
		// No font set up yet - baking one needs stb_truetype/stb_rect_pack, which aren't
		// vendored yet. Text won't render until that's done; layout and non-text widgets
		// still work without it.
		nk_init_fixed(m_NuklearContext, m_NuklearContextMemory.Data(), m_NuklearContextMemory.Size(), nullptr);

#if !TYR_FINAL
		m_ImGuiContext = ImGui::CreateContext();
		// Tells ImGui this backend handles texture create/update/destroy requests itself
		// instead of needing a pre-built font atlas up front.
		ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
#endif
	}

	void GUIModule::Shutdown()
	{
#if !TYR_FINAL
		ImGui::SetCurrentContext(m_ImGuiContext);
		for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
		{
			if (tex->Status != ImTextureStatus_Destroyed && tex->BackendUserData != nullptr)
			{
				m_RendererAPI->DeleteTexture(UnpackTextureHandle(tex->BackendUserData));
			}
		}

		ImGui::DestroyContext(m_ImGuiContext);
		m_ImGuiContext = nullptr;
#endif

		nk_free(m_NuklearContext);
		delete m_NuklearContext;
		m_NuklearContext = nullptr;
	}

	void GUIModule::BeginFrame()
	{
		// Starts this frame's UI. BeginFrame/Update/EndFrame each run as a full pass over every
		// module before the next phase starts, so this is guaranteed to run before any module
		// draws widgets, regardless of registration order.
		const double currentTimeMs = m_Timer.GetMillisecondsPrecise();
		const float deltaTime = (float)((currentTimeMs - m_LastFrameTimeMs) / 1000.0);
		m_LastFrameTimeMs = currentTimeMs;

		nk_input_begin(m_NuklearContext);
		nk_input_end(m_NuklearContext);

#if !TYR_FINAL
		ImGui::SetCurrentContext(m_ImGuiContext);
		ImGuiIO& io = ImGui::GetIO();
		io.DeltaTime = deltaTime > 0.0f ? deltaTime : (1.0f / 60.0f);
		io.DisplaySize = m_WindowModule
			? ImVec2((float)m_WindowModule->GetWindowWidth(m_Window), (float)m_WindowModule->GetWindowHeight(m_Window))
			: ImVec2(1920.0f, 1080.0f);
		ApplyInputToImGui();
		ImGui::NewFrame();
#endif
	}

	void GUIModule::Update(float deltaTime)
	{
		// Must run after every module that draws UI widgets, and before RendererModule's own
		// Update - module registration order controls this.

		// The update loop can tick more than once before a render frame slot is actually
		// consumed - reset first so a stale submission never sits alongside a new one.
		m_RendererAPI->ResetGUIDrawData();

		// Nuklear is for the in-game HUD, not used anywhere yet - not calling this keeps it
		// from converting/submitting/drawing anything, while keeping the implementation ready
		// for when the HUD starts using it.
		// SubmitNuklearDrawData();

#if !TYR_FINAL
		ImGui::SetCurrentContext(m_ImGuiContext);
		ImGui::Render();
		ProcessImGuiTextures();
		SubmitImGuiDrawData();
#endif
	}

	void GUIModule::SubmitNuklearDrawData()
	{
		nk_buffer cmds, vertices, indices;
		nk_buffer_init_fixed(&cmds, m_NuklearCmdMemory.Data(), m_NuklearCmdMemory.Size());
		nk_buffer_init_fixed(&vertices, m_NuklearVertexMemory.Data(), m_NuklearVertexMemory.Size());
		nk_buffer_init_fixed(&indices, m_NuklearIndexMemory.Data(), m_NuklearIndexMemory.Size());

		nk_convert_config config{};
		config.global_alpha = 1.0f;
		config.line_AA = NK_ANTI_ALIASING_ON;
		config.shape_AA = NK_ANTI_ALIASING_ON;
		config.circle_segment_count = 22;
		config.arc_segment_count = 22;
		config.curve_segment_count = 22;
		config.vertex_layout = g_NuklearVertexLayout;
		config.vertex_size = sizeof(GUIVertex);
		config.vertex_alignment = alignof(GUIVertex);
		// No white-pixel texture set up yet - solid shape fills won't sample correctly until
		// a real font atlas exists.

		nk_convert(m_NuklearContext, &cmds, &vertices, &indices, &config);

		const uint vertexCount = (uint)(nk_buffer_total(&vertices) / sizeof(GUIVertex));
		const uint indexCount = (uint)(nk_buffer_total(&indices) / sizeof(nk_draw_index));

		if (vertexCount > 0 && indexCount > 0)
		{
			// Reused every frame rather than a fresh local GUIDrawData, to avoid a heap
			// allocation per frame.
			m_NuklearDrawData.Clear();
			// Must be the real window size - the shader divides by this to build its
			// scale/translate, so a zero or placeholder size would corrupt every vertex's
			// clip-space position.
			m_NuklearDrawData.displaySize = m_WindowModule
				? Vector2((float)m_WindowModule->GetWindowWidth(m_Window), (float)m_WindowModule->GetWindowHeight(m_Window))
				: Vector2(1920.0f, 1080.0f);

			m_NuklearDrawData.vertices.Resize(vertexCount);
			memcpy(m_NuklearDrawData.vertices.Data(), nk_buffer_memory_const(&vertices), vertexCount * sizeof(GUIVertex));
			m_NuklearDrawData.indices.Resize(indexCount);
			memcpy(m_NuklearDrawData.indices.Data(), nk_buffer_memory_const(&indices), indexCount * sizeof(nk_draw_index));

			const nk_draw_command* cmd = nullptr;
			uint indexOffset = 0;
			nk_draw_foreach(cmd, m_NuklearContext, &cmds)
			{
				if (cmd->elem_count == 0)
				{
					continue;
				}

				// Nuklear's clip rects can reach far past the screen, which a scissor can't.
				const Vector2& displaySize = m_NuklearDrawData.displaySize;
				const float clipMinX = std::max(cmd->clip_rect.x, 0.0f);
				const float clipMinY = std::max(cmd->clip_rect.y, 0.0f);
				const float clipMaxX = std::min(cmd->clip_rect.x + cmd->clip_rect.w, displaySize.x);
				const float clipMaxY = std::min(cmd->clip_rect.y + cmd->clip_rect.h, displaySize.y);
				if (clipMaxX <= clipMinX || clipMaxY <= clipMinY)
				{
					indexOffset += cmd->elem_count;
					continue;
				}

				GUIDrawCommand& drawCommand = m_NuklearDrawData.commands.ExpandOne();
				drawCommand.clipMinX = (uint)clipMinX;
				drawCommand.clipMinY = (uint)clipMinY;
				drawCommand.clipMaxX = (uint)clipMaxX;
				drawCommand.clipMaxY = (uint)clipMaxY;
				drawCommand.textureIndex = (uint)cmd->texture.id;
				drawCommand.indexOffset = indexOffset;
				drawCommand.indexCount = cmd->elem_count;

				indexOffset += cmd->elem_count;
			}

			m_RendererAPI->SubmitGUIDrawData(m_NuklearDrawData);
		}

		nk_clear(m_NuklearContext);
		nk_buffer_clear(&cmds);
		nk_buffer_clear(&vertices);
		nk_buffer_clear(&indices);
	}

#if !TYR_FINAL
	void GUIModule::ApplyInputToImGui()
	{
		if (!m_InputModule)
		{
			return;
		}

		const InputManager* input = m_InputModule->GetInputManager();
		ImGuiIO& io = ImGui::GetIO();

		const Vector2& mousePos = input->GetMousePosition();
		io.AddMousePosEvent(mousePos.x, mousePos.y);
		io.AddMouseButtonEvent(ImGuiMouseButton_Left, input->IsMouseButtonDown(MouseButton::Left));
		io.AddMouseButtonEvent(ImGuiMouseButton_Right, input->IsMouseButtonDown(MouseButton::Right));
		io.AddMouseButtonEvent(ImGuiMouseButton_Middle, input->IsMouseButtonDown(MouseButton::Middle));
		io.AddMouseWheelEvent(0.0f, input->GetScrollDelta());

		for (const KeyMapping& mapping : g_KeyMappings)
		{
			io.AddKeyEvent(mapping.imguiKey, input->IsKeyDown(mapping.vk));
		}
		// 'A'-'Z'/'0'-'9'/F1-F12 follow a simple contiguous range each, so map them here
		// instead of listing all of them individually. Loop counter must be wider than
		// KeyCode (uint8), or `vk < c_MaxKeyCodes` (256) would never terminate once vk wraps.
		for (uint vk = 0; vk < c_MaxKeyCodes; ++vk)
		{
			const ImGuiKey key = VkDigitOrLetterToImGuiKey((KeyCode)vk);
			if (key != ImGuiKey_None)
			{
				io.AddKeyEvent(key, input->IsKeyDown((KeyCode)vk));
			}
		}

		uint charCount = input->GetTypedCharCount();
		const char* chars = input->GetTypedChars();
		for (uint i = 0; i < charCount; ++i)
		{
			io.AddInputCharacter((unsigned char)chars[i]);
		}
	}

	void GUIModule::ProcessImGuiTextures()
	{
		for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
		{
			if (tex->Status == ImTextureStatus_WantCreate)
			{
				const TextureHandle handle = CreateImGuiTexture(*m_RendererAPI, *tex);

				// +1 because ImGui treats a texture ID of 0 as unset.
				tex->SetTexID((ImTextureID)(intptr_t)(handle.h.index + 1));
				tex->BackendUserData = PackTextureHandle(handle);
				tex->SetStatus(ImTextureStatus_OK);
			}
			else if (tex->Status == ImTextureStatus_WantDestroy)
			{
				m_RendererAPI->DeleteTexture(UnpackTextureHandle(tex->BackendUserData));
				tex->BackendUserData = nullptr;
				tex->SetStatus(ImTextureStatus_Destroyed);
			}
			else if (tex->Status == ImTextureStatus_WantUpdates)
			{
				// Recreates rather than updating the existing texture in place - UploadToTextures
				// assumes every texture only ever goes through it once, right after creation, so a
				// genuine in-place re-upload isn't safe there yet.
				const TextureHandle oldHandle = UnpackTextureHandle(tex->BackendUserData);
				const TextureHandle handle = CreateImGuiTexture(*m_RendererAPI, *tex);
				m_RendererAPI->DeleteTexture(oldHandle);
				tex->SetTexID((ImTextureID)(intptr_t)(handle.h.index + 1));
				tex->BackendUserData = PackTextureHandle(handle);
				tex->SetStatus(ImTextureStatus_OK);
			}
		}
	}

	void GUIModule::SubmitImGuiDrawData()
	{
		const ImDrawData* imDrawData = ImGui::GetDrawData();
		// CmdListsCount is an obsolete field ImGui only ever resets to 0 and never updates -
		// CmdLists.Size is the real count.
		if (!imDrawData || imDrawData->CmdLists.Size == 0)
		{
			return;
		}

		m_ImGuiDrawData.Clear();
		m_ImGuiDrawData.displaySize = Vector2(imDrawData->DisplaySize.x, imDrawData->DisplaySize.y);

		uint totalVertices = 0;
		uint totalIndices = 0;
		for (int i = 0; i < imDrawData->CmdLists.Size; ++i)
		{
			totalVertices += (uint)imDrawData->CmdLists[i]->VtxBuffer.Size;
			totalIndices += (uint)imDrawData->CmdLists[i]->IdxBuffer.Size;
		}

		if (totalVertices == 0 || totalIndices == 0)
		{
			return;
		}

		m_ImGuiDrawData.vertices.Reserve(totalVertices);
		m_ImGuiDrawData.indices.Reserve(totalIndices);

		uint vertexBase = 0;
		uint indexBase = 0;
		for (int i = 0; i < imDrawData->CmdLists.Size; ++i)
		{
			const ImDrawList* cmdList = imDrawData->CmdLists[i];

			for (int v = 0; v < cmdList->VtxBuffer.Size; ++v)
			{
				const ImDrawVert& src = cmdList->VtxBuffer[v];
				GUIVertex& dst = m_ImGuiDrawData.vertices.ExpandOne();
				dst.pos = Vector2(src.pos.x, src.pos.y);
				dst.uv = Vector2(src.uv.x, src.uv.y);
				dst.colour = src.col;
			}

			// Rebased by this submission's running vertex count, not just concatenated - each
			// ImDrawList's indices are local to its own VtxBuffer (0-based), but GUIPass reads
			// one flattened vertex array per submission with no per-command vertex offset.
			for (int idx = 0; idx < cmdList->IdxBuffer.Size; ++idx)
			{
				m_ImGuiDrawData.indices.Add((uint16)(cmdList->IdxBuffer[idx] + vertexBase));
			}

			for (int c = 0; c < cmdList->CmdBuffer.Size; ++c)
			{
				const ImDrawCmd& src = cmdList->CmdBuffer[c];
				if (src.ElemCount == 0)
				{
					continue;
				}

				// Clip rects can reach past the screen, such as a popup's at its edge, and the
				// scissor they become can't, so they're clamped like ImGui's own backends do.
				const float clipMinX = std::max(src.ClipRect.x - imDrawData->DisplayPos.x, 0.0f);
				const float clipMinY = std::max(src.ClipRect.y - imDrawData->DisplayPos.y, 0.0f);
				const float clipMaxX = std::min(src.ClipRect.z - imDrawData->DisplayPos.x, imDrawData->DisplaySize.x);
				const float clipMaxY = std::min(src.ClipRect.w - imDrawData->DisplayPos.y, imDrawData->DisplaySize.y);
				if (clipMaxX <= clipMinX || clipMaxY <= clipMinY)
				{
					continue;
				}

				GUIDrawCommand& drawCommand = m_ImGuiDrawData.commands.ExpandOne();
				drawCommand.clipMinX = (uint)clipMinX;
				drawCommand.clipMinY = (uint)clipMinY;
				drawCommand.clipMaxX = (uint)clipMaxX;
				drawCommand.clipMaxY = (uint)clipMaxY;
				// ImGui 1.92+ dropped ImDrawCmd::TextureId in favour of this getter. -1 undoes
				// the +1 offset ProcessImGuiTextures applies when calling SetTexID (see its
				// comment) to keep a real index of 0 from looking like ImGui's "unset" sentinel.
				drawCommand.textureIndex = (uint)(uintptr_t)src.GetTexID() - 1;
				drawCommand.indexOffset = indexBase + src.IdxOffset;
				drawCommand.indexCount = src.ElemCount;
			}

			vertexBase += (uint)cmdList->VtxBuffer.Size;
			indexBase += (uint)cmdList->IdxBuffer.Size;
		}

		m_RendererAPI->SubmitGUIDrawData(m_ImGuiDrawData);
	}
#endif
}
