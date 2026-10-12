#pragma once

#include "Core.h"
#include "simdjson.h"

namespace tyr
{
	struct TypeInfo;
	class JsonWriter;

	// Reads and writes reflected objects as JSON, covering the same fields and value types the
	// Properties panel can edit. Vectors are [x, y, z], rotations [x, y, z, w] (or [x, y, z] Euler
	// degrees when set), and assets their path.
	namespace McpReflection
	{
		// Writes the object's visible fields as a JSON object.
		void WriteObject(JsonWriter& writer, const void* object, const TypeInfo& typeInfo);

		// Sets the fields named in values on the object, leaving the rest alone. On failure, writes
		// why into error and stops, leaving the object partly changed.
		bool ReadObject(const simdjson::dom::object& values, void* object, const TypeInfo& typeInfo, char* error, size_t errorSize);
	}
}
