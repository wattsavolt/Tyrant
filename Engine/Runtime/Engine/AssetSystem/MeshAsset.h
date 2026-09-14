#pragma once

#include "EngineMacros.h"
#include "Core.h"
#include "TextureAsset.h"
#include "RenderResource/MeshDesc.h"

namespace tyr
{
	// The mesh asset file has one mesh header and a lod header per lod. It can have any number of chunks.
	// Each chunk has a non-encrypted chunk header and Zstd encrypted blob containing the meshlets, vertices and indices. 
	// Meshlet, vertex and offsets are local to chunk indices and vertices. 
	// Each chunk will only have meshlets from the same LOD. No LOD mixing in a chunk!
	// Originally, all chunks will be loaded and one chunk per LOD but could be streamed in later based on distance/visibilty. 

	struct MeshChunkHeader
	{
		uint decompressedMeshletsSize;
		uint decompressedVerticesSize;
		uint decompressedIndicesSize;
		uint compressedBlobSize;
		uint decompressedBlobSize;
		uint blobFileOffset;
		uint meshletsOffset;
		uint verticesOffset;
		uint indicesOffset;
	};

	struct MeshLODHeader
	{
		uint chunkOffset = 0;
		uint chunkCount = 0;
		// Submeshes for this LOD only - a submesh's meshlet range/count can differ per LOD.
		uint submeshOffset = 0;
		uint submeshCount = 0;
	};

	// One material-homogeneous group of meshlets within a single LOD (matches a glTF
	// primitive, or Unreal's "section" - same idea under different names).
	struct MeshSubmesh
	{
		uint meshletOffset = 0;
		uint meshletCount = 0;
		// Index into MeshHeader::materials.
		uint materialSlot = 0;
	};

	// For rigid meshes
	// The default material will be added to the material component of the entity generated when importing the mesh (same for skeletal)
	struct MeshHeader
	{
		LocalArray<MeshLODHeader, MeshConstants::c_MaxLods> lods;
		// Currently there will be only one chunk per LOD until streamed chunks are supported.
		// This could be handy later but could be optimized to a local array if chunk streaming not used and becomes a bottleneck
		Array<MeshChunkHeader> chunks;
		// Flat list of submeshes across all LODs - each MeshLODHeader indexes its own
		// range via submeshOffset/submeshCount.
		Array<MeshSubmesh> submeshes;
		// This mesh's material slots (referenced by MeshSubmesh::materialSlot), one
		// AssetID per slot - mirrors how a glTF mesh's primitives each reference an
		// index into the asset's shared materials list.
		Array<AssetID> materials;
		BoundingSphere sphere;
		Vector3 aabbMin;
		Vector3 aabbMax;

		void Reset()
		{
			lods.Clear();
			chunks.Clear();
			submeshes.Clear();
			materials.Clear();
			sphere = {};
			aabbMin = {};
			aabbMax = {};
		}
	};

	// One per skeleton mesh. ECS system will tie them together on character import
	struct SkeletalMeshHeader
	{
		AssetID skeleton;
		// TODO: Complete
	};

	template<> void Serialize(BufferedFileStream& stream, const MeshChunkHeader& header);
	template<> void Deserialize(BufferedFileStream& stream, MeshChunkHeader& header);

	template<> void Serialize(BufferedFileStream& stream, const MeshLODHeader& header);
	template<> void Deserialize(BufferedFileStream& stream, MeshLODHeader& header);

	template<> void Serialize(BufferedFileStream& stream, const MeshSubmesh& submesh);
	template<> void Deserialize(BufferedFileStream& stream, MeshSubmesh& submesh);

	template<> void Serialize(BufferedFileStream& stream, const MeshHeader& header);
	template<> void Deserialize(BufferedFileStream& stream, MeshHeader& header);
}