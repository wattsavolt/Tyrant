#include "McpTools.h"
#include "McpServer.h"
#include "McpReflection.h"
#include "Editor.h"
#include "UndoHistory.h"
#include "Actor/ActorRegistry.h"
#include "Actor/ActorTypes.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetConstants.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/MaterialAsset.h"
#include "ECS/ComponentRegistry.h"
#include "Importing/ModelImporter.h"
#include "Importing/MaterialImporter.h"
#include "Logging/Logger.h"
#include "Memory/StackAllocation.h"
#include "Reflection/TypeRegistry.h"
#include "Rendering/RendererAPI.h"
#include "Utility/PathUtil.h"
#include "World/Camera.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "ECS/Components.h"
#include "Gameplay/Interaction.h"
#include "Math/Math.h"
#include "Platform/Platform.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace tyr
{
	namespace
	{
		using namespace simdjson;

		static_assert(McpServer::c_BodyPadding >= SIMDJSON_PADDING, "simdjson reads up to SIMDJSON_PADDING bytes past the end of a body.");

		// The MCP versions this server speaks, newest first.
		const char* const c_ProtocolVersions[] = { "2025-06-18", "2025-03-26", "2024-11-05" };

		constexpr const char* c_Instructions =
			"Controls the Tyrant engine's editor. Units are metres, Y is up and space is left-handed with +Z forward. "
			"Rotations are Euler degrees [x, y, z] or quaternions [x, y, z, w]. Asset paths are relative to the "
			"project's Assets folder, such as Models/Cube/Cube.mesh. The level can only be edited while the game is "
			"stopped. Each tool call that edits the level is one undo step, shared with the editor's own Ctrl+Z. "
			"capture_screenshot shows the viewport, and read_log shows errors and warnings.";

		// How many frames a screenshot waits for the GPU before giving up.
		constexpr uint c_MaxCaptureFrames = 600;
		// How many frames a material tool waits for its material to load before giving up.
		constexpr uint c_MaxMaterialLoadFrames = 600;
		constexpr uint c_DefaultCaptureWidth = 1280;

		// JSON-RPC's error codes.
		constexpr int c_ParseError = -32700;
		constexpr int c_InvalidRequest = -32600;
		constexpr int c_MethodNotFound = -32601;
		constexpr int c_InvalidParams = -32602;

		bool Equals(std::string_view text, const char* value)
		{
			return text.size() == strlen(value) && text == value;
		}

		const char* GetPlayStateName(PlayState state)
		{
			switch (state)
			{
			case PlayState::Playing: return "playing";
			case PlayState::Paused: return "paused";
			default: return "editing";
			}
		}

		const char* GetLogLevelName(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::Fatal: return "fatal";
			case LogLevel::Error: return "error";
			case LogLevel::Warning: return "warning";
			default: return "info";
			}
		}

		const ActorTypeDesc* FindActorTypeByName(const char* name)
		{
			for (const std::pair<const Id64&, const ActorTypeDesc&> type : ActorRegistry::Instance().GetAll())
			{
				if (type.second.name == name)
				{
					return &type.second;
				}
			}
			return nullptr;
		}

		// 8-bit sRGB to linear, for averaging pixels.
		float SRGBToLinear(uint8 value)
		{
			static float s_Table[256];
			static bool s_TableBuilt = false;
			if (!s_TableBuilt)
			{
				for (uint i = 0; i < 256; ++i)
				{
					const float srgb = i / 255.0f;
					s_Table[i] = srgb <= 0.04045f ? srgb / 12.92f : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
				}
				s_TableBuilt = true;
			}
			return s_Table[value];
		}

		uint8 LinearToSRGB(float linear)
		{
			const float srgb = linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
			return static_cast<uint8>(Math::Clamp(srgb * 255.0f + 0.5f, 0.0f, 255.0f));
		}
	}

	const McpTools::Tool McpTools::c_Tools[] =
	{
		{ "get_editor_state", "The editor's state: whether the game is playing, the open level and whether it has unsaved changes, whether there's anything to undo or redo, the selected actor and the camera.",
			R"({"type":"object","properties":{}})", &McpTools::GetEditorState },
		{ "list_assets", "Lists imported assets by path. Filter by type (the file extension without its dot: mesh, skmesh, mat, tex, level, skel, anim) and by folder.",
			R"({"type":"object","properties":{"type":{"type":"string"},"folder":{"type":"string","description":"Only assets whose path starts with this, such as Models/"},"limit":{"type":"integer","default":500}}})", &McpTools::ListAssets },
		{ "import_model", "Imports a glTF or GLB model, with its materials and textures, into the Assets folder. Importing to the same folder and name again reimports it, keeping its asset IDs.",
			R"({"type":"object","properties":{"sourcePath":{"type":"string","description":"Absolute path to the .gltf or .glb file"},"outputFolder":{"type":"string","description":"Folder under Assets, such as Models/Crate"},"name":{"type":"string"},"generateLods":{"type":"boolean","default":true},"lodCount":{"type":"integer","default":3}},"required":["sourcePath","outputFolder","name"]})", &McpTools::ImportModel },
		{ "list_actor_types", "The actor types that can be spawned.",
			R"({"type":"object","properties":{}})", &McpTools::ListActorTypes },
		{ "list_actors", "The level's actors with their types, hierarchy folders and transforms. world \"game\" lists the running game's instead.",
			R"({"type":"object","properties":{"world":{"type":"string","enum":["level","game"],"default":"level"},"folder":{"type":"string","description":"Only actors in this folder or below"}}})", &McpTools::ListActors },
		{ "get_actor", "An actor's components and all their fields, per entity.",
			R"({"type":"object","properties":{"name":{"type":"string"},"world":{"type":"string","enum":["level","game"],"default":"level"}},"required":["name"]})", &McpTools::GetActor },
		{ "spawn_actor", "Adds an actor to the level and selects it. Give a mesh asset to place a static or skeletal mesh actor, or an actor type. It goes in front of the camera unless given a position.",
			R"({"type":"object","properties":{"type":{"type":"string","description":"From list_actor_types. Optional with a mesh."},"mesh":{"type":"string","description":"A .mesh or .skmesh asset path"},"animation":{"type":"string","description":"An .anim asset to play, for a skeletal mesh"},"name":{"type":"string"},"folder":{"type":"string"},"position":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},"rotation":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":4},"scale":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3}}})", &McpTools::SpawnActor },
		{ "delete_actor", "Removes an actor from the level.",
			R"({"type":"object","properties":{"name":{"type":"string"}},"required":["name"]})", &McpTools::DeleteActor },
		{ "set_actor_transform", "Moves, rotates or scales an actor. Only the parts given change.",
			R"({"type":"object","properties":{"name":{"type":"string"},"position":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},"rotation":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":4},"scale":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3}},"required":["name"]})", &McpTools::SetActorTransform },
		{ "set_component", "Sets fields of one of an actor's components, named as get_actor shows them. Only the fields given change. Returns the component afterwards.",
			R"({"type":"object","properties":{"actor":{"type":"string"},"component":{"type":"string","description":"The component's type, such as PointLightComponent"},"entity":{"type":"integer","default":0,"description":"Which of the actor's entities, 0 being its root"},"values":{"type":"object"}},"required":["actor","component","values"]})", &McpTools::SetComponent },
		{ "select_actor", "Selects an actor in the editor, or clears the selection when no name is given.",
			R"({"type":"object","properties":{"name":{"type":"string"}}})", &McpTools::SelectActor },
		{ "open_level", "Opens a level by asset path, such as Levels/Main.level.",
			R"({"type":"object","properties":{"path":{"type":"string"},"discardChanges":{"type":"boolean","default":false}},"required":["path"]})", &McpTools::OpenLevel },
		{ "save_level", "Saves the open level, or saves it under a new name in the Levels folder.",
			R"({"type":"object","properties":{"name":{"type":"string","description":"Saves as Levels/<name>.level instead"}}})", &McpTools::SaveLevel },
		{ "new_level", "Makes and opens a level holding just a directional light, saved as Levels/<name>.level.",
			R"({"type":"object","properties":{"name":{"type":"string"},"discardChanges":{"type":"boolean","default":false}},"required":["name"]})", &McpTools::NewLevel },
		{ "play", "Starts the game in the viewport, saving the level first if it has changes. Resumes it if paused.",
			R"({"type":"object","properties":{}})", &McpTools::Play },
		{ "stop", "Stops the game and goes back to editing the level.",
			R"({"type":"object","properties":{}})", &McpTools::Stop },
		{ "build_navmesh", "Builds the level's navmesh from everything in it that doesn't move. Save the level to keep it.",
			R"({"type":"object","properties":{}})", &McpTools::BuildNavmesh },
		{ "find_path", "Finds a walkable path across the navmesh of the world on screen, as the corners to walk to.",
			R"({"type":"object","properties":{"from":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},"to":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3}},"required":["from","to"]})", &McpTools::FindPath },
		{ "undo", "Undoes the last edits to the level, whether made by a tool or in the editor. Opening or making a level clears the history.",
			R"({"type":"object","properties":{"steps":{"type":"integer","default":1}}})", &McpTools::Undo },
		{ "redo", "Redoes edits that were undone, until another edit is made.",
			R"({"type":"object","properties":{"steps":{"type":"integer","default":1}}})", &McpTools::Redo },
		{ "capture_screenshot", "A PNG of what the viewport shows: the level, or the game while playing. The editor's own panels aren't in it.",
			R"({"type":"object","properties":{"maxWidth":{"type":"integer","default":1280,"description":"Scaled down to this width if wider"}}})", &McpTools::CaptureScreenshot },
		{ "read_log", "Recent log messages, oldest first. Pass the last sequence number seen to get only newer ones.",
			R"({"type":"object","properties":{"afterSequence":{"type":"integer","default":0},"maxEntries":{"type":"integer","default":100},"minLevel":{"type":"string","enum":["error","warning","info"],"default":"info"}}})", &McpTools::ReadLog },
		{ "set_camera", "Moves the editor's camera and points it at a target.",
			R"({"type":"object","properties":{"position":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3},"target":{"type":"array","items":{"type":"number"},"minItems":3,"maxItems":3}}})", &McpTools::SetCamera },
		{ "get_material", "A material's textures and parameters. list_assets with type mat lists materials.",
			R"({"type":"object","properties":{"path":{"type":"string","description":"Such as Models/Crate/Crate_Material0.mat"}},"required":["path"]})", &McpTools::GetMaterial },
		{ "set_material", "Changes a material's parameters, named as get_material shows them, and saves it. Only the values given change. Albedo Tint is linear [r, g, b, a] multiplying the albedo texture; roughness and metallic map the texture's 0 to 1 onto their Min to Max, so equal values give a constant; Blend Mode is 0 opaque, 1 masked, 2 translucent. Every actor using the material changes. Returns the material afterwards.",
			R"({"type":"object","properties":{"path":{"type":"string"},"values":{"type":"object"}},"required":["path","values"]})", &McpTools::SetMaterial },
		{ "get_level_settings", "The open level's settings: ambient light, the sky (a cubemap from import_sky, its tint, intensity and yaw in degrees) and exposure in EV100.",
			R"({"type":"object","properties":{}})", &McpTools::GetLevelSettings },
		{ "set_level_settings", "Changes the open level's settings, named as get_level_settings shows them, as an undoable edit. Only the values given change. Sky Texture is an asset path. Auto exposure keeps between Min and Max Exposure, and the lower Min Exposure is the more a dark view is brightened; Exposure Compensation adds stops, brighter when positive. Returns the settings afterwards.",
			R"({"type":"object","properties":{"values":{"type":"object"}},"required":["values"]})", &McpTools::SetLevelSettings },
		{ "import_sky", "Imports an equirectangular HDR image (.hdr), such as one from Poly Haven, as a sky cubemap for set_level_settings' Sky Texture. Importing to the same folder and name again keeps its asset ID.",
			R"({"type":"object","properties":{"sourcePath":{"type":"string","description":"Absolute path to the .hdr file"},"outputFolder":{"type":"string","description":"Folder under Assets, such as Skies/Night"},"name":{"type":"string"},"faceSize":{"type":"integer","description":"Pixels across each face, 0 to match the image","default":0}},"required":["sourcePath","outputFolder","name"]})", &McpTools::ImportSky },
		{ "get_display", "The display settings (HDR on or off, paper white, UI and peak brightness in nits), what the window's display reports, and whether HDR10 or SDR is being output.",
			R"({"type":"object","properties":{}})", &McpTools::GetDisplay },
		{ "set_display", "Changes the display settings get_display shows. Only the values given change.",
			R"({"type":"object","properties":{"hdr":{"type":"boolean"},"paperWhiteNits":{"type":"number"},"uiNits":{"type":"number"},"peakNits":{"type":"number","description":"0 uses what the display reports"}}})", &McpTools::SetDisplay },
		{ "set_material_layer", "Gives a material a layer: another material's albedo, normal and AO roughness metallic textures, blended over its own in patches, such as grass over gravel. set_material's Layer UV Scale, Layer Patch Size (metres), Layer Coverage (0 to 1) and Layer Sharpness shape it. An empty layerMaterial removes the layer. Saves the material.",
			R"({"type":"object","properties":{"path":{"type":"string"},"layerMaterial":{"type":"string","description":"The .mat to take the textures from, or empty to remove the layer"}},"required":["path","layerMaterial"]})", &McpTools::SetMaterialLayer },
		{ "interact", "While playing, uses an actor in the game as the player would with E: opens or closes a door, uses a door panel, or picks up an item. Returns whether it worked and the message the player would see.",
			R"({"type":"object","properties":{"actor":{"type":"string"}},"required":["actor"]})", &McpTools::Interact },
		{ "give_item", "While playing, puts an item such as a key card into the player's inventory.",
			R"({"type":"object","properties":{"item":{"type":"string"}},"required":["item"]})", &McpTools::GiveItem },
	};

	McpTools::McpTools(Editor& editor)
		: m_Editor(editor)
	{
		// Tools called without arguments read from this.
		static const char c_EmptyObject[SIMDJSON_PADDING + 3] = "{}";
		dom::element empty;
		const error_code error = m_EmptyArgsParser.parse(c_EmptyObject, 2, false).get(empty);
		TYR_ASSERT(!error);
		(void)error;
		empty.get_object().get(m_EmptyArgs);
	}

	void McpTools::Update(McpServer& server)
	{
		uint connection;
		const char* body;
		uint bodySize;
		// Requests wait while the level's assets are being loaded again after an import.
		while (!m_Editor.IsRefreshingAssets() && server.TakeRequest(connection, body, bodySize))
		{
			HandleMessage(server, connection, body, bodySize);
		}
		UpdateCaptures(server);
		UpdatePendingMaterials(server);
	}

	void McpTools::UpdatePendingMaterials(McpServer& server)
	{
		for (uint i = m_PendingMaterials.Size(); i-- > 0;)
		{
			PendingMaterial& pending = m_PendingMaterials[i];
			const bool loaded = m_Editor.m_AssetManager->GetMaterialFile(pending.material) != nullptr;
			if (!loaded && ++pending.framesWaited < c_MaxMaterialLoadFrames)
			{
				continue;
			}

			// Taken off the list first, since running the request again can add to it.
			const PendingMaterial finished = pending;
			m_PendingMaterials.SwapAndPopBack(i);
			if (loaded)
			{
				HandleMessage(server, finished.connection, finished.body, finished.bodySize);
			}
			else
			{
				// Says which of its textures it's still waiting on.
				char waiting[512] = {};
				uint dependencyCount;
				const AssetID* dependencies = AssetRegistry::Instance().GetAssetDependencies(finished.material, dependencyCount);
				for (uint d = 0; d < dependencyCount; ++d)
				{
					if (m_Editor.m_AssetManager->GetLoadState(dependencies[d]) != AssetLoadState::Loaded)
					{
						const size_t used = strlen(waiting);
						snprintf(waiting + used, sizeof(waiting) - used, " %s (state %u)", AssetRegistry::Instance().GetAssetData(dependencies[d]).filePath.CStr(),
							(uint)m_Editor.m_AssetManager->GetLoadState(dependencies[d]));
					}
				}
				Fail("The material didn't finish loading. Still waiting on:%s", waiting[0] ? waiting : " nothing it lists");
				ReplyToolResult(server, finished.connection, finished.id, true);
			}
			m_Editor.m_AssetManager->DeleteMaterial(finished.material);
		}
	}

	void McpTools::HandleMessage(McpServer& server, uint connection, const char* body, uint bodySize)
	{
		m_CurrentBody = body;
		m_CurrentBodySize = bodySize;
		dom::element root;
		if (m_Parser.parse(body, bodySize, false).get(root))
		{
			ReplyError(server, connection, nullptr, c_ParseError, "The message isn't valid JSON.");
			return;
		}
		dom::object message;
		if (root.get_object().get(message))
		{
			ReplyError(server, connection, nullptr, c_InvalidRequest, "Expected a single JSON-RPC message object.");
			return;
		}

		std::string_view method;
		dom::element idElement;
		const bool hasMethod = !message["method"].get(method);
		const bool hasID = !message["id"].get(idElement);
		// Notifications, and replies to requests this server never sends, need no answer.
		if (!hasMethod || !hasID)
		{
			server.Reply(connection, nullptr, 0);
			return;
		}

		RequestID id;
		int64 number;
		std::string_view text;
		if (!idElement.get_int64().get(number))
		{
			id.kind = RequestID::Kind::Number;
			id.number = number;
		}
		else if (!idElement.get_string().get(text))
		{
			id.kind = RequestID::Kind::String;
			snprintf(id.text, sizeof(id.text), "%.*s", static_cast<int>(text.size()), text.data());
		}

		dom::object params = m_EmptyArgs;
		message["params"].get_object().get(params);

		if (Equals(method, "initialize"))
		{
			std::string_view requested;
			const char* version = c_ProtocolVersions[0];
			if (!params["protocolVersion"].get(requested))
			{
				for (const char* supported : c_ProtocolVersions)
				{
					version = Equals(requested, supported) ? supported : version;
				}
			}

			m_Result.Clear();
			m_Result.BeginObject();
			m_Result.KeyString("protocolVersion", version);
			m_Result.Key("capabilities");
			m_Result.BeginObject();
			m_Result.Key("tools");
			m_Result.BeginObject();
			m_Result.KeyBool("listChanged", false);
			m_Result.EndObject();
			m_Result.EndObject();
			m_Result.Key("serverInfo");
			m_Result.BeginObject();
			m_Result.KeyString("name", "tyrant-editor");
			m_Result.KeyString("title", "Tyrant Editor");
			m_Result.KeyString("version", "0.1.0");
			m_Result.EndObject();
			m_Result.KeyString("instructions", c_Instructions);
			m_Result.EndObject();
			ReplyResult(server, connection, id);
		}
		else if (Equals(method, "ping"))
		{
			m_Result.Clear();
			m_Result.BeginObject();
			m_Result.EndObject();
			ReplyResult(server, connection, id);
		}
		else if (Equals(method, "tools/list"))
		{
			m_Result.Clear();
			m_Result.BeginObject();
			m_Result.Key("tools");
			m_Result.BeginArray();
			for (const Tool& tool : c_Tools)
			{
				m_Result.BeginObject();
				m_Result.KeyString("name", tool.name);
				m_Result.KeyString("description", tool.description);
				m_Result.Key("inputSchema");
				m_Result.Raw(tool.inputSchema);
				m_Result.EndObject();
			}
			m_Result.EndArray();
			m_Result.EndObject();
			ReplyResult(server, connection, id);
		}
		else if (Equals(method, "tools/call"))
		{
			std::string_view name;
			const Tool* tool = nullptr;
			if (!params["name"].get(name))
			{
				for (const Tool& candidate : c_Tools)
				{
					tool = Equals(name, candidate.name) ? &candidate : tool;
				}
			}
			if (!tool)
			{
				ReplyError(server, connection, &id, c_InvalidParams, "Unknown tool.");
				return;
			}

			dom::object args = m_EmptyArgs;
			params["arguments"].get_object().get(args);

			m_Result.Clear();
			m_Error[0] = '\0';
			m_ArgumentError = false;
			m_CurrentConnection = connection;
			m_CurrentID = &id;
			const ToolResult result = (this->*tool->function)(args);
			m_CurrentID = nullptr;
			// Each tool call's edits are their own undo step.
			if (m_Editor.m_PlayState == PlayState::Editing)
			{
				m_Editor.m_UndoHistory->Commit();
			}
			if (result != ToolResult::Pending)
			{
				ReplyToolResult(server, connection, id, result == ToolResult::Failed);
			}
		}
		else
		{
			ReplyError(server, connection, &id, c_MethodNotFound, "Unknown method.");
		}
	}

	void McpTools::WriteID(const RequestID* id)
	{
		if (!id || id->kind == RequestID::Kind::Null)
		{
			m_Reply.Null();
		}
		else if (id->kind == RequestID::Kind::String)
		{
			m_Reply.String(id->text);
		}
		else
		{
			m_Reply.Int(id->number);
		}
	}

	void McpTools::ReplyResult(McpServer& server, uint connection, const RequestID& id)
	{
		m_Reply.Clear();
		m_Reply.BeginObject();
		m_Reply.KeyString("jsonrpc", "2.0");
		m_Reply.Key("id");
		WriteID(&id);
		m_Reply.Key("result");
		m_Reply.Raw(m_Result.Data(), m_Result.Size());
		m_Reply.EndObject();
		server.Reply(connection, m_Reply.Data(), m_Reply.Size());
	}

	void McpTools::ReplyToolResult(McpServer& server, uint connection, const RequestID& id, bool isError)
	{
		m_Reply.Clear();
		m_Reply.BeginObject();
		m_Reply.KeyString("jsonrpc", "2.0");
		m_Reply.Key("id");
		WriteID(&id);
		m_Reply.Key("result");
		m_Reply.BeginObject();
		m_Reply.Key("content");
		m_Reply.BeginArray();
		m_Reply.BeginObject();
		m_Reply.KeyString("type", "text");
		m_Reply.Key("text");
		if (isError)
		{
			m_Reply.String(m_Error);
		}
		else
		{
			m_Reply.String(m_Result.Data(), m_Result.Size());
		}
		m_Reply.EndObject();
		m_Reply.EndArray();
		m_Reply.KeyBool("isError", isError);
		m_Reply.EndObject();
		m_Reply.EndObject();
		server.Reply(connection, m_Reply.Data(), m_Reply.Size());
	}

	void McpTools::ReplyError(McpServer& server, uint connection, const RequestID* id, int code, const char* message)
	{
		m_Reply.Clear();
		m_Reply.BeginObject();
		m_Reply.KeyString("jsonrpc", "2.0");
		m_Reply.Key("id");
		WriteID(id);
		m_Reply.Key("error");
		m_Reply.BeginObject();
		m_Reply.KeyInt("code", code);
		m_Reply.KeyString("message", message);
		m_Reply.EndObject();
		m_Reply.EndObject();
		server.Reply(connection, m_Reply.Data(), m_Reply.Size());
	}

	McpTools::ToolResult McpTools::Fail(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		vsnprintf(m_Error, sizeof(m_Error), format, args);
		va_end(args);
		return ToolResult::Failed;
	}

	bool McpTools::GetString(const dom::object& args, const char* key, char* out, size_t outSize)
	{
		dom::element value;
		if (args[key].get(value) || value.is_null())
		{
			return false;
		}
		std::string_view text;
		if (value.get_string().get(text) || text.size() >= outSize)
		{
			m_ArgumentError = true;
			snprintf(m_Error, sizeof(m_Error), "'%s' should be a string under %zu characters.", key, outSize);
			return false;
		}
		snprintf(out, outSize, "%.*s", static_cast<int>(text.size()), text.data());
		return true;
	}

	bool McpTools::GetNumber(const dom::object& args, const char* key, double& out)
	{
		dom::element value;
		if (args[key].get(value) || value.is_null())
		{
			return false;
		}
		if (value.get_double().get(out))
		{
			m_ArgumentError = true;
			snprintf(m_Error, sizeof(m_Error), "'%s' should be a number.", key);
			return false;
		}
		return true;
	}

	bool McpTools::GetVector3(const dom::object& args, const char* key, Vector3& out)
	{
		dom::element value;
		if (args[key].get(value) || value.is_null())
		{
			return false;
		}
		dom::array array;
		double components[3];
		bool valid = !value.get_array().get(array) && array.size() == 3;
		for (uint i = 0; valid && i < 3; ++i)
		{
			valid = !array.at(i).get_double().get(components[i]);
		}
		if (!valid)
		{
			m_ArgumentError = true;
			snprintf(m_Error, sizeof(m_Error), "'%s' should be [x, y, z].", key);
			return false;
		}
		out = Vector3((float)components[0], (float)components[1], (float)components[2]);
		return true;
	}

	bool McpTools::GetRotation(const dom::object& args, const char* key, Quaternion& out)
	{
		dom::element value;
		if (args[key].get(value) || value.is_null())
		{
			return false;
		}
		dom::array array;
		double components[4];
		const bool isArray = !value.get_array().get(array);
		const size_t count = isArray ? array.size() : 0;
		bool valid = count == 3 || count == 4;
		for (uint i = 0; valid && i < count; ++i)
		{
			valid = !array.at(i).get_double().get(components[i]);
		}
		if (!valid)
		{
			m_ArgumentError = true;
			snprintf(m_Error, sizeof(m_Error), "'%s' should be Euler degrees [x, y, z] or a quaternion [x, y, z, w].", key);
			return false;
		}

		if (count == 4)
		{
			out = Quaternion((float)components[3], (float)components[0], (float)components[1], (float)components[2]);
		}
		else
		{
			out.FromEulerAngles((float)components[0] * Math::c_DegToRad, (float)components[1] * Math::c_DegToRad, (float)components[2] * Math::c_DegToRad);
		}
		return true;
	}

	World* McpTools::GetWorld(const dom::object& args)
	{
		char worldName[16] = "level";
		GetString(args, "world", worldName, sizeof(worldName));
		if (strcmp(worldName, "game") == 0)
		{
			if (m_Editor.m_PlayState == PlayState::Editing)
			{
				Fail("The game isn't running.");
				return nullptr;
			}
			return &m_Editor.m_WorldManager->GetWorld(m_Editor.m_WorldManager->GetActiveWorld());
		}
		if (strcmp(worldName, "level") != 0)
		{
			Fail("'world' should be \"level\" or \"game\".");
			return nullptr;
		}
		return &m_Editor.m_WorldManager->GetWorld(m_Editor.m_LevelEditorWorld);
	}

	ActorInstance* McpTools::FindActor(World& world, const char* name)
	{
		for (ActorInstance& actor : world.actorInstances)
		{
			if (actor.name == name)
			{
				return &actor;
			}
		}
		Fail("No actor named %s. list_actors lists them.", name);
		return nullptr;
	}

	bool McpTools::RequireEditing()
	{
		if (m_Editor.m_PlayState != PlayState::Editing)
		{
			Fail("The level can't be changed while the game is running. Call stop first.");
			return false;
		}
		return true;
	}

	void McpTools::WriteTransform(const Transform& transform)
	{
		Vector3 radians;
		transform.rotation.ToEulerAngles(radians.x, radians.y, radians.z);
		const Vector3 degrees = radians * Math::c_RadToDeg;

		m_Result.Key("position");
		m_Result.BeginArray();
		m_Result.Number(transform.position.x);
		m_Result.Number(transform.position.y);
		m_Result.Number(transform.position.z);
		m_Result.EndArray();
		m_Result.Key("rotation");
		m_Result.BeginArray();
		m_Result.Number(degrees.x);
		m_Result.Number(degrees.y);
		m_Result.Number(degrees.z);
		m_Result.EndArray();
		m_Result.Key("scale");
		m_Result.BeginArray();
		m_Result.Number(transform.scale.x);
		m_Result.Number(transform.scale.y);
		m_Result.Number(transform.scale.z);
		m_Result.EndArray();
	}

	void McpTools::WriteActorSummary(World& world, const ActorInstance& actor)
	{
		const ActorTypeDesc* type = ActorRegistry::Instance().FindActorType(actor.typeID);
		m_Result.KeyString("name", actor.name.CStr());
		m_Result.KeyString("type", type ? type->name.CStr() : "");
		m_Result.KeyString("folder", actor.folderPath.CStr());
		const Entity root = actor.RootEntity();
		if (world.entities.HasComponent<ComponentTransform>(root))
		{
			WriteTransform(world.entities.GetComponent<ComponentTransform>(root).local);
		}
	}

	McpTools::ToolResult McpTools::GetEditorState(const dom::object& args)
	{
		const World& level = m_Editor.m_WorldManager->GetWorld(m_Editor.m_LevelEditorWorld);
		const AssetRegistry& registry = AssetRegistry::Instance();

		m_Result.BeginObject();
		m_Result.KeyString("playState", GetPlayStateName(m_Editor.m_PlayState));
		m_Result.Key("level");
		if (AssetUtil::IsValidAssetID(m_Editor.m_Level) && registry.GetAssets().Contains(m_Editor.m_Level))
		{
			m_Result.String(registry.GetAssetData(m_Editor.m_Level).filePath.CStr());
		}
		else
		{
			m_Result.Null();
		}
		m_Result.KeyBool("levelDirty", m_Editor.IsLevelDirty());
		m_Result.KeyBool("canUndo", m_Editor.m_UndoHistory->CanUndo());
		m_Result.KeyBool("canRedo", m_Editor.m_UndoHistory->CanRedo());

		m_Result.Key("selectedActor");
		const ActorInstance* selected = nullptr;
		for (const ActorInstance& actor : level.actorInstances)
		{
			selected = actor.RootEntity() == m_Editor.m_SelectedActor ? &actor : selected;
		}
		if (selected)
		{
			m_Result.String(selected->name.CStr());
		}
		else
		{
			m_Result.Null();
		}

		const Camera& camera = *m_Editor.m_Camera;
		m_Result.Key("camera");
		m_Result.BeginObject();
		m_Result.Key("position");
		m_Result.BeginArray();
		m_Result.Number(camera.GetPosition().x);
		m_Result.Number(camera.GetPosition().y);
		m_Result.Number(camera.GetPosition().z);
		m_Result.EndArray();
		m_Result.Key("forward");
		m_Result.BeginArray();
		m_Result.Number(camera.GetForward().x);
		m_Result.Number(camera.GetForward().y);
		m_Result.Number(camera.GetForward().z);
		m_Result.EndArray();
		m_Result.EndObject();
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::ListAssets(const dom::object& args)
	{
		char type[16] = {};
		char folder[PathConstants::c_MaxAssetPathTotalSize] = {};
		double limit = 500.0;
		GetString(args, "type", type, sizeof(type));
		GetString(args, "folder", folder, sizeof(folder));
		GetNumber(args, "limit", limit);
		if (HasArgumentError())
		{
			return ToolResult::Failed;
		}

		const size_t folderLength = strlen(folder);
		uint total = 0;
		m_Result.BeginObject();
		m_Result.Key("assets");
		m_Result.BeginArray();
		for (const std::pair<const AssetID&, const RegAssetData&> asset : AssetRegistry::Instance().GetAssets())
		{
			const char* path = asset.second.filePath.CStr();
			const char* extension = strrchr(path, '.');
			const char* assetType = extension ? extension + 1 : "";
			if ((type[0] != '\0' && strcmp(assetType, type) != 0) || strncmp(path, folder, folderLength) != 0)
			{
				continue;
			}
			if (total++ < (uint)limit)
			{
				m_Result.BeginObject();
				m_Result.KeyString("path", path);
				m_Result.KeyString("type", assetType);
				m_Result.EndObject();
			}
		}
		m_Result.EndArray();
		m_Result.KeyUInt("total", total);
		m_Result.KeyBool("truncated", total > (uint)limit);
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::ImportModel(const dom::object& args)
	{
		char sourcePath[PathConstants::c_MaxPathTotalSize];
		char outputFolder[PathConstants::c_MaxRelativePathTotalSize];
		char name[PathConstants::c_MaxFileNameTotalSize];
		const bool hasSource = GetString(args, "sourcePath", sourcePath, sizeof(sourcePath));
		const bool hasFolder = GetString(args, "outputFolder", outputFolder, sizeof(outputFolder));
		const bool hasName = GetString(args, "name", name, sizeof(name));
		ModelImportOptions options;
		double lodCount = (double)options.lodCount;
		GetNumber(args, "lodCount", lodCount);
		dom::element generateLods;
		if (!args["generateLods"].get(generateLods))
		{
			generateLods.get_bool().get(options.generateLods);
		}
		if (HasArgumentError())
		{
			return ToolResult::Failed;
		}
		if (!hasSource || !hasFolder || !hasName)
		{
			return Fail("sourcePath, outputFolder and name are needed.");
		}
		if (!Platform::FileExists(sourcePath))
		{
			return Fail("There's no file at %s.", sourcePath);
		}
		options.lodCount = (uint)Math::Clamp(lodCount, 0.0, (double)(MeshConstants::c_MaxLods - 1));

		const bool imported = ModelImporter::Instance().ImportModel(sourcePath, outputFolder, name, options);
		if (imported)
		{
			m_Editor.RefreshAssets();
		}
		if (!imported)
		{
			return Fail("The import failed. read_log has the details.");
		}
		// Saved straight away, the same as an import from the File menu.
		AssetRegistry::Instance().Save();

		m_Result.BeginObject();
		m_Result.Key("assets");
		m_Result.BeginArray();
		const size_t folderLength = strlen(outputFolder);
		for (const std::pair<const AssetID&, const RegAssetData&> asset : AssetRegistry::Instance().GetAssets())
		{
			const char* path = asset.second.filePath.CStr();
			if (strncmp(path, outputFolder, folderLength) == 0)
			{
				m_Result.String(path);
			}
		}
		m_Result.EndArray();
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::ListActorTypes(const dom::object& args)
	{
		m_Result.BeginObject();
		m_Result.Key("types");
		m_Result.BeginArray();
		for (const std::pair<const Id64&, const ActorTypeDesc&> type : ActorRegistry::Instance().GetAll())
		{
			m_Result.String(type.second.name.CStr());
		}
		m_Result.EndArray();
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::ListActors(const dom::object& args)
	{
		World* world = GetWorld(args);
		char folder[PathConstants::c_MaxRelativePathTotalSize] = {};
		GetString(args, "folder", folder, sizeof(folder));
		if (!world || HasArgumentError())
		{
			return ToolResult::Failed;
		}

		const size_t folderLength = strlen(folder);
		m_Result.BeginObject();
		m_Result.Key("actors");
		m_Result.BeginArray();
		for (const ActorInstance& actor : world->actorInstances)
		{
			if (strncmp(actor.folderPath.CStr(), folder, folderLength) != 0)
			{
				continue;
			}
			m_Result.BeginObject();
			WriteActorSummary(*world, actor);
			m_Result.EndObject();
		}
		m_Result.EndArray();
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::GetActor(const dom::object& args)
	{
		char name[ActorName::c_Capacity + 1];
		const bool hasName = GetString(args, "name", name, sizeof(name));
		World* world = GetWorld(args);
		if (!world || HasArgumentError())
		{
			return ToolResult::Failed;
		}
		if (!hasName)
		{
			return Fail("name is needed.");
		}
		const ActorInstance* actor = FindActor(*world, name);
		if (!actor)
		{
			return ToolResult::Failed;
		}

		m_Result.BeginObject();
		WriteActorSummary(*world, *actor);
		m_Result.Key("entities");
		m_Result.BeginArray();
		for (Entity entity : actor->entities)
		{
			m_Result.BeginObject();
			m_Result.Key("components");
			m_Result.BeginObject();
			world->entities.ForEachComponentType(entity, [this, world, entity](ComponentTypeID typeID)
			{
				const TypeInfo* typeInfo = TypeRegistry::Instance().FindType(ComponentRegistry::Instance().GetReflectionTypeID(typeID));
				if (typeInfo)
				{
					m_Result.Key(typeInfo->name.CStr());
					McpReflection::WriteObject(m_Result, world->entities.GetComponentData(entity, typeID), *typeInfo);
				}
			});
			m_Result.EndObject();
			m_Result.EndObject();
		}
		m_Result.EndArray();
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::SpawnActor(const dom::object& args)
	{
		char typeName[64] = {};
		char meshPath[PathConstants::c_MaxAssetPathTotalSize] = {};
		char animationPath[PathConstants::c_MaxAssetPathTotalSize] = {};
		char name[ActorName::c_Capacity + 1] = {};
		char folder[PathConstants::c_MaxRelativePathTotalSize] = {};
		const bool hasType = GetString(args, "type", typeName, sizeof(typeName));
		const bool hasMesh = GetString(args, "mesh", meshPath, sizeof(meshPath));
		const bool hasAnimation = GetString(args, "animation", animationPath, sizeof(animationPath));
		const bool hasName = GetString(args, "name", name, sizeof(name));
		GetString(args, "folder", folder, sizeof(folder));
		Vector3 position;
		Quaternion rotation;
		Vector3 scale;
		const bool hasPosition = GetVector3(args, "position", position);
		const bool hasRotation = GetRotation(args, "rotation", rotation);
		const bool hasScale = GetVector3(args, "scale", scale);
		if (HasArgumentError() || !RequireEditing())
		{
			return ToolResult::Failed;
		}

		const AssetRegistry& registry = AssetRegistry::Instance();
		AssetID mesh;
		bool skinned = false;
		if (hasMesh)
		{
			mesh = registry.GetAssetIDSafe(meshPath);
			skinned = AssetUtil::HasExtension(meshPath, AssetConstants::c_SkeletalMeshFileExtension);
			if (!AssetUtil::IsValidAssetID(mesh) || !(skinned || AssetUtil::HasExtension(meshPath, AssetConstants::c_MeshFileExtension)))
			{
				return Fail("There's no mesh at %s. list_assets with type mesh or skmesh lists them.", meshPath);
			}
		}
		AssetID animation;
		if (hasAnimation)
		{
			animation = registry.GetAssetIDSafe(animationPath);
			if (!AssetUtil::IsValidAssetID(animation) || !AssetUtil::HasExtension(animationPath, AssetConstants::c_AnimationFileExtension))
			{
				return Fail("There's no animation at %s.", animationPath);
			}
		}

		if (!hasType)
		{
			if (!hasMesh)
			{
				return Fail("Give a type or a mesh.");
			}
			snprintf(typeName, sizeof(typeName), "%s", skinned ? "SkeletalMeshActor" : "StaticMeshActor");
		}
		const ActorTypeDesc* type = FindActorTypeByName(typeName);
		if (!type)
		{
			return Fail("There's no actor type %s. list_actor_types lists them.", typeName);
		}

		const Handle levelWorld = m_Editor.m_LevelEditorWorld;
		WorldManager& worldManager = *m_Editor.m_WorldManager;
		if (!worldManager.HasRoomForActorLights(levelWorld, *type))
		{
			return Fail("The level already has as many of this actor's lights or decals as the renderer supports.");
		}

		// Named after the mesh, like one dropped from the asset browser.
		if (!hasName)
		{
			if (hasMesh)
			{
				PathUtil::GetFileNameWithoutExtension(meshPath, name);
			}
			else
			{
				snprintf(name, sizeof(name), "%s", type->name.CStr());
			}
		}

		const Entity root = m_Editor.AddActor(*type, name, folder);
		if (root == c_InvalidEntity)
		{
			return Fail("The actor couldn't be made.");
		}
		World& world = worldManager.GetWorld(levelWorld);

		if (hasMesh)
		{
			bool set = false;
			if (skinned && world.entities.HasComponent<SkeletalMeshComponent>(root))
			{
				SkeletalMeshComponent component = world.entities.GetComponent<SkeletalMeshComponent>(root);
				component.mesh = mesh;
				set = worldManager.SetComponentData(levelWorld, root, ComponentRegistry::GetComponentTypeID<SkeletalMeshComponent>(), &component);
			}
			else if (!skinned && world.entities.HasComponent<MeshComponent>(root))
			{
				MeshComponent component = world.entities.GetComponent<MeshComponent>(root);
				component.mesh = mesh;
				set = worldManager.SetComponentData(levelWorld, root, ComponentRegistry::GetComponentTypeID<MeshComponent>(), &component);
			}
			if (!set)
			{
				worldManager.RemoveActorInstance(levelWorld, root);
				return Fail("A %s can't show that mesh.", type->name.CStr());
			}
		}
		if (hasAnimation && world.entities.HasComponent<AnimatorComponent>(root))
		{
			AnimatorComponent animator = world.entities.GetComponent<AnimatorComponent>(root);
			animator.clip = animation;
			worldManager.SetComponentData(levelWorld, root, ComponentRegistry::GetComponentTypeID<AnimatorComponent>(), &animator);
		}

		// In front of the camera unless placed, as the actor picker does.
		if (world.entities.HasComponent<ComponentTransform>(root))
		{
			Transform transform = world.entities.GetComponent<ComponentTransform>(root).local;
			transform.position = hasPosition ? position : m_Editor.GetBackgroundDropPosition(m_Editor.GetCameraRay());
			transform.rotation = hasRotation ? rotation : transform.rotation;
			transform.scale = hasScale ? scale : transform.scale;
			worldManager.SetActorTransform(levelWorld, root, transform);
		}
		m_Editor.m_SelectedActor = root;

		m_Result.BeginObject();
		for (const ActorInstance& actor : world.actorInstances)
		{
			if (actor.RootEntity() == root)
			{
				WriteActorSummary(world, actor);
			}
		}
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::DeleteActor(const dom::object& args)
	{
		char name[ActorName::c_Capacity + 1];
		const bool hasName = GetString(args, "name", name, sizeof(name));
		if (HasArgumentError() || !RequireEditing())
		{
			return ToolResult::Failed;
		}
		if (!hasName)
		{
			return Fail("name is needed.");
		}

		World& world = m_Editor.m_WorldManager->GetWorld(m_Editor.m_LevelEditorWorld);
		const ActorInstance* actor = FindActor(world, name);
		if (!actor)
		{
			return ToolResult::Failed;
		}
		const Entity root = actor->RootEntity();
		if (m_Editor.m_SelectedActor == root)
		{
			m_Editor.m_SelectedActor = c_InvalidEntity;
		}
		m_Editor.m_WorldManager->RemoveActorInstance(m_Editor.m_LevelEditorWorld, root);

		m_Result.BeginObject();
		m_Result.KeyString("deleted", name);
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::SetActorTransform(const dom::object& args)
	{
		char name[ActorName::c_Capacity + 1];
		const bool hasName = GetString(args, "name", name, sizeof(name));
		Vector3 position;
		Quaternion rotation;
		Vector3 scale;
		const bool hasPosition = GetVector3(args, "position", position);
		const bool hasRotation = GetRotation(args, "rotation", rotation);
		const bool hasScale = GetVector3(args, "scale", scale);
		if (HasArgumentError() || !RequireEditing())
		{
			return ToolResult::Failed;
		}
		if (!hasName)
		{
			return Fail("name is needed.");
		}

		World& world = m_Editor.m_WorldManager->GetWorld(m_Editor.m_LevelEditorWorld);
		const ActorInstance* actor = FindActor(world, name);
		if (!actor)
		{
			return ToolResult::Failed;
		}
		const Entity root = actor->RootEntity();
		if (!world.entities.HasComponent<ComponentTransform>(root))
		{
			return Fail("%s has no transform.", name);
		}

		Transform transform = world.entities.GetComponent<ComponentTransform>(root).local;
		transform.position = hasPosition ? position : transform.position;
		transform.rotation = hasRotation ? rotation : transform.rotation;
		transform.scale = hasScale ? scale : transform.scale;
		m_Editor.m_WorldManager->SetActorTransform(m_Editor.m_LevelEditorWorld, root, transform);

		m_Result.BeginObject();
		WriteActorSummary(world, *actor);
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::SetComponent(const dom::object& args)
	{
		char actorName[ActorName::c_Capacity + 1];
		char componentName[64];
		const bool hasActor = GetString(args, "actor", actorName, sizeof(actorName));
		const bool hasComponent = GetString(args, "component", componentName, sizeof(componentName));
		double entityIndex = 0.0;
		GetNumber(args, "entity", entityIndex);
		dom::object values;
		const bool hasValues = !args["values"].get_object().get(values);
		if (HasArgumentError() || !RequireEditing())
		{
			return ToolResult::Failed;
		}
		if (!hasActor || !hasComponent || !hasValues)
		{
			return Fail("actor, component and values (an object) are needed.");
		}

		World& world = m_Editor.m_WorldManager->GetWorld(m_Editor.m_LevelEditorWorld);
		const ActorInstance* actor = FindActor(world, actorName);
		if (!actor)
		{
			return ToolResult::Failed;
		}
		if (entityIndex < 0.0 || (uint)entityIndex >= actor->entities.Size())
		{
			return Fail("%s has %u entities.", actorName, actor->entities.Size());
		}
		const Entity entity = actor->entities[(uint)entityIndex];

		ComponentTypeID componentType = 0;
		const TypeInfo* typeInfo = nullptr;
		world.entities.ForEachComponentType(entity, [&componentType, &typeInfo, &componentName](ComponentTypeID typeID)
		{
			const TypeInfo* candidate = TypeRegistry::Instance().FindType(ComponentRegistry::Instance().GetReflectionTypeID(typeID));
			if (candidate && candidate->name == componentName)
			{
				componentType = typeID;
				typeInfo = candidate;
			}
		});
		if (!typeInfo)
		{
			return Fail("%s's entity %u has no %s. get_actor shows its components.", actorName, (uint)entityIndex, componentName);
		}
		TYR_ASSERT(typeInfo->size <= c_MaxActorComponentDataSize);

		// Edited as a copy and handed over whole, the same as the Properties panel does.
		alignas(16) uint8 component[c_MaxActorComponentDataSize];
		memcpy(component, world.entities.GetComponentData(entity, componentType), typeInfo->size);
		if (!McpReflection::ReadObject(values, component, *typeInfo, m_Error, sizeof(m_Error)))
		{
			return ToolResult::Failed;
		}
		if (!m_Editor.m_WorldManager->SetComponentData(m_Editor.m_LevelEditorWorld, entity, componentType, component))
		{
			return Fail("The change wasn't accepted, such as a mesh that's still loading or an asset of the wrong type. read_log may say why.");
		}

		m_Result.BeginObject();
		m_Result.Key(typeInfo->name.CStr());
		McpReflection::WriteObject(m_Result, world.entities.GetComponentData(entity, componentType), *typeInfo);
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::SelectActor(const dom::object& args)
	{
		char name[ActorName::c_Capacity + 1];
		const bool hasName = GetString(args, "name", name, sizeof(name));
		if (HasArgumentError())
		{
			return ToolResult::Failed;
		}

		Entity selected = c_InvalidEntity;
		if (hasName)
		{
			World& world = m_Editor.m_WorldManager->GetWorld(m_Editor.m_LevelEditorWorld);
			const ActorInstance* actor = FindActor(world, name);
			if (!actor)
			{
				return ToolResult::Failed;
			}
			selected = actor->RootEntity();
		}
		m_Editor.m_SelectedActor = selected;

		m_Result.BeginObject();
		m_Result.Key("selectedActor");
		if (hasName)
		{
			m_Result.String(name);
		}
		else
		{
			m_Result.Null();
		}
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::OpenLevel(const dom::object& args)
	{
		char path[PathConstants::c_MaxAssetPathTotalSize];
		const bool hasPath = GetString(args, "path", path, sizeof(path));
		dom::element discardElement;
		bool discard = false;
		if (!args["discardChanges"].get(discardElement))
		{
			discardElement.get_bool().get(discard);
		}
		if (HasArgumentError() || !RequireEditing())
		{
			return ToolResult::Failed;
		}
		if (!hasPath)
		{
			return Fail("path is needed.");
		}

		const AssetID level = AssetRegistry::Instance().GetAssetIDSafe(path);
		if (!AssetUtil::IsValidAssetID(level) || !AssetUtil::HasExtension(path, AssetConstants::c_LevelFileExtension))
		{
			return Fail("There's no level at %s. list_assets with type level lists them.", path);
		}
		if (m_Editor.IsLevelDirty() && !discard)
		{
			return Fail("The open level has unsaved changes. Save it first, or pass discardChanges: true.");
		}
		m_Editor.OpenLevel(level);
		return GetEditorState(args);
	}

	McpTools::ToolResult McpTools::SaveLevel(const dom::object& args)
	{
		char name[PathConstants::c_MaxFileNameTotalSize];
		const bool hasName = GetString(args, "name", name, sizeof(name));
		if (HasArgumentError() || !RequireEditing())
		{
			return ToolResult::Failed;
		}

		if (hasName)
		{
			m_Editor.SaveLevelAs(name);
		}
		else if (AssetUtil::IsValidAssetID(m_Editor.m_Level))
		{
			m_Editor.SaveLevel();
		}
		else
		{
			return Fail("No level is open. Give a name to save it as.");
		}
		if (m_Editor.IsLevelDirty())
		{
			return Fail("The level couldn't be saved. read_log may say why.");
		}
		return GetEditorState(args);
	}

	McpTools::ToolResult McpTools::NewLevel(const dom::object& args)
	{
		char name[PathConstants::c_MaxFileNameTotalSize];
		const bool hasName = GetString(args, "name", name, sizeof(name));
		dom::element discardElement;
		bool discard = false;
		if (!args["discardChanges"].get(discardElement))
		{
			discardElement.get_bool().get(discard);
		}
		if (HasArgumentError() || !RequireEditing())
		{
			return ToolResult::Failed;
		}
		if (!hasName)
		{
			return Fail("name is needed.");
		}
		if (m_Editor.IsLevelDirty() && !discard)
		{
			return Fail("The open level has unsaved changes. Save it first, or pass discardChanges: true.");
		}
		m_Editor.NewLevel(name);
		return GetEditorState(args);
	}

	McpTools::ToolResult McpTools::Play(const dom::object& args)
	{
		if (m_Editor.m_PlayState != PlayState::Playing)
		{
			m_Editor.SetPlayState(PlayState::Playing);
		}
		return GetEditorState(args);
	}

	McpTools::ToolResult McpTools::Stop(const dom::object& args)
	{
		if (m_Editor.m_PlayState != PlayState::Editing)
		{
			m_Editor.SetPlayState(PlayState::Editing);
		}
		return GetEditorState(args);
	}

	Entity McpTools::FindGamePlayer(World*& outWorld, Handle& outWorldHandle)
	{
		if (m_Editor.m_PlayState == PlayState::Editing)
		{
			Fail("The game isn't running.");
			return c_InvalidEntity;
		}
		outWorldHandle = m_Editor.m_WorldManager->GetActiveWorld();
		outWorld = &m_Editor.m_WorldManager->GetWorld(outWorldHandle);
		for (const ActorInstance& actor : outWorld->actorInstances)
		{
			if (outWorld->entities.HasComponent<InventoryComponent>(actor.RootEntity()))
			{
				return actor.RootEntity();
			}
		}
		Fail("The game has no player with an inventory.");
		return c_InvalidEntity;
	}

	McpTools::ToolResult McpTools::Interact(const dom::object& args)
	{
		char name[ActorName::c_Capacity + 1];
		if (!GetString(args, "actor", name, sizeof(name)))
		{
			return Fail("actor is needed.");
		}
		World* world = nullptr;
		Handle worldHandle;
		const Entity player = FindGamePlayer(world, worldHandle);
		if (player == c_InvalidEntity)
		{
			return ToolResult::Failed;
		}
		const ActorInstance* actor = FindActor(*world, name);
		if (!actor)
		{
			return ToolResult::Failed;
		}
		Entity target = c_InvalidEntity;
		for (Entity entity : actor->entities)
		{
			target = target == c_InvalidEntity ? Interaction::FindUsable(*world, entity) : target;
		}
		if (target == c_InvalidEntity)
		{
			return Fail("%s isn't something that can be used.", name);
		}

		InteractionText message;
		const bool used = Interaction::Use(*m_Editor.m_WorldManager, worldHandle, target, player, message);
		m_Result.BeginObject();
		m_Result.Key("used");
		m_Result.Bool(used);
		m_Result.Key("message");
		m_Result.String(message.CStr());
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::GiveItem(const dom::object& args)
	{
		char item[ItemName::c_Capacity + 1];
		if (!GetString(args, "item", item, sizeof(item)) || item[0] == 0 || strlen(item) > c_MaxItemName)
		{
			return Fail("item is needed, at most %u characters.", c_MaxItemName);
		}
		World* world = nullptr;
		Handle worldHandle;
		const Entity player = FindGamePlayer(world, worldHandle);
		if (player == c_InvalidEntity)
		{
			return ToolResult::Failed;
		}
		InventoryComponent& inventory = world->entities.GetComponent<InventoryComponent>(player);
		if (inventory.items.Size() >= GameplayConstants::c_MaxInventoryItems)
		{
			return Fail("The player's inventory is full.");
		}
		inventory.items.Add(ItemName(item));
		m_Result.BeginObject();
		m_Result.Key("items");
		m_Result.BeginArray();
		for (const ItemName& held : inventory.items)
		{
			m_Result.String(held.CStr());
		}
		m_Result.EndArray();
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::BuildNavmesh(const dom::object& /*args*/)
	{
		if (!RequireEditing())
		{
			return ToolResult::Failed;
		}
		if (!m_Editor.BuildNavmesh())
		{
			return Fail("The level has nothing to walk on, so it has no navmesh.");
		}
		m_Result.BeginObject();
		m_Result.KeyBool("built", true);
		m_Result.KeyUInt("bytes", m_Editor.m_WorldManager->GetWorld(m_Editor.m_LevelEditorWorld).navmeshData.Size());
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::FindPath(const dom::object& args)
	{
		Vector3 from;
		Vector3 to;
		GetVector3(args, "from", from);
		GetVector3(args, "to", to);
		if (HasArgumentError())
		{
			return ToolResult::Failed;
		}
		const World& world = m_Editor.m_WorldManager->GetWorld(m_Editor.m_WorldManager->GetActiveWorld());
		NavmeshComponentsManager& navmesh = m_Editor.m_WorldManager->GetNavmeshComponentsManager();
		if (!navmesh.HasNavmesh(world))
		{
			return Fail("The world has no navmesh. Build one with build_navmesh.");
		}
		NavPath path;
		if (!navmesh.FindPath(world, from, to, path))
		{
			return Fail("There's no navmesh near the start or end.");
		}
		m_Result.BeginObject();
		m_Result.KeyBool("partial", path.partial);
		m_Result.Key("points");
		m_Result.BeginArray();
		for (const Vector3& point : path.points)
		{
			m_Result.BeginArray();
			m_Result.Number(point.x);
			m_Result.Number(point.y);
			m_Result.Number(point.z);
			m_Result.EndArray();
		}
		m_Result.EndArray();
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::Undo(const dom::object& args)
	{
		return UndoSteps(args, false);
	}

	McpTools::ToolResult McpTools::Redo(const dom::object& args)
	{
		return UndoSteps(args, true);
	}

	McpTools::ToolResult McpTools::UndoSteps(const dom::object& args, bool redo)
	{
		double steps = 1.0;
		GetNumber(args, "steps", steps);
		if (HasArgumentError() || !RequireEditing())
		{
			return ToolResult::Failed;
		}
		if (steps < 1.0)
		{
			return Fail("steps must be at least 1.");
		}

		uint done = 0;
		while (done < static_cast<uint>(steps) && m_Editor.ApplyUndo(redo))
		{
			++done;
		}
		if (done == 0)
		{
			return Fail(redo ? "There's nothing to redo." : "There's nothing to undo.");
		}

		m_Result.BeginObject();
		m_Result.KeyUInt(redo ? "redone" : "undone", done);
		m_Result.KeyBool("canUndo", m_Editor.m_UndoHistory->CanUndo());
		m_Result.KeyBool("canRedo", m_Editor.m_UndoHistory->CanRedo());
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::CaptureScreenshot(const dom::object& args)
	{
		double maxWidth = c_DefaultCaptureWidth;
		GetNumber(args, "maxWidth", maxWidth);
		if (HasArgumentError())
		{
			return ToolResult::Failed;
		}
		if (m_PendingCaptures.Size() == c_MaxPendingCaptures)
		{
			return Fail("Too many screenshots are still being taken.");
		}

		PendingCapture& capture = m_PendingCaptures.ExpandOne();
		capture.connection = m_CurrentConnection;
		capture.id = *m_CurrentID;
		capture.captureID = m_Editor.m_RendererAPI->RequestViewportCapture();
		capture.maxWidth = (uint)Math::Clamp(maxWidth, 64.0, 4096.0);
		capture.framesWaited = 0;
		return ToolResult::Pending;
	}

	void McpTools::UpdateCaptures(McpServer& server)
	{
		for (uint i = m_PendingCaptures.Size(); i-- > 0;)
		{
			PendingCapture& capture = m_PendingCaptures[i];
			uint width = 0;
			uint height = 0;
			const ViewportCaptureStatus status = m_Editor.m_RendererAPI->TakeViewportCapture(capture.captureID, m_CapturePixels, width, height);
			if (status == ViewportCaptureStatus::Waiting && ++capture.framesWaited < c_MaxCaptureFrames)
			{
				continue;
			}

			if (status != ViewportCaptureStatus::Ready)
			{
				Fail("Nothing was drawn to capture. The viewport may not be visible yet.");
				ReplyToolResult(server, capture.connection, capture.id, true);
				m_PendingCaptures.SwapAndPopBack(i);
				continue;
			}

			EncodeCapture(width, height, capture.maxWidth);

			m_Reply.Clear();
			m_Reply.BeginObject();
			m_Reply.KeyString("jsonrpc", "2.0");
			m_Reply.Key("id");
			WriteID(&capture.id);
			m_Reply.Key("result");
			m_Reply.BeginObject();
			m_Reply.Key("content");
			m_Reply.BeginArray();
			m_Reply.BeginObject();
			m_Reply.KeyString("type", "image");
			m_Reply.Key("data");
			m_Reply.BeginRawString();
			m_Reply.AppendBase64(m_Png.Data(), m_Png.Size());
			m_Reply.EndRawString();
			m_Reply.KeyString("mimeType", "image/png");
			m_Reply.EndObject();
			m_Reply.BeginObject();
			m_Reply.KeyString("type", "text");
			char description[128];
			snprintf(description, sizeof(description), "Viewport capture, %ux%u pixels.", width, height);
			m_Reply.KeyString("text", description);
			m_Reply.EndObject();
			m_Reply.EndArray();
			m_Reply.KeyBool("isError", false);
			m_Reply.EndObject();
			m_Reply.EndObject();
			server.Reply(capture.connection, m_Reply.Data(), m_Reply.Size());
			m_PendingCaptures.SwapAndPopBack(i);
		}
	}

	void McpTools::EncodeCapture(uint width, uint height, uint maxWidth)
	{
		// Averaged over each output pixel's area in linear colour, then converted back to sRGB.
		const uint outWidth = width > maxWidth ? maxWidth : width;
		const uint outHeight = width > maxWidth ? (uint)((uint64)height * maxWidth / width) : height;
		m_ImagePixels.Resize(outWidth * outHeight * 3);
		for (uint y = 0; y < outHeight; ++y)
		{
			const uint y0 = (uint)((uint64)y * height / outHeight);
			const uint y1 = std::max(y0 + 1, (uint)((uint64)(y + 1) * height / outHeight));
			for (uint x = 0; x < outWidth; ++x)
			{
				const uint x0 = (uint)((uint64)x * width / outWidth);
				const uint x1 = std::max(x0 + 1, (uint)((uint64)(x + 1) * width / outWidth));
				float sums[3] = {};
				for (uint sy = y0; sy < y1; ++sy)
				{
					const uint8* row = m_CapturePixels.Data() + ((size_t)sy * width + x0) * 4;
					for (uint sx = x0; sx < x1; ++sx, row += 4)
					{
						sums[0] += SRGBToLinear(row[0]);
						sums[1] += SRGBToLinear(row[1]);
						sums[2] += SRGBToLinear(row[2]);
					}
				}
				const float count = (float)((x1 - x0) * (y1 - y0));
				uint8* out = m_ImagePixels.Data() + ((size_t)y * outWidth + x) * 3;
				out[0] = LinearToSRGB(sums[0] / count);
				out[1] = LinearToSRGB(sums[1] / count);
				out[2] = LinearToSRGB(sums[2] / count);
			}
		}

		m_Png.Clear();
		m_PngEncoder.Encode(m_ImagePixels.Data(), outWidth, outHeight, 3, m_Png);
	}

	McpTools::ToolResult McpTools::ReadLog(const dom::object& args)
	{
		double afterSequence = 0.0;
		double maxEntries = 100.0;
		char minLevelName[16] = "info";
		GetNumber(args, "afterSequence", afterSequence);
		GetNumber(args, "maxEntries", maxEntries);
		GetString(args, "minLevel", minLevelName, sizeof(minLevelName));
		if (HasArgumentError())
		{
			return ToolResult::Failed;
		}
		const LogLevel minLevel = strcmp(minLevelName, "error") == 0 ? LogLevel::Error : (strcmp(minLevelName, "warning") == 0 ? LogLevel::Warning : LogLevel::Info);

		const uint maxCount = (uint)Math::Clamp(maxEntries, 1.0, (double)Logger::c_HistorySize);
		SmartStack<LogEntry> entriesStack = SmartStackAlloc<LogEntry>(Logger::c_HistorySize);
		LogEntry* entries = entriesStack;
		const uint count = Logger::Instance().GetRecentEntries((uint64)std::max(afterSequence, 0.0), entries, Logger::c_HistorySize);

		uint64 lastSequence = (uint64)std::max(afterSequence, 0.0);
		uint written = 0;
		m_Result.BeginObject();
		m_Result.Key("entries");
		m_Result.BeginArray();
		for (uint i = 0; i < count && written < maxCount; ++i)
		{
			lastSequence = entries[i].sequence;
			if (entries[i].level > minLevel)
			{
				continue;
			}
			m_Result.BeginObject();
			m_Result.KeyUInt("sequence", entries[i].sequence);
			m_Result.KeyString("level", GetLogLevelName(entries[i].level));
			m_Result.KeyString("text", entries[i].text);
			m_Result.EndObject();
			++written;
		}
		m_Result.EndArray();
		m_Result.KeyUInt("lastSequence", lastSequence);
		m_Result.EndObject();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::SetCamera(const dom::object& args)
	{
		Vector3 position;
		Vector3 target;
		const bool hasPosition = GetVector3(args, "position", position);
		const bool hasTarget = GetVector3(args, "target", target);
		if (HasArgumentError())
		{
			return ToolResult::Failed;
		}

		Camera& camera = *m_Editor.m_Camera;
		if (hasPosition)
		{
			camera.SetPosition(position);
		}
		if (hasTarget)
		{
			camera.Target(target);
		}
		return GetEditorState(args);
	}

	McpTools::ToolResult McpTools::FindLoadedMaterial(const dom::object& args, AssetID& outMaterial)
	{
		char path[PathConstants::c_MaxAssetPathTotalSize];
		const bool hasPath = GetString(args, "path", path, sizeof(path));
		if (HasArgumentError())
		{
			return ToolResult::Failed;
		}
		if (!hasPath)
		{
			return Fail("path is needed.");
		}

		outMaterial = AssetRegistry::Instance().GetAssetID(path);
		if (!AssetUtil::IsValidAssetID(outMaterial) || !AssetUtil::HasExtension(path, AssetConstants::c_MaterialFileExtension))
		{
			return Fail("There's no material at %s. list_assets with type mat lists them.", path);
		}
		if (m_Editor.m_AssetManager->GetMaterialFile(outMaterial))
		{
			return ToolResult::Done;
		}

		return WaitForMaterial(outMaterial);
	}

	McpTools::ToolResult McpTools::WaitForMaterial(AssetID material)
	{
		if (m_PendingMaterials.Size() == c_MaxPendingMaterials)
		{
			return Fail("Too many materials are still loading.");
		}
		PendingMaterial& pending = m_PendingMaterials.ExpandOne();
		pending.connection = m_CurrentConnection;
		pending.id = *m_CurrentID;
		pending.body = m_CurrentBody;
		pending.bodySize = m_CurrentBodySize;
		pending.material = material;
		pending.framesWaited = 0;
		m_Editor.m_AssetManager->LoadMaterial(material);
		return ToolResult::Pending;
	}

	void McpTools::ReleaseHeldLayerMaterial()
	{
		if (AssetUtil::IsValidAssetID(m_HeldLayerMaterial))
		{
			m_Editor.m_AssetManager->DeleteMaterial(m_HeldLayerMaterial);
			m_HeldLayerMaterial = {};
		}
	}

	void McpTools::WriteMaterial(AssetID material)
	{
		const MaterialAssetFile& file = *m_Editor.m_AssetManager->GetMaterialFile(material);
		const AssetRegistry& registry = AssetRegistry::Instance();
		m_Result.BeginObject();
		m_Result.KeyString("path", registry.GetAssetData(material).filePath.CStr());
		m_Result.Key("textures");
		m_Result.BeginArray();
		for (AssetID texture : file.textures)
		{
			m_Result.String(registry.GetAssetData(texture).filePath.CStr());
		}
		m_Result.EndArray();
		m_Result.Key("parameters");
		McpReflection::WriteObject(m_Result, &file.params, TypeRegistry::Instance().GetType(GetTypeID<MaterialParams>()));
		m_Result.EndObject();
	}

	McpTools::ToolResult McpTools::GetMaterial(const dom::object& args)
	{
		AssetID material;
		const ToolResult found = FindLoadedMaterial(args, material);
		if (found != ToolResult::Done)
		{
			return found;
		}
		WriteMaterial(material);
		return ToolResult::Done;
	}

	void McpTools::WriteLevelSettings()
	{
		const World& level = m_Editor.m_WorldManager->GetWorld(m_Editor.m_LevelEditorWorld);
		McpReflection::WriteObject(m_Result, &level.settings, TypeRegistry::Instance().GetType(GetTypeID<LevelSettings>()));
	}

	McpTools::ToolResult McpTools::GetLevelSettings(const dom::object& /*args*/)
	{
		WriteLevelSettings();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::SetLevelSettings(const dom::object& args)
	{
		dom::object values;
		if (args["values"].get_object().get(values))
		{
			return Fail("values (an object) is needed.");
		}
		if (!RequireEditing())
		{
			return ToolResult::Failed;
		}

		// Edited as a copy, so a bad value leaves the settings as they were.
		LevelSettings settings = m_Editor.m_WorldManager->GetWorld(m_Editor.m_LevelEditorWorld).settings;
		if (!McpReflection::ReadObject(values, &settings, TypeRegistry::Instance().GetType(GetTypeID<LevelSettings>()), m_Error, sizeof(m_Error)))
		{
			return ToolResult::Failed;
		}
		m_Editor.m_WorldManager->SetLevelSettings(m_Editor.m_LevelEditorWorld, settings);
		WriteLevelSettings();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::ImportSky(const dom::object& args)
	{
		char sourcePath[PathConstants::c_MaxPathTotalSize];
		char outputFolder[PathConstants::c_MaxRelativePathTotalSize];
		char name[PathConstants::c_MaxFileNameTotalSize];
		const bool hasSource = GetString(args, "sourcePath", sourcePath, sizeof(sourcePath));
		const bool hasFolder = GetString(args, "outputFolder", outputFolder, sizeof(outputFolder));
		const bool hasName = GetString(args, "name", name, sizeof(name));
		double faceSize = 0.0;
		GetNumber(args, "faceSize", faceSize);
		if (HasArgumentError())
		{
			return ToolResult::Failed;
		}
		if (!hasSource || !hasFolder || !hasName)
		{
			return Fail("sourcePath, outputFolder and name are needed.");
		}
		if (!Platform::FileExists(sourcePath))
		{
			return Fail("There's no file at %s.", sourcePath);
		}

		AssetID textureID;
		const bool importedSky = MaterialImporter::Instance().ImportSkyCubemap(sourcePath, outputFolder, name, (uint)Math::Clamp(faceSize, 0.0, 4096.0), textureID);
		if (importedSky)
		{
			m_Editor.RefreshAssets();
		}
		if (!importedSky)
		{
			return Fail("The import failed. read_log has the details.");
		}
		AssetRegistry::Instance().Save();

		m_Result.BeginObject();
		m_Result.KeyString("path", AssetRegistry::Instance().GetAssetData(textureID).filePath.CStr());
		m_Result.EndObject();
		return ToolResult::Done;
	}

	void McpTools::WriteDisplay()
	{
		const DisplaySettings& settings = m_Editor.m_RendererAPI->GetDisplaySettings();
		m_Result.BeginObject();
		m_Result.KeyBool("hdr", settings.hdrEnabled);
		m_Result.KeyNumber("paperWhiteNits", settings.paperWhiteNits);
		m_Result.KeyNumber("uiNits", settings.uiNits);
		m_Result.KeyNumber("peakNits", settings.peakNits);
		DisplayHDRInfo info;
		DisplayOutput output;
		if (m_Editor.m_RendererAPI->GetActiveDisplay(info, output))
		{
			m_Result.KeyString("output", output == DisplayOutput::HDR10 ? "HDR10" : "SDR");
			m_Result.KeyBool("displayHdrOn", info.osHdrEnabled);
			m_Result.KeyBool("hdrSupported", info.hdrSupported);
			m_Result.KeyNumber("displayPeakNits", info.maxLuminance);
			m_Result.KeyNumber("displayFullFrameNits", info.maxFullFrameLuminance);
			m_Result.KeyNumber("displayMinNits", info.minLuminance);
		}
		m_Result.EndObject();
	}

	McpTools::ToolResult McpTools::GetDisplay(const dom::object& /*args*/)
	{
		WriteDisplay();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::SetDisplay(const dom::object& args)
	{
		DisplaySettings settings = m_Editor.m_RendererAPI->GetDisplaySettings();
		dom::element hdr;
		if (!args["hdr"].get(hdr))
		{
			hdr.get_bool().get(settings.hdrEnabled);
		}
		double paperWhite = settings.paperWhiteNits;
		double ui = settings.uiNits;
		double peak = settings.peakNits;
		GetNumber(args, "paperWhiteNits", paperWhite);
		GetNumber(args, "uiNits", ui);
		GetNumber(args, "peakNits", peak);
		if (HasArgumentError())
		{
			return ToolResult::Failed;
		}
		settings.paperWhiteNits = (float)Math::Clamp(paperWhite, 1.0, 10000.0);
		settings.uiNits = (float)Math::Clamp(ui, 1.0, 10000.0);
		settings.peakNits = (float)Math::Clamp(peak, 0.0, 10000.0);
		m_Editor.m_RendererAPI->SetDisplaySettings(settings);
		WriteDisplay();
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::SetMaterialLayer(const dom::object& args)
	{
		char layerPath[PathConstants::c_MaxAssetPathTotalSize] = {};
		if (!GetString(args, "layerMaterial", layerPath, sizeof(layerPath)) && !HasArgumentError())
		{
			return Fail("layerMaterial is needed.");
		}
		AssetID layer;
		if (layerPath[0] != 0)
		{
			layer = AssetRegistry::Instance().GetAssetID(layerPath);
			if (!AssetUtil::IsValidAssetID(layer) || !AssetUtil::HasExtension(layerPath, AssetConstants::c_MaterialFileExtension))
			{
				return Fail("There's no material at %s.", layerPath);
			}
			// Held loaded while waiting for the material itself, which runs this again.
			if (layer != m_HeldLayerMaterial)
			{
				ReleaseHeldLayerMaterial();
				m_Editor.m_AssetManager->LoadMaterial(layer);
				m_HeldLayerMaterial = layer;
			}
		}

		AssetID material;
		ToolResult found = FindLoadedMaterial(args, material);
		if (found == ToolResult::Done && AssetUtil::IsValidAssetID(layer) && !m_Editor.m_AssetManager->GetMaterialFile(layer))
		{
			found = WaitForMaterial(layer);
		}
		if (found != ToolResult::Done)
		{
			return found;
		}

		const bool set = m_Editor.m_AssetManager->SetMaterialLayer(material, layer);
		ReleaseHeldLayerMaterial();
		if (!set)
		{
			return Fail("%s doesn't have the three textures a layer needs.", layerPath);
		}
		WriteMaterial(material);
		return ToolResult::Done;
	}

	McpTools::ToolResult McpTools::SetMaterial(const dom::object& args)
	{
		dom::object values;
		if (args["values"].get_object().get(values))
		{
			return Fail("values (an object) is needed.");
		}
		AssetID material;
		const ToolResult found = FindLoadedMaterial(args, material);
		if (found != ToolResult::Done)
		{
			return found;
		}

		// Edited as a copy, so a bad value leaves the material as it was.
		MaterialParams params = m_Editor.m_AssetManager->GetMaterialFile(material)->params;
		if (!McpReflection::ReadObject(values, &params, TypeRegistry::Instance().GetType(GetTypeID<MaterialParams>()), m_Error, sizeof(m_Error)))
		{
			return ToolResult::Failed;
		}
		m_Editor.m_AssetManager->SetMaterialParams(material, params);
		m_Editor.m_AssetManager->SaveMaterial(material);
		WriteMaterial(material);
		return ToolResult::Done;
	}
}
