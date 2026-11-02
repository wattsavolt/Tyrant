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
#include "AssetSystem/FontAsset.h"

#define NK_IMPLEMENTATION
#include "GameUI.h"

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
		// Enough for the fonts' glyphs, and for the scratch memory baking them takes, the pixels included.
		constexpr uint c_NuklearFontMemorySize = 512 * 1024;
		constexpr uint c_NuklearFontScratchSize = 16 * 1024 * 1024;
		constexpr size_t c_NuklearFontAlignment = 16;
		// Pixel heights of the game UI's fonts, in GameFont order.
		constexpr float c_GameFontSizes[] = { 22.0f, 36.0f, 24.0f, 84.0f };
		static_assert(sizeof(c_GameFontSizes) / sizeof(c_GameFontSizes[0]) == static_cast<uint>(GameFont::Count), "Each game font needs a size.");

		// Matches GUIVertex (Rendering/GUIDrawData.h) field for field, so nk_convert can write
		// straight into that layout instead of Nuklear's own default vertex format.
		const nk_draw_vertex_layout_element g_NuklearVertexLayout[] =
		{
			{ NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(GUIVertex, pos) },
			{ NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(GUIVertex, uv) },
			{ NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, offsetof(GUIVertex, colour) },
			{ NK_VERTEX_LAYOUT_END }
		};

		// Creates a GPU texture for UI pixels and queues them for upload.
		// False, making nothing, when there's no upload memory free just now, such as while a level's
		// assets are loading, so the caller can try again next frame.
		bool CreateGUITexture(RendererAPI& rendererAPI, const char* name, uint width, uint height, const void* pixels, uint rowSize,
			TextureHandle& outHandle)
		{
			// Uploaded texture rows have to be padded to the row pitch alignment.
			const uint alignedRowSize = MemoryUtil::Align(rowSize, RenderConstants::c_RowPitchAlignment);
			UploadBufferAllocation alloc;
			if (!rendererAPI.RequestResourceUploadAllocation((size_t)alignedRowSize * height, alloc))
			{
				return false;
			}

			TextureDesc desc;
			desc.debugName = name;
			desc.info.width = width;
			desc.info.height = height;
			desc.info.depth = 1;
			desc.info.arrayLayerCount = 1;
			desc.info.mipCount = 1;
			desc.info.format = PixelFormat::PF_R8G8B8A8_UNORM;
			desc.info.type = ImageType::Image2D;
			desc.sampleCount = SampleCount::OneBit;
			desc.usage = static_cast<ImageUsage>(IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT);
			desc.layout = ImageLayout::IMAGE_LAYOUT_GENERAL;

			outHandle = rendererAPI.CreateTexture(desc);
			const uint8* src = static_cast<const uint8*>(pixels);
			uint8* dst = static_cast<uint8*>(alloc.cpuPtr);
			for (uint row = 0; row < height; ++row)
			{
				memcpy(dst + (size_t)row * alignedRowSize, src + (size_t)row * rowSize, rowSize);
			}
			rendererAPI.FlushBufferUploadAllocation(alloc);

			TextureUploadRequest request;
			request.srcBuffer = alloc.buffer;
			request.srcOffset = alloc.offset;
			request.dstTexture = outHandle;
			request.highestMip = 0;
			request.mipCount = 1;
			request.type = TextureUploadRequestType::GUI;
			request.resourceId = alloc.resourceId;
			rendererAPI.AddTextureUploadRequest(request);
			return true;
		}

		// Memory for Nuklear's font baking, handed out from the bottom up like a stack. Freeing the
		// newest block gives it back, along with any freed ones under it, which suits the rasteriser
		// freeing each glyph's memory once it's drawn.
		struct FontArena
		{
			uint8* data;
			size_t capacity;
			size_t used;
			// Where the newest block's header is, or SIZE_MAX with none.
			size_t top;
			size_t peak;
		};

		struct FontBlock
		{
			size_t previousTop;
			bool freed;
		};

		constexpr size_t c_FontBlockHeader = (sizeof(FontBlock) + c_NuklearFontAlignment - 1) & ~(c_NuklearFontAlignment - 1);

		void* FontArenaAlloc(nk_handle handle, void* /*old*/, nk_size size)
		{
			FontArena& arena = *static_cast<FontArena*>(handle.ptr);
			const size_t start = (arena.used + c_NuklearFontAlignment - 1) & ~(c_NuklearFontAlignment - 1);
			if (start + c_FontBlockHeader + size > arena.capacity)
			{
				TYR_ASSERT(false);
				return nullptr;
			}
			FontBlock* block = reinterpret_cast<FontBlock*>(arena.data + start);
			block->previousTop = arena.top;
			block->freed = false;
			arena.top = start;
			arena.used = start + c_FontBlockHeader + size;
			arena.peak = std::max(arena.peak, arena.used);
			return arena.data + start + c_FontBlockHeader;
		}

		void FontArenaFree(nk_handle handle, void* memory)
		{
			if (!memory)
			{
				return;
			}
			FontArena& arena = *static_cast<FontArena*>(handle.ptr);
			reinterpret_cast<FontBlock*>(static_cast<uint8*>(memory) - c_FontBlockHeader)->freed = true;
			while (arena.top != SIZE_MAX && reinterpret_cast<FontBlock*>(arena.data + arena.top)->freed)
			{
				arena.used = arena.top;
				arena.top = reinterpret_cast<FontBlock*>(arena.data + arena.top)->previousTop;
			}
		}

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
		bool CreateImGuiTexture(RendererAPI& rendererAPI, ImTextureData& tex, TextureHandle& outHandle)
		{
			return CreateGUITexture(rendererAPI, "ImGui Texture", (uint)tex.Width, (uint)tex.Height, tex.GetPixels(), (uint)tex.GetPitch(), outHandle);
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
		nk_init_fixed(m_NuklearContext, m_NuklearContextMemory.Data(), m_NuklearContextMemory.Size(), nullptr);
		const FontFile* noFiles[static_cast<uint>(GameFont::Count)] = {};
		CreateGameFonts(noFiles);

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

		// The fonts' memory is the module's own, so the atlas has nothing of its own to free.
		delete m_FontAtlas;
		m_FontAtlas = nullptr;
		m_RendererAPI->DeleteTexture(m_FontTexture);
	}

	void GUIModule::SetGameFonts(const FontFile* const* files)
	{
		// Everything the old fonts had was in the module's memory, which the new ones reuse.
		delete m_FontAtlas;
		m_FontAtlas = nullptr;
		for (nk_font*& font : m_Fonts)
		{
			font = nullptr;
		}
		m_RendererAPI->DeleteTexture(m_FontTexture);
		m_FontTexture = {};
		CreateGameFonts(files);
	}

	void GUIModule::CreateGameFonts(const FontFile* const* files)
	{
		// The fonts are kept in the module's memory. Baking's scratch memory is only needed until
		// it's done.
		m_NuklearFontMemory.Resize(c_NuklearFontMemorySize);
		Array<uint8> scratch(Array<uint8>::UninitializedTag{}, c_NuklearFontScratchSize);
		FontArena permanentArena{ m_NuklearFontMemory.Data(), m_NuklearFontMemory.Size(), 0, SIZE_MAX, 0 };
		FontArena scratchArena{ scratch.Data(), scratch.Size(), 0, SIZE_MAX, 0 };
		nk_allocator permanent;
		permanent.userdata = nk_handle_ptr(&permanentArena);
		permanent.alloc = &FontArenaAlloc;
		permanent.free = &FontArenaFree;
		nk_allocator temporary = permanent;
		temporary.userdata = nk_handle_ptr(&scratchArena);

		m_FontAtlas = new nk_font_atlas();
		nk_font_atlas_init_custom(m_FontAtlas, &permanent, &temporary);
		nk_font_atlas_begin(m_FontAtlas);
		for (uint i = 0; i < static_cast<uint>(GameFont::Count); ++i)
		{
			if (files[i] && !files[i]->data.IsEmpty())
			{
				// Read from the file while baking rather than copied, as it outlives the baking.
				struct nk_font_config config = nk_font_config(c_GameFontSizes[i]);
				config.ttf_blob = const_cast<uint8*>(files[i]->data.Data());
				config.ttf_size = files[i]->data.Size();
				config.ttf_data_owned_by_atlas = 1;
				// Drawn at whole pixels, so letters stay sharp.
				config.pixel_snap = 1;
				config.oversample_h = 1;
				config.oversample_v = 1;
				m_Fonts[i] = nk_font_atlas_add(m_FontAtlas, &config);
			}
			if (!m_Fonts[i])
			{
				// Not oversampled either, which would make the big sizes overflow the scratch memory.
				struct nk_font_config config = nk_font_config(c_GameFontSizes[i]);
				config.oversample_h = 1;
				config.oversample_v = 1;
				m_Fonts[i] = nk_font_atlas_add_default(m_FontAtlas, c_GameFontSizes[i], &config);
			}
		}
		int width = 0;
		int height = 0;
		const void* pixels = nk_font_atlas_bake(m_FontAtlas, &width, &height, NK_FONT_ATLAS_RGBA32);
		if (!pixels)
		{
			delete m_FontAtlas;
			m_FontAtlas = nullptr;
			for (nk_font*& font : m_Fonts)
			{
				font = nullptr;
			}
			// A broken font file falls back on Nuklear's own font.
			bool anyFiles = false;
			for (uint i = 0; i < static_cast<uint>(GameFont::Count); ++i)
			{
				anyFiles = anyFiles || files[i];
			}
			TYR_LOG_ERROR("Couldn't bake the game UI's fonts.");
			if (anyFiles)
			{
				const FontFile* noFiles[static_cast<uint>(GameFont::Count)] = {};
				CreateGameFonts(noFiles);
			}
			return;
		}
		if (!CreateGUITexture(*m_RendererAPI, "Game UI Font", (uint)width, (uint)height, pixels, (uint)width * 4, m_FontTexture))
		{
			TYR_LOG_ERROR("No upload memory was free for the game UI's fonts.");
		}

		// +1 so a texture at index 0 isn't mistaken for none.
		nk_draw_null_texture whiteTexel;
		nk_font_atlas_end(m_FontAtlas, nk_handle_id((int)m_FontTexture.h.index + 1), &whiteTexel);
		m_WhiteTexelUV = Vector2(whiteTexel.uv.x, whiteTexel.uv.y);
		TYR_LOG_INFO("Baked the game UI's fonts into %dx%d, using %u KB of font memory and %u KB of scratch at most.", width, height,
			static_cast<uint>(permanentArena.peak / 1024), static_cast<uint>(scratchArena.peak / 1024));
		// The allocator only lived for the baking, and nothing calls it after this.
		m_FontAtlas->permanent = {};
		m_FontAtlas->temporary = {};
		nk_style_set_font(m_NuklearContext, &m_Fonts[static_cast<uint>(GameFont::Body)]->handle);
	}

	const nk_user_font* GUIModule::GetGameFont(GameFont font) const
	{
		const nk_font* baked = m_Fonts[static_cast<uint>(font)];
		return baked ? &baked->handle : nullptr;
	}

	void GUIModule::SetGameViewRect(float x, float y, float width, float height)
	{
		m_HasGameViewRect = true;
		m_GameViewX = x;
		m_GameViewY = y;
		m_GameViewWidth = width;
		m_GameViewHeight = height;
	}

	Vector2 GUIModule::GetWindowSize() const
	{
		return m_WindowModule
			? Vector2((float)m_WindowModule->GetWindowWidth(m_Window), (float)m_WindowModule->GetWindowHeight(m_Window))
			: Vector2(1920.0f, 1080.0f);
	}

	Vector2 GUIModule::GetGameViewSize() const
	{
		return m_HasGameViewRect ? Vector2(m_GameViewWidth, m_GameViewHeight) : GetWindowSize();
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
		// Update, as the update order in EngineLoop has it.

		// The update loop can tick more than once before a render frame slot is actually
		// consumed - reset first so a stale submission never sits alongside a new one.
		m_RendererAPI->ResetGUIDrawData();

#if !TYR_FINAL
		ImGui::SetCurrentContext(m_ImGuiContext);
		ImGui::Render();
		ProcessImGuiTextures();
		SubmitImGuiDrawData();
#endif

		// After ImGui, so the game's UI is drawn over the editor's viewport showing the game.
		SubmitNuklearDrawData();
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
		config.tex_null.texture = nk_handle_id((int)m_FontTexture.h.index + 1);
		config.tex_null.uv = nk_vec2(m_WhiteTexelUV.x, m_WhiteTexelUV.y);

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
			m_NuklearDrawData.displaySize = GetWindowSize();

			m_NuklearDrawData.vertices.Resize(vertexCount);
			memcpy(m_NuklearDrawData.vertices.Data(), nk_buffer_memory_const(&vertices), vertexCount * sizeof(GUIVertex));
			// Laid out in the game view's own space, and moved to where it is in the window.
			const Vector2 viewOffset = m_HasGameViewRect ? Vector2(m_GameViewX, m_GameViewY) : Vector2(0.0f, 0.0f);
			const Vector2 viewSize = GetGameViewSize();
			for (GUIVertex& vertex : m_NuklearDrawData.vertices)
			{
				vertex.pos = vertex.pos + viewOffset;
			}
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

				// Nuklear's clip rects can reach far past the view, which a scissor can't.
				const float clipMinX = std::max(cmd->clip_rect.x, 0.0f) + viewOffset.x;
				const float clipMinY = std::max(cmd->clip_rect.y, 0.0f) + viewOffset.y;
				const float clipMaxX = std::min(cmd->clip_rect.x + cmd->clip_rect.w, viewSize.x) + viewOffset.x;
				const float clipMaxY = std::min(cmd->clip_rect.y + cmd->clip_rect.h, viewSize.y) + viewOffset.y;
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
				drawCommand.textureIndex = (uint)cmd->texture.id - 1;
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

		// While the game has the input, every button and key reads as up apart from Escape, and
		// nothing is typed or scrolled.
		const bool blocked = input->IsUIInputBlocked();
		const Vector2& mousePos = input->GetMousePosition();
		io.AddMousePosEvent(mousePos.x, mousePos.y);
		io.AddMouseButtonEvent(ImGuiMouseButton_Left, !blocked && input->IsMouseButtonDown(MouseButton::Left));
		io.AddMouseButtonEvent(ImGuiMouseButton_Right, !blocked && input->IsMouseButtonDown(MouseButton::Right));
		io.AddMouseButtonEvent(ImGuiMouseButton_Middle, !blocked && input->IsMouseButtonDown(MouseButton::Middle));
		io.AddMouseWheelEvent(0.0f, blocked ? 0.0f : input->GetScrollDelta());

		for (const KeyMapping& mapping : g_KeyMappings)
		{
			io.AddKeyEvent(mapping.imguiKey, (!blocked || mapping.imguiKey == ImGuiKey_Escape) && input->IsKeyDown(mapping.vk));
		}
		// 'A'-'Z'/'0'-'9'/F1-F12 follow a simple contiguous range each, so map them here
		// instead of listing all of them individually. Loop counter must be wider than
		// KeyCode (uint8), or `vk < c_MaxKeyCodes` (256) would never terminate once vk wraps.
		for (uint vk = 0; vk < c_MaxKeyCodes; ++vk)
		{
			const ImGuiKey key = VkDigitOrLetterToImGuiKey((KeyCode)vk);
			if (key != ImGuiKey_None)
			{
				io.AddKeyEvent(key, !blocked && input->IsKeyDown((KeyCode)vk));
			}
		}

		const uint charCount = blocked ? 0 : input->GetTypedCharCount();
		const char* chars = input->GetTypedChars();
		for (uint i = 0; i < charCount; ++i)
		{
			io.AddInputCharacter((unsigned char)chars[i]);
		}
	}

	void GUIModule::ProcessImGuiTextures()
	{
		// A texture that can't be uploaded yet keeps its status, so it's tried again next frame.
		for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
		{
			TextureHandle handle;
			if (tex->Status == ImTextureStatus_WantCreate)
			{
				if (!CreateImGuiTexture(*m_RendererAPI, *tex, handle))
				{
					continue;
				}
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
				if (!CreateImGuiTexture(*m_RendererAPI, *tex, handle))
				{
					continue;
				}
				m_RendererAPI->DeleteTexture(UnpackTextureHandle(tex->BackendUserData));
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
				// Nothing's drawn with a texture still waiting for upload memory.
				const bool textureWaiting = src.TexRef._TexData && src.TexRef._TexData->TexID == ImTextureID_Invalid;
				if (src.ElemCount == 0 || textureWaiting)
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
