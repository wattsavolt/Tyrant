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
#include "Utility/PathUtil.h"
#include "RenderResource/MeshUtil.h"
#include "Shaders/ShaderTypes.h"
#include "AssetSystem/CollisionMeshAsset.h"
#include "Platform/Platform.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <meshoptimizer.h>
#include <zstd.h>

namespace tyr
{
	namespace
	{
		// The same file, or the same bytes inside the model.
		bool IsSameSource(const TextureSource& a, const TextureSource& b)
		{
			if (a.path || b.path)
			{
				return a.path && b.path && strcmp(a.path, b.path) == 0;
			}
			return a.data == b.data && a.dataSize == b.dataSize;
		}
	}

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

		constexpr const char* c_CollisionMeshPrefix = "UCX_";
		constexpr size_t c_CollisionMeshPrefixLength = 4;

		bool HasCollisionPrefix(const char* name)
		{
			return strncmp(name, c_CollisionMeshPrefix, c_CollisionMeshPrefixLength) == 0;
		}

		bool IsCollisionMesh(const ModelImportMesh& mesh)
		{
			return HasCollisionPrefix(mesh.name.CStr()) || HasCollisionPrefix(mesh.nodeName.CStr());
		}

		// The name after a UCX_ mesh's prefix, such as Wall_400 or Wall_400_01.
		const char* GetCollisionTarget(const ModelImportMesh& collisionMesh)
		{
			const char* name = HasCollisionPrefix(collisionMesh.name.CStr()) ? collisionMesh.name.CStr() : collisionMesh.nodeName.CStr();
			return name + c_CollisionMeshPrefixLength;
		}

		// Exactly the name, or unless exact is set, the name followed by _ and a number.
		bool MatchesCollisionTarget(const char* target, const char* name, bool exact)
		{
			const size_t length = strlen(name);
			if (length == 0 || strncmp(target, name, length) != 0)
			{
				return false;
			}
			const char* rest = target + length;
			if (exact || *rest == '\0')
			{
				return *rest == '\0';
			}
			if (rest[0] != '_' || rest[1] == '\0')
			{
				return false;
			}
			for (++rest; *rest != '\0'; ++rest)
			{
				if (!std::isdigit(static_cast<unsigned char>(*rest)))
				{
					return false;
				}
			}
			return true;
		}

		bool MatchesCollisionTarget(const char* target, const ModelImportMesh& mesh, bool exact)
		{
			return MatchesCollisionTarget(target, mesh.name.CStr(), exact) || MatchesCollisionTarget(target, mesh.nodeName.CStr(), exact);
		}

		// A numbered UCX_ mesh only belongs to a mesh when no other mesh has its whole name.
		bool IsCollisionFor(const ModelImportResult& result, const ModelImportMesh& collisionMesh, const ModelImportMesh& mesh)
		{
			const char* target = GetCollisionTarget(collisionMesh);
			if (MatchesCollisionTarget(target, mesh, true))
			{
				return true;
			}
			if (!MatchesCollisionTarget(target, mesh, false))
			{
				return false;
			}
			for (uint i = 0; i < result.meshCount; ++i)
			{
				if (!IsCollisionMesh(result.meshes[i]) && MatchesCollisionTarget(target, result.meshes[i], true))
				{
					return false;
				}
			}
			return true;
		}

		bool HasEarlierMeshNamed(const ModelImportResult& result, uint meshIndex)
		{
			for (uint i = 0; i < meshIndex; ++i)
			{
				if (strcmp(result.meshes[i].name.CStr(), result.meshes[meshIndex].name.CStr()) == 0)
				{
					return true;
				}
			}
			return false;
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

		AssetID skeletonID = AssetConstants::c_InvalidAssetID;
		if (m_Result.skeleton.BoneCount() > 0 && !ImportSkeletonAndAnimations(m_Result, outputFolderPath, modelName, skeletonID))
		{
			return false;
		}

		uint renderMeshCount = 0;
		for (uint i = 0; i < m_Result.meshCount; ++i)
		{
			renderMeshCount += IsCollisionMesh(m_Result.meshes[i]) ? 0 : 1;
		}

		for (uint i = 0; i < m_Result.meshCount; ++i)
		{
			// A model holding several meshes, such as a kit of building pieces, names each after its source mesh.
			const ModelImportMesh& mesh = m_Result.meshes[i];
			if (IsCollisionMesh(mesh))
			{
				continue;
			}
			char meshName[PathConstants::c_MaxAssetNameTotalSize];
			if (renderMeshCount == 1)
			{
				snprintf(meshName, sizeof(meshName), "%s", modelName);
			}
			else if (mesh.name.CStr()[0] != '\0' && !HasEarlierMeshNamed(m_Result, i))
			{
				snprintf(meshName, sizeof(meshName), "%s", mesh.name.CStr());
			}
			else
			{
				snprintf(meshName, sizeof(meshName), "%s_Mesh%u", modelName, i);
			}

			const bool imported = mesh.skinWeights.IsEmpty()
				? ImportMesh(mesh, outputFolderPath, meshName, m_MaterialIDs, options)
				: ImportSkinnedMesh(mesh, m_Result.skeleton, skeletonID, outputFolderPath, meshName, m_MaterialIDs);
			if (!imported)
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
			// Materials made from the same images, such as one texture set in several colours,
			// share the textures the first of them imported.
			for (uint j = 0; j < i; ++j)
			{
				const PbrMaterialImportDesc& earlier = result.materials[j];
				uint dependencyCount;
				const AssetID* textures = AssetRegistry::Instance().GetAssetDependencies(outMaterialIDs[j], dependencyCount);
				if (dependencyCount <= MaterialConstants::c_PbrAoRoughnessMetallicIndex)
				{
					continue;
				}
				const bool sameAlpha = earlier.params.blendMode == desc.params.blendMode && earlier.params.alphaCutoff == desc.params.alphaCutoff;
				if (desc.albedoID == 0 && desc.albedoSource.IsPresent() && sameAlpha && IsSameSource(earlier.albedoSource, desc.albedoSource))
				{
					desc.albedoID = textures[MaterialConstants::c_PbrAlbedoIndex];
				}
				if (desc.normalHeightID == 0 && desc.normalSource.IsPresent() && IsSameSource(earlier.normalSource, desc.normalSource)
					&& IsSameSource(earlier.heightSource, desc.heightSource))
				{
					desc.normalHeightID = textures[MaterialConstants::c_PbrNormalHeightIndex];
				}
				if (desc.aoRoughnessMetallicID == 0 && desc.roughnessMetallicSource.IsPresent()
					&& IsSameSource(earlier.roughnessMetallicSource, desc.roughnessMetallicSource) && IsSameSource(earlier.occlusionSource, desc.occlusionSource))
				{
					desc.aoRoughnessMetallicID = textures[MaterialConstants::c_PbrAoRoughnessMetallicIndex];
				}
			}

			char materialName[PathConstants::c_MaxAssetNameTotalSize];
			snprintf(materialName, sizeof(materialName), "%s_Material%u", modelName, i);

			desc.outputFolderPath = outputFolderPath;
			desc.materialName = materialName;

			AssetID materialID;
			if (!MaterialImporter::Instance().ImportPbrMaterial(desc, materialID))
			{
				TYR_LOG_ERROR("Failed to import material %s for model %s.", materialName, modelName);
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

		// Slots are given out in the order the mesh first uses each of the model's materials, so a
		// mesh only holds the materials it draws with.
		constexpr uint c_NoSlot = ~0u;
		m_MaterialSlots.Clear();
		m_MaterialSlots.Resize(materialIDs.Size(), c_NoSlot);
		for (const ModelImportSubmesh& submesh : mesh.submeshes)
		{
			if (m_MaterialSlots[submesh.materialIndex] == c_NoSlot)
			{
				m_MaterialSlots[submesh.materialIndex] = m_Header.materials.Size();
				m_Header.materials.Add(materialIDs[submesh.materialIndex]);
			}
		}
		if (m_Header.materials.Size() > MeshConstants::c_MaxSubmeshes)
		{
			TYR_LOG_ERROR("Mesh %s uses %u materials - at most %u are supported.", meshName, m_Header.materials.Size(), (uint)MeshConstants::c_MaxSubmeshes);
			return false;
		}

		// m_Header.lods/submeshes/chunks are filled in by ImportMeshGeometry below, one LOD at a
		// time - every LOD shares the same submesh/material structure as the source mesh, just
		// with progressively simplified geometry, so there's nothing LOD-independent to set up
		// here first.

		char meshPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(meshPath, sizeof(meshPath), "%s/%s%s", outputFolderPath, meshName, AssetConstants::c_MeshFileExtension);

		char absMeshFolderPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absMeshFolderPath, outputFolderPath);

		if (!PathUtil::CreateDirectories(absMeshFolderPath))
		{
			TYR_LOG_ERROR("Error creating directory %s.", absMeshFolderPath);
			return false;
		}

		char absMeshFilePath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absMeshFilePath, meshPath);

		// The header points at the collision mesh, so it's written first.
		ImportCollisionMesh(mesh, outputFolderPath, meshName, m_Header.collision);

		if (!ImportMeshGeometry(mesh, m_Header, options, absMeshFilePath, m_MaterialSlots.Data()))
		{
			TYR_LOG_ERROR("Failed to export geometry for mesh %s.", meshName);
			return false;
		}

		m_Dependencies.Clear();
		m_Dependencies.Reserve(m_Header.materials.Size() + 1);
		for (const AssetID& materialID : m_Header.materials)
		{
			m_Dependencies.Add(materialID);
		}
		if (AssetUtil::IsValidAssetID(m_Header.collision))
		{
			m_Dependencies.Add(m_Header.collision);
		}
		AssetRegistry::Instance().AddOrUpdateAsset(meshPath, m_Dependencies.Data(), m_Dependencies.Size());
		return true;
	}

	void ModelImporter::ImportCollisionMesh(const ModelImportMesh& mesh, const char* outputFolderPath, const char* meshName, AssetID& outCollisionID) const
	{
		outCollisionID = AssetConstants::c_InvalidAssetID;

		// Each UCX_ mesh is one convex piece. Without any, the mesh's own triangles are used.
		bool authored = false;
		for (uint i = 0; i < m_Result.meshCount && !authored; ++i)
		{
			authored = IsCollisionMesh(m_Result.meshes[i]) && IsCollisionFor(m_Result, m_Result.meshes[i], mesh);
		}

		// Positions are gathered into one array first, so the pieces can point into it once it stops growing.
		m_CollisionPieces.Clear();
		m_CollisionPositions.Clear();
		for (uint i = 0; i < m_Result.meshCount; ++i)
		{
			const ModelImportMesh& source = m_Result.meshes[i];
			const bool used = authored ? IsCollisionMesh(source) && IsCollisionFor(m_Result, source, mesh) : &source == &mesh;
			if (!used)
			{
				continue;
			}
			CollisionMeshPiece& piece = m_CollisionPieces.ExpandOne();
			piece.vertexCount = source.vertices.Size();
			piece.indices = source.indices.Data();
			piece.indexCount = source.indices.Size();
			for (const Vertex& vertex : source.vertices)
			{
				m_CollisionPositions.Add(vertex.position);
			}
		}
		uint positionOffset = 0;
		for (CollisionMeshPiece& piece : m_CollisionPieces)
		{
			piece.positions = m_CollisionPositions.Data() + positionOffset;
			positionOffset += piece.vertexCount;
		}

		char collisionPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(collisionPath, sizeof(collisionPath), "%s/%s%s", outputFolderPath, meshName, AssetConstants::c_CollisionMeshFileExtension);
		char absCollisionPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absCollisionPath, collisionPath);

		bool written = false;
		{
			constexpr size_t c_CollisionStreamBufferSize = 65536;
			SmartStack<uint8> streamBufferStack = SmartStackAlloc<uint8>((uint)c_CollisionStreamBufferSize);
			BufferedFileStream stream(streamBufferStack, c_CollisionStreamBufferSize, absCollisionPath, BinaryStream::Operation::Write, true);
			Serialize<uint>(stream, CollisionMesh::c_FileVersion);
			written = PhysicsShapes::WriteMeshShapes(stream, m_CollisionPieces.Data(), m_CollisionPieces.Size(), meshName);
		}

		if (!written)
		{
			TYR_LOG_WARNING("Mesh %s has no collision, so Mesh and Convex Hull colliders won't work with it.", meshName);
			Platform::RemoveFile(absCollisionPath);
			AssetRegistry::Instance().RemoveAssetIfExists(collisionPath);
			return;
		}

		outCollisionID = AssetRegistry::Instance().AddOrUpdateAsset(collisionPath);
	}

	namespace
	{
		template<typename T>
		void WriteAssetFile(const char* absFilePath, const T& data)
		{
			constexpr size_t c_StreamBufferSize = 65536;
			SmartStack<uint8> streamBufferStack = SmartStackAlloc<uint8>((uint)c_StreamBufferSize);
			BufferedFileStream stream(streamBufferStack, c_StreamBufferSize, absFilePath, BinaryStream::Operation::Write, true);
			Serialize<T>(stream, data);
		}
	}

	bool ModelImporter::ImportSkeletonAndAnimations(ModelImportResult& result, const char* outputFolderPath, const char* modelName, AssetID& outSkeletonID) const
	{
		char absFolderPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absFolderPath, outputFolderPath);
		if (!PathUtil::CreateDirectories(absFolderPath))
		{
			TYR_LOG_ERROR("Error creating directory %s.", absFolderPath);
			return false;
		}

		char assetPath[PathConstants::c_MaxAssetPathTotalSize];
		char absFilePath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetRegistry& registry = AssetRegistry::Instance();

		snprintf(assetPath, sizeof(assetPath), "%s/%s_Skeleton%s", outputFolderPath, modelName, AssetConstants::c_SkeletonFileExtension);
		AssetUtil::CreateFullPath(absFilePath, assetPath);
		WriteAssetFile(absFilePath, result.skeleton);
		outSkeletonID = registry.AddOrUpdateAsset(assetPath);

		for (uint i = 0; i < result.animationCount; ++i)
		{
			ModelImportAnimation& animation = result.animations[i];

			// Clips that share a name get the clip's index added so they don't overwrite each other.
			bool nameTaken = false;
			for (uint j = 0; j < i && !nameTaken; ++j)
			{
				nameTaken = strcmp(result.animations[j].name.CStr(), animation.name.CStr()) == 0;
			}
			if (nameTaken)
			{
				snprintf(assetPath, sizeof(assetPath), "%s/%s_%s_%u%s", outputFolderPath, modelName, animation.name.CStr(), i, AssetConstants::c_AnimationFileExtension);
			}
			else
			{
				snprintf(assetPath, sizeof(assetPath), "%s/%s_%s%s", outputFolderPath, modelName, animation.name.CStr(), AssetConstants::c_AnimationFileExtension);
			}

			animation.clip.skeleton = outSkeletonID;
			AssetUtil::CreateFullPath(absFilePath, assetPath);
			WriteAssetFile(absFilePath, animation.clip);
			registry.AddOrUpdateAsset(assetPath, &outSkeletonID, 1);
		}

		return true;
	}

	bool ModelImporter::ImportSkinnedMesh(const ModelImportMesh& mesh, const Skeleton& skeleton, AssetID skeletonID, const char* outputFolderPath,
		const char* meshName, const Array<AssetID>& materialIDs) const
	{
		m_Header.Reset();
		m_Header.sphere = mesh.sphere;
		m_Header.aabbMin = mesh.aabbMin;
		m_Header.aabbMax = mesh.aabbMax;
		m_Header.skeleton = skeletonID;

		// Slots are given out in the order the mesh first uses each material.
		constexpr uint c_NoSlot = ~0u;
		m_MaterialSlots.Clear();
		m_MaterialSlots.Resize(materialIDs.Size(), c_NoSlot);
		for (const ModelImportSubmesh& submesh : mesh.submeshes)
		{
			if (m_MaterialSlots[submesh.materialIndex] == c_NoSlot)
			{
				m_MaterialSlots[submesh.materialIndex] = m_Header.materials.Size();
				m_Header.materials.Add(materialIDs[submesh.materialIndex]);
			}
		}
		if (m_Header.materials.Size() > MeshConstants::c_MaxSubmeshes)
		{
			TYR_LOG_ERROR("Skinned mesh %s uses %u materials - at most %u are supported.", meshName, m_Header.materials.Size(), (uint)MeshConstants::c_MaxSubmeshes);
			return false;
		}

		// The furthest a vertex sits from its most weighted bone's bind position.
		const uint boneCount = skeleton.BoneCount();
		SmartStack<Vector3> bonePositionsStack = SmartStackAlloc<Vector3>(boneCount);
		Vector3* bonePositions = bonePositionsStack;
		for (uint bone = 0; bone < boneCount; ++bone)
		{
			const Matrix4 bindModel = skeleton.inverseBindMatrices[bone].Inverse();
			bonePositions[bone] = Vector3(bindModel[3].x, bindModel[3].y, bindModel[3].z);
		}
		float margin = 0.0f;
		for (uint v = 0; v < mesh.vertices.Size(); ++v)
		{
			const ModelImportSkinWeights& weights = mesh.skinWeights[v];
			uint main = 0;
			for (uint i = 1; i < 4; ++i)
			{
				main = weights.weights[i] > weights.weights[main] ? i : main;
			}
			const float distance = (mesh.vertices[v].position - bonePositions[weights.bones[main]]).Length();
			margin = distance > margin ? distance : margin;
		}
		m_Header.boneBoundsMargin = margin;

		char meshPath[PathConstants::c_MaxAssetPathTotalSize];
		snprintf(meshPath, sizeof(meshPath), "%s/%s%s", outputFolderPath, meshName, AssetConstants::c_SkeletalMeshFileExtension);

		char absMeshFolderPath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absMeshFolderPath, outputFolderPath);
		if (!PathUtil::CreateDirectories(absMeshFolderPath))
		{
			TYR_LOG_ERROR("Error creating directory %s.", absMeshFolderPath);
			return false;
		}

		char absMeshFilePath[TYR_MAX_PATH_TOTAL_SIZE];
		AssetUtil::CreateFullPath(absMeshFilePath, meshPath);

		ModelImportOptions lod0Only;
		lod0Only.generateLods = false;
		if (!ImportMeshGeometry(mesh, m_Header, lod0Only, absMeshFilePath, m_MaterialSlots.Data()))
		{
			TYR_LOG_ERROR("Failed to export geometry for skinned mesh %s.", meshName);
			return false;
		}

		// Depends on its materials and its skeleton.
		LocalArray<AssetID, MeshConstants::c_MaxSubmeshes + 1> dependencies;
		for (const AssetID& materialID : m_Header.materials)
		{
			dependencies.Add(materialID);
		}
		dependencies.Add(skeletonID);
		AssetRegistry::Instance().AddOrUpdateAsset(meshPath, dependencies.Data(), dependencies.Size());
		return true;
	}

	namespace
	{
		// 64 vertices / 124 triangles is the commonly recommended sweet spot for good
		// occupancy on typical mesh-shader hardware.
		constexpr size_t c_MaxMeshletVertices = 64;
		constexpr size_t c_MaxMeshletTriangles = 124;
		// How much meshlet building favours triangles facing the same way, so more meshlets can be
		// cone culled as back-facing. 0.25 is meshoptimizer's own recommendation when cone culling.
		constexpr float c_MeshletConeWeight = 0.25f;
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
		// A source file's own LODs come with no error, so LOD1 is taken to be off by this much of
		// the mesh's size, doubling each LOD after.
		constexpr float c_PreAuthoredLodError = 0.01f;

		// Four signed bytes: the cone axis' x, y and z, then the cosine of its half angle.
		uint PackMeshletCone(const meshopt_Bounds& bounds)
		{
			return (uint)(uint8)bounds.cone_axis_s8[0]
				| ((uint)(uint8)bounds.cone_axis_s8[1] << 8)
				| ((uint)(uint8)bounds.cone_axis_s8[2] << 16)
				| ((uint)(uint8)bounds.cone_cutoff_s8 << 24);
		}

		// Weights are quantized to bytes that add up to exactly 255.
		ShaderSkinVertex PackSkinWeights(const ModelImportSkinWeights& weights)
		{
			uint quantized[4];
			uint total = 0;
			uint largest = 0;
			for (uint i = 0; i < 4; ++i)
			{
				quantized[i] = (uint)Math::Clamp(Math::Round(weights.weights[i] * 255.0f), 0.0f, 255.0f);
				total += quantized[i];
				largest = quantized[i] > quantized[largest] ? i : largest;
			}
			quantized[largest] = (uint)((int)quantized[largest] + (255 - (int)total));

			ShaderSkinVertex packed;
			packed.boneIndices = (uint)weights.bones[0] | ((uint)weights.bones[1] << 8) | ((uint)weights.bones[2] << 16) | ((uint)weights.bones[3] << 24);
			packed.boneWeights = quantized[0] | (quantized[1] << 8) | (quantized[2] << 16) | (quantized[3] << 24);
			return packed;
		}

		// Rounds down to a whole number of triangles, never below c_MinLodTriangleCount
		// unless the submesh has fewer than that to begin with.
		uint ComputeLodTargetIndexCount(uint originalIndexCount, uint lodIndex)
		{
			const float scale = Math::Pow(c_LodIndexCountFactor, (float)lodIndex);
			uint target = (uint)(originalIndexCount * scale);
			target -= target % 3;

			const uint minIndexCount = std::min(c_MinLodTriangleCount * 3, originalIndexCount);
			return target > minIndexCount ? target : minIndexCount;
		}
	}

	bool ModelImporter::ImportMeshGeometry(const ModelImportMesh& mesh, MeshHeader& header, const ModelImportOptions& options, const char* absFilePath,
		const uint* materialSlots) const
	{
		const bool skinned = !mesh.skinWeights.IsEmpty();
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

		// Turns meshoptimizer's errors, relative to the mesh's size, into mesh-space units.
		const float meshScale = meshopt_simplifyScale(&mesh.vertices[0].position.x, mesh.vertices.Size(), sizeof(Vertex));

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
			// Raised to the largest of this LOD's submeshes' errors below. LOD selection relies on
			// each LOD being at least as far off as the one before.
			lod.error = lodIndex > 0 ? header.lods[lodIndex - 1].error : 0.0f;
			if (preAuthored)
			{
				lod.error = std::max(lod.error, meshScale * c_PreAuthoredLodError * Math::Pow(2.0f, (float)(lodIndex - 1)));
			}

			for (const ModelImportSubmesh& submesh : lodSubmeshes)
			{
				MeshSubmesh& outSubmesh = header.submeshes.ExpandOne();
				// meshletOffset/meshletCount get filled in below, once this LOD's meshlets for
				// this submesh have actually been built.
				outSubmesh.meshletOffset = 0;
				outSubmesh.meshletCount = 0;
				outSubmesh.materialSlot = materialSlots ? materialSlots[submesh.materialIndex] : submesh.materialIndex;
			}

			// Worst-case bounds for everything this LOD accumulates across all its submeshes.
			// Meshlets are built per submesh, so each submesh rounds up to whole meshlets on its own.
			size_t maxMeshlets = 0;
			for (const ModelImportSubmesh& submesh : lodSubmeshes)
			{
				maxMeshlets += meshopt_buildMeshletsBound(submesh.indexCount, c_MaxMeshletVertices, c_MaxMeshletTriangles);
			}

			SmartStack<MeshChunkMeshlet> meshletsStack = SmartStackAlloc<MeshChunkMeshlet>((uint)maxMeshlets);
			SmartStack<ShaderVertex> verticesStack = SmartStackAlloc<ShaderVertex>(lodIndexCount);
			SmartStack<uint> indicesStack = SmartStackAlloc<uint>(lodIndexCount);
			// Only filled for a skinned mesh, one per vertex.
			SmartStack<ShaderSkinVertex> skinStack = SmartStackAlloc<ShaderSkinVertex>(skinned ? lodIndexCount : 1);
			MeshChunkMeshlet* meshlets = meshletsStack;
			ShaderVertex* vertices = verticesStack;
			uint* indices = indicesStack;
			ShaderSkinVertex* skinVertices = skinStack;

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
					float relativeError = 0.0f;
					const size_t newIndexCount = meshopt_simplify(simplifiedIndices, originalSubmeshIndices, submesh.indexCount,
						&lodVertices[0].position.x, lodVertices.Size(), sizeof(Vertex),
						targetIndexCount, c_LodSimplifyTargetError, meshopt_SimplifyLockBorder, &relativeError);

					submeshIndices = simplifiedIndices;
					submeshIndexCount = (uint)newIndexCount;
					lod.error = std::max(lod.error, relativeError * meshScale);
				}

				const size_t submeshMaxMeshlets = meshopt_buildMeshletsBound(submeshIndexCount, c_MaxMeshletVertices, c_MaxMeshletTriangles);
				// Sized as meshoptimizer asks: every meshlet may be partly filled, and triangles are padded.
				SmartStack<meshopt_Meshlet> rawMeshletsStack = SmartStackAlloc<meshopt_Meshlet>((uint)submeshMaxMeshlets);
				SmartStack<uint> meshletVerticesStack = SmartStackAlloc<uint>((uint)submeshMaxMeshlets * c_MaxMeshletVertices);
				SmartStack<uint8> meshletTrianglesStack = SmartStackAlloc<uint8>((uint)submeshMaxMeshlets * c_MaxMeshletTriangles * 3);
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

					const meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshletVertices[raw.vertex_offset], &meshletTriangles[raw.triangle_offset],
						raw.triangle_count, &lodVertices[0].position.x, lodVertices.Size(), sizeof(Vertex));

					MeshChunkMeshlet& outMeshlet = meshlets[meshletCount++];
					outMeshlet.vertexOffset = vertexCount;
					outMeshlet.vertexCount = raw.vertex_count;
					outMeshlet.indexOffset = outIndexCount;
					outMeshlet.indexCount = raw.triangle_count * 3;
					outMeshlet.materialSlot = outSubmesh.materialSlot;
					outMeshlet.center = Vector3(bounds.center[0], bounds.center[1], bounds.center[2]);
					outMeshlet.radius = bounds.radius;
					outMeshlet.cone = PackMeshletCone(bounds);

					for (uint v = 0; v < raw.vertex_count; ++v)
					{
						const uint originalVertexIndex = meshletVertices[raw.vertex_offset + v];
						if (skinned)
						{
							skinVertices[vertexCount] = PackSkinWeights(mesh.skinWeights[originalVertexIndex]);
						}
						MeshUtil::VertexToShaderVertex(lodVertices[originalVertexIndex], vertices[vertexCount++]);
					}

					// triangle_offset already counts indices into meshletTriangles, not triangles.
					const uint triangleIndexCount = raw.triangle_count * 3;
					for (uint i = 0; i < triangleIndexCount; ++i)
					{
						indices[outIndexCount++] = meshletTriangles[raw.triangle_offset + i];
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

			// A skinned mesh's bone weights go after the indices.
			if (skinned)
			{
				MeshChunkSkin& chunkSkin = header.chunkSkins.ExpandOne();
				chunkSkin.skinOffset = chunk.decompressedBlobSize;
				chunkSkin.decompressedSkinSize = vertexCount * (uint)sizeof(ShaderSkinVertex);
				chunk.decompressedBlobSize += chunkSkin.decompressedSkinSize;
			}

			SmartStack<uint8> decompressedBlobStack = SmartStackAlloc<uint8>(chunk.decompressedBlobSize);
			uint8* decompressedBlob = decompressedBlobStack;
			memcpy(decompressedBlob + chunk.meshletsOffset, meshlets, chunk.decompressedMeshletsSize);
			memcpy(decompressedBlob + chunk.verticesOffset, vertices, chunk.decompressedVerticesSize);
			memcpy(decompressedBlob + chunk.indicesOffset, indices, chunk.decompressedIndicesSize);
			if (skinned)
			{
				const MeshChunkSkin& chunkSkin = header.chunkSkins.Back();
				memcpy(decompressedBlob + chunkSkin.skinOffset, skinVertices, chunkSkin.decompressedSkinSize);
			}

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
		if (skinned)
		{
			SerializeSkinnedMesh(stream, header);
		}
		else
		{
			Serialize<MeshHeader>(stream, header);
		}
		for (uint i = 0; i < m_LodCompressedBlobs.Size(); ++i)
		{
			const Array<uint8>& blob = m_LodCompressedBlobs[i];
			stream.Write(blob.Data(), blob.Size());
		}

		return true;
	}
}
