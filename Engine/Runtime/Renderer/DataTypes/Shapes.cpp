#include "Shapes.h"
#include "Math/Vector3.h"
#include "Math/Matrix3.h"
#include "DataUtility/MeshUtil.h"

namespace tyr
{
	const Sphere Sphere::c_UnitSphere = Sphere(1.0f);

	Sphere::Sphere(float radius, uint16 latLines, uint16 longLines)
	{
		const uint16 numVertices = ((latLines - 2) * longLines) + 2;
		const uint16 numFaces = ((latLines - 3) * longLines * 2) + (longLines * 2);
		const uint16 numIndices = numFaces * 3;
		const float scale = radius / 1.0f;

		m_Vertices.Resize(numVertices);
		m_Indices.Resize(numIndices);

		Vector3 currVertPos = Vector3(0.0f, 0.0f, 1.0f);

		m_Vertices[0].position = currVertPos;

		for (uint16 i = 0; i < latLines - 2; ++i)
		{
			float spherePitch = (i + 1) * (Math::c_Pi / (latLines - 1));
			for (uint16 j = 0; j < longLines; ++j)
			{
				float sphereYaw = j * (Math::c_TwoPi / (longLines));
				Matrix3 rotationXY(spherePitch, sphereYaw, 0.0f);
				currVertPos = rotationXY.Multiply(Vector3::c_Forward);
				currVertPos.SafeNormalize();
				uint index = i * longLines + j + 1;
				m_Vertices[index].position = currVertPos * scale;

				m_Vertices[index].texCoord.x = Math::Atan2(currVertPos.x, currVertPos.z) * Math::c_InvTwoPi + 0.5f;
				m_Vertices[index].texCoord.y = currVertPos.y * 0.5f + 0.5f;
			}
		}

		m_Vertices[numVertices - 1].position = Vector3(0 , 0, -1);

		int k = 0;
		for (uint16 l = 0; l < longLines - 1; ++l)
		{
			m_Indices[k] = 0;
			m_Indices[k + 1] = l + 1;
			m_Indices[k + 2] = l + 2;
			k += 3;
		}

		m_Indices[k] = 0;
		m_Indices[k + 1] = longLines;
		m_Indices[k + 2] = 1;
		k += 3;

		for (uint16 i = 0; i < latLines - 3; ++i)
		{
			for (uint16 j = 0; j < longLines - 1; ++j)
			{
				m_Indices[k] = i * longLines + j + 1;
				m_Indices[k + 1] = i * longLines + j + 2;
				m_Indices[k + 2] = (i + 1) * longLines + j + 1;

				m_Indices[k + 3] = (i + 1) * longLines + j + 1;
				m_Indices[k + 4] = i * longLines + j + 2;
				m_Indices[k + 5] = (i + 1) * longLines + j + 2;

				k += 6; // Next quad
			}

			m_Indices[k] = (i * longLines) + longLines;
			m_Indices[k + 1] = (i * longLines) + 1;
			m_Indices[k + 2] = ((i + 1) * longLines) + longLines;

			m_Indices[k + 3] = ((i + 1) * longLines) + longLines;
			m_Indices[k + 4] = (i * longLines) + 1;
			m_Indices[k + 5] = ((i + 1) * longLines) + 1;

			k += 6;
		}

		for (uint16 l = 0; l < longLines - 1; ++l)
		{
			m_Indices[k] = numVertices - 1;
			m_Indices[k + 1] = (numVertices - 1) - (l + 1);
			m_Indices[k + 2] = (numVertices - 1) - (l + 2);
			k += 3;
		}

		m_Indices[k] = numVertices - 1;
		m_Indices[k + 1] = (numVertices - 1) - longLines;
		m_Indices[k + 2] = numVertices - 2;

		MeshUtil::CreateNormalsAndTangents(m_Vertices.Data(), numVertices, (const uint*)m_Indices.Data(), numIndices);
	}
}