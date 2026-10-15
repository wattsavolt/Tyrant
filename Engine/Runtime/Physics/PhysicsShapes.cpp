#include "PhysicsShapes.h"
#include "Private/JoltUtil.h"
#include "IO/BufferedFileStream.h"
#include <algorithm>
#include <cmath>

#include <Jolt/Jolt.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Core/UnorderedMap.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/CollisionCollector.h>

namespace tyr
{
	using namespace JoltUtil;

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

		// The shape moved off the body's origin by offset, with a reference held, or null if it failed.
		const PhysicsShape* TakeOffsetShape(const JPH::ShapeSettings::ShapeResult& result, const Vector3& offset, const char* what)
		{
			if (result.HasError() || offset == Vector3::c_Zero)
			{
				return TakeShape(result, what);
			}
			return TakeShape(JPH::RotatedTranslatedShapeSettings(ToJolt(offset), JPH::Quat::sIdentity(), result.Get()).Create(), what);
		}

		// Gathers a shape's leaf shapes, where compound and offset shapes break down to.
		class LeafCollector final : public JPH::TransformedShapeCollector
		{
		public:
			void AddHit(const JPH::TransformedShape& shape) override
			{
				leaves.push_back(shape);
			}

			JPH::Array<JPH::TransformedShape> leaves;
		};

		// Calls onTriangle with each of the shape's triangles, three points, placed at the transform.
		// Only leaf shapes give triangles, so compound and offset shapes are broken down first.
		template<typename OnTriangle>
		void ForEachTriangle(const JPH::Shape* shape, JPH::Vec3Arg position, JPH::QuatArg rotation, OnTriangle onTriangle)
		{
			if (!shape)
			{
				return;
			}
			LeafCollector collector;
			shape->CollectTransformedShapes(JPH::AABox::sBiggest(), position + rotation * shape->GetCenterOfMass(), rotation, JPH::Vec3::sOne(),
				JPH::SubShapeIDCreator(), collector, JPH::ShapeFilter());

			constexpr int c_BatchSize = JPH::Shape::cGetTrianglesMinTrianglesRequested;
			JPH::Float3 vertices[c_BatchSize * 3];
			for (const JPH::TransformedShape& leaf : collector.leaves)
			{
				JPH::Shape::GetTrianglesContext context;
				leaf.GetTrianglesStart(context, JPH::AABox::sBiggest(), JPH::RVec3::sZero());
				for (int count = leaf.GetTrianglesNext(context, c_BatchSize, vertices); count > 0; count = leaf.GetTrianglesNext(context, c_BatchSize, vertices))
				{
					for (int t = 0; t < count; ++t)
					{
						onTriangle(&vertices[t * 3]);
					}
				}
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

	namespace PhysicsShapes
	{
		const PhysicsShape* CreateBox(const Vector3& halfExtents, const Vector3& offset)
		{
			// The rounded edge can't be thicker than the box.
			const float convexRadius = std::min(JPH::cDefaultConvexRadius, std::min(halfExtents.x, std::min(halfExtents.y, halfExtents.z)) * 0.5f);
			return TakeOffsetShape(JPH::BoxShapeSettings(ToJolt(halfExtents), convexRadius).Create(), offset, "a box collider");
		}

		const PhysicsShape* CreateSphere(float radius, const Vector3& offset)
		{
			return TakeOffsetShape(JPH::SphereShapeSettings(radius).Create(), offset, "a sphere collider");
		}

		const PhysicsShape* CreateCapsule(float halfHeight, float radius, const Vector3& offset)
		{
			return TakeOffsetShape(JPH::CapsuleShapeSettings(halfHeight, radius).Create(), offset, "a capsule collider");
		}

		const PhysicsShape* CreateScaled(const PhysicsShape* shape, const Vector3& scale)
		{
			const JPH::Shape* joltShape = ToJolt(shape);
			if (!joltShape)
			{
				return nullptr;
			}
			if (scale == Vector3::c_One)
			{
				joltShape->AddRef();
				return shape;
			}
			return TakeShape(JPH::ScaledShapeSettings(joltShape, joltShape->MakeScaleValid(ToJolt(scale))).Create(), "a scaled collider");
		}

		void AddRef(const PhysicsShape* shape)
		{
			if (shape)
			{
				ToJolt(shape)->AddRef();
			}
		}

		bool WriteMeshShapes(BufferedFileStream& stream, const CollisionMeshPiece* pieces, uint pieceCount, const char* meshName)
		{
			const JPH::ShapeRefC triangleShape = CreateTriangleShape(pieces, pieceCount, meshName);
			const JPH::ShapeRefC convexShape = CreateConvexShape(pieces, pieceCount, meshName);

			FileStreamOut out(stream);
			out.Write(c_JoltVersion);
			WriteShape(out, triangleShape);
			WriteShape(out, convexShape);
			return triangleShape != nullptr || convexShape != nullptr;
		}

		void ReadMeshShapes(BufferedFileStream& stream, const PhysicsShape*& outTriangleShape, const PhysicsShape*& outConvexShape)
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
			outTriangleShape = FromJolt(ReadShape(in));
			outConvexShape = FromJolt(ReadShape(in));
		}

		void Release(const PhysicsShape* shape)
		{
			if (shape)
			{
				ToJolt(shape)->Release();
			}
		}

		void AddTriangles(const PhysicsShape* physicsShape, const Vector3& position, const Quaternion& rotation, Array<Vector3>& outTriangles)
		{
			ForEachTriangle(ToJolt(physicsShape), ToJolt(position), ToJolt(rotation), [&outTriangles](const JPH::Float3* triangle)
			{
				for (int v = 0; v < 3; ++v)
				{
					outTriangles.Add(Vector3(triangle[v].x, triangle[v].y, triangle[v].z));
				}
			});
		}

		void AddLines(const PhysicsShape* physicsShape, Array<Vector3>& outLines)
		{
			ForEachTriangle(ToJolt(physicsShape), JPH::Vec3::sZero(), JPH::Quat::sIdentity(), [&outLines](const JPH::Float3* triangle)
			{
				for (int e = 0; e < 3; ++e)
				{
					const JPH::Float3& a = triangle[e];
					const JPH::Float3& b = triangle[(e + 1) % 3];
					outLines.Add(Vector3(a.x, a.y, a.z));
					outLines.Add(Vector3(b.x, b.y, b.z));
				}
			});
		}
	}
}
