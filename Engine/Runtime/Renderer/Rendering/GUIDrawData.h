#pragma once

#include "Core.h"
#include "Containers/Array.h"
#include "Math/Vector2.h"

namespace tyr
{
	// Matches the layout GUIVS.hlsl reads a vertex as - pos/uv as plain floats, colour packed
	// into 4 bytes (R in the lowest byte).
	struct GUIVertex
	{
		Vector2 pos;
		Vector2 uv;
		uint colour;
	};

	struct GUIDrawCommand
	{
		uint clipMinX;
		uint clipMinY;
		uint clipMaxX;
		uint clipMaxY;
		uint textureIndex;
		uint indexOffset;
		uint indexCount;
	};

	// What a caller (GUIModule, converting ImGui/Nuklear output) builds and passes to
	// RendererAPI::SubmitGUIDrawData for one frame.
	struct GUIDrawData
	{
		Vector2 displaySize;
		Array<GUIVertex> vertices;
		Array<uint16> indices;
		Array<GUIDrawCommand> commands;

		// Clears the arrays without releasing their capacity, so a caller can keep one of these
		// as a persistent member and reuse it every frame instead of reallocating a fresh one
		// each call.
		void Clear()
		{
			displaySize = {};
			vertices.Clear();
			indices.Clear();
			commands.Clear();
		}
	};

	// Just the draw commands and where this submission's data landed in the shared GUI
	// vertex/index buffers - the raw vertex/index bytes never need to come back to the CPU
	// once uploaded.
	struct GUIDrawSubmission
	{
		Vector2 displaySize;
		Array<GUIDrawCommand> commands;
		uint vertexOffset = 0;
		uint indexOffset = 0;
	};
}
