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

	bool ModelImporter::ImportModel(const char* filePath, const char* outputFolderPath, const char* modelName, const ModelImportOptions& options) const
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

			if (!ImportMesh(m_Result.meshes[i], outputFolderPath, meshName, m_MaterialIDs, options))
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

	bool ModelImporter::ImportMesh(const ModelImportMesh& mesh, const char* outputFolderPath, const char* meshName,
		const Array<AssetID>& materialIDs, const ModelImportOptions& options) const
	{
		// Reused rather than a fresh local - see m_Header's own comment in the header.
		m_Header.Reset();
		m_Header.sphere = mesh.sphere;
		m_Header.aabbMin = mesh.aabbMin;
		m_Header.aabbMax = mesh.aabbMax;

		// Every submesh's material slot maps 1:1 onto the model's flat material list -
		// GltfModelLoader already dedupes glTF materials into that same flat list, so we
		// just carry every material we imported over as a slot, used or not.
		m_Header.materials.Reserve(materialIDs.Size());
		for (const AssetID& materialID : materialIDs)
		{
			m_Header.materials.Add(materialID);
		}

		// m_Header.lods/submeshes/chunks are filled in by ImportMeshGeometry below, one LOD at a
		// time - every LOD shares the same submesh/material structure as the source mesh, just
		// with progressively simplified geometry, so there's nothing LOD-independent to set up
		// here first.

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

		if (!ImportMeshGeometry(mesh, m_Header, options, absMeshFilePath))
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

		// Each generated LOD targets half the previous level's triangle count - a simple,
		// commonly used progression (LOD1 ~50% of LOD0, LOD2 ~25%, LOD3 ~12.5%, ...).
		constexpr float c_LodIndexCountFactor = 0.5f;
		// Relative error allowed during simplification, as a fraction of the mesh's own extents.
		// Generous on purpose so meshopt_simplify prioritizes reaching the target triangle count
		// over stopping early - it never exceeds the mesh's actual topology constraints
		// regardless of how high this is set.
		constexpr float c_LodSimplifyTargetError = 0.1f;
		// Never simplify a submesh below this many triangles.
		constexpr uint c_MinLodTriangleCount = 8;

		// Rounds down to a whole number of triangles, never below c_MinLodTriangleCount.
		uint ComputeLodTargetIndexCount(uint originalIndexCount, uint lodIndex)
		{
			const float scale = Math::Pow(c_LodIndexCountFactor, (float)lodIndex);
			uint target = (uint)(originalIndexCount * scale);
			target -= target % 3;

			const uint minIndexCount = c_MinLodTriangleCount * 3;
			return target > minIndexCount ? target : minIndexCount;
		}
	}

	bool ModelImporter::ImportMeshGeometry(const ModelImportMesh& mesh, MeshHeader& header, const ModelImportOptions& options, const char* absFilePath) const
	{
		const uint indexCount = mesh.indices.Size();
		if (indexCount == 0 || mesh.vertices.Size() == 0)
		{
			TYR_LOG_ERROR("Mesh has no geometry to export.");
			return false;
		}

		// Pre-authored LODs the source file already provided are never discarded, even if that's
		// more than requested or the generate-LODs option is off entirely - they cost nothing to
		// include (no simplification work, already in memory). The requested count only ever
		// asks for MORE than that baseline, via simplification, never fewer.
		uint additionalLods = mesh.preAuthoredLods.Size();
		if (options.generateLods && options.lodCount > additionalLods)
		{
			additionalLods = options.lodCount;
		}
		additionalLods = Math::Clamp(additionalLods, 0u, (uint)MeshConstants::c_MaxLods - 1);
		const uint lodLevelCount = 1 + additionalLods;

		m_LodCompressedBlobs.Clear();

		for (uint lodIndex = 0; lodIndex < lodLevelCount; ++lodIndex)
		{
			// Which geometry this LOD actually builds from: LOD0 is always the mesh's own
			// original data; LOD1+ uses the source file's own pre-authored alternate if one
			// exists for this level (and forceLodGeneration isn't overriding that), otherwise
			// falls back to simplifying LOD0's own geometry down, as before.
			const ModelImportMeshLod* preAuthored = nullptr;
			if (lodIndex > 0)
			{
				const uint preAuthoredIndex = lodIndex - 1;
				if (!options.forceLodGeneration && preAuthoredIndex < mesh.preAuthoredLods.Size())
				{
					preAuthored = &mesh.preAuthoredLods[preAuthoredIndex];
				}
			}
			const bool needsSimplification = (lodIndex > 0) && (preAuthored == nullptr);

			const Array<Vertex>& lodVertices = preAuthored ? preAuthored->vertices : mesh.vertices;
			const Array<uint>& lodIndices = preAuthored ? preAuthored->indices : mesh.indices;
			const Array<ModelImportSubmesh>& lodSubmeshes = preAuthored ? preAuthored->submeshes : mesh.submeshes;
			const uint lodIndexCount = lodIndices.Size();

			MeshLODHeader& lod = header.lods.ExpandOne();
			lod.submeshOffset = header.submeshes.Size();
			lod.submeshCount = lodSubmeshes.Size();

			for (const ModelImportSubmesh& submesh : lodSubmeshes)
			{
				MeshSubmesh& outSubmesh = header.submeshes.ExpandOne();
				// meshletOffset/meshletCount get filled in below, once this LOD's meshlets for
				// this submesh have actually been built.
				outSubmesh.meshletOffset = 0;
				outSubmesh.meshletCount = 0;
				outSubmesh.materialSlot = submesh.materialIndex;
			}

			// Worst-case bounds for everything this LOD accumulates across all its submeshes -
			// meshlet_vertices/meshlet_triangles can never exceed this LOD's own total index
			// count each (per meshoptimizer's own docs), so that safely bounds every array below.
			const size_t maxMeshlets = meshopt_buildMeshletsBound(lodIndexCount, c_MaxMeshletVertices, c_MaxMeshletTriangles);

			SmartStack<MeshChunkMeshlet> meshletsStack = SmartStackAlloc<MeshChunkMeshlet>((uint)maxMeshlets);
			SmartStack<ShaderVertex> verticesStack = SmartStackAlloc<ShaderVertex>(lodIndexCount);
			SmartStack<uint> indicesStack = SmartStackAlloc<uint>(lodIndexCount);
			MeshChunkMeshlet* meshlets = meshletsStack;
			ShaderVertex* vertices = verticesStack;
			uint* indices = indicesStack;

			uint meshletCount = 0;
			uint vertexCount = 0;
			uint outIndexCount = 0;

			for (uint s = 0; s < lodSubmeshes.Size(); ++s)
			{
				const ModelImportSubmesh& submesh = lodSubmeshes[s];
				const uint* originalSubmeshIndices = lodIndices.Data() + submesh.indexOffset;

				const uint* submeshIndices = originalSubmeshIndices;
				uint submeshIndexCount = submesh.indexCount;

				// Always allocated (even when unused) so this stays a simple value, not a
				// reassignment - SmartStack can't be move-assigned, only move-constructed, so a
				// conditionally-sized declaration would need duplicating this whole block per
				// branch instead.
				SmartStack<uint> simplifiedIndicesStack = SmartStackAlloc<uint>(submesh.indexCount);
				if (needsSimplification)
				{
					uint* simplifiedIndices = simplifiedIndicesStack;
					const uint targetIndexCount = ComputeLodTargetIndexCount(submesh.indexCount, lodIndex);

					// Simplifying per-submesh (rather than the whole LOD's indices at once)
					// keeps each submesh's own material assignment exact, matching how meshlets
					// are already built per-submesh below. meshopt_SimplifyLockBorder keeps
					// vertices on a submesh's own open edges - including the seams where it
					// touches a neighbouring submesh - from moving independently, which would
					// otherwise open visible cracks between submeshes simplified separately.
					const size_t newIndexCount = meshopt_simplify(simplifiedIndices, originalSubmeshIndices, submesh.indexCount,
						&lodVertices[0].position.x, lodVertices.Size(), sizeof(Vertex),
						targetIndexCount, c_LodSimplifyTargetError, meshopt_SimplifyLockBorder, nullptr);

					submeshIndices = simplifiedIndices;
					submeshIndexCount = (uint)newIndexCount;
				}

				const size_t submeshMaxMeshlets = meshopt_buildMeshletsBound(submeshIndexCount, c_MaxMeshletVertices, c_MaxMeshletTriangles);
				SmartStack<meshopt_Meshlet> rawMeshletsStack = SmartStackAlloc<meshopt_Meshlet>((uint)submeshMaxMeshlets);
				SmartStack<uint> meshletVerticesStack = SmartStackAlloc<uint>(submeshIndexCount);
				SmartStack<uint8> meshletTrianglesStack = SmartStackAlloc<uint8>(submeshIndexCount);
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
					SmartStack<uint> cacheOptimizedIndicesStack = SmartStackAlloc<uint>(submeshIndexCount);
					uint* cacheOptimizedIndices = cacheOptimizedIndicesStack;
					meshopt_optimizeVertexCache(cacheOptimizedIndices, submeshIndices, submeshIndexCount, lodVertices.Size());

					rawMeshletCount = meshopt_buildMeshlets(
						rawMeshlets, meshletVertices, meshletTriangles,
						cacheOptimizedIndices, submeshIndexCount,
						&lodVertices[0].position.x, lodVertices.Size(), sizeof(Vertex),
						c_MaxMeshletVertices, c_MaxMeshletTriangles, c_MeshletConeWeight);
				}

				MeshSubmesh& outSubmesh = header.submeshes[lod.submeshOffset + s];
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
						MeshUtil::VertexToShaderVertex(lodVertices[originalVertexIndex], vertices[vertexCount++]);
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
			uint8* compressedBlobScratch = compressedBlobStack;

			const size_t compressedSize = ZSTD_compress(compressedBlobScratch, compressedBound, decompressedBlob, chunk.decompressedBlobSize, c_ZstdCompressionLevel);
			if (ZSTD_isError(compressedSize))
			{
				TYR_LOG_ERROR("Failed to compress mesh geometry: %s", ZSTD_getErrorName(compressedSize));
				return false;
			}
			chunk.compressedBlobSize = (uint)compressedSize;

			lod.chunkOffset = header.chunks.Size() - 1;
			lod.chunkCount = 1;

			// Copied out of the stack-scratch buffer above into a slot that survives past this
			// loop iteration - every LOD's compressed bytes need to stay alive until the header
			// (serialized further down, after every LOD has been built) has been written, since
			// chunks are written to the file only after it, back to back, in this same order.
			Array<uint8>& blob = m_LodCompressedBlobs.ExpandOne();
			blob.Resize((uint)compressedSize);
			memcpy(blob.Data(), compressedBlobScratch, compressedSize);
		}

		SmartStack<uint8> streamBufferStack = SmartStackAlloc<uint8>((uint)c_StreamBufferSize);
		BufferedFileStream stream(streamBufferStack, c_StreamBufferSize, absFilePath, BinaryStream::Operation::Write, true);
		Serialize<MeshHeader>(stream, header);
		for (uint i = 0; i < m_LodCompressedBlobs.Size(); ++i)
		{
			const Array<uint8>& blob = m_LodCompressedBlobs[i];
			stream.Write(blob.Data(), blob.Size());
		}

		return true;
	}
}
