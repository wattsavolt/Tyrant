#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "AssetSystem/AssetID.h"
#include "IModelLoader.h"

namespace tyr
{
	// Only forward-declared here on purpose - GltfModelLoader.h pulls in fastgltf's
	// headers, which aren't on every consumer of TyrantEngine's include path (fastgltf is
	// linked PRIVATE). Keeping it to a pointer here means only ModelImporter.cpp, which
	// already needs the full glTF-parsing machinery, has to see that header.
	class GltfModelLoader;
	struct MeshHeader;

	// Imports a 3D model file (glTF/GLB today) into mesh and material assets. Called by the
	// editor when a user imports a model.
	//
	// TODO: only static (non-skeletal) meshes are supported - skeletal import needs its own
	// pass once skins/joints/animations are read by a loader.
	class TYR_ENGINE_API ModelImporter final : public INonCopyable
	{
	public:
		static ModelImporter& Instance();

		// filePath must be absolute. outputFolderPath must be relative to the assets
		// directory - the model's mesh(es) and material(s) are written under it.
		bool ImportModel(const char* filePath, const char* outputFolderPath, const char* modelName) const;

	private:
		ModelImporter();
		~ModelImporter();

		bool ImportMaterials(const ModelImportResult& result, const char* outputFolderPath, const char* modelName, Array<AssetID>& outMaterialIDs) const;
		bool ImportMesh(const ModelImportMesh& mesh, const char* outputFolderPath, const char* meshName, const Array<AssetID>& materialIDs) const;

		// Generates meshlets for one LOD's geometry (one call per submesh internally, so
		// meshlets never mix materials), compresses the result with Zstd, fills in
		// 'header's remaining LOD/chunk fields, and writes the whole mesh file (header then
		// chunk, back to back - see MeshChunkHeader's comment on compressedBlobSize) to
		// absFilePath. header's sphere/aabb/submeshes/materials must already be filled in;
		// its lods array must already have this LOD's entry (submeshOffset/submeshCount set,
		// chunkOffset/chunkCount not yet).
		bool ImportMeshGeometry(const ModelImportMesh& mesh, MeshHeader& header, const char* absFilePath) const;

		// ImportModel is only ever called from the editor, one import at a time, so these
		// are safe to reuse across calls rather than declaring fresh, heap-backed locals
		// every time - see ModelImportResult::Reset()/AddMesh() for how the mesh arrays'
		// capacity specifically gets carried over between imports.
		// TODO: if a second IModelLoader (FBX, etc.) is ever added, this single reused
		// GltfModelLoader won't generalize as-is - it'll need to become one reused loader
		// per format, picked the same way ImportModel already picks by extension.
		mutable URef<GltfModelLoader> m_GltfLoader;
		mutable ModelImportResult m_Result;
		mutable Array<AssetID> m_MaterialIDs;
	};
}
