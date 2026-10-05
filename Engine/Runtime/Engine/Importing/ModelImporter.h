#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "AssetSystem/AssetID.h"
#include "AssetSystem/MeshAsset.h"
#include "IModelLoader.h"

namespace tyr
{
	// Only forward-declared here on purpose - GltfModelLoader.h pulls in fastgltf's
	// headers, which aren't on every consumer of TyrantEngine's include path (fastgltf is
	// linked PRIVATE). Keeping it to a pointer here means only ModelImporter.cpp, which
	// already needs the full glTF-parsing machinery, has to see that header.
	class GltfModelLoader;

	// Only applies to rigid (static, non-skeletal) meshes - skeletal LOD generation needs
	// skin-weight-aware simplification, which this doesn't do, and ModelImporter doesn't import
	// skeletal meshes at all yet regardless (see the class comment below).
	struct ModelImportOptions
	{
		bool generateLods = true;
		// How many LODs to generate beyond LOD0 (the original, highest-detail mesh). Pre-authored
		// LODs the source file already provides (see GltfModelLoader) are never discarded even
		// if this is lower than how many of those exist - see ImportMeshGeometry.
		uint lodCount = 3;
		// Generates LODs via simplification even for a level the source file already provided
		// (detected via the MSFT_lod extension or a "<name>LOD<N>" naming convention - see
		// GltfModelLoader), instead of using that pre-authored geometry as-is.
		bool forceLodGeneration = false;
	};

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
		bool ImportModel(const char* filePath, const char* outputFolderPath, const char* modelName,
			const ModelImportOptions& options = ModelImportOptions()) const;

	private:
		ModelImporter();
		~ModelImporter();

		bool ImportMaterials(const ModelImportResult& result, const char* outputFolderPath, const char* modelName, Array<AssetID>& outMaterialIDs) const;
		bool ImportMesh(const ModelImportMesh& mesh, const char* outputFolderPath, const char* meshName,
			const Array<AssetID>& materialIDs, const ModelImportOptions& options) const;

		// Builds every requested LOD's meshlets (one call per submesh internally, so meshlets
		// never mix materials), simplifying the geometry via meshoptimizer for LOD1+, compresses
		// each LOD's result with Zstd, fills in 'header's lods/submeshes/chunks, and writes the
		// whole mesh file (header, then every LOD's chunk back to back - see MeshChunkHeader's
		// comment on compressedBlobSize) to absFilePath. header's sphere/aabb/materials must
		// already be filled in; lods/submeshes/chunks must still be empty - this fills them in.
		bool ImportMeshGeometry(const ModelImportMesh& mesh, MeshHeader& header, const ModelImportOptions& options, const char* absFilePath) const;

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
		// One mesh's header, reused the same way as the members above (ImportMesh calls
		// m_Header.Reset() rather than declaring a fresh MeshHeader local) - MeshHeader owns
		// several Array<T> members (chunks/submeshes/materials) that would otherwise heap-churn
		// on every single mesh imported.
		mutable MeshHeader m_Header;
		// Holds each LOD's compressed chunk bytes until every LOD has been built and the header
		// (which lives before them in the file and needs their final sizes) has been written -
		// see ImportMeshGeometry. One slot per LOD, reused/regrown across imports like the
		// members above rather than reallocated.
		mutable LocalArray<Array<uint8>, MeshConstants::c_MaxLods> m_LodCompressedBlobs;
	};
}
