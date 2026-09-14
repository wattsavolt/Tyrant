#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "MaterialImporter.h"
#include "RenderResource/Vertex.h"
#include "Geometry/BoundingSphere.h"

namespace tyr
{
	// One submesh - what glTF calls a primitive, or what Unreal calls a section. It's a
	// contiguous range of indices that all use the same material.
	struct ModelImportSubmesh
	{
		uint indexOffset = 0;
		uint indexCount = 0;
		// Index into ModelImportResult::materials.
		uint materialIndex = 0;
	};

	// One mesh worth of geometry. By the time a loader hands this back, it must already be
	// in the engine's own left-handed space, wound correctly, and laid out as tyr::Vertex -
	// any conversion a source format needs happens inside the loader, never after this point.
	struct ModelImportMesh
	{
		Array<Vertex> vertices;
		Array<uint> indices;
		Array<ModelImportSubmesh> submeshes;
		Vector3 aabbMin;
		Vector3 aabbMax;
		BoundingSphere sphere;

		// Clears this mesh's data but keeps every array's capacity, so the next model
		// imported into this same slot reuses the memory instead of freeing and
		// reallocating it.
		void Reset()
		{
			vertices.Clear();
			indices.Clear();
			submeshes.Clear();
			aabbMin = Vector3();
			aabbMax = Vector3();
			sphere = BoundingSphere();
		}
	};

	// Everything pulled out of a model file, ready for ModelImporter to hand off to
	// MaterialImporter and write out as mesh assets.
	struct ModelImportResult
	{
		Array<ModelImportMesh> meshes;
		// Shared across every mesh in this result - a submesh's materialIndex indexes here.
		Array<PbrMaterialImportDesc> materials;

		// How many of `meshes` belong to the current import. meshes.Size() only ever
		// grows - a slot past meshCount still holds a previous import's (already-Reset)
		// arrays, kept around so their capacity gets reused instead of freed and
		// reallocated on the next import.
		uint meshCount = 0;

		// Returns the next mesh slot to fill in, reusing a slot (and its arrays' existing
		// capacity) left over from a previous import if one is available.
		ModelImportMesh& AddMesh()
		{
			if (meshCount < meshes.Size())
			{
				return meshes[meshCount++];
			}
			meshCount++;
			return meshes.ExpandOne();
		}

		// Clears everything but keeps every mesh slot's arrays' capacity - see
		// ModelImportMesh::Reset() and AddMesh() above.
		void Reset()
		{
			for (ModelImportMesh& mesh : meshes)
			{
				mesh.Reset();
			}
			meshCount = 0;
			materials.Clear();
		}
	};

	// A loader wraps one specific file format (glTF today, maybe FBX or others later) and
	// normalizes it into a ModelImportResult. This is the only interface ModelImporter
	// talks to, so adding another format later just means adding another IModelLoader and
	// picking it by file extension - nothing downstream needs to change.
	//
	// Lifetime note: some loaders (GltfModelLoader included) hand back TextureSource
	// entries in ModelImportResult that point at memory the loader itself owns. The loader
	// instance must stay alive for as long as that data is needed - see GltfModelLoader.h.
	class IModelLoader
	{
	public:
		virtual ~IModelLoader() = default;

		virtual bool Load(const char* filePath, ModelImportResult& outResult) = 0;
	};
}
