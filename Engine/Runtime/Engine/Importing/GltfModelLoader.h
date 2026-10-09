#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "IModelLoader.h"

#include <fastgltf/core.hpp>

namespace tyr
{
	// Wraps fastgltf to load glTF/GLB files into a ModelImportResult.
	//
	// Only static (non-skeletal) meshes and core PBR metallic-roughness materials are read.
	// TODO: skins, joints and animations are not read at all yet - skeletal import is a
	// separate, later piece of work.
	//
	// Lifetime note: embedded textures (GLB bufferView-backed images) come back as
	// TextureSource entries pointing straight at bytes owned by this loader's parsed
	// fastgltf::Asset. That means this GltfModelLoader instance must stay alive for as
	// long as the ModelImportResult it produced is still being read - in practice, keep it
	// alive until every material in the result has been handed to MaterialImporter.
	//
	// ModelImporter keeps one GltfModelLoader around and reuses it across imports rather
	// than making a fresh one each time, so Load() resets every field below itself at the
	// top of the call - nothing here can be assumed to still hold only what it held right
	// after construction.
	class GltfModelLoader final : public IModelLoader
	{
	public:
		bool Load(const char* filePath, ModelImportResult& outResult) override;

	private:
		void CollectMeshNodes(size_t nodeIndex, Array<size_t>& outMeshNodeIndices) const;
		ModelImportMesh* LoadMesh(size_t meshIndex, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap);
		bool LoadPreAuthoredLod(size_t meshIndex, ModelImportMesh& mesh, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap);
		bool LoadMeshPrimitives(size_t meshIndex, Array<Vertex>& outVertices, Array<uint>& outIndices,
			Array<ModelImportSubmesh>& outSubmeshes, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap);
		uint ResolveMaterial(size_t gltfMaterialIndex, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap);
		uint GetOrCreateDefaultMaterial(ModelImportResult& outResult);

		// Pre-authored LOD detection, run once per Load() before the main mesh-loading loop -
		// see the .cpp for the two mechanisms (MSFT_lod extension, "<name>LOD<N>" naming
		// convention) and how they combine. Fill m_LodChains (a mesh node -> its ordered
		// alternate-LOD node indices) and m_ConsumedLodNodeIndices (every node already claimed
		// as someone else's alternate, so the main loop doesn't also import it standalone).
		void ParseMsftLodChains();
		void BuildNameBasedLodChains();

		// Keeps the file's bytes and the parsed asset alive for the lifetime of this loader -
		// see the lifetime note above.
		fastgltf::GltfDataBuffer m_DataBuffer;
		fastgltf::Asset m_Asset;

		// Resolved absolute paths for externally-referenced (loose file) textures. Reserved
		// up front to asset.images.size() so pointers we hand out via TextureSource::path
		// stay valid - Array would otherwise invalidate them if it ever had to grow. Cleared
		// (not freed) at the top of Load() so a later, bigger model reuses this capacity.
		Array<Path> m_ResolvedImagePaths;

		// Scratch space for walking the scene graph and deduplicating materials - reused
		// the same way, cleared at the top of Load() rather than declared fresh each time.
		Array<size_t> m_MeshNodeIndices;
		HashMap<uint, uint> m_MaterialIndexMap;

		// A mesh node's index -> its ordered pre-authored alternate-LOD node indices (index 0
		// is LOD1, etc.), and the set of node indices that are themselves such an alternate
		// (and so should be skipped when the main loop walks m_MeshNodeIndices, since they get
		// pulled in via their host's entry here instead of being imported as their own mesh).
		// Both reused across imports like the members above.
		HashMap<uint, LocalArray<uint, MeshConstants::c_MaxLods - 1>> m_LodChains;
		HashMap<uint, bool> m_ConsumedLodNodeIndices;

		bool m_HasDefaultMaterial = false;
		uint m_DefaultMaterialIndex = 0;

		// The model file's folder, which loose texture paths are relative to.
		Path m_BaseDirectory;
	};
}
