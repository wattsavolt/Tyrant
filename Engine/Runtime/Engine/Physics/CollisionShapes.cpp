#include "CollisionShapes.h"
#include "IO/BufferedFileStream.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Core/UnorderedMap.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

namespace tyr
{
	namespace
	{
		// Jolt's saved shapes can only be read by the version that wrote them.
		constexpr uint c_JoltVersion = (JPH_VERSION_MAJOR << 16) | (JPH_VERSION_MINOR << 8) | JPH_VERSION_PATCH;

		class FileStreamOut final : public JPH::StreamOut
		{
		public:
			explicit FileStreamOut(BufferedFileStream& stream)
				: m_Stream(stream)
			{
			}

			// Jolt writes empty arrays as zero bytes, which the stream doesn't take.
			void WriteBytes(const void* data, size_t size) override
			{
				if (size > 0)
				{
					m_Stream.Write(data, size);
				}
			}

			bool IsFailed() const override
			{
				return false;
			}

		private:
			BufferedFileStream& m_Stream;
		};

		class FileStreamIn final : public JPH::StreamIn
		{
		public:
			explicit FileStreamIn(BufferedFileStream& stream)
				: m_Stream(stream)
			{
			}

			void ReadBytes(void* data, size_t size) override
			{
				if (size > 0)
				{
					m_Failed |= m_Stream.Read(data, size) != size;
				}
			}

			bool IsEOF() const override
			{
				return m_Stream.IsEOF();
			}

			bool IsFailed() const override
			{
				return m_Failed;
			}

		private:
			BufferedFileStream& m_Stream;
			bool m_Failed = false;
		};

		JPH::ShapeRefC CreateTriangleShape(const CollisionMeshPiece* pieces, uint pieceCount, const char* meshName)
		{
			JPH::VertexList vertices;
			JPH::IndexedTriangleList triangles;
			for (uint p = 0; p < pieceCount; ++p)
			{
				const CollisionMeshPiece& piece = pieces[p];
				const uint baseVertex = static_cast<uint>(vertices.size());
				for (uint v = 0; v < piece.vertexCount; ++v)
				{
					vertices.push_back(JPH::Float3(piece.positions[v].x, piece.positions[v].y, piece.positions[v].z));
				}
				for (uint i = 0; i + 2 < piece.indexCount; i += 3)
				{
					triangles.push_back(JPH::IndexedTriangle(baseVertex + piece.indices[i], baseVertex + piece.indices[i + 1], baseVertex + piece.indices[i + 2]));
				}
			}

			JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
			JPH::ShapeSettings::ShapeResult result = settings.Create();
			if (result.HasError())
			{
				TYR_LOG_WARNING("Couldn't make %s's triangle mesh collider: %s", meshName, result.GetError().c_str());
				return nullptr;
			}
			return result.Get();
		}

		// More than one piece makes a compound of hulls, each in the mesh's space.
		JPH::ShapeRefC CreateConvexShape(const CollisionMeshPiece* pieces, uint pieceCount, const char* meshName)
		{
			JPH::StaticCompoundShapeSettings compound;
			JPH::ShapeRefC single;
			for (uint p = 0; p < pieceCount; ++p)
			{
				const CollisionMeshPiece& piece = pieces[p];
				JPH::Array<JPH::Vec3> points;
				points.reserve(piece.vertexCount);
				for (uint v = 0; v < piece.vertexCount; ++v)
				{
					points.push_back(JPH::Vec3(piece.positions[v].x, piece.positions[v].y, piece.positions[v].z));
				}

				JPH::ShapeSettings::ShapeResult result = JPH::ConvexHullShapeSettings(points).Create();
				if (result.HasError())
				{
					TYR_LOG_WARNING("Couldn't make a convex hull collider for %s: %s", meshName, result.GetError().c_str());
					continue;
				}
				single = result.Get();
				compound.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), single);
			}

			if (compound.mSubShapes.size() < 2)
			{
				return single;
			}
			JPH::ShapeSettings::ShapeResult result = compound.Create();
			if (result.HasError())
			{
				TYR_LOG_WARNING("Couldn't make %s's compound collider: %s", meshName, result.GetError().c_str());
				return nullptr;
			}
			return result.Get();
		}

		void WriteShape(FileStreamOut& out, const JPH::Shape* shape)
		{
			const uint8 present = shape != nullptr;
			out.Write(present);
			if (shape)
			{
				JPH::Shape::ShapeToIDMap shapeMap;
				JPH::Shape::MaterialToIDMap materialMap;
				shape->SaveWithChildren(out, shapeMap, materialMap);
			}
		}

		const JPH::Shape* ReadShape(FileStreamIn& in)
		{
			uint8 present = 0;
			in.Read(present);
			if (!present || in.IsFailed())
			{
				return nullptr;
			}

			JPH::Shape::IDToShapeMap shapeMap;
			JPH::Shape::IDToMaterialMap materialMap;
			JPH::Shape::ShapeResult result = JPH::Shape::sRestoreWithChildren(in, shapeMap, materialMap);
			if (result.HasError() || in.IsFailed())
			{
				TYR_LOG_ERROR("Couldn't read a mesh collider: %s", result.HasError() ? result.GetError().c_str() : "the file ended early");
				return nullptr;
			}
			const JPH::Shape* shape = result.Get().GetPtr();
			shape->AddRef();
			return shape;
		}
	}

	namespace CollisionShapes
	{
		bool Write(BufferedFileStream& stream, const CollisionMeshPiece* pieces, uint pieceCount, const char* meshName)
		{
			const JPH::ShapeRefC triangleShape = CreateTriangleShape(pieces, pieceCount, meshName);
			const JPH::ShapeRefC convexShape = CreateConvexShape(pieces, pieceCount, meshName);

			FileStreamOut out(stream);
			out.Write(c_JoltVersion);
			WriteShape(out, triangleShape);
			WriteShape(out, convexShape);
			return triangleShape != nullptr || convexShape != nullptr;
		}

		void Read(BufferedFileStream& stream, const JPH::Shape*& outTriangleShape, const JPH::Shape*& outConvexShape)
		{
			outTriangleShape = nullptr;
			outConvexShape = nullptr;

			FileStreamIn in(stream);
			uint joltVersion = 0;
			in.Read(joltVersion);
			if (joltVersion != c_JoltVersion)
			{
				TYR_LOG_ERROR("A mesh collider was saved by another version of Jolt. Reimport its mesh.");
				return;
			}
			outTriangleShape = ReadShape(in);
			outConvexShape = ReadShape(in);
		}

		void Release(const JPH::Shape* shape)
		{
			if (shape)
			{
				shape->Release();
			}
		}

		void AddLines(const JPH::Shape* shape, Array<Vector3>& outLines)
		{
			if (!shape)
			{
				return;
			}

			// Triangles come relative to the centre of mass, so it's put back where the shape has it.
			JPH::Shape::GetTrianglesContext context;
			shape->GetTrianglesStart(context, JPH::AABox::sBiggest(), shape->GetCenterOfMass(), JPH::Quat::sIdentity(), JPH::Vec3::sOne());
			constexpr int c_BatchSize = JPH::Shape::cGetTrianglesMinTrianglesRequested;
			JPH::Float3 vertices[c_BatchSize * 3];
			for (int count = shape->GetTrianglesNext(context, c_BatchSize, vertices); count > 0; count = shape->GetTrianglesNext(context, c_BatchSize, vertices))
			{
				for (int t = 0; t < count; ++t)
				{
					const JPH::Float3* triangle = &vertices[t * 3];
					for (int e = 0; e < 3; ++e)
					{
						const JPH::Float3& a = triangle[e];
						const JPH::Float3& b = triangle[(e + 1) % 3];
						outLines.Add(Vector3(a.x, a.y, a.z));
						outLines.Add(Vector3(b.x, b.y, b.z));
					}
				}
			}
		}
	}
}
