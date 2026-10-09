#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Quaternion.h"

namespace tyr
{
	class RendererAPI;

	// A colour for debug drawing, packed into 4 bytes with R in the lowest.
	constexpr uint MakeDebugColour(uint r, uint g, uint b, uint a = 255)
	{
		return r | (g << 8) | (b << 16) | (a << 24);
	}

	struct DebugColour final
	{
		static constexpr uint c_White = MakeDebugColour(255, 255, 255);
		static constexpr uint c_Grey = MakeDebugColour(128, 128, 128);
		static constexpr uint c_Red = MakeDebugColour(230, 50, 50);
		static constexpr uint c_Green = MakeDebugColour(60, 210, 70);
		static constexpr uint c_Blue = MakeDebugColour(60, 110, 240);
		static constexpr uint c_Yellow = MakeDebugColour(250, 220, 50);
		static constexpr uint c_Orange = MakeDebugColour(250, 150, 40);
		static constexpr uint c_Cyan = MakeDebugColour(50, 220, 230);
		static constexpr uint c_Magenta = MakeDebugColour(220, 60, 220);
	};

	// A label at a point in the world, drawn by whatever shows the viewport.
	struct DebugText
	{
		static constexpr uint c_MaxLength = 63;

		Vector3 position;
		LocalString<c_MaxLength> text;
		uint colour;
	};

	// Lines, shapes and labels drawn over the active world, in every build except final ones. A
	// duration of 0 draws for one frame, otherwise for that many seconds. Depth-tested drawing is
	// hidden behind the scene, the rest is drawn on top. Main thread only.
	class TYR_ENGINE_API DebugDraw final
	{
	public:
		static void Line(const Vector3& start, const Vector3& end, uint colour, float duration = 0.0f, bool depthTest = true);

		static void Box(const Vector3& center, const Vector3& halfExtents, const Quaternion& rotation, uint colour, float duration = 0.0f, bool depthTest = true);

		static void Sphere(const Vector3& center, float radius, uint colour, float duration = 0.0f, bool depthTest = true);

		// Stands along the rotation's Y axis. halfHeight is from the center to either sphere's center.
		static void Capsule(const Vector3& center, float radius, float halfHeight, const Quaternion& rotation, uint colour, float duration = 0.0f, bool depthTest = true);

		// halfAngle is in radians, measured from direction.
		static void Cone(const Vector3& apex, const Vector3& direction, float length, float halfAngle, uint colour, float duration = 0.0f, bool depthTest = true);

		static void Arrow(const Vector3& start, const Vector3& end, uint colour, float duration = 0.0f, bool depthTest = true);

		// Cut short to DebugText::c_MaxLength characters.
		static void Text(const Vector3& position, const char* text, uint colour, float duration = 0.0f);

		// The world new drawing goes to, set by the WorldManager when the active world changes.
		static void SetWorld(Handle world);

		// Sends the world's lines to the renderer and ages timed drawing. Once a frame.
		static void Flush(Handle world, float deltaTime, RendererAPI& rendererAPI);

		// Drops everything drawn into the world, such as when it's removed.
		static void ClearWorld(Handle world);

		// The labels from the last flush, for whatever shows the flushed world's viewport.
		static const DebugText* GetTexts(uint& count);
	};
}
