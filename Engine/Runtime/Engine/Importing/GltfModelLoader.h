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
		void CollectMeshNodes(size_t nodeIndex, Array<size_t>& outMeshIndices) const;
		bool LoadMesh(size_t meshIndex, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap);
		uint ResolveMaterial(size_t gltfMaterialIndex, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap);
		uint GetOrCreateDefaultMaterial(ModelImportResult& outResult);

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
		Array<size_t> m_MeshIndices;
		HashMap<uint, uint> m_MaterialIndexMap;

		bool m_HasDefaultMaterial = false;
		uint m_DefaultMaterialIndex = 0;

		std::filesystem::path m_BaseDirectory;
	};
}
