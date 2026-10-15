#pragma once

#include "Core.h"
#include "McpJson.h"
#include "PngEncoder.h"
#include "ECS/Components.h"
#include "simdjson.h"

namespace tyr
{
	class Editor;
	class McpServer;
	struct World;
	struct ActorInstance;

	// Answers the MCP JSON-RPC messages the server receives, on the main thread. Its tools go
	// through the same editor functions as the UI, so the two can't drift apart.
	class McpTools final
	{
	public:
		explicit McpTools(Editor& editor);

		// Answers every message waiting on the server, and finishes tool calls waiting on the renderer.
		void Update(McpServer& server);

	private:
		// A JSON-RPC request's ID, echoed back in its reply.
		struct RequestID
		{
			enum class Kind : uint8
			{
				Null,
				Number,
				String
			};

			Kind kind = Kind::Null;
			int64 number = 0;
			char text[64] = {};
		};

		enum class ToolResult : uint8
		{
			Done,
			Failed,
			// Replied to later, from Update.
			Pending
		};

		using ToolFunction = ToolResult (McpTools::*)(const simdjson::dom::object& args);

		struct Tool
		{
			const char* name;
			const char* description;
			// A JSON schema object for the arguments.
			const char* inputSchema;
			ToolFunction function;
		};

		// A screenshot waiting for the GPU to finish the frame it was taken in.
		struct PendingCapture
		{
			uint connection;
			RequestID id;
			uint captureID;
			uint maxWidth;
			uint framesWaited;
		};

		// A material tool waiting for its material to load, then run again from its request. The
		// request's body stays in the server's buffer until it's replied to.
		struct PendingMaterial
		{
			uint connection;
			RequestID id;
			const char* body;
			uint bodySize;
			AssetID material;
			uint framesWaited;
		};

		void HandleMessage(McpServer& server, uint connection, const char* body, uint bodySize);
		void ReplyResult(McpServer& server, uint connection, const RequestID& id);
		void ReplyToolResult(McpServer& server, uint connection, const RequestID& id, bool isError);
		void ReplyError(McpServer& server, uint connection, const RequestID* id, int code, const char* message);
		void WriteID(const RequestID* id);
		void UpdateCaptures(McpServer& server);
		void UpdatePendingMaterials(McpServer& server);
		// Finds the material at args' path. Pending until it's loaded, when the tool is run again.
		ToolResult FindLoadedMaterial(const simdjson::dom::object& args, AssetID& outMaterial);
		// Replies once the material has loaded, by running the request again.
		ToolResult WaitForMaterial(AssetID material);
		void ReleaseHeldLayerMaterial();
		void WriteMaterial(AssetID material);
		// Downscales and converts the capture to sRGB, then writes its PNG into m_Png.
		void EncodeCapture(uint width, uint height, uint maxWidth);

		ToolResult Fail(const char* format, ...);

		// Argument readers. Each returns false when the argument is missing, and also records an
		// error when it's there but the wrong kind, which the tool then fails with.
		bool GetString(const simdjson::dom::object& args, const char* key, char* out, size_t outSize);
		bool GetNumber(const simdjson::dom::object& args, const char* key, double& out);
		bool GetVector3(const simdjson::dom::object& args, const char* key, Vector3& out);
		bool GetRotation(const simdjson::dom::object& args, const char* key, Quaternion& out);
		bool HasArgumentError() const { return m_ArgumentError; }

		// The level world, or the game's while playing when args ask for "game". Null with an error otherwise.
		World* GetWorld(const simdjson::dom::object& args);
		ActorInstance* FindActor(World& world, const char* name);
		// Fails if the game is running, since the level can only be edited while it isn't.
		bool RequireEditing();
		void WriteTransform(const Transform& transform);
		void WriteActorSummary(World& world, const ActorInstance& actor);

		ToolResult GetEditorState(const simdjson::dom::object& args);
		ToolResult ListAssets(const simdjson::dom::object& args);
		ToolResult ImportModel(const simdjson::dom::object& args);
		ToolResult ListActorTypes(const simdjson::dom::object& args);
		ToolResult ListActors(const simdjson::dom::object& args);
		ToolResult GetActor(const simdjson::dom::object& args);
		ToolResult SetMaterialLayer(const simdjson::dom::object& args);
		ToolResult Interact(const simdjson::dom::object& args);
		ToolResult GiveItem(const simdjson::dom::object& args);
		// The running game's player, with the world it's in.
		Entity FindGamePlayer(World*& outWorld, Handle& outWorldHandle);
		ToolResult SpawnActor(const simdjson::dom::object& args);
		ToolResult DeleteActor(const simdjson::dom::object& args);
		ToolResult SetActorTransform(const simdjson::dom::object& args);
		ToolResult SetComponent(const simdjson::dom::object& args);
		ToolResult SelectActor(const simdjson::dom::object& args);
		ToolResult OpenLevel(const simdjson::dom::object& args);
		ToolResult SaveLevel(const simdjson::dom::object& args);
		ToolResult NewLevel(const simdjson::dom::object& args);
		ToolResult Play(const simdjson::dom::object& args);
		ToolResult Stop(const simdjson::dom::object& args);
		ToolResult Undo(const simdjson::dom::object& args);
		ToolResult BuildNavmesh(const simdjson::dom::object& args);
		ToolResult FindPath(const simdjson::dom::object& args);
		ToolResult Redo(const simdjson::dom::object& args);
		ToolResult UndoSteps(const simdjson::dom::object& args, bool redo);
		ToolResult CaptureScreenshot(const simdjson::dom::object& args);
		ToolResult ReadLog(const simdjson::dom::object& args);
		ToolResult SetCamera(const simdjson::dom::object& args);
		ToolResult GetMaterial(const simdjson::dom::object& args);
		ToolResult SetMaterial(const simdjson::dom::object& args);
		ToolResult GetLevelSettings(const simdjson::dom::object& args);
		ToolResult SetLevelSettings(const simdjson::dom::object& args);
		ToolResult ImportSky(const simdjson::dom::object& args);
		ToolResult GetDisplay(const simdjson::dom::object& args);
		ToolResult SetDisplay(const simdjson::dom::object& args);
		void WriteLevelSettings();
		void WriteDisplay();

		static const Tool c_Tools[];

		Editor& m_Editor;
		simdjson::dom::parser m_Parser;
		simdjson::dom::parser m_EmptyArgsParser;
		simdjson::dom::object m_EmptyArgs;
		// The whole reply, and a tool's own result, which goes into the reply as text.
		JsonWriter m_Reply;
		JsonWriter m_Result;
		char m_Error[512] = {};
		bool m_ArgumentError = false;
		// The tool call being run, for one that finishes later.
		uint m_CurrentConnection = 0;
		const RequestID* m_CurrentID = nullptr;
		const char* m_CurrentBody = nullptr;
		uint m_CurrentBodySize = 0;

		static constexpr uint c_MaxPendingCaptures = 4;
		LocalArray<PendingCapture, c_MaxPendingCaptures> m_PendingCaptures;
		static constexpr uint c_MaxPendingMaterials = 8;
		LocalArray<PendingMaterial, c_MaxPendingMaterials> m_PendingMaterials;
		Array<uint8> m_CapturePixels;
		Array<uint8> m_ImagePixels;
		Array<uint8> m_Png;
		PngEncoder m_PngEncoder;
		// The layer material set_material_layer keeps loaded until it has taken its textures.
		AssetID m_HeldLayerMaterial;
	};
}
