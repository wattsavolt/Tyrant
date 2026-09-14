#include "ModelImporter.h"
#include "IModelLoader.h"
#include "GltfModelLoader.h"
#include "MaterialImporter.h"
#include "AssetSystem/MeshAsset.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetConstants.h"
#include <cstring>

namespace tyr
{
	ModelImporter& ModelImporter::Instance()
	{
		static ModelImporter importer;
		return importer;
	}

	ModelImporter::ModelImporter()
		: m_GltfLoader(MakeURef<GltfModelLoader>())
	{
	}

	// Needs to be defined here rather than defaulted in the header - GltfModelLoader is
	// only forward-declared there, and URef's destructor needs the full type.
	ModelImporter::~ModelImporter() = default;

	namespace
	{
		bool HasExtension(const char* filePath, const char* extension)
		{
			const size_t pathLength = std::strlen(filePath);
			const size_t extLength = std::strlen(extension);
			if (extLength > pathLength)
			{
				return false;
			}
			return _stricmp(filePath + (pathLength - extLength), extension) == 0;
		}
	}

	bool ModelImporter::ImportModel(const char* filePath, const char* outputFolderPath, const char* modelName) const
	{
		// Only glTF/GLB is supported for now - more IModelLoader implementations (FBX, etc.)
		// can be added here later without anything downstream needing to change.
		if (!HasExtension(filePath, ".gltf") && !HasExtension(filePath, ".glb"))
		{
			TYR_LOG_ERROR("Model file %s is not a .gltf or .glb file - no loader for it.", filePath);
			return false;
		}

		// m_Result is reused across calls (see the class comment in ModelImporter.h), so it
		// needs clearing of whatever the previous import left in it before this one starts.
		m_Result.Reset();

		// m_GltfLoader owns the parsed data that some of m_Result's TextureSources point at
		// (embedded GLB textures), so it has to stay alive until we're done importing every
		// material below - see GltfModelLoader.h's lifetime note. It's reused too, and
		// resets its own state at the top of Load().
		if (!m_GltfLoader->Load(filePath, m_Result))
		{
			TYR_LOG_ERROR("Failed to load model %s.", filePath);
			return false;
		}

		m_MaterialIDs.Clear();
		if (!ImportMaterials(m_Result, outputFolderPath, modelName, m_MaterialIDs))
		{
			return false;
		}

		if (m_Result.meshCount == 0)
		{
			TYR_LOG_ERROR("Model %s produced no meshes.", filePath);
			return false;
		}

		for (uint i = 0; i < m_Result.meshCount; ++i)
		{
			char meshName[PathConstants::c_MaxAssetNameTotalSize];
			if (m_Result.meshCount == 1)
			{
				snprintf(meshName, sizeof(meshName), "%s", modelName);
			}
			else
			{
				snprintf(meshName, sizeof(meshName), "%s_Mesh%u", modelName, i);
			}

			if (!ImportMesh(m_Result.meshes[i], outputFolderPath, meshName, m_MaterialIDs))
			{
				return false;
			}
		}

		return true;
	}

	bool ModelImporter::ImportMaterials(const ModelImportResult& result, const char* outputFolderPath, const char* modelName, Array<AssetID>& outMaterialIDs) const
	{
		outMaterialIDs.Reserve(result.materials.Size());

		for (uint i = 0; i < result.materials.Size(); ++i)
		{
			PbrMaterialImportDesc desc = result.materials[i];

			char materialName[PathConstants::c_MaxAssetNameTotalSize];
			snprintf(materialName, sizeof(materialName), "%s_Material%u", modelName, i);

			desc.outputFolderPath = outputFolderPath;
			desc.materialName = materialName;

			if (!MaterialImporter::Instance().ImportPbrMaterial(desc))
			{
				TYR_LOG_ERROR("Failed to import material %s for model %s.", materialName, modelName);
				return false;
			}

			// ImportPbrMaterial doesn't hand its new AssetID back directly, but it always
			// registers the material at this exact deterministic path, so we can just look
			// it up rather than changing MaterialImporter's public signature for this.
			char materialPath[PathConstants::c_MaxAssetPathTotalSize];
			snprintf(materialPath, sizeof(materialPath), "%s/%s%s", outputFolderPath, materialName, AssetConstants::c_MaterialFileExtension);

			const AssetID materialID = AssetRegistry::Instance().GetAssetIDSafe(materialPath);
			if (!AssetUtil::IsValidAssetID(materialID))
			{
				TYR_LOG_ERROR("Material %s imported but could not be found in the asset registry afterwards.", materialPath);
				return false;
			}

			outMaterialIDs.Add(materialID);
		}

		return true;
	}

	bool ModelImporter::ImportMesh(const ModelImportMesh& mesh, const char* outputFolderPath, const char* meshName, const Array<AssetID>& materialIDs) const
	{
		MeshHeader header;
		header.sphere = mesh.sphere;
		header.aabbMin = mesh.aabbMin;
		header.aabbMax = mesh.aabbMax;

		// Every submesh's material slot maps 1:1 onto the model's flat material list -
		// GltfModelLoader already dedupes glTF materials into that same flat list, so we
		// just carry every material we imported over as a slot, used or not.
		header.materials.Reserve(materialIDs.Size());
		for (const AssetID& materialID : materialIDs)
		{
			header.materials.Add(materialID);
		}

		header.submeshes.Reserve(mesh.submeshes.Size());
		for (const ModelImportSubmesh& submesh : mesh.submeshes)
		{
			MeshSubmesh& outSubmesh = header.submeshes.ExpandOne();
			// TODO: meshletOffset/meshletCount should describe a range of meshlets once
			// meshlet generation exists. Until then this just carries the material slot -
			// there is no actual GPU-ready geometry payload for this submesh yet.
			outSubmesh.meshletOffset = 0;
			outSubmesh.meshletCount = 0;
			outSubmesh.materialSlot = submesh.materialIndex;
		}

		MeshLODHeader& lod = header.lods.ExpandOne();
		lod.submeshOffset = 0;
		lod.submeshCount = header.submeshes.Size();
		// TODO: meshlet generation and Zstd chunk compression are a separate, not yet
		// designed subsystem - this mesh asset has no chunks, so no drawable geometry
		// payload is actually written yet. The header (bounds, submeshes, materials) is
		// real and complete; the vertex/index/meshlet data behind it is not.
		lod.chunkOffset = 0;
		lod.chunkCount = 0;

		char meshPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(meshPath, sizeof(meshPath), "%s/%s%s", outputFolderPath, meshName, AssetConstants::c_MeshFileExtension);

		char absMeshFolderPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absMeshFolderPath, outputFolderPath);

		{
			std::error_code ec;
			if (!fs::exists(absMeshFolderPath) && !fs::create_directories(absMeshFolderPath, ec))
			{
				TYR_LOG_ERROR("Error creating directory %s.", absMeshFolderPath);
				return false;
			}
		}

		AssetUtil::SaveAsset<MeshHeader>(meshPath, header);

		const AssetID meshID = AssetUtil::CreateAssetID();
		AssetRegistry::Instance().AddAsset(meshID, meshPath, materialIDs.Data(), materialIDs.Size());

		return true;
	}
}
