#pragma once

#include "RendererMacros.h"
#include "Core.h"

namespace tyr
{
	class Vector3;
	struct Vertex;
	struct ShaderVertex;
	class TYR_RENDERER_API MeshUtil
	{
	public:
		// Note: The tangent sign accumulation calculation in this function is okay for simple meshes such as cubes and spheres 
		// but isn't accurate if a vertex in the mesh is shared by faces with different tangent signs
		// though vertices should be duplicated in the mesh on creation to avoid this
		static void CreateNormalsAndTangents(Vertex* vertices, uint numVertices, const uint* indices, uint numIndices);
		static uint EncodeOct(const Vector3& n);
		static Vector3 DecodeOct(uint packed);
		static void VertexToShaderVertex(const Vertex& vertex, ShaderVertex& shaderVertex);
		// shaderVertices expected to be preallocated to size of numVertices
		static void CreateShaderVertices(const Vertex* vertices, ShaderVertex* shaderVertices, uint numVertices);
	};
}