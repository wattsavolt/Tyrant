#include "MeshUtil.h"
#include "RenderResource/Vertex.h"
#include "Shaders/ShaderTypes.h"
#include "Memory/StackAllocation.h"
#include "Math/Vector3.h"
#include "Math/Half2.h"

namespace tyr
{
	void MeshUtil::CreateNormalsAndTangents(Vertex* vertices, uint numVertices, const uint* indices, uint numIndices)
	{
		const uint numFaces = numIndices / 3;

		StackAllocManager alloc;
		Vector3* faceNormals = StackNew<Vector3>(numFaces);
		Vector3* faceTangents = StackNew<Vector3>(numFaces);
		float* faceSigns = StackNew<float>(numFaces);

		// Compute face normals and tangents, and tangent signs
		for (uint i = 0; i < numFaces; ++i)
		{
			const uint index = i * 3;
			Vertex& v0 = vertices[indices[index]];
			Vertex& v1 = vertices[indices[index + 2]];
			Vertex& v2 = vertices[indices[index + 1]];

			// Edge vectors
			const Vector3 edge1 = v0.position - v1.position; // 0 -> 2
			const Vector3 edge2 = v1.position - v2.position; // 2 -> 1

			// Un-normalized face normal
			faceNormals[i] = Vector3::Cross(edge1, edge2);

			// UV edges
			const Vector2 tc1 = v0.uv - v1.uv;
			const Vector2 tc2 = v1.uv - v2.uv;

			// Tangent from UVs and positions
			const float r = 1.0f / (tc1.x * tc2.y - tc2.x * tc1.y);
			faceTangents[i] = (tc1.y * edge1 - tc2.y * edge2) * r;

			// Compute the bitangent
			const Vector3 bitangent = (edge2 * tc1.x - edge1 * tc2.x) * r;

			// Compute tangent sign: +1 if cross(N, T) aligns with bitangent, else -1
			faceSigns[i] = (Vector3::Dot(Vector3::Cross(faceNormals[i], faceTangents[i]), bitangent) < 0.0f) ? -1.0f : 1.0f;
		}

		// Compute vertex normals and tangents (normal/tangent averaging)

		int facesUsing = 0;

		// Go through each vertex
		for (uint i = 0; i < numVertices; ++i)
		{
			Vertex& vertex = vertices[i];
			vertex.normal = Vector3::c_Zero;
			Vector3 tangent = Vector3::c_Zero;
			vertex.tangent = Vector4(0, 0, 0, 1);

			float accumulatedSign = 0.0f;

			// Check which triangles use this vertex
			for (uint j = 0; j < numFaces; ++j)
			{
				const uint index = j * 3;
				if (indices[index] == i || indices[index + 1] == i || indices[index + 2] == i)
				{
					vertex.normal += faceNormals[j];
					tangent += faceTangents[j];
					accumulatedSign += faceSigns[j];
					facesUsing++;
				}
			}

			// Average normals/tangents
			vertex.normal /= facesUsing;
			tangent /= facesUsing;

			vertex.normal.SafeNormalize();
			tangent.SafeNormalize();

			// Set tangent w to averaged sign
			const float vertexSign = (accumulatedSign < 0.0f) ? -1.0f : 1.0f;
			vertex.tangent = Vector4(tangent.x, tangent.y, tangent.z, vertexSign);

			facesUsing = 0;
		}

		StackDelete<Vector3>(faceTangents);
		StackDelete<Vector3>(faceNormals);
		StackDelete<float>(faceSigns);
	}

	uint MeshUtil::EncodeOct(const Vector3& n)
	{
		const float invL1 = 1.0f / (fabsf(n.x) + fabsf(n.y) + fabsf(n.z));

		float x = n.x * invL1;
		float y = n.y * invL1;
		const float z = n.z * invL1;

		const float t = std::max(-z, 0.0f);

		x += (x >= 0.0f ? -t : t);
		y += (y >= 0.0f ? -t : t);

		const int sx = Math::FloatToSnorm16(x);
		const int sy = Math::FloatToSnorm16(y);

		return (uint(uint16(sy)) << 16) | uint(uint16(sx));
	}

	Vector3 MeshUtil::DecodeOct(uint packed)
	{
		const int sx = int16_t(packed & 0xFFFF);
		const int sy = int16_t(packed >> 16);

		float x = Math::Snorm16ToFloat(sx);
		float y = Math::Snorm16ToFloat(sy);

		Vector3 v;

		v.x = x;
		v.y = y;
		v.z = 1.0f - fabsf(x) - fabsf(y);

		const float t = std::max(-v.z, 0.0f);

		v.x += (v.x >= 0.0f ? -t : t);
		v.y += (v.y >= 0.0f ? -t : t);

		const float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);

		v.x /= len;
		v.y /= len;
		v.z /= len;

		return v;
	}

	void MeshUtil::VertexToShaderVertex(const Vertex& vertex, ShaderVertex& shaderVertex)
	{
		shaderVertex.position = vertex.position;
		shaderVertex.normalOct = EncodeOct(vertex.normal);
		const Vector3 tangent = Vector3(vertex.tangent.x, vertex.tangent.y, vertex.tangent.z);
		shaderVertex.tangentOct = EncodeOct(tangent);
		const Vector3 crossNT = Vector3::Cross(vertex.normal, tangent);
		const uint signBit = vertex.tangent.w < 0.0f ? 1u : 0u;

		// Pack sign into highest bit
		shaderVertex.tangentOct = (shaderVertex.tangentOct & 0x7FFFFFFF) | (signBit << 31);
		shaderVertex.uv = Half2(vertex.uv.x, vertex.uv.y);
	}

	void MeshUtil::CreateShaderVertices(const Vertex* vertices, ShaderVertex* shaderVertices, uint numVertices)
	{
		for (uint i = 0; i < numVertices; ++i)
		{
			VertexToShaderVertex(vertices[i], shaderVertices[i]);
		}
	}
}