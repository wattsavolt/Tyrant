#pragma once

#include "Core.h"
#include "String/LocalString.h"

namespace tyr
{
	class AppBase;

	// Reads a game MCP tool's arguments. Each returns false when the argument is missing or isn't
	// the right kind.
	class McpArguments
	{
	public:
		virtual bool GetString(const char* key, char* out, size_t outSize) const = 0;
		virtual bool GetNumber(const char* key, double& out) const = 0;
		virtual bool GetBool(const char* key, bool& out) const = 0;

	protected:
		~McpArguments() = default;
	};

	// Writes a game MCP tool's result as JSON, putting the commas between values in itself.
	class McpResult
	{
	public:
		virtual void BeginObject() = 0;
		virtual void EndObject() = 0;
		virtual void BeginArray() = 0;
		virtual void EndArray() = 0;
		// Starts an object member, whose value is written next.
		virtual void Key(const char* key) = 0;
		virtual void String(const char* value) = 0;
		virtual void Number(double value) = 0;
		virtual void Bool(bool value) = 0;

		void KeyString(const char* key, const char* value) { Key(key); String(value); }
		void KeyNumber(const char* key, double value) { Key(key); Number(value); }
		void KeyBool(const char* key, bool value) { Key(key); Bool(value); }

	protected:
		~McpResult() = default;
	};

	using McpToolError = LocalString<255>;

	// A tool a game adds to the editor's MCP server, such as one that reports on its enemies. The
	// app it's run with is the game's own.
	struct McpGameTool
	{
		const char* name;
		const char* description;
		// A JSON schema object for the arguments.
		const char* inputSchema;
		// Returns false with outError saying why when it fails.
		bool (*run)(AppBase& app, const McpArguments& args, McpResult& result, McpToolError& outError);
	};
}
