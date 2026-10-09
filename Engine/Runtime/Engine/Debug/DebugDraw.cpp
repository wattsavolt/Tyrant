#include "DebugDraw.h"
#include "Rendering/RendererAPI.h"
#include "Rendering/RenderConstants.h"
#include "Rendering/DebugLineTypes.h"
#include "Math/Math.h"

namespace tyr
{
#if !TYR_FINAL
	namespace
	{
		constexpr uint c_CircleSegments = 24;
		constexpr uint c_MaxTimedLines = 16384;
		constexpr uint c_MaxTexts = 256;

		struct TimedLine
		{
			Vector3 start;
			Vector3 end;
			uint colour;
			float remaining;
			Handle world;
			bool depthTest;
		};

		struct TimedText
		{
			DebugText text;
			float remaining;
			Handle world;
		};

		// Everything drawn, kept in arrays reserved once and reused every frame.
		struct DebugDrawState
		{
			Handle world;
			// This frame's lines as vertex pairs, for the current world.
			Array<DebugLineVertex> depthTested;
			Array<DebugLineVertex> onTop;
			Array<TimedLine> timedLines;
			Array<DebugText> frameTexts;
			Array<TimedText> timedTexts;
			// The labels from the last flush.
			Array<DebugText> visibleTexts;
			bool warnedFull = false;

			DebugDrawState()
			{
				depthTested.Reserve(RenderConstants::c_MaxDebugLineVertices);
				onTop.Reserve(RenderConstants::c_MaxDebugLineVertices);
				timedLines.Reserve(c_MaxTimedLines);
				frameTexts.Reserve(c_MaxTexts);
				timedTexts.Reserve(c_MaxTexts);
				visibleTexts.Reserve(c_MaxTexts);
			}
		};

		DebugDrawState& GetState()
		{
			static DebugDrawState s_State;
			return s_State;
		}

		void WarnFull(DebugDrawState& state)
		{
			if (!state.warnedFull)
			{
				TYR_LOG_WARNING("Too much debug drawing this frame, so some of it was dropped.");
				state.warnedFull = true;
			}
		}

		// Adds a line pair for this frame, if there's room for it within one frame's vertices.
		void AddFrameLine(DebugDrawState& state, const Vector3& start, const Vector3& end, uint colour, bool depthTest)
		{
			if (state.depthTested.Size() + state.onTop.Size() + 2 > RenderConstants::c_MaxDebugLineVertices)
			{
				WarnFull(state);
				return;
			}
			Array<DebugLineVertex>& vertices = depthTest ? state.depthTested : state.onTop;
			vertices.Add({ start, colour });
			vertices.Add({ end, colour });
		}

		void AddLine(const Vector3& start, const Vector3& end, uint colour, float duration, bool depthTest)
		{
			DebugDrawState& state = GetState();
			if (duration <= 0.0f)
			{
				AddFrameLine(state, start, end, colour, depthTest);
				return;
			}
			if (state.timedLines.Size() >= c_MaxTimedLines)
			{
				WarnFull(state);
				return;
			}
			state.timedLines.Add({ start, end, colour, duration, state.world, depthTest });
		}

		// A circle around center in the plane of the two axes, which must be unit length and at right angles.
		void AddCircle(const Vector3& center, const Vector3& axisA, const Vector3& axisB, float radius, uint colour, float duration, bool depthTest)
		{
			Vector3 previous = center + axisA * radius;
			for (uint i = 1; i <= c_CircleSegments; ++i)
			{
				const float angle = Math::c_TwoPi * static_cast<float>(i) / static_cast<float>(c_CircleSegments);
				const Vector3 point = center + (axisA * Math::Cos(angle) + axisB * Math::Sin(angle)) * radius;
				AddLine(previous, point, colour, duration, depthTest);
				previous = point;
			}
		}

		// Half a circle, from axisA round through axisB to -axisA.
		void AddHalfCircle(const Vector3& center, const Vector3& axisA, const Vector3& axisB, float radius, uint colour, float duration, bool depthTest)
		{
			constexpr uint c_Segments = c_CircleSegments / 2;
			Vector3 previous = center + axisA * radius;
			for (uint i = 1; i <= c_Segments; ++i)
			{
				const float angle = Math::c_Pi * static_cast<float>(i) / static_cast<float>(c_Segments);
				const Vector3 point = center + (axisA * Math::Cos(angle) + axisB * Math::Sin(angle)) * radius;
				AddLine(previous, point, colour, duration, depthTest);
				previous = point;
			}
		}

		// Two unit axes at right angles to direction and each other.
		void GetPerpendicularAxes(const Vector3& direction, Vector3& axisA, Vector3& axisB)
		{
			axisA = Vector3::SafeNormalize(direction.Perpendicular());
			axisB = Vector3::Cross(direction, axisA);
		}
	}

	void DebugDraw::Line(const Vector3& start, const Vector3& end, uint colour, float duration, bool depthTest)
	{
		AddLine(start, end, colour, duration, depthTest);
	}

	void DebugDraw::Box(const Vector3& center, const Vector3& halfExtents, const Quaternion& rotation, uint colour, float duration, bool depthTest)
	{
		// Corner i has its x, y and z at the positive extent where bits 0, 1 and 2 are set.
		Vector3 corners[8];
		for (uint i = 0; i < 8; ++i)
		{
			const Vector3 local((i & 1) ? halfExtents.x : -halfExtents.x, (i & 2) ? halfExtents.y : -halfExtents.y, (i & 4) ? halfExtents.z : -halfExtents.z);
			corners[i] = center + rotation.Rotate(local);
		}

		// Each edge joins two corners differing in exactly one bit.
		for (uint i = 0; i < 8; ++i)
		{
			for (uint bit = 1; bit < 8; bit <<= 1)
			{
				if ((i & bit) == 0)
				{
					AddLine(corners[i], corners[i | bit], colour, duration, depthTest);
				}
			}
		}
	}

	void DebugDraw::Sphere(const Vector3& center, float radius, uint colour, float duration, bool depthTest)
	{
		AddCircle(center, Vector3::c_Right, Vector3::c_Up, radius, colour, duration, depthTest);
		AddCircle(center, Vector3::c_Right, Vector3::c_Forward, radius, colour, duration, depthTest);
		AddCircle(center, Vector3::c_Up, Vector3::c_Forward, radius, colour, duration, depthTest);
	}

	void DebugDraw::Capsule(const Vector3& center, float radius, float halfHeight, const Quaternion& rotation, uint colour, float duration, bool depthTest)
	{
		const Vector3 up = rotation.Rotate(Vector3::c_Up);
		const Vector3 right = rotation.Rotate(Vector3::c_Right);
		const Vector3 forward = rotation.Rotate(Vector3::c_Forward);
		const Vector3 top = center + up * halfHeight;
		const Vector3 bottom = center - up * halfHeight;

		AddCircle(top, right, forward, radius, colour, duration, depthTest);
		AddCircle(bottom, right, forward, radius, colour, duration, depthTest);
		AddLine(top + right * radius, bottom + right * radius, colour, duration, depthTest);
		AddLine(top - right * radius, bottom - right * radius, colour, duration, depthTest);
		AddLine(top + forward * radius, bottom + forward * radius, colour, duration, depthTest);
		AddLine(top - forward * radius, bottom - forward * radius, colour, duration, depthTest);

		// The rounded ends, in two planes through the capsule's axis.
		AddHalfCircle(top, right, up, radius, colour, duration, depthTest);
		AddHalfCircle(top, forward, up, radius, colour, duration, depthTest);
		const Vector3 down = up * -1.0f;
		AddHalfCircle(bottom, right, down, radius, colour, duration, depthTest);
		AddHalfCircle(bottom, forward, down, radius, colour, duration, depthTest);
	}

	void DebugDraw::Cone(const Vector3& apex, const Vector3& direction, float length, float halfAngle, uint colour, float duration, bool depthTest)
	{
		const Vector3 axis = Vector3::SafeNormalize(direction);
		Vector3 axisA;
		Vector3 axisB;
		GetPerpendicularAxes(axis, axisA, axisB);

		const Vector3 baseCenter = apex + axis * (length * Math::Cos(halfAngle));
		const float baseRadius = length * Math::Sin(halfAngle);
		AddCircle(baseCenter, axisA, axisB, baseRadius, colour, duration, depthTest);
		AddLine(apex, baseCenter + axisA * baseRadius, colour, duration, depthTest);
		AddLine(apex, baseCenter - axisA * baseRadius, colour, duration, depthTest);
		AddLine(apex, baseCenter + axisB * baseRadius, colour, duration, depthTest);
		AddLine(apex, baseCenter - axisB * baseRadius, colour, duration, depthTest);
	}

	void DebugDraw::Arrow(const Vector3& start, const Vector3& end, uint colour, float duration, bool depthTest)
	{
		AddLine(start, end, colour, duration, depthTest);

		const Vector3 shaft = end - start;
		const float length = shaft.Length();
		if (length < 1e-4f)
		{
			return;
		}

		// A head a fifth of the arrow's length, as four lines back from the tip.
		const Vector3 axis = shaft * (1.0f / length);
		Vector3 axisA;
		Vector3 axisB;
		GetPerpendicularAxes(axis, axisA, axisB);
		const float headLength = length * 0.2f;
		const Vector3 headBase = end - axis * headLength;
		const float headRadius = headLength * 0.4f;
		AddLine(end, headBase + axisA * headRadius, colour, duration, depthTest);
		AddLine(end, headBase - axisA * headRadius, colour, duration, depthTest);
		AddLine(end, headBase + axisB * headRadius, colour, duration, depthTest);
		AddLine(end, headBase - axisB * headRadius, colour, duration, depthTest);
	}

	void DebugDraw::Text(const Vector3& position, const char* text, uint colour, float duration)
	{
		DebugDrawState& state = GetState();
		DebugText debugText;
		debugText.position = position;
		debugText.text = LocalString<DebugText::c_MaxLength>(text, std::min<size_t>(strlen(text), DebugText::c_MaxLength));
		debugText.colour = colour;

		if (duration <= 0.0f)
		{
			if (state.frameTexts.Size() >= c_MaxTexts)
			{
				WarnFull(state);
				return;
			}
			state.frameTexts.Add(debugText);
			return;
		}
		if (state.timedTexts.Size() >= c_MaxTexts)
		{
			WarnFull(state);
			return;
		}
		state.timedTexts.Add({ debugText, duration, state.world });
	}

	void DebugDraw::SetWorld(Handle world)
	{
		DebugDrawState& state = GetState();
		if (!(state.world == world))
		{
			// What was drawn this frame was for the world being left.
			state.depthTested.Clear();
			state.onTop.Clear();
			state.frameTexts.Clear();
			state.world = world;
		}
	}

	void DebugDraw::Flush(Handle world, float deltaTime, RendererAPI& rendererAPI)
	{
		DebugDrawState& state = GetState();

		// Timed drawing for this world is added for this frame, then aged.
		for (uint i = state.timedLines.Size(); i-- > 0;)
		{
			TimedLine& line = state.timedLines[i];
			if (!(line.world == world))
			{
				continue;
			}
			AddFrameLine(state, line.start, line.end, line.colour, line.depthTest);
			line.remaining -= deltaTime;
			if (line.remaining <= 0.0f)
			{
				state.timedLines.SwapAndPopBack(i);
			}
		}

		state.visibleTexts.Clear();
		for (const DebugText& text : state.frameTexts)
		{
			state.visibleTexts.Add(text);
		}
		for (uint i = state.timedTexts.Size(); i-- > 0;)
		{
			TimedText& timedText = state.timedTexts[i];
			if (!(timedText.world == world))
			{
				continue;
			}
			if (state.visibleTexts.Size() < c_MaxTexts)
			{
				state.visibleTexts.Add(timedText.text);
			}
			timedText.remaining -= deltaTime;
			if (timedText.remaining <= 0.0f)
			{
				state.timedTexts.SwapAndPopBack(i);
			}
		}

		rendererAPI.SubmitDebugLines(state.depthTested.Data(), state.depthTested.Size(), state.onTop.Data(), state.onTop.Size());

		state.depthTested.Clear();
		state.onTop.Clear();
		state.frameTexts.Clear();
		state.warnedFull = false;
	}

	void DebugDraw::ClearWorld(Handle world)
	{
		DebugDrawState& state = GetState();
		for (uint i = state.timedLines.Size(); i-- > 0;)
		{
			if (state.timedLines[i].world == world)
			{
				state.timedLines.SwapAndPopBack(i);
			}
		}
		for (uint i = state.timedTexts.Size(); i-- > 0;)
		{
			if (state.timedTexts[i].world == world)
			{
				state.timedTexts.SwapAndPopBack(i);
			}
		}
		if (state.world == world)
		{
			state.depthTested.Clear();
			state.onTop.Clear();
			state.frameTexts.Clear();
			state.visibleTexts.Clear();
		}
	}

	const DebugText* DebugDraw::GetTexts(uint& count)
	{
		const DebugDrawState& state = GetState();
		count = state.visibleTexts.Size();
		return state.visibleTexts.Data();
	}
#else
	void DebugDraw::Line(const Vector3&, const Vector3&, uint, float, bool) {}
	void DebugDraw::Box(const Vector3&, const Vector3&, const Quaternion&, uint, float, bool) {}
	void DebugDraw::Sphere(const Vector3&, float, uint, float, bool) {}
	void DebugDraw::Capsule(const Vector3&, float, float, const Quaternion&, uint, float, bool) {}
	void DebugDraw::Cone(const Vector3&, const Vector3&, float, float, uint, float, bool) {}
	void DebugDraw::Arrow(const Vector3&, const Vector3&, uint, float, bool) {}
	void DebugDraw::Text(const Vector3&, const char*, uint, float) {}
	void DebugDraw::SetWorld(Handle) {}
	void DebugDraw::Flush(Handle, float, RendererAPI&) {}
	void DebugDraw::ClearWorld(Handle) {}

	const DebugText* DebugDraw::GetTexts(uint& count)
	{
		count = 0;
		return nullptr;
	}
#endif
}
