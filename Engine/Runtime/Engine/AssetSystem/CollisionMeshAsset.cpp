#include "CollisionMeshAsset.h"
#include "Physics/CollisionShapes.h"

namespace tyr
{
	void CollisionMesh::Reset()
	{
		CollisionShapes::Release(triangleShape);
		CollisionShapes::Release(convexShape);
		triangleShape = nullptr;
		convexShape = nullptr;
#if TYR_EDITOR
		triangleLines.Clear();
		convexLines.Clear();
#endif
	}

	template<>
	void Deserialize(BufferedFileStream& stream, CollisionMesh& collisionMesh)
	{
		uint fileVersion = 0;
		Deserialize<uint>(stream, fileVersion);
		if (fileVersion != CollisionMesh::c_FileVersion)
		{
			TYR_LOG_ERROR("A mesh collider was saved in an older format. Reimport its mesh.");
			return;
		}

		CollisionShapes::Read(stream, collisionMesh.triangleShape, collisionMesh.convexShape);
#if TYR_EDITOR
		CollisionShapes::AddLines(collisionMesh.triangleShape, collisionMesh.triangleLines);
		CollisionShapes::AddLines(collisionMesh.convexShape, collisionMesh.convexLines);
#endif
	}
}
