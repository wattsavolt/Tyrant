#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "IModelLoader.h"

#include <fastgltf/core.hpp>

namespace tyr
{
	// Wraps fastgltf to load glTF/GLB files into a ModelImportResult.
	//
	// Reads meshes, core PBR metallic-roughness materials, and one skin with its animations.
	// Meshes bound to a different skin are imported unskinned.
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
		ModelImportMesh* LoadMesh(size_t meshIndex, bool skinned, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap);
		bool LoadPreAuthoredLod(size_t meshIndex, ModelImportMesh& mesh, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap);
		// outSkinWeights is null for a mesh that isn't skinned.
		bool LoadMeshPrimitives(size_t meshIndex, Array<Vertex>& outVertices, Array<uint>& outIndices,
			Array<ModelImportSubmesh>& outSubmeshes, Array<ModelImportSkinWeights>* outSkinWeights,
			ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap);
		void LoadSkinWeights(const fastgltf::Primitive& primitive, uint vertexCount, ModelImportSkinWeights* outWeights) const;
		uint ResolveMaterial(size_t gltfMaterialIndex, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap);
		uint GetOrCreateDefaultMaterial(ModelImportResult& outResult);

		// Pre-authored LOD detection, run once per Load() before the main mesh-loading loop -
		// see the .cpp for the two mechanisms (MSFT_lod extension, "<name>LOD<N>" naming
		// convention) and how they combine. Fill m_LodChains (a mesh node -> its ordered
		// alternate-LOD node indices) and m_ConsumedLodNodeIndices (every node already claimed
		// as someone else's alternate, so the main loop doesn't also import it standalone).
		void ParseMsftLodChains();
		void BuildNameBasedLodChains();

		// Fills the skeleton from one glTF skin, with bones ordered parents first.
		bool BuildSkeleton(size_t skinIndex, Skeleton& outSkeleton);
		// A node's own transform in engine space.
		Matrix4 GetNodeLocalMatrix(size_t nodeIndex) const;
		void LoadAnimations(ModelImportResult& outResult);
		bool LoadAnimation(const fastgltf::Animation& animation, const Skeleton& skeleton, AnimationClip& outClip);
		// Reads a channel's key times and values, converted to engine space.
		void ReadChannelKeys(const fastgltf::AnimationSampler& sampler, fastgltf::AnimationPath path);
		Vector4 SampleChannelKeys(fastgltf::AnimationInterpolation interpolation, bool isRotation, float time) const;

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

		// The skin the skeleton was built from, or -1 when nothing is skinned.
		int m_SkinIndex = -1;
		// Per node: its parent node (-1 for scene roots) and the bone it became, if any.
		Array<int> m_NodeParents;
		Array<uint16> m_NodeToBone;
		// Per bone: the glTF node it came from, and the fixed transform of any non-bone nodes
		// between it and its parent bone, which is folded into the bone's own transform.
		Array<uint> m_BoneNodes;
		Array<Matrix4> m_BoneParentOffsets;
		Array<bool> m_BoneHasParentOffset;

		// Scratch for one animation channel and one bone's resampled keys, reused across calls.
		Array<float> m_KeyTimes;
		Array<Vector4> m_KeyValues;
		Array<Vector3> m_TrackTranslations;
		Array<Quaternion> m_TrackRotations;
		Array<Vector3> m_TrackScales;
	};
}
