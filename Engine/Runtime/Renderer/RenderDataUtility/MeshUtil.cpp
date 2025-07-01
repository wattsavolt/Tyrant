#include "MeshUtil.h"
#include "RenderDataTypes/Vertex.h"
#include "Memory/StackAllocation.h"

namespace tyr
{
	void MeshUtil::CreateNormalsAndTangents(Vertex* vertices, uint numVertices, const uint* indices, uint numIndices)
	{
		const uint numFaces = numIndices / 3;

		Vector3* faceNormals = StackNew<Vector3>(numFaces);
		Vector3* faceTangents = StackNew<Vector3>(numFaces);

		// Compute face normals
		for (uint i = 0; i < numFaces; ++i)
		{
			const uint index = i * 3;
			Vertex& v0 = vertices[indices[index]];
			Vertex& v1 = vertices[indices[index + 2]];
			// Get the vector describing one edge of our triangle (edge 0,2)
			const Vector3 edge1 = vertices[indices[index]].position - vertices[indices[index + 2]].position;

			// Get the vector describing another edge of our triangle (edge 2,1)
			const Vector3 edge2 = vertices[indices[index + 2]].position - vertices[indices[index + 1]].position;

			// Cross multiply the two edge vectors to get the un-normalized face normal
			faceNormals[i] = Vector3::Cross(edge1, edge2);

			// Find first image coordinate edge 2d vector
			const Vector2 tc1 = vertices[indices[index]].texCoord - vertices[indices[index + 2]].texCoord;

			// Find second image coordinate edge 2d vector
			const Vector2 tc2 = vertices[indices[index + 2]].texCoord - vertices[indices[index + 1]].texCoord;

			// Find tangent using both tex coord edges and position edges
			faceTangents[i] = (tc1.y * edge1 - tc2.y * edge2) * (1.0f / (tc1.x * tc2.y - tc2.x * tc1.y));
		}

		// Compute vertex normals and tangents (normal/tangent averaging)

		int facesUsing = 0;

		//Go through each vertex
		for (uint i = 0; i < numVertices; ++i)
		{
			Vertex& vertex = vertices[i];
			vertex.normal = Vector3::c_Zero;
			vertex.tangent = Vector3::c_Zero;

			//Check which triangles use this vertex
			for (uint j = 0; j < numFaces; ++j)
			{
				const uint index = j * 3;
				if (indices[index] == i ||
					indices[index + 1] == i ||
					indices[index + 2] == i)
				{
					vertex.normal += faceNormals[j];
					vertex.tangent += faceTangents[j];
					facesUsing++;
				}
			}

			// Get the actual normal and tangent by dividing the sums by the number of faces sharing the vertex
			vertex.normal /= facesUsing;
			vertex.tangent /= facesUsing;

			vertex.normal.SafeNormalize();
			vertex.tangent.SafeNormalize();

			facesUsing = 0;
		}

		StackDelete<Vector3>(faceTangents);
		StackDelete<Vector3>(faceNormals);
	}
}