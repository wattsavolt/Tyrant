#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "MaterialImporter.h"
#include "RenderResource/Vertex.h"
#include "RenderResource/MeshDesc.h"
#include "Geometry/BoundingSphere.h"
#include "AssetSystem/SkeletonAsset.h"
#include "AssetSystem/AnimationAsset.h"

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

	// Up to four bones per vertex, as skeleton bone indices. Unused slots have a weight of 0.
	struct ModelImportSkinWeights
	{
		uint8 bones[4];
		float weights[4];
	};

	// One pre-authored LOD's geometry (LOD1+) - a source file's own alternate, already at the
	// detail level it should be used at, as opposed to one ModelImporter generates itself via
	// simplification. LOD0 lives directly on ModelImportMesh below, not here.
	struct ModelImportMeshLod
	{
		Array<Vertex> vertices;
		Array<uint> indices;
		Array<ModelImportSubmesh> submeshes;

		void Reset()
		{
			vertices.Clear();
			indices.Clear();
			submeshes.Clear();
		}
	};

	// One mesh worth of geometry. By the time a loader hands this back, it must already be
	// in the engine's own left-handed space, wound correctly, and laid out as tyr::Vertex -
	// any conversion a source format needs happens inside the loader, never after this point.
	struct ModelImportMesh
	{
		// The source mesh's name, kept to characters that are safe in a file name. May be empty.
		LocalString<63> name;
		// The name of the node it was found on, kept the same way. Blender exports objects as nodes.
		LocalString<63> nodeName;
		Array<Vertex> vertices;
		Array<uint> indices;
		Array<ModelImportSubmesh> submeshes;
		// One per vertex for a mesh bound to the result's skeleton, otherwise empty.
		Array<ModelImportSkinWeights> skinWeights;
		Vector3 aabbMin;
		Vector3 aabbMax;
		BoundingSphere sphere;

		// Additional LODs the source file already provided (via the MSFT_lod extension or a
		// "<name>LOD<N>" naming convention) beyond LOD0 above - empty when the source only has
		// one LOD, which is the common case. Index 0 here is LOD1, index 1 is LOD2, and so on.
		LocalArray<ModelImportMeshLod, MeshConstants::c_MaxLods - 1> preAuthoredLods;

		// Clears this mesh's data but keeps every array's capacity, so the next model
		// imported into this same slot reuses the memory instead of freeing and
		// reallocating it.
		void Reset()
		{
			name.Reset();
			nodeName.Reset();
			vertices.Clear();
			indices.Clear();
			submeshes.Clear();
			skinWeights.Clear();
			aabbMin = Vector3();
			aabbMax = Vector3();
			sphere = BoundingSphere();
			preAuthoredLods.Clear();
		}
	};

	// One animation, already resampled into the result's skeleton bone order.
	struct ModelImportAnimation
	{
		LocalString<63> name;
		AnimationClip clip;
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

		// The skeleton skinned meshes are bound to, empty when nothing is skinned.
		Skeleton skeleton;
		// Reused across imports like meshes, see meshCount.
		Array<ModelImportAnimation> animations;
		uint animationCount = 0;

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

		ModelImportAnimation& AddAnimation()
		{
			if (animationCount < animations.Size())
			{
				return animations[animationCount++];
			}
			animationCount++;
			return animations.ExpandOne();
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
			skeleton.Reset();
			for (ModelImportAnimation& animation : animations)
			{
				animation.name.Reset();
				animation.clip.Reset();
			}
			animationCount = 0;
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
