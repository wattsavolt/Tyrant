#include "ModelImporter.h"
#include "IModelLoader.h"
#include "GltfModelLoader.h"
#include "MaterialImporter.h"
#include "AssetSystem/MeshAsset.h"
#include "AssetSystem/AssetUtil.h"
#include "AssetSystem/AssetRegistry.h"
#include "AssetSystem/AssetConstants.h"
#include "IO/BufferedFileStream.h"
#include "Memory/StackAllocation.h"
#include "RenderResource/MeshUtil.h"
#include "Shaders/ShaderTypes.h"
#include <cstring>
#include <meshoptimizer.h>
#include <zstd.h>

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
			// meshletOffset/meshletCount get filled in by ImportMeshGeometry below, once it's
			// actually built this submesh's meshlets.
			outSubmesh.meshletOffset = 0;
			outSubmesh.meshletCount = 0;
			outSubmesh.materialSlot = submesh.materialIndex;
		}

		MeshLODHeader& lod = header.lods.ExpandOne();
		lod.submeshOffset = 0;
		lod.submeshCount = header.submeshes.Size();
		// chunkOffset/chunkCount get filled in by ImportMeshGeometry below, once it knows
		// how many chunks this LOD actually produced.

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

		char absMeshFilePath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absMeshFilePath, meshPath);

		if (!ImportMeshGeometry(mesh, header, absMeshFilePath))
		{
			TYR_LOG_ERROR("Failed to export geometry for mesh %s.", meshName);
			return false;
		}

		const AssetID meshID = AssetUtil::CreateAssetID();
		AssetRegistry::Instance().AddAsset(meshID, meshPath, materialIDs.Data(), materialIDs.Size());

		return true;
	}

	namespace
	{
		// 64 vertices / 124 triangles is the commonly recommended sweet spot for good
		// occupancy on typical mesh-shader hardware.
		constexpr size_t c_MaxMeshletVertices = 64;
		constexpr size_t c_MaxMeshletTriangles = 124;
		// No cone culling yet - GeometryPass doesn't do backface/cone culling yet. Revisit
		// once it does; meshoptimizer just needs a value between 0 and 1 to weight for it.
		constexpr float c_MeshletConeWeight = 0.0f;
		// Offline, import-time only, so this favors compression ratio over speed. 19 is
		// zstd's own "very good compression, still reasonable time" recommendation - its
		// "ultra" levels (20+) cost a lot more time for comparatively little extra gain.
		constexpr int c_ZstdCompressionLevel = 19;
		// Matches Serializer's own internal buffer size (see Serializer::Serializer).
		constexpr size_t c_StreamBufferSize = 65536;
	}

	bool ModelImporter::ImportMeshGeometry(const ModelImportMesh& mesh, MeshHeader& header, const char* absFilePath) const
	{
		const uint indexCount = mesh.indices.Size();
		if (indexCount == 0 || mesh.vertices.Size() == 0)
		{
			TYR_LOG_ERROR("Mesh has no geometry to export.");
			return false;
		}

		// Worst-case bounds for everything this LOD accumulates across all its submeshes -
		// meshlet_vertices/meshlet_triangles can never exceed indexCount each (per
		// meshoptimizer's own docs), so indexCount safely bounds every array below too.
		const size_t maxMeshlets = meshopt_buildMeshletsBound(indexCount, c_MaxMeshletVertices, c_MaxMeshletTriangles);

		SmartStack<MeshChunkMeshlet> meshletsStack = SmartStackAlloc<MeshChunkMeshlet>((uint)maxMeshlets);
		SmartStack<ShaderVertex> verticesStack = SmartStackAlloc<ShaderVertex>(indexCount);
		SmartStack<uint> indicesStack = SmartStackAlloc<uint>(indexCount);
		MeshChunkMeshlet* meshlets = meshletsStack;
		ShaderVertex* vertices = verticesStack;
		uint* indices = indicesStack;

		uint meshletCount = 0;
		uint vertexCount = 0;
		uint outIndexCount = 0;

		for (uint s = 0; s < mesh.submeshes.Size(); ++s)
		{
			const ModelImportSubmesh& submesh = mesh.submeshes[s];
			const uint* submeshIndices = mesh.indices.Data() + submesh.indexOffset;

			const size_t submeshMaxMeshlets = meshopt_buildMeshletsBound(submesh.indexCount, c_MaxMeshletVertices, c_MaxMeshletTriangles);
			SmartStack<meshopt_Meshlet> rawMeshletsStack = SmartStackAlloc<meshopt_Meshlet>((uint)submeshMaxMeshlets);
			SmartStack<uint> meshletVerticesStack = SmartStackAlloc<uint>(submesh.indexCount);
			SmartStack<uint8> meshletTrianglesStack = SmartStackAlloc<uint8>(submesh.indexCount);
			meshopt_Meshlet* rawMeshlets = rawMeshletsStack;
			uint* meshletVertices = meshletVerticesStack;
			uint8* meshletTriangles = meshletTrianglesStack;

			size_t rawMeshletCount;
			{
				// Optimize vertex cache locality for this submesh's triangles before
				// clustering - meshoptimizer's own recommended order, improves GPU vertex
				// shader cache hits. Scoped on its own so this buffer is freed the moment
				// meshopt_buildMeshlets is done reading it, instead of sitting allocated
				// alongside the (differently-typed) meshlet output buffers above for the
				// rest of the submesh.
				SmartStack<uint> cacheOptimizedIndicesStack = SmartStackAlloc<uint>(submesh.indexCount);
				uint* cacheOptimizedIndices = cacheOptimizedIndicesStack;
				meshopt_optimizeVertexCache(cacheOptimizedIndices, submeshIndices, submesh.indexCount, mesh.vertices.Size());

				rawMeshletCount = meshopt_buildMeshlets(
					rawMeshlets, meshletVertices, meshletTriangles,
					cacheOptimizedIndices, submesh.indexCount,
					&mesh.vertices[0].position.x, mesh.vertices.Size(), sizeof(Vertex),
					c_MaxMeshletVertices, c_MaxMeshletTriangles, c_MeshletConeWeight);
			}

			MeshSubmesh& outSubmesh = header.submeshes[s];
			outSubmesh.meshletOffset = meshletCount;
			outSubmesh.meshletCount = (uint)rawMeshletCount;

			for (size_t m = 0; m < rawMeshletCount; ++m)
			{
				const meshopt_Meshlet& raw = rawMeshlets[m];

				MeshChunkMeshlet& outMeshlet = meshlets[meshletCount++];
				outMeshlet.vertexOffset = vertexCount;
				outMeshlet.vertexCount = raw.vertex_count;
				outMeshlet.indexOffset = outIndexCount;
				outMeshlet.indexCount = raw.triangle_count * 3;
				outMeshlet.materialSlot = outSubmesh.materialSlot;

				for (uint v = 0; v < raw.vertex_count; ++v)
				{
					const uint originalVertexIndex = meshletVertices[raw.vertex_offset + v];
					MeshUtil::VertexToShaderVertex(mesh.vertices[originalVertexIndex], vertices[vertexCount++]);
				}

				const uint triangleIndexCount = raw.triangle_count * 3;
				for (uint i = 0; i < triangleIndexCount; ++i)
				{
					indices[outIndexCount++] = meshletTriangles[raw.triangle_offset * 3 + i];
				}
			}
		}

		// Meshlets, then vertices, then indices, back to back - each region's size is
		// already a multiple of 4 bytes (all three element types are uint-sized-or-larger),
		// so no extra padding is needed to keep every region naturally aligned.
		MeshChunkHeader& chunk = header.chunks.ExpandOne();
		chunk.decompressedMeshletsSize = meshletCount * (uint)sizeof(MeshChunkMeshlet);
		chunk.decompressedVerticesSize = vertexCount * (uint)sizeof(ShaderVertex);
		chunk.decompressedIndicesSize = outIndexCount * (uint)sizeof(uint);
		chunk.meshletsOffset = 0;
		chunk.verticesOffset = chunk.decompressedMeshletsSize;
		chunk.indicesOffset = chunk.verticesOffset + chunk.decompressedVerticesSize;
		chunk.decompressedBlobSize = chunk.indicesOffset + chunk.decompressedIndicesSize;

		SmartStack<uint8> decompressedBlobStack = SmartStackAlloc<uint8>(chunk.decompressedBlobSize);
		uint8* decompressedBlob = decompressedBlobStack;
		memcpy(decompressedBlob + chunk.meshletsOffset, meshlets, chunk.decompressedMeshletsSize);
		memcpy(decompressedBlob + chunk.verticesOffset, vertices, chunk.decompressedVerticesSize);
		memcpy(decompressedBlob + chunk.indicesOffset, indices, chunk.decompressedIndicesSize);

		const size_t compressedBound = ZSTD_compressBound(chunk.decompressedBlobSize);
		SmartStack<uint8> compressedBlobStack = SmartStackAlloc<uint8>((uint)compressedBound);
		uint8* compressedBlob = compressedBlobStack;

		const size_t compressedSize = ZSTD_compress(compressedBlob, compressedBound, decompressedBlob, chunk.decompressedBlobSize, c_ZstdCompressionLevel);
		if (ZSTD_isError(compressedSize))
		{
			TYR_LOG_ERROR("Failed to compress mesh geometry: %s", ZSTD_getErrorName(compressedSize));
			return false;
		}
		chunk.compressedBlobSize = (uint)compressedSize;

		MeshLODHeader& lod = header.lods.Back();
		lod.chunkOffset = header.chunks.Size() - 1;
		lod.chunkCount = 1;

		SmartStack<uint8> streamBufferStack = SmartStackAlloc<uint8>((uint)c_StreamBufferSize);
		BufferedFileStream stream(streamBufferStack, c_StreamBufferSize, absFilePath, BinaryStream::Operation::Write, true);
		Serialize<MeshHeader>(stream, header);
		stream.Write(compressedBlob, compressedSize);

		return true;
	}
}
