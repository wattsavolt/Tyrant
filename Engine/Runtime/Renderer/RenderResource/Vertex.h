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
			float tx, float ty, float tz, float tw, 
			float u, float v)
			: position(x, y, z)
			, normal(nx, ny, nz)
			, tangent(tx, ty, tz, tw)
			, uv(u, v)
		{

		}

		Vector3 position;
		Vector3 normal;
		Vector2 uv;
		// w stores the tangent sign
		Vector4 tangent;
	};
}