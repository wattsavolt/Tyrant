#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Quaternion.h"

namespace tyr
{
	/// Rigid mesh vertex  
	struct Vertex
	{
		Vertex() = default;
		constexpr Vertex(float x, float y, float z,
			float nx, float ny, float nz,
			float u, float v,
			float tx, float ty, float tz)
			: position(x, y, z)
			, normal(nx, ny, nz)
			, texCoord(u, v)
			, tangent(tx, ty, tz)
		{

		}

		Vector3 position;
		Vector3 normal;
		Vector2 texCoord;
		Vector3 tangent;
	};

	/// Anim vertex used by skeletal meshes
	// TODO: Implement
	struct AnimVertex
	{
		AnimVertex() = default;
		constexpr AnimVertex(float x, float y, float z,
			float nx, float ny, float nz,
			float u, float v,
			float tx, float ty, float tz)
			: position(x, y, z)
			, normal(nx, ny, nz)
			, uv(u, v)
			, tangent(tx, ty, tz)
		{

		}

		Vector3 position;
		Vector3 normal;
		Vector2 uv;
		Vector3 tangent;
	};
}