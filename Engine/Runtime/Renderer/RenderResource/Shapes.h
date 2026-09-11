#pragma once

#include "RendererMacros.h"
#include "Core.h"
#include "Vertex.h"

namespace tyr
{
    struct ShaderVertex;
	class TYR_RENDERER_API Cube final
	{
	public:
		static constexpr uint c_NumVertices = 24;
		static constexpr uint c_NumIndices = 36;

		// Vertices for a unit cube
        static constexpr Vertex c_Vertices[c_NumVertices] =
        {
            // Front Face
            {
                -0.5f, -0.5f, -0.5f,
                0, 0, -1,
                1, 0, 0, 1,
                0, 1
            },
            {
                -0.5f, 0.5f, -0.5f,
                0, 0, -1,
                1, 0, 0, 1,
                0, 0
            },
            {
                0.5f, 0.5f, -0.5f,
                0, 0, -1,
                1, 0, 0, 1,
                0, 1
            },
            {
                0.5f, -0.5f, -0.5f,
                0, 0, -1,
                1, 0, 0, 1,
                0, 0
            },

            // Back Face
            {
                -0.5f, -0.5f, 0.5f,
                0, 0, -1,
                1, 0, 0, 1,
                0, 1
            },
            {
                0.5f, -0.5f, 0.5f,
                0, 0, -1,
                1, 0, 0, 1,
                0, 0
            },
            {
                0.5f, 0.5f, 0.5f,
                0, 0, -1,
                1, 0, 0, 1,
                0, 1
            },
            {
                -0.5f, 0.5f, 0.5f,
                0, 0, -1,
                1, 0, 0, 1,
                0, 0
            },

            // Top Face
            {
                -0.5f, 0.5f, -0.5f,
                0, 1, 0,
                1, 0, 0, 1,
                0, 1
            },
            {
                -0.5f, 0.5f, 0.5f,
                0, 1, 0,
                1, 0, 0, 1,
                0, 0
            },
            {
                0.5f, 0.5f, 0.5f,
                0, 1, 0,
                1, 0, 0, 1,
                0, 1
            },
            {
                0.5f, 0.5f, -0.5f,
                0, 1, 0,
                1, 0, 0, 1,
                0, 0
            },

            // Bottom Face
            {
                -0.5f, -0.5f, 0.5f,
                0, -1, 0,
                -1, 0, 0, 1,
                0, -1
            },
            {
                -0.5f, -0.5f, -0.5f,
                0, -1, 0,
                -1, 0, 0, 1,
                0, 0
            },
            {
                0.5f, -0.5f, -0.5f,
                0, -1, 0,
                -1, 0, 0, 1,
                0, 1
            },
            {
                0.5f, -0.5f, 0.5f,
                0, -1, 0,
                -1, 0, 0, 1,
                0, 0
            },

            // Left Face
            {
                -0.5f, -0.5f, 0.5f,
                -1, 0, 0,
                0, 0, -1, 1,
                0, -1
            },
            {
                -0.5f, 0.5f, 0.5f,
                -1, 0, 0,
                0, 0, -1, 1,
                0, 0
            },
            {
                -0.5f, 0.5f, -0.5f,
                -1, 0, 0,
                0, 0, -1, 1,
                0, 1
            },
            {
                -0.5f, -0.5f, -0.5f,
                -1, 0, 0,
                0, 0, -1, 1,
                0, 0
            },

            // Right Face
            {
                0.5f, -0.5f, -0.5f,
                1, 0, 0,
                0, 0, 1, 1,
                0, -1
            },
            {
                0.5f, 0.5f, -0.5f,
                1, 0, 0,
                0, 0, 1, 1,
                0, 0
            },
            {
                0.5f, 0.5f, 0.5f,
                1, 0, 0,
                0, 0, 1, 1,
                0, 1
            },
            {
                0.5f, -0.5f, 0.5f,
                1, 0, 0,
                0, 0, 1, 1,
                0, 0
            }
        };
			
		// Indices in clock-wise order
		static constexpr uint c_Indices[c_NumIndices] = 
        {
			// Front Face
			0, 1, 2,
			0, 2, 3,

			// Back Face
			4, 5, 6,
			4, 6, 7,

			// Top Face
			8, 9, 10,
			8, 10, 11,

			// Bottom Face
			12, 13, 14,
			12, 14, 15,

			// Left Face
			16, 17, 18,
			16, 18, 19,

			// Right Face
			20, 21, 22,
			20, 22, 23
		};

        Cube();

        static const Cube c_UnitCube;

        const ShaderVertex* GetShaderVertices() const;

    private:
        Array<ShaderVertex> m_ShaderVertices;
	};

	class TYR_RENDERER_API Sphere
	{
	public:
		Sphere(float radius = 1.0f, uint16 latLines = 30u, uint16 longLines = 30u);

		static const Sphere c_UnitSphere;

        const Vertex* GetVertices(uint& size) const;

        const ShaderVertex* GetShaderVertices(uint& size) const;

        const uint* GetIndices(uint& size) const;

	private:
		Array<Vertex> m_Vertices;
        Array<ShaderVertex> m_ShaderVertices;
		Array<uint> m_Indices;
	};
}