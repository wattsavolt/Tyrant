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
	};

	// What actually gets kept in RenderFrame once SubmitGUIDrawData has uploaded the vertex/
	// index bytes - just the draw commands and where this submission's data landed in the
	// shared GUI vertex/index buffers, since GUIPass never needs the raw bytes back on the CPU
	// side.
	struct GUIDrawSubmission
	{
		Vector2 displaySize;
		Array<GUIDrawCommand> commands;
		uint vertexOffset = 0;
		uint indexOffset = 0;
	};
}
